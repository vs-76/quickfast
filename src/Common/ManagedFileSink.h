// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef QUICKFAST_MANAGEDFILESINK_H
#define QUICKFAST_MANAGEDFILESINK_H

#ifndef QUICKFAST_HAS_SPDLOG
# error "ManagedFileSink requires QUICKFAST_USE_SPDLOG=ON (QUICKFAST_HAS_SPDLOG)."
#endif

#include <Common/QuickFAST_Export.h>

#include <asio.hpp>
#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace QuickFAST
{
  namespace Common
  {
    /// Retention policy for finished rotated / compressed log files.
    enum class RetentionMode
    {
      /// Cap total size of managed files (active + rotated + .gz). Default.
      ManagedBytes,
      /// Evict while filesystem free space percent is below a minimum.
      FilesystemFreePercent
    };

    /// Configuration for @c managed_file_sink_mt.
    struct ManagedFileSinkConfig
    {
      std::filesystem::path base_path;

      std::size_t max_file_bytes = 100ull << 20;

      RetentionMode retention = RetentionMode::ManagedBytes;
      std::size_t max_managed_bytes = 32ull << 20; // 32 MiB default
      unsigned free_percent_min = 10;

      /// IANA zone id (e.g. "Europe/Moscow"). Empty / nullopt = system zone.
      std::optional<std::string> time_zone;
      int rotation_hour = 0;
      int rotation_minute = 0;

      bool compress = true;
      int gzip_level = 6;
      bool recover_uncompressed_on_start = true;

      /// spdlog pattern applied to this sink (see spdlog pattern flags).
      std::string pattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v";
    };

    /// Fluent builder for @c ManagedFileSinkConfig.
    class QuickFAST_Export ManagedFileSinkConfigBuilder
    {
    public:
      ManagedFileSinkConfigBuilder & base_path(std::filesystem::path path);
      ManagedFileSinkConfigBuilder & max_file_bytes(std::size_t bytes);
      ManagedFileSinkConfigBuilder & retention(RetentionMode mode);
      ManagedFileSinkConfigBuilder & max_managed_bytes(std::size_t bytes);
      ManagedFileSinkConfigBuilder & free_percent_min(unsigned percent);
      ManagedFileSinkConfigBuilder & time_zone(std::string zone);
      ManagedFileSinkConfigBuilder & system_time_zone();
      ManagedFileSinkConfigBuilder & rotation_time(int hour, int minute);
      ManagedFileSinkConfigBuilder & compress(bool enabled);
      ManagedFileSinkConfigBuilder & gzip_level(int level);
      ManagedFileSinkConfigBuilder & recover_uncompressed_on_start(bool enabled);
      ManagedFileSinkConfigBuilder & pattern(std::string spdlog_pattern);

      /// @returns a config copy. Does not validate paths/zones (sink ctor does).
      /// @throws std::invalid_argument if @c base_path was never set.
      ManagedFileSinkConfig build() const;

    private:
      ManagedFileSinkConfig cfg_{};
      bool has_base_path_ = false;
    };

    /// @brief spdlog file sink: size + scheduled rotation, gzip, retention, Asio timer.
    ///
    /// Shares an application @c asio::io_context (e.g. @c AsioService::ioService()) for
    /// hard time-based rotation without requiring log traffic. Does not install its own
    /// @c signal_set; call @c request_shutdown() from the app's signal handler.
    class QuickFAST_Export managed_file_sink_mt
      : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
      managed_file_sink_mt(asio::io_context & io, ManagedFileSinkConfig cfg);
      ~managed_file_sink_mt() override;

      managed_file_sink_mt(const managed_file_sink_mt &) = delete;
      managed_file_sink_mt & operator=(const managed_file_sink_mt &) = delete;

      /// @brief Cancel timer, stop compression workers, flush and close the active file.
      ///
      /// Blocks until any in-flight rotation timer handler has finished, so the sink
      /// may be destroyed afterwards even while the shared io_context keeps running.
      /// Idempotent.
      void request_shutdown();

      /// Active log path.
      std::filesystem::path active_filename() const;

      /// @brief Rotate the active file now, without waiting for size or schedule.
      ///
      /// Useful for on-demand rotation (e.g. from a SIGHUP handler). No-op after
      /// @c request_shutdown().
      void rotate_now();

      /// @brief Wait until no rotated file is queued or being compressed.
      /// @returns false on timeout.
      bool wait_for_compression_idle(std::chrono::milliseconds timeout);

      /// Total size of managed files (active + rotated + compressed).
      std::uint64_t managed_bytes();

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
      /// Test helper: cancel current schedule and fire the Asio rotation timer soon.
      void arm_rotation_after_for_test(std::chrono::milliseconds delay);
#endif

    protected:
      void sink_it_(const spdlog::details::log_msg & msg) override;
      void flush_() override;

    private:
      void open_active_file_unlocked_();
      void rotate_unlocked_(std::chrono::system_clock::time_point when);
      std::filesystem::path make_rotated_path_unlocked_(
        std::chrono::system_clock::time_point when);
      void arm_rotation_timer_unlocked_();
      void async_wait_rotation_unlocked_();
      void on_rotation_timer_(const asio::error_code & ec);
      void detach_timer_state_();

      void enqueue_compress_unlocked_(std::filesystem::path path);
      void compress_worker_();
      static bool gzip_file_(
        const std::filesystem::path & src,
        const std::filesystem::path & dst_gz,
        int level);

      void recover_uncompressed_unlocked_();
      void apply_retention_unlocked_();
      std::uint64_t managed_bytes_unlocked_() const;
      std::vector<std::filesystem::path> list_managed_finished_unlocked_() const;
      bool is_managed_name_(const std::filesystem::path & path) const;
      bool retention_exceeded_unlocked_() const;

      const std::chrono::time_zone * resolve_zone_() const;
      std::chrono::system_clock::time_point next_rotation_tp_(
        std::chrono::system_clock::time_point from) const;

      ManagedFileSinkConfig cfg_;
      const std::chrono::time_zone * zone_;
      std::filesystem::path dir_;
      std::string stem_;
      std::string extension_; // includes leading '.'

      asio::io_context & io_;
      asio::system_timer rotation_timer_;

      /// @brief Keeps queued Asio timer handlers safe against sink destruction.
      ///
      /// Cancelling a timer only queues its handler, and a stopped @c io_context may
      /// never run it at all, so handlers hold a weak reference to this block rather
      /// than to the sink. Shutdown clears @c sink under @c mutex, which also waits
      /// out a handler that is already running.
      struct RotationTimerState
      {
        std::mutex mutex;
        managed_file_sink_mt * sink = nullptr;
      };

      std::shared_ptr<RotationTimerState> timer_state_ =
        std::make_shared<RotationTimerState>();

      std::FILE * file_ = nullptr;
      std::size_t current_size_ = 0;
      int same_second_seq_ = 0;
      std::chrono::system_clock::time_point last_rotate_tp_{};

      std::set<std::filesystem::path> inflight_compress_;
      std::deque<std::filesystem::path> compress_queue_;
      mutable std::mutex compress_mutex_;
      std::condition_variable compress_cv_;
      bool compress_stop_ = false;
      std::thread compress_thread_;
      bool shutting_down_ = false;
    };
  }
}
#endif /* QUICKFAST_MANAGEDFILESINK_H */
