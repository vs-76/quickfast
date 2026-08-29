// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Common/ManagedFileSink.h>
#include <Common/SpdlogLogger.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <zlib.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace QuickFAST;
using namespace QuickFAST::Common;
namespace fs = std::filesystem;

namespace
{
  class IoRunner
  {
  public:
    IoRunner()
      : work_(asio::make_work_guard(io_))
      , thread_([this] { io_.run(); })
    {
    }

    ~IoRunner()
    {
      stop();
    }

    asio::io_context & io()
    {
      return io_;
    }

    void stop()
    {
      if(stopped_)
      {
        return;
      }
      stopped_ = true;
      work_.reset();
      io_.stop();
      if(thread_.joinable())
      {
        thread_.join();
      }
    }

  private:
    asio::io_context io_;
    asio::executor_work_guard<asio::io_context::executor_type> work_;
    std::thread thread_;
    bool stopped_ = false;
  };

  std::string
  gunzip_to_string(const fs::path & gz_path)
  {
    gzFile in = gzopen(gz_path.string().c_str(), "rb");
    EXPECT_NE(nullptr, in);
    if(in == nullptr)
    {
      return {};
    }
    std::string out;
    char buf[4096];
    int n = 0;
    while((n = gzread(in, buf, sizeof(buf))) > 0)
    {
      out.append(buf, static_cast<std::size_t>(n));
    }
    gzclose(in);
    return out;
  }

  std::vector<fs::path>
  list_regular_files(const fs::path & dir)
  {
    std::vector<fs::path> out;
    std::error_code ec;
    if(!fs::exists(dir, ec))
    {
      return out;
    }
    for(const auto & e : fs::directory_iterator(dir, ec))
    {
      if(!ec && e.is_regular_file(ec))
      {
        out.push_back(e.path());
      }
    }
    std::sort(out.begin(), out.end());
    return out;
  }

  std::size_t
  count_suffix(const std::vector<fs::path> & files, const std::string & suffix)
  {
    std::size_t n = 0;
    for(const auto & p : files)
    {
      const std::string name = p.filename().string();
      if(name.size() >= suffix.size()
         && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
      {
        ++n;
      }
    }
    return n;
  }

  bool
  wait_until(std::chrono::milliseconds timeout, const std::function<bool()> & pred)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while(std::chrono::steady_clock::now() < deadline)
    {
      if(pred())
      {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
  }

  class ManagedFileSinkTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      const auto * info = ::testing::UnitTest::GetInstance()->current_test_info();
      dir_ = fs::temp_directory_path()
             / (std::string("qf_mfs_") + info->name() + "_"
                + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
      fs::remove_all(dir_);
      fs::create_directories(dir_);
    }

    void TearDown() override
    {
      if(sink_)
      {
        sink_->request_shutdown();
      }
      // Stop the io_context before destroying the sink so cancelled timer
      // handlers cannot run against a freed managed_file_sink_mt.
      runner_.stop();
      sink_.reset();
      std::error_code ec;
      fs::remove_all(dir_, ec);
    }

    ManagedFileSinkConfig defaultConfig() const
    {
      return ManagedFileSinkConfigBuilder()
        .base_path(dir_ / "app.log")
        .max_file_bytes(1ull << 20)
        .max_managed_bytes(32ull << 20)
        .compress(true)
        .gzip_level(1)
        .recover_uncompressed_on_start(true)
        .pattern("%v")
        .build();
    }

    std::shared_ptr<spdlog::logger> makeLogger(
      ManagedFileSinkConfig cfg,
      const std::string & name)
    {
      sink_ = std::make_shared<managed_file_sink_mt>(runner_.io(), std::move(cfg));
      auto logger = std::make_shared<spdlog::logger>(name, sink_);
      // Pattern comes from ManagedFileSinkConfig (applied on the sink).
      logger->set_level(spdlog::level::trace);
      return logger;
    }

    IoRunner runner_;
    fs::path dir_;
    std::shared_ptr<managed_file_sink_mt> sink_;
  };
}

TEST_F(ManagedFileSinkTest, RejectsInvalidZone)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.time_zone = "Not/A_Real_Zone_XYZ";
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::runtime_error);
}

TEST_F(ManagedFileSinkTest, RejectsEmptyBasePath)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.base_path.clear();
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, RejectsBadRotationHour)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.rotation_hour = 24;
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, RejectsBadRotationMinute)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.rotation_minute = 60;
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, RejectsBadGzipLevel)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.gzip_level = 10;
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, RejectsZeroMaxFileBytes)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 0;
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, AcceptsUtcZoneAndEmptyMeansSystem)
{
  {
    ManagedFileSinkConfig cfg = defaultConfig();
    cfg.time_zone = "UTC";
    auto logger = makeLogger(cfg, "mfs_utc");
    logger->info("utc-ok");
    logger->flush();
    EXPECT_EQ(dir_ / "app.log", sink_->active_filename());
    sink_->request_shutdown();
    sink_.reset();
  }
  {
    ManagedFileSinkConfig cfg = defaultConfig();
    cfg.time_zone = "";
    auto logger = makeLogger(cfg, "mfs_sys");
    logger->info("system-ok");
    logger->flush();
  }
}

TEST_F(ManagedFileSinkTest, SizeRotationProducesGzipAndNewActive)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 64;
  auto logger = makeLogger(cfg, "mfs_size");

  logger->info("abcdefghijklmnopqrstuvwxyz0123456789");
  logger->info("second-line-should-be-in-new-file");
  logger->flush();

  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(5)));

  const auto files = list_regular_files(dir_);
  EXPECT_GE(count_suffix(files, ".gz"), 1u);
  EXPECT_TRUE(fs::exists(cfg.base_path));

  bool found = false;
  for(const auto & p : files)
  {
    if(count_suffix({p}, ".gz") == 1)
    {
      const std::string body = gunzip_to_string(p);
      if(body.find("abcdefghijklmnopqrstuvwxyz") != std::string::npos)
      {
        found = true;
      }
      EXPECT_EQ(std::string::npos, body.find("second-line-should-be-in-new-file"));
    }
  }
  EXPECT_TRUE(found);

  const std::string active = [&] {
    std::ifstream in(cfg.base_path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  EXPECT_NE(std::string::npos, active.find("second-line-should-be-in-new-file"));
}

TEST_F(ManagedFileSinkTest, CompressDisabledKeepsPlainRotatedFiles)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 48;
  cfg.compress = false;
  auto logger = makeLogger(cfg, "mfs_nocomp");

  logger->info("plain-rotate-aaaaaaaaaaaaaaaa");
  logger->info("stays-active-bbbbbbbbbbbbbbbb");
  logger->flush();

  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(1)));

  const auto files = list_regular_files(dir_);
  EXPECT_EQ(0u, count_suffix(files, ".gz"));
  EXPECT_GE(files.size(), 2u);

  bool sawRotatedPlain = false;
  for(const auto & p : files)
  {
    if(p == cfg.base_path)
    {
      continue;
    }
    const std::string body = [&] {
      std::ifstream in(p);
      return std::string(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }();
    if(body.find("plain-rotate-aaaa") != std::string::npos)
    {
      sawRotatedPlain = true;
    }
  }
  EXPECT_TRUE(sawRotatedPlain);
}

TEST_F(ManagedFileSinkTest, ForceTimeRotationWithoutTraffic)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  auto logger = makeLogger(cfg, "mfs_time_force");
  logger->info("before-rotate");
  logger->flush();

  const auto before = list_regular_files(dir_);
  sink_->force_time_rotation_for_test();
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(5)));
  const auto after = list_regular_files(dir_);
  EXPECT_GT(after.size(), before.size());
  EXPECT_GE(count_suffix(after, ".gz"), 1u);
}

TEST_F(ManagedFileSinkTest, AsioTimerFiresHardRotationWithoutTraffic)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  auto logger = makeLogger(cfg, "mfs_timer");
  logger->info("pre-timer");
  logger->flush();

  const auto beforeGz = count_suffix(list_regular_files(dir_), ".gz");
  sink_->arm_rotation_after_for_test(std::chrono::milliseconds(20));

  ASSERT_TRUE(wait_until(std::chrono::seconds(3), [&] {
    return count_suffix(list_regular_files(dir_), ".gz") > beforeGz;
  }));
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(5)));
}

TEST_F(ManagedFileSinkTest, MultipleRotationsInSameSecondGetSequenceSuffix)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.compress = false;
  auto logger = makeLogger(cfg, "mfs_seq");
  logger->info("seed");
  logger->flush();

  sink_->force_time_rotation_for_test();
  sink_->force_time_rotation_for_test();
  sink_->force_time_rotation_for_test();

  std::size_t rotated = 0;
  for(const auto & p : list_regular_files(dir_))
  {
    if(p == cfg.base_path)
    {
      continue;
    }
    ++rotated;
    EXPECT_NE(std::string::npos, p.filename().string().find("app."));
  }
  EXPECT_GE(rotated, 2u);
}

TEST_F(ManagedFileSinkTest, RestartRecoveryCompressesLeftovers)
{
  const fs::path leftover = dir_ / "app.2020-01-01_000000.log";
  {
    std::ofstream out(leftover, std::ios::binary);
    out << "leftover-payload";
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  auto logger = makeLogger(cfg, "mfs_recover");
  (void)logger;
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(5)));

  EXPECT_FALSE(fs::exists(leftover));
  EXPECT_TRUE(fs::exists(fs::path(leftover.string() + ".gz")));
  EXPECT_NE(
    std::string::npos,
    gunzip_to_string(leftover.string() + ".gz").find("leftover-payload"));
}

TEST_F(ManagedFileSinkTest, RestartRecoveryCanBeDisabled)
{
  const fs::path leftover = dir_ / "app.2020-01-01_000000.log";
  {
    std::ofstream out(leftover, std::ios::binary);
    out << "keep-me-uncompressed";
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.recover_uncompressed_on_start = false;
  auto logger = makeLogger(cfg, "mfs_norecover");
  (void)logger;
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(1)));

  EXPECT_TRUE(fs::exists(leftover));
  EXPECT_FALSE(fs::exists(fs::path(leftover.string() + ".gz")));
}

TEST_F(ManagedFileSinkTest, SkipsLeftoverThatAlreadyHasGzipSibling)
{
  const fs::path leftover = dir_ / "app.2021-02-02_010101.log";
  const fs::path gz = fs::path(leftover.string() + ".gz");
  {
    std::ofstream out(leftover, std::ios::binary);
    out << "should-remain";
  }
  {
    std::ofstream out(gz, std::ios::binary);
    out << "already-gzipped-placeholder";
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  auto logger = makeLogger(cfg, "mfs_skipgz");
  (void)logger;
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(2)));

  EXPECT_TRUE(fs::exists(leftover));
  EXPECT_EQ("should-remain", [&] {
    std::ifstream in(leftover);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }());
}

TEST_F(ManagedFileSinkTest, ManagedBytesEvictionKeepsActive)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 32;
  cfg.max_managed_bytes = 96;
  cfg.compress = false;
  cfg.recover_uncompressed_on_start = false;
  auto logger = makeLogger(cfg, "mfs_evict");

  for(int i = 0; i < 20; ++i)
  {
    logger->info("evict-line-{:02d}-xxxxxxxxxxxxxxxxxxxx", i);
    logger->flush();
  }

  EXPECT_LE(sink_->managed_bytes_for_test(), cfg.max_managed_bytes + cfg.max_file_bytes);
  EXPECT_TRUE(fs::exists(cfg.base_path));
}

TEST_F(ManagedFileSinkTest, ManagedBytesEvictionWithGzip)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 40;
  cfg.max_managed_bytes = 120;
  cfg.compress = true;
  cfg.gzip_level = 1;
  auto logger = makeLogger(cfg, "mfs_evict_gz");

  for(int i = 0; i < 30; ++i)
  {
    logger->info("gz-evict-{:02d}-yyyyyyyyyyyyyyyyyyyy", i);
    logger->flush();
  }
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(5)));

  EXPECT_LE(sink_->managed_bytes_for_test(), cfg.max_managed_bytes + cfg.max_file_bytes);
  EXPECT_TRUE(fs::exists(cfg.base_path));
}

TEST_F(ManagedFileSinkTest, FilesystemFreePercentForcesEviction)
{
  // Plant large finished artifacts, then open sink with impossible free%% floor.
  for(int i = 0; i < 5; ++i)
  {
    std::ofstream out(dir_ / ("app.2019-01-0" + std::to_string(i) + "_000000.log"));
    out << std::string(2048, 'x');
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.compress = false;
  cfg.recover_uncompressed_on_start = false;
  cfg.retention = RetentionMode::FilesystemFreePercent;
  cfg.free_percent_min = 101; // always "exceeded"
  auto logger = makeLogger(cfg, "mfs_free");
  (void)logger;

  // Construction runs apply_retention; finished planted files should be gone or reduced.
  const auto files = list_regular_files(dir_);
  std::size_t finished = 0;
  for(const auto & p : files)
  {
    if(p != cfg.base_path)
    {
      ++finished;
    }
  }
  EXPECT_EQ(0u, finished);
}

TEST_F(ManagedFileSinkTest, IgnoresUnrelatedFilesInDirectory)
{
  const fs::path stranger = dir_ / "other-app.log";
  const fs::path notes = dir_ / "readme.txt";
  {
    std::ofstream out(stranger);
    out << "do-not-touch";
  }
  {
    std::ofstream out(notes);
    out << "notes";
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 32;
  cfg.max_managed_bytes = 64;
  cfg.compress = false;
  auto logger = makeLogger(cfg, "mfs_stranger");

  for(int i = 0; i < 15; ++i)
  {
    logger->info("noise-{:02d}-zzzzzzzzzzzzzzzzzzzz", i);
    logger->flush();
  }

  EXPECT_TRUE(fs::exists(stranger));
  EXPECT_TRUE(fs::exists(notes));
  EXPECT_EQ("do-not-touch", [&] {
    std::ifstream in(stranger);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }());
}

TEST_F(ManagedFileSinkTest, AppendsToExistingActiveFileOnStart)
{
  {
    std::ofstream out(dir_ / "app.log", std::ios::binary);
    out << "preexisting\n";
  }

  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.compress = false;
  auto logger = makeLogger(cfg, "mfs_append");
  logger->info("appended-line");
  logger->flush();

  const std::string body = [&] {
    std::ifstream in(cfg.base_path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  EXPECT_NE(std::string::npos, body.find("preexisting"));
  EXPECT_NE(std::string::npos, body.find("appended-line"));
}

TEST_F(ManagedFileSinkTest, ShutdownIsIdempotentAndStopsLogging)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  auto logger = makeLogger(cfg, "mfs_shutdown");
  logger->info("bye");
  logger->flush();
  sink_->request_shutdown();
  sink_->request_shutdown();

  logger->info("after-shutdown-should-be-dropped");
  logger->flush();

  const std::string body = [&] {
    std::ifstream in(cfg.base_path);
    if(!in)
    {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  EXPECT_EQ(std::string::npos, body.find("after-shutdown-should-be-dropped"));
}

TEST_F(ManagedFileSinkTest, ConcurrentWritersDoNotCrash)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.max_file_bytes = 256;
  cfg.compress = true;
  auto logger = makeLogger(cfg, "mfs_conc");

  constexpr int kThreads = 4;
  constexpr int kPerThread = 50;
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for(int t = 0; t < kThreads; ++t)
  {
    threads.emplace_back([logger, t] {
      for(int i = 0; i < kPerThread; ++i)
      {
        logger->info("t{}-i{}-payload-xxxxxxxx", t, i);
      }
      logger->flush();
    });
  }
  for(auto & th : threads)
  {
    th.join();
  }
  ASSERT_TRUE(sink_->wait_for_compression_idle_for_test(std::chrono::seconds(10)));
  EXPECT_TRUE(fs::exists(dir_ / "app.log"));
}

TEST_F(ManagedFileSinkTest, SpdlogLoggerAdapterForwardsThroughManagedSink)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.compress = false;
  auto logger = makeLogger(cfg, "mfs_adapter");
  SpdlogLogger adapter(logger);

  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_INFO));
  EXPECT_TRUE(adapter.logMessage(Logger::QF_LOG_INFO, "via-adapter"));
  EXPECT_TRUE(adapter.reportDecodingError("decode-err"));
  logger->flush();

  const std::string body = [&] {
    std::ifstream in(cfg.base_path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  EXPECT_NE(std::string::npos, body.find("via-adapter"));
  EXPECT_NE(std::string::npos, body.find("decode-err"));
}

TEST_F(ManagedFileSinkTest, CreatesMissingParentDirectories)
{
  ManagedFileSinkConfig cfg = ManagedFileSinkConfigBuilder()
    .base_path(dir_ / "nested" / "deep" / "app.log")
    .pattern("%v")
    .compress(false)
    .build();
  auto logger = makeLogger(cfg, "mfs_mkdir");
  logger->info("nested-ok");
  logger->flush();
  EXPECT_TRUE(fs::exists(cfg.base_path));
}

TEST_F(ManagedFileSinkTest, ConfigDefaults)
{
  ManagedFileSinkConfig cfg;
  EXPECT_EQ(32ull << 20, cfg.max_managed_bytes);
  EXPECT_EQ(RetentionMode::ManagedBytes, cfg.retention);
  EXPECT_FALSE(cfg.pattern.empty());
  EXPECT_NE(std::string::npos, cfg.pattern.find("%v"));
}

TEST_F(ManagedFileSinkTest, BuilderRequiresBasePath)
{
  EXPECT_THROW(
    { (void)ManagedFileSinkConfigBuilder().pattern("%v").build(); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, BuilderRejectsEmptyPattern)
{
  EXPECT_THROW(
    {
      (void)ManagedFileSinkConfigBuilder()
        .base_path(dir_ / "app.log")
        .pattern("")
        .build();
    },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, SinkRejectsEmptyPattern)
{
  ManagedFileSinkConfig cfg = defaultConfig();
  cfg.pattern.clear();
  EXPECT_THROW(
    { managed_file_sink_mt sink(runner_.io(), cfg); },
    std::invalid_argument);
}

TEST_F(ManagedFileSinkTest, BuilderFluentOptionsRoundTrip)
{
  const ManagedFileSinkConfig cfg = ManagedFileSinkConfigBuilder()
    .base_path(dir_ / "built.log")
    .max_file_bytes(12345)
    .retention(RetentionMode::FilesystemFreePercent)
    .free_percent_min(15)
    .time_zone("UTC")
    .rotation_time(3, 30)
    .compress(false)
    .gzip_level(2)
    .recover_uncompressed_on_start(false)
    .pattern("[%l] %v")
    .build();

  EXPECT_EQ(dir_ / "built.log", cfg.base_path);
  EXPECT_EQ(12345u, cfg.max_file_bytes);
  EXPECT_EQ(RetentionMode::FilesystemFreePercent, cfg.retention);
  EXPECT_EQ(15u, cfg.free_percent_min);
  ASSERT_TRUE(cfg.time_zone.has_value());
  EXPECT_EQ("UTC", *cfg.time_zone);
  EXPECT_EQ(3, cfg.rotation_hour);
  EXPECT_EQ(30, cfg.rotation_minute);
  EXPECT_FALSE(cfg.compress);
  EXPECT_EQ(2, cfg.gzip_level);
  EXPECT_FALSE(cfg.recover_uncompressed_on_start);
  EXPECT_EQ("[%l] %v", cfg.pattern);
}

TEST_F(ManagedFileSinkTest, BuilderSystemTimeZoneClearsOptional)
{
  const ManagedFileSinkConfig cfg = ManagedFileSinkConfigBuilder()
    .base_path(dir_ / "app.log")
    .time_zone("UTC")
    .system_time_zone()
    .pattern("%v")
    .build();
  EXPECT_FALSE(cfg.time_zone.has_value());
}

TEST_F(ManagedFileSinkTest, PatternFromConfigIsApplied)
{
  ManagedFileSinkConfig cfg = ManagedFileSinkConfigBuilder()
    .base_path(dir_ / "app.log")
    .compress(false)
    .pattern("PFX-%v-SFX")
    .build();
  auto logger = makeLogger(cfg, "mfs_pattern");
  logger->info("payload");
  logger->flush();

  const std::string body = [&] {
    std::ifstream in(cfg.base_path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  EXPECT_NE(std::string::npos, body.find("PFX-payload-SFX"));
}
