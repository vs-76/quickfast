// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "ManagedFileSink.h"

#include <spdlog/spdlog.h>

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/statvfs.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Common;
namespace fs = std::filesystem;

namespace
{
  std::string
  format_local_timestamp(
    const std::chrono::time_zone * zone,
    std::chrono::system_clock::time_point tp)
  {
    const auto zt = std::chrono::zoned_time{zone, tp};
    return std::format("{:%Y-%m-%d_%H%M%S}", zt);
  }

  unsigned
  filesystem_free_percent(const fs::path & path)
  {
    struct statvfs st {};
    if(statvfs(path.c_str(), &st) != 0)
    {
      return 100;
    }
    if(st.f_blocks == 0)
    {
      return 100;
    }
    const auto free_blocks = static_cast<double>(st.f_bavail);
    const auto total_blocks = static_cast<double>(st.f_blocks);
    return static_cast<unsigned>((free_blocks * 100.0) / total_blocks);
  }
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::base_path(std::filesystem::path path)
{
  cfg_.base_path = std::move(path);
  has_base_path_ = true;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::max_file_bytes(std::size_t bytes)
{
  cfg_.max_file_bytes = bytes;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::retention(RetentionMode mode)
{
  cfg_.retention = mode;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::max_managed_bytes(std::size_t bytes)
{
  cfg_.max_managed_bytes = bytes;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::free_percent_min(unsigned percent)
{
  cfg_.free_percent_min = percent;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::time_zone(std::string zone)
{
  cfg_.time_zone = std::move(zone);
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::system_time_zone()
{
  cfg_.time_zone = std::nullopt;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::rotation_time(int hour, int minute)
{
  cfg_.rotation_hour = hour;
  cfg_.rotation_minute = minute;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::compress(bool enabled)
{
  cfg_.compress = enabled;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::gzip_level(int level)
{
  cfg_.gzip_level = level;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::recover_uncompressed_on_start(bool enabled)
{
  cfg_.recover_uncompressed_on_start = enabled;
  return *this;
}

ManagedFileSinkConfigBuilder &
ManagedFileSinkConfigBuilder::pattern(std::string spdlog_pattern)
{
  cfg_.pattern = std::move(spdlog_pattern);
  return *this;
}

ManagedFileSinkConfig
ManagedFileSinkConfigBuilder::build() const
{
  if(!has_base_path_)
  {
    throw std::invalid_argument(
      "ManagedFileSinkConfigBuilder: base_path() is required before build()");
  }
  if(cfg_.pattern.empty())
  {
    throw std::invalid_argument(
      "ManagedFileSinkConfigBuilder: pattern must not be empty");
  }
  return cfg_;
}

managed_file_sink_mt::managed_file_sink_mt(asio::io_context & io, ManagedFileSinkConfig cfg)
  : cfg_(std::move(cfg))
  , zone_(resolve_zone_())
  , io_(io)
  , rotation_timer_(io_)
{
  if(cfg_.base_path.empty())
  {
    throw std::invalid_argument("ManagedFileSinkConfig.base_path must not be empty");
  }
  if(cfg_.pattern.empty())
  {
    throw std::invalid_argument("ManagedFileSinkConfig.pattern must not be empty");
  }
  if(cfg_.rotation_hour < 0 || cfg_.rotation_hour > 23
     || cfg_.rotation_minute < 0 || cfg_.rotation_minute > 59)
  {
    throw std::invalid_argument("ManagedFileSinkConfig rotation time out of range");
  }
  if(cfg_.gzip_level < 0 || cfg_.gzip_level > 9)
  {
    throw std::invalid_argument("ManagedFileSinkConfig.gzip_level must be 0..9");
  }
  if(cfg_.max_file_bytes == 0)
  {
    throw std::invalid_argument("ManagedFileSinkConfig.max_file_bytes must be > 0");
  }

  this->set_pattern(cfg_.pattern);

  dir_ = cfg_.base_path.parent_path();
  if(dir_.empty())
  {
    dir_ = ".";
  }
  stem_ = cfg_.base_path.stem().string();
  extension_ = cfg_.base_path.extension().string();
  if(extension_.empty())
  {
    extension_ = ".log";
  }

  fs::create_directories(dir_);

  {
    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    open_active_file_unlocked_();
    if(cfg_.recover_uncompressed_on_start && cfg_.compress)
    {
      recover_uncompressed_unlocked_();
    }
    apply_retention_unlocked_();
    arm_rotation_timer_unlocked_();
  }

  if(cfg_.compress)
  {
    compress_thread_ = std::thread([this] { compress_worker_(); });
  }
}

managed_file_sink_mt::~managed_file_sink_mt()
{
  request_shutdown();
}

const std::chrono::time_zone *
managed_file_sink_mt::resolve_zone_() const
{
  if(!cfg_.time_zone.has_value() || cfg_.time_zone->empty())
  {
    return std::chrono::current_zone();
  }
  return std::chrono::locate_zone(*cfg_.time_zone);
}

std::chrono::system_clock::time_point
managed_file_sink_mt::next_rotation_tp_(std::chrono::system_clock::time_point from) const
{
  using namespace std::chrono;
  const zoned_time zt{zone_, floor<seconds>(from)};
  const local_time<seconds> local = zt.get_local_time();
  const local_days day = floor<days>(local);
  local_time<seconds> candidate =
    local_time<seconds>{day} + hours{cfg_.rotation_hour} + minutes{cfg_.rotation_minute};
  if(candidate <= local)
  {
    candidate += days{1};
  }
  return zoned_time{zone_, candidate}.get_sys_time();
}

void
managed_file_sink_mt::open_active_file_unlocked_()
{
  if(file_ != nullptr)
  {
    std::fflush(file_);
    std::fclose(file_);
    file_ = nullptr;
  }
  file_ = std::fopen(cfg_.base_path.string().c_str(), "ab");
  if(file_ == nullptr)
  {
    throw spdlog::spdlog_ex("Failed to open log file: " + cfg_.base_path.string());
  }
  if(std::fseek(file_, 0, SEEK_END) != 0)
  {
    std::fclose(file_);
    file_ = nullptr;
    throw spdlog::spdlog_ex("Failed to seek log file: " + cfg_.base_path.string());
  }
  const long pos = std::ftell(file_);
  current_size_ = pos >= 0 ? static_cast<std::size_t>(pos) : 0;
}

std::filesystem::path
managed_file_sink_mt::make_rotated_path_unlocked_(std::chrono::system_clock::time_point when)
{
  if(last_rotate_tp_ == when)
  {
    ++same_second_seq_;
  }
  else
  {
    last_rotate_tp_ = when;
    same_second_seq_ = 0;
  }

  std::ostringstream name;
  name << stem_ << '.' << format_local_timestamp(zone_, when);
  if(same_second_seq_ > 0)
  {
    name << '.' << same_second_seq_;
  }
  name << extension_;
  return dir_ / name.str();
}

void
managed_file_sink_mt::rotate_unlocked_(std::chrono::system_clock::time_point when)
{
  if(file_ == nullptr || shutting_down_)
  {
    return;
  }

  std::fflush(file_);
  std::fclose(file_);
  file_ = nullptr;

  const fs::path rotated = make_rotated_path_unlocked_(when);
  std::error_code ec;
  fs::rename(cfg_.base_path, rotated, ec);
  if(ec)
  {
    // If rename fails (e.g. empty never created), reopen active and continue.
    open_active_file_unlocked_();
    return;
  }

  open_active_file_unlocked_();
  if(cfg_.compress)
  {
    enqueue_compress_unlocked_(rotated);
  }
  apply_retention_unlocked_();
}

void
managed_file_sink_mt::arm_rotation_timer_unlocked_()
{
  if(shutting_down_)
  {
    return;
  }
  const auto next = next_rotation_tp_(std::chrono::system_clock::now());
  rotation_timer_.expires_at(next);
  rotation_timer_.async_wait(
    [self = this](const asio::error_code & ec) { self->on_rotation_timer_(ec); });
}

void
managed_file_sink_mt::on_rotation_timer_(const asio::error_code & ec)
{
  if(ec == asio::error::operation_aborted)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  if(shutting_down_)
  {
    return;
  }
  rotate_unlocked_(std::chrono::system_clock::now());
  arm_rotation_timer_unlocked_();
}

void
managed_file_sink_mt::sink_it_(const spdlog::details::log_msg & msg)
{
  if(shutting_down_ || file_ == nullptr)
  {
    return;
  }

  spdlog::memory_buf_t formatted;
  base_sink<std::mutex>::formatter_->format(msg, formatted);
  const std::size_t nbytes = formatted.size();

  if(current_size_ > 0 && current_size_ + nbytes >= cfg_.max_file_bytes)
  {
    rotate_unlocked_(msg.time);
  }

  if(file_ == nullptr)
  {
    return;
  }
  const std::size_t written = std::fwrite(formatted.data(), 1, nbytes, file_);
  current_size_ += written;
}

void
managed_file_sink_mt::flush_()
{
  if(file_ != nullptr)
  {
    std::fflush(file_);
  }
}

void
managed_file_sink_mt::request_shutdown()
{
  {
    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    if(shutting_down_)
    {
      // Still join compress thread below if needed.
    }
    else
    {
      shutting_down_ = true;
      rotation_timer_.cancel();
      if(file_ != nullptr)
      {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(compress_mutex_);
    compress_stop_ = true;
  }
  compress_cv_.notify_all();
  if(compress_thread_.joinable())
  {
    compress_thread_.join();
  }
}

std::filesystem::path
managed_file_sink_mt::active_filename() const
{
  return cfg_.base_path;
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
void
managed_file_sink_mt::force_time_rotation_for_test()
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  rotate_unlocked_(std::chrono::system_clock::now());
}

bool
managed_file_sink_mt::wait_for_compression_idle_for_test(std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::unique_lock<std::mutex> lock(compress_mutex_);
  return compress_cv_.wait_until(lock, deadline, [this] {
    return compress_queue_.empty() && inflight_compress_.empty();
  });
}

void
managed_file_sink_mt::arm_rotation_after_for_test(std::chrono::milliseconds delay)
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  if(shutting_down_)
  {
    return;
  }
  rotation_timer_.expires_after(delay);
  rotation_timer_.async_wait(
    [self = this](const asio::error_code & ec) { self->on_rotation_timer_(ec); });
}

std::uint64_t
managed_file_sink_mt::managed_bytes_for_test()
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  return managed_bytes_unlocked_();
}
#endif

void
managed_file_sink_mt::enqueue_compress_unlocked_(std::filesystem::path path)
{
  {
    std::lock_guard<std::mutex> lock(compress_mutex_);
    if(compress_stop_)
    {
      return;
    }
    if(!inflight_compress_.insert(path).second)
    {
      return;
    }
    compress_queue_.push_back(std::move(path));
  }
  compress_cv_.notify_one();
}

void
managed_file_sink_mt::compress_worker_()
{
  for(;;)
  {
    fs::path path;
    {
      std::unique_lock<std::mutex> lock(compress_mutex_);
      compress_cv_.wait(lock, [this] {
        return compress_stop_ || !compress_queue_.empty();
      });
      if(compress_queue_.empty())
      {
        if(compress_stop_)
        {
          return;
        }
        continue;
      }
      path = std::move(compress_queue_.front());
      compress_queue_.pop_front();
    }

    const fs::path gz = path.string() + ".gz";
    const bool ok = gzip_file_(path, gz, cfg_.gzip_level);
    if(ok)
    {
      std::error_code ec;
      fs::remove(path, ec);
    }

    {
      std::lock_guard<std::mutex> clock(compress_mutex_);
      inflight_compress_.erase(path);
    }
    compress_cv_.notify_all();

    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    apply_retention_unlocked_();
  }
}

bool
managed_file_sink_mt::gzip_file_(
  const fs::path & src,
  const fs::path & dst_gz,
  int level)
{
  const fs::path tmp = fs::path(dst_gz.string() + ".tmp");
  std::ifstream in(src, std::ios::binary);
  if(!in)
  {
    return false;
  }

  std::string mode = "wb";
  mode.push_back(static_cast<char>('0' + level));
  gzFile out = gzopen(tmp.string().c_str(), mode.c_str());
  if(out == nullptr)
  {
    return false;
  }

  char buffer[64 * 1024];
  bool ok = true;
  while(in)
  {
    in.read(buffer, sizeof(buffer));
    const std::streamsize got = in.gcount();
    if(got > 0)
    {
      if(gzwrite(out, buffer, static_cast<unsigned>(got)) != static_cast<int>(got))
      {
        ok = false;
        break;
      }
    }
  }
  if(gzclose(out) != Z_OK)
  {
    ok = false;
  }
  in.close();

  if(!ok)
  {
    std::error_code ec;
    fs::remove(tmp, ec);
    return false;
  }

  std::error_code ec;
  fs::rename(tmp, dst_gz, ec);
  if(ec)
  {
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

bool
managed_file_sink_mt::is_managed_name_(const fs::path & path) const
{
  const std::string name = path.filename().string();
  if(name == cfg_.base_path.filename().string())
  {
    return true;
  }
  // stem.YYYY-MM-DD_HHMMSS[.N].ext or + .gz
  const std::string prefix = stem_ + ".";
  if(name.rfind(prefix, 0) != 0)
  {
    return false;
  }
  return name.find(extension_) != std::string::npos
         || (name.size() > 3 && name.substr(name.size() - 3) == ".gz");
}

std::vector<std::filesystem::path>
managed_file_sink_mt::list_managed_finished_unlocked_() const
{
  std::vector<fs::path> out;
  std::error_code ec;
  if(!fs::exists(dir_, ec))
  {
    return out;
  }
  for(const auto & entry : fs::directory_iterator(dir_, ec))
  {
    if(ec || !entry.is_regular_file(ec))
    {
      continue;
    }
    const fs::path p = entry.path();
    if(p == cfg_.base_path)
    {
      continue;
    }
    if(!is_managed_name_(p))
    {
      continue;
    }
    {
      std::lock_guard<std::mutex> clock(compress_mutex_);
      if(inflight_compress_.count(p) != 0)
      {
        continue;
      }
    }
    // Skip incomplete temp compress outputs.
    const std::string name = p.filename().string();
    if(name.size() >= 4 && name.substr(name.size() - 4) == ".tmp")
    {
      continue;
    }
    out.push_back(p);
  }
  std::sort(out.begin(), out.end(), [](const fs::path & a, const fs::path & b) {
    std::error_code e1;
    std::error_code e2;
    const auto ta = fs::last_write_time(a, e1);
    const auto tb = fs::last_write_time(b, e2);
    if(e1 || e2)
    {
      return a.filename().string() < b.filename().string();
    }
    return ta < tb;
  });
  return out;
}

std::uint64_t
managed_file_sink_mt::managed_bytes_unlocked_() const
{
  std::uint64_t total = 0;
  std::error_code ec;
  if(fs::exists(cfg_.base_path, ec))
  {
    total += static_cast<std::uint64_t>(fs::file_size(cfg_.base_path, ec));
  }
  for(const auto & p : list_managed_finished_unlocked_())
  {
    total += static_cast<std::uint64_t>(fs::file_size(p, ec));
  }
  return total;
}

bool
managed_file_sink_mt::retention_exceeded_unlocked_() const
{
  if(cfg_.retention == RetentionMode::ManagedBytes)
  {
    return managed_bytes_unlocked_() > cfg_.max_managed_bytes;
  }
  return filesystem_free_percent(dir_) < cfg_.free_percent_min;
}

void
managed_file_sink_mt::apply_retention_unlocked_()
{
  // Bound loops to avoid pathological stalls.
  for(int i = 0; i < 1024 && retention_exceeded_unlocked_(); ++i)
  {
    auto finished = list_managed_finished_unlocked_();
    if(finished.empty())
    {
      break;
    }
    std::error_code ec;
    fs::remove(finished.front(), ec);
    if(ec)
    {
      break;
    }
  }
}

void
managed_file_sink_mt::recover_uncompressed_unlocked_()
{
  std::error_code ec;
  if(!fs::exists(dir_, ec))
  {
    return;
  }
  std::vector<fs::path> pending;
  for(const auto & entry : fs::directory_iterator(dir_, ec))
  {
    if(ec || !entry.is_regular_file(ec))
    {
      continue;
    }
    const fs::path p = entry.path();
    if(p == cfg_.base_path)
    {
      continue;
    }
    if(!is_managed_name_(p))
    {
      continue;
    }
    const std::string name = p.filename().string();
    if(name.size() >= 3 && name.substr(name.size() - 3) == ".gz")
    {
      continue;
    }
    if(name.size() >= 4 && name.substr(name.size() - 4) == ".tmp")
    {
      continue;
    }
    const fs::path gz = fs::path(p.string() + ".gz");
    if(fs::exists(gz, ec))
    {
      continue;
    }
    pending.push_back(p);
  }
  std::sort(pending.begin(), pending.end());
  for(auto & p : pending)
  {
    enqueue_compress_unlocked_(std::move(p));
  }
}
