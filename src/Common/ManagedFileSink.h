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
#include <atomic>
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
      /// @brief Path of the active log file; rotated names derive from it.
      ///
      /// The parent directory is created if it does not exist. The stem and
      /// extension are reused for rotated files, so @c app.log rotates to names
      /// of the form @c app-20260830-120000.log.gz.
      std::filesystem::path base_path;

      /// @brief Rotate the active file once it exceeds this size.
      ///
      /// Must not exceed @c max_managed_bytes under @c RetentionMode::ManagedBytes:
      /// retention never evicts the active file, so a larger value would let the
      /// active file alone overrun the whole budget with nothing to reclaim.
      std::size_t max_file_bytes = 8ull << 20; // 8 MiB default

      /// @brief Which resource retention watches when evicting old files.
      RetentionMode retention = RetentionMode::ManagedBytes;
      /// Budget for active + rotated + compressed files. Fits four default files.
      std::size_t max_managed_bytes = 32ull << 20; // 32 MiB default
      /// @brief Free space floor, in percent, for @c RetentionMode::FilesystemFreePercent.
      unsigned free_percent_min = 10;

      /// IANA zone id (e.g. "Europe/Moscow"). Empty / nullopt = system zone.
      std::optional<std::string> time_zone;
      /// @brief Local hour (0-23) of the scheduled daily rotation.
      int rotation_hour = 0;
      /// @brief Local minute (0-59) of the scheduled daily rotation.
      int rotation_minute = 0;

      /// @brief When true, gzip each rotated file on a background worker.
      bool compress = true;
      /// @brief zlib compression level, 0 (store) .. 9 (smallest).
      int gzip_level = 6;
      /// @brief When true, gzip rotated files left uncompressed by an earlier run.
      bool recover_uncompressed_on_start = true;

      /// spdlog pattern applied to this sink (see spdlog pattern flags).
      std::string pattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v";
    };

    /// Fluent builder for @c ManagedFileSinkConfig.
    ///
    /// Every setter returns @c *this, so a whole configuration reads as one
    /// expression ending in build().
    ///
    /// @par Example
    /// @code
    /// const auto cfg = QuickFAST::Common::ManagedFileSinkConfigBuilder()
    ///   .base_path("/var/log/quickfast/feed.log")
    ///   .max_file_bytes(64ull << 20)                 // rotate at 64 MiB
    ///   .max_managed_bytes(1ull << 30)               // keep at most 1 GiB
    ///   .time_zone("Europe/Moscow")
    ///   .rotation_time(0, 0)                         // and daily at midnight
    ///   .compress(true)
    ///   .build();
    /// @endcode
    class QuickFAST_Export ManagedFileSinkConfigBuilder
    {
    public:
      /// @brief Set ManagedFileSinkConfig::base_path.
      /// @param path active log file path
      /// @returns *this
      ManagedFileSinkConfigBuilder & base_path(std::filesystem::path path);
      /// @brief Set ManagedFileSinkConfig::max_file_bytes.
      /// @param bytes size at which the active file rotates
      /// @returns *this
      ManagedFileSinkConfigBuilder & max_file_bytes(std::size_t bytes);
      /// @brief Set ManagedFileSinkConfig::retention.
      /// @param mode resource retention watches
      /// @returns *this
      ManagedFileSinkConfigBuilder & retention(RetentionMode mode);
      /// @brief Set ManagedFileSinkConfig::max_managed_bytes.
      /// @param bytes budget for active + rotated + compressed files
      /// @returns *this
      ManagedFileSinkConfigBuilder & max_managed_bytes(std::size_t bytes);
      /// @brief Set ManagedFileSinkConfig::free_percent_min.
      /// @param percent free space floor for FilesystemFreePercent retention
      /// @returns *this
      ManagedFileSinkConfigBuilder & free_percent_min(unsigned percent);
      /// @brief Schedule rotation in a named IANA zone.
      /// @param zone zone id, e.g. "Europe/Moscow"
      /// @returns *this
      ManagedFileSinkConfigBuilder & time_zone(std::string zone);
      /// @brief Schedule rotation in the host's local zone.
      /// @returns *this
      ManagedFileSinkConfigBuilder & system_time_zone();
      /// @brief Set the daily rotation wall-clock time.
      /// @param hour local hour, 0-23
      /// @param minute local minute, 0-59
      /// @returns *this
      ManagedFileSinkConfigBuilder & rotation_time(int hour, int minute);
      /// @brief Enable or disable gzip of rotated files.
      /// @param enabled true to compress
      /// @returns *this
      ManagedFileSinkConfigBuilder & compress(bool enabled);
      /// @brief Set the zlib compression level.
      /// @param level 0 (store) .. 9 (smallest)
      /// @returns *this
      ManagedFileSinkConfigBuilder & gzip_level(int level);
      /// @brief Compress rotated files a previous run left behind.
      /// @param enabled true to sweep at construction
      /// @returns *this
      ManagedFileSinkConfigBuilder & recover_uncompressed_on_start(bool enabled);
      /// @brief Set the spdlog formatting pattern for this sink.
      /// @param spdlog_pattern pattern string (see spdlog pattern flags)
      /// @returns *this
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
    ///
    /// @par Example
    /// @code
    /// QuickFAST::Communication::AsioService service;
    ///
    /// auto sink = std::make_shared<QuickFAST::Common::managed_file_sink_mt>(
    ///   service.ioService(),
    ///   QuickFAST::Common::ManagedFileSinkConfigBuilder()
    ///     .base_path("/var/log/quickfast/feed.log")
    ///     .max_file_bytes(64ull << 20)
    ///     .max_managed_bytes(1ull << 30)
    ///     .build());
    ///
    /// auto logger = std::make_shared<spdlog::logger>("feed", sink);
    /// QuickFAST::Common::SpdlogLogger adapter(logger);
    ///
    /// // ... run the decoder, sharing service.ioService() ...
    ///
    /// sink->request_shutdown();  // from the app's signal handler or at exit
    /// @endcode
    ///
    /// @see SpdlogLogger to route QuickFAST log output into the same logger.
    class QuickFAST_Export managed_file_sink_mt
      : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
      /// @brief Open (or append to) the active file and arm the rotation timer.
      /// @param io shared io_context used for scheduled rotation; must outlive the sink
      /// @param cfg validated configuration; see ManagedFileSinkConfigBuilder
      /// @throws std::invalid_argument if @c cfg is inconsistent (for example
      ///         @c max_file_bytes above @c max_managed_bytes) or its directory
      ///         or time zone cannot be resolved
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

      /// @brief Count of scheduled rotations that raised an error.
      ///
      /// A rotation handler must never let an exception reach the shared
      /// @c io_context, so failures are counted here instead. Non-zero is worth
      /// alerting on.
      std::uint64_t rotation_failures() const noexcept;

      /// @brief True once a failed rotation also failed to re-arm its timer.
      ///
      /// Size-based rotation still works; time-based rotation does not until the
      /// process restarts.
      bool rotation_schedule_lost() const noexcept;

      /// @brief First instant at or after @p from at which @p hour:@p minute occurs
      ///        in @p zone.
      ///
      /// DST-safe: a local time inside a DST gap resolves to the instant the offset
      /// changes, and an ambiguous local time to the later of its two instants.
      /// Never throws for a valid zone.
      ///
      /// @par Example
      /// @code
      /// const auto next = managed_file_sink_mt::next_rotation_after(
      ///   std::chrono::locate_zone("America/Santiago"),
      ///   std::chrono::system_clock::now(), 0, 0);
      /// @endcode
      static std::chrono::system_clock::time_point next_rotation_after(
        const std::chrono::time_zone * zone,
        std::chrono::system_clock::time_point from,
        int hour,
        int minute);

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
      /// Test helper: cancel current schedule and fire the Asio rotation timer soon.
      void arm_rotation_after_for_test(std::chrono::milliseconds delay);
#endif

    protected:
      /// @brief Format and append one record, rotating first if it would overflow.
      /// @param msg the record spdlog is delivering
      void sink_it_(const spdlog::details::log_msg & msg) override;
      /// @brief Flush the active file to the operating system.
      void flush_() override;

    private:
      void open_active_file_unlocked_();
      void rotate_unlocked_(std::chrono::system_clock::time_point when);
      std::filesystem::path make_rotated_path_unlocked_(
        std::chrono::system_clock::time_point when);
      void arm_rotation_timer_unlocked_();
      void async_wait_rotation_unlocked_();
      void on_rotation_timer_(const asio::error_code & ec);
      void reschedule_after_failure_();
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

      std::atomic<std::uint64_t> rotation_failures_{0};
      std::atomic<bool> rotation_schedule_lost_{false};

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
