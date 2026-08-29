// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "ManagedFileSink.h"

#include <spdlog/spdlog.h>

#include <zlib.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
# include <windows.h>
#else
# include <sys/statvfs.h>
#endif

using namespace ::QuickFAST;
using namespace ::QuickFAST::Common;
namespace fs = std::filesystem;

namespace
{
  /// Bound on evictions per retention pass, so a pathological directory cannot
  /// stall the caller (a rotation or a compression completion) indefinitely.
  const int DEF_MFS_MAX_EVICTIONS_PER_PASS = 1024;

  /// Copy chunk used while gzipping a rotated file.
  const std::size_t DEF_MFS_GZIP_CHUNK_BYTES = 64u * 1024u;

  const char * const DEF_MFS_GZ_SUFFIX = ".gz";
  const char * const DEF_MFS_TMP_SUFFIX = ".tmp";

  /// Timer state whose handler this thread is currently executing, if any. Lets
  /// request_shutdown() recognise a call made from its own rotation handler, where
  /// the state mutex is already held by this thread.
  thread_local const void * tls_active_rotation_state = nullptr;

  bool
  has_suffix(const std::string & value, const std::string & suffix)
  {
    return value.size() >= suffix.size()
           && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  /// @brief Rotated-file timestamp, truncated to whole seconds.
  ///
  /// The truncation is required: chrono's %S prints fractional digits for a
  /// time_point with sub-second precision, which would put nanoseconds in every
  /// rotated file name and make the same-second sequence suffix unreachable.
  std::string
  format_local_timestamp(
    const std::chrono::time_zone * zone,
    std::chrono::system_clock::time_point tp)
  {
    const auto zt =
      std::chrono::zoned_time{zone, std::chrono::floor<std::chrono::seconds>(tp)};
    return std::format("{:%Y-%m-%d_%H%M%S}", zt);
  }

  /// @returns free space percent, or nullopt when the filesystem cannot be queried.
  std::optional<unsigned>
  filesystem_free_percent(const fs::path & path)
  {
#if defined(_WIN32)
    ULARGE_INTEGER available{};
    ULARGE_INTEGER total{};
    if(GetDiskFreeSpaceExW(path.c_str(), &available, &total, nullptr) == 0
       || total.QuadPart == 0)
    {
      return std::nullopt;
    }
    const auto free_units = static_cast<double>(available.QuadPart);
    const auto total_units = static_cast<double>(total.QuadPart);
#else
    struct statvfs st {};
    if(statvfs(path.c_str(), &st) != 0 || st.f_blocks == 0)
    {
      return std::nullopt;
    }
    const auto free_units = static_cast<double>(st.f_bavail);
    const auto total_units = static_cast<double>(st.f_blocks);
#endif
    return static_cast<unsigned>((free_units * 100.0) / total_units);
  }

  /// Publishes the timer state whose handler is running, for the scope's lifetime.
  class RotationHandlerGuard
  {
  public:
    explicit RotationHandlerGuard(const void * state)
      : previous_(tls_active_rotation_state)
    {
      tls_active_rotation_state = state;
    }

    ~RotationHandlerGuard()
    {
      tls_active_rotation_state = previous_;
    }

    RotationHandlerGuard(const RotationHandlerGuard &) = delete;
    RotationHandlerGuard & operator=(const RotationHandlerGuard &) = delete;

  private:
    const void * previous_;
  };
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
  if(cfg_.retention == RetentionMode::ManagedBytes
     && cfg_.max_file_bytes > cfg_.max_managed_bytes)
  {
    // Retention never evicts the active file, so this combination cannot be
    // honoured: the active file alone would exceed the budget and eviction would
    // have nothing left to reclaim.
    throw std::invalid_argument(
      "ManagedFileSinkConfig.max_file_bytes must not exceed max_managed_bytes");
  }

  this->set_pattern(cfg_.pattern);

  timer_state_->sink = this;

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
managed_file_sink_mt::next_rotation_after(
  const std::chrono::time_zone * zone,
  std::chrono::system_clock::time_point from,
  int hour,
  int minute)
{
  using namespace std::chrono;
  const zoned_time zt{zone, floor<seconds>(from)};
  const local_time<seconds> local = zt.get_local_time();
  const local_days day = floor<days>(local);
  local_time<seconds> candidate =
    local_time<seconds>{day} + hours{hour} + minutes{minute};
  if(candidate <= local)
  {
    candidate += days{1};
  }
  // choose::latest is required, not a preference: the two-argument zoned_time
  // throws when the local time falls in a DST gap or is ambiguous, and midnight
  // is exactly such a time once a year in zones that shift the clock at 24:00
  // (America/Santiago, America/Havana, Asia/Beirut, ...). A throw here would
  // escape the Asio completion handler and tear down the shared io_context.
  // For a gap this yields the instant the offset changes; for an ambiguous time,
  // the later of the two.
  return zoned_time{zone, candidate, choose::latest}.get_sys_time();
}

std::chrono::system_clock::time_point
managed_file_sink_mt::next_rotation_tp_(std::chrono::system_clock::time_point from) const
{
  return next_rotation_after(zone_, from, cfg_.rotation_hour, cfg_.rotation_minute);
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
  // Compare at the resolution the file name carries, so repeated rotations inside
  // one second get distinct ".N" suffixes instead of overwriting each other.
  const auto second = std::chrono::floor<std::chrono::seconds>(when);
  if(last_rotate_tp_ == second)
  {
    ++same_second_seq_;
  }
  else
  {
    last_rotate_tp_ = second;
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
  rotation_timer_.expires_at(next_rotation_tp_(std::chrono::system_clock::now()));
  async_wait_rotation_unlocked_();
}

void
managed_file_sink_mt::async_wait_rotation_unlocked_()
{
  rotation_timer_.async_wait(
    [state = std::weak_ptr<RotationTimerState>(timer_state_)](const asio::error_code & ec)
    {
      const std::shared_ptr<RotationTimerState> locked = state.lock();
      if(!locked)
      {
        return;
      }
      std::lock_guard<std::mutex> lock(locked->mutex);
      if(locked->sink == nullptr)
      {
        return;
      }
      const RotationHandlerGuard guard(locked.get());
      locked->sink->on_rotation_timer_(ec);
    });
}

void
managed_file_sink_mt::on_rotation_timer_(const asio::error_code & ec)
{
  if(ec == asio::error::operation_aborted)
  {
    return;
  }
  // This runs as an Asio completion handler on an io_context the application also
  // uses for its feed handlers. An escaping exception would propagate out of
  // io_context::run() and take that thread down, so losing a rotation is always
  // preferable to letting one out.
  try
  {
    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    if(shutting_down_)
    {
      return;
    }
    rotate_unlocked_(std::chrono::system_clock::now());
    arm_rotation_timer_unlocked_();
  }
  catch(...)
  {
    rotation_failures_.fetch_add(1, std::memory_order_relaxed);
    reschedule_after_failure_();
  }
}

void
managed_file_sink_mt::reschedule_after_failure_()
{
  // Keep the schedule alive so one failed rotation does not disable time-based
  // rotation until the process restarts.
  try
  {
    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    if(!shutting_down_)
    {
      arm_rotation_timer_unlocked_();
    }
  }
  catch(...)
  {
    // Re-arming failed too, so there is no live timer any more and nothing else is
    // safe to attempt from a completion handler. Publish the state instead of
    // swallowing it: size-based rotation in sink_it_() still works, but scheduled
    // rotation is gone until the process restarts.
    rotation_schedule_lost_.store(true, std::memory_order_relaxed);
  }
}

std::uint64_t
managed_file_sink_mt::rotation_failures() const noexcept
{
  return rotation_failures_.load(std::memory_order_relaxed);
}

bool
managed_file_sink_mt::rotation_schedule_lost() const noexcept
{
  return rotation_schedule_lost_.load(std::memory_order_relaxed);
}

void
managed_file_sink_mt::detach_timer_state_()
{
  if(tls_active_rotation_state == timer_state_.get())
  {
    // Called from this sink's own rotation handler, which already holds the state
    // mutex on this thread; locking again would deadlock.
    timer_state_->sink = nullptr;
    return;
  }
  // Blocks until a handler that is already running has returned. Deliberately does
  // not wait for merely queued handlers: a stopped io_context would never run them.
  std::lock_guard<std::mutex> lock(timer_state_->mutex);
  timer_state_->sink = nullptr;
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
  if(written != nbytes)
  {
    throw spdlog::spdlog_ex(
      "Short write to log file: " + cfg_.base_path.string(), errno);
  }
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
  // Before the sink mutex: rotation handlers take the state mutex first, so the
  // reverse order here would risk a deadlock.
  detach_timer_state_();

  {
    std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
    if(!shutting_down_)
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

void
managed_file_sink_mt::rotate_now()
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  rotate_unlocked_(std::chrono::system_clock::now());
}

bool
managed_file_sink_mt::wait_for_compression_idle(std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::unique_lock<std::mutex> lock(compress_mutex_);
  return compress_cv_.wait_until(lock, deadline, [this] {
    return compress_queue_.empty() && inflight_compress_.empty();
  });
}

std::uint64_t
managed_file_sink_mt::managed_bytes()
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  return managed_bytes_unlocked_();
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
void
managed_file_sink_mt::arm_rotation_after_for_test(std::chrono::milliseconds delay)
{
  std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
  if(shutting_down_)
  {
    return;
  }
  rotation_timer_.expires_after(delay);
  async_wait_rotation_unlocked_();
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

    const fs::path gz = path.string() + DEF_MFS_GZ_SUFFIX;
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
  const fs::path tmp = fs::path(dst_gz.string() + DEF_MFS_TMP_SUFFIX);
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

  char buffer[DEF_MFS_GZIP_CHUNK_BYTES];
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
  return has_suffix(name, extension_)
         || has_suffix(name, extension_ + DEF_MFS_GZ_SUFFIX);
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
    // A fresh code per entry: sharing one would let a single unstatable
    // entry, such as a dangling symlink, short-circuit every entry after it.
    std::error_code entryEc;
    if(!entry.is_regular_file(entryEc) || entryEc)
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
    if(has_suffix(p.filename().string(), DEF_MFS_TMP_SUFFIX))
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
  // file_size reports failure by returning uintmax_t(-1), so an unchecked
  // error_code would wrap the total and make retention evict good logs.  A
  // file vanishing under us is routine: the compress worker removes files
  // outside the sink mutex.
  const auto add_size = [](std::uint64_t & running, const fs::path & path) {
    std::error_code sizeEc;
    const auto size = fs::file_size(path, sizeEc);
    if(!sizeEc)
    {
      running += static_cast<std::uint64_t>(size);
    }
  };

  std::uint64_t total = 0;
  std::error_code ec;
  if(fs::exists(cfg_.base_path, ec))
  {
    add_size(total, cfg_.base_path);
  }
  for(const auto & p : list_managed_finished_unlocked_())
  {
    add_size(total, p);
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
  const auto free_percent = filesystem_free_percent(dir_);
  if(!free_percent.has_value())
  {
    // Free space is unknown; evicting on a guess could discard logs that are still
    // wanted, so treat the policy as not triggered.
    return false;
  }
  return *free_percent < cfg_.free_percent_min;
}

void
managed_file_sink_mt::apply_retention_unlocked_()
{
  for(int i = 0; i < DEF_MFS_MAX_EVICTIONS_PER_PASS && retention_exceeded_unlocked_(); ++i)
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
    // See list_managed_finished_unlocked_: a shared error_code would drop
    // every entry after the first one that cannot be stat'd.
    std::error_code entryEc;
    if(!entry.is_regular_file(entryEc) || entryEc)
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
    if(has_suffix(name, DEF_MFS_GZ_SUFFIX) || has_suffix(name, DEF_MFS_TMP_SUFFIX))
    {
      continue;
    }
    const fs::path gz = fs::path(p.string() + DEF_MFS_GZ_SUFFIX);
    if(fs::exists(gz, entryEc))
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
