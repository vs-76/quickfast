// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Microbench: multi-producer SingleServerBufferQueue handoff under one mutex,
// mirroring Receiver::bufferMutex_ usage in AsynchReceiver.
//
// Reports mutex *wait* (time blocked acquiring the lock) vs *hold* (time spent
// in the critical section). High wait under realistic --service-ns is the
// signal that a lock-free queue might matter; high hold with low wait is not.
#include <Common/QuickFASTPch.h>

#include <Communication/LinkedBuffer.h>
#include <Communication/SingleServerBufferQueue.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace QuickFAST;
using Clock = std::chrono::steady_clock;

namespace
{
  struct MutexTiming
  {
    std::atomic<std::uint64_t> waitNs{0};
    std::atomic<std::uint64_t> holdNs{0};
    std::atomic<std::uint64_t> parkNs{0}; // CV wait for a free buffer (not mutex spin)
    std::atomic<std::uint64_t> lockCount{0};
  };

  /// @brief unique_lock that accumulates acquire-wait and hold durations.
  class TimedLock
  {
  public:
    TimedLock(std::mutex & mutex, MutexTiming & timing)
      : timing_(timing)
    {
      const auto t0 = Clock::now();
      lock_ = std::unique_lock<std::mutex>(mutex);
      acquired_ = Clock::now();
      timing_.waitNs.fetch_add(
        static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(acquired_ - t0).count()),
        std::memory_order_relaxed);
      timing_.lockCount.fetch_add(1, std::memory_order_relaxed);
    }

    ~TimedLock()
    {
      flushHold();
    }

    /// @brief Block until pred; CV sleep is park time, not hold/wait.
    template<typename Pred>
    void wait(std::condition_variable & cv, Pred pred)
    {
      flushHold();
      const auto p0 = Clock::now();
      cv.wait(lock_, pred);
      const auto p1 = Clock::now();
      timing_.parkNs.fetch_add(
        static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(p1 - p0).count()),
        std::memory_order_relaxed);
      acquired_ = Clock::now();
      holding_ = true;
    }

    std::unique_lock<std::mutex> & get()
    {
      return lock_;
    }

  private:
    void flushHold()
    {
      if(holding_ && lock_.owns_lock())
      {
        const auto released = Clock::now();
        timing_.holdNs.fetch_add(
          static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(released - acquired_).count()),
          std::memory_order_relaxed);
        holding_ = false;
      }
    }

    MutexTiming & timing_;
    std::unique_lock<std::mutex> lock_;
    Clock::time_point acquired_{};
    bool holding_{true};
  };

  void busyWaitNs(std::uint64_t ns)
  {
    if(ns == 0)
    {
      return;
    }
    const auto deadline = Clock::now() + std::chrono::nanoseconds(ns);
    while(Clock::now() < deadline)
    {
    }
  }

  struct Options
  {
    unsigned producers = 4;
    std::uint64_t opsPerProducer = 100000;
    std::uint64_t serviceNs = 0;
    unsigned buffers = 0; // 0 => 8 * producers
    bool sweep = false;
  };

  void usage(const char * argv0)
  {
    std::cerr
      << "Usage: " << argv0 << " [options]\n"
      << "  --producers N       producer threads (default 4)\n"
      << "  --ops N             pushes per producer (default 100000)\n"
      << "  --service-ns N      busy-wait per buffer while servicing (default 0)\n"
      << "  --buffers N         LinkedBuffer pool size (default 8*producers)\n"
      << "  --sweep             run producers=1,2,4,8 with the same other options\n"
      << "  -h, --help          this message\n"
      << "\n"
      << "Columns: wait_* is mutex acquire contention; park_* is CV backpressure when\n"
      << "the free buffer pool is empty (not a reason to lock-free the queue).\n"
      << "Try --service-ns 0 (exaggerates lock cost) then decode-like 5000..50000.\n";
  }

  bool parseArgs(int argc, char * argv[], Options & opt)
  {
    for(int i = 1; i < argc; ++i)
    {
      const std::string a(argv[i]);
      auto need = [&](std::uint64_t & out) -> bool {
        if(i + 1 >= argc)
        {
          std::cerr << "missing value for " << a << '\n';
          return false;
        }
        out = std::strtoull(argv[++i], nullptr, 10);
        return true;
      };
      auto needU = [&](unsigned & out) -> bool {
        std::uint64_t v = 0;
        if(!need(v))
        {
          return false;
        }
        out = static_cast<unsigned>(v);
        return true;
      };

      if(a == "-h" || a == "--help")
      {
        usage(argv[0]);
        std::exit(0);
      }
      else if(a == "--producers")
      {
        if(!needU(opt.producers) || opt.producers == 0)
        {
          return false;
        }
      }
      else if(a == "--ops")
      {
        if(!need(opt.opsPerProducer) || opt.opsPerProducer == 0)
        {
          return false;
        }
      }
      else if(a == "--service-ns")
      {
        if(!need(opt.serviceNs))
        {
          return false;
        }
      }
      else if(a == "--buffers")
      {
        if(!needU(opt.buffers) || opt.buffers == 0)
        {
          return false;
        }
      }
      else if(a == "--sweep")
      {
        opt.sweep = true;
      }
      else
      {
        std::cerr << "unknown option: " << a << '\n';
        usage(argv[0]);
        return false;
      }
    }
    return true;
  }

  struct RunResult
  {
    unsigned producers = 0;
    std::uint64_t totalOps = 0;
    double wallSec = 0;
    std::uint64_t waitNs = 0;
    std::uint64_t holdNs = 0;
    std::uint64_t parkNs = 0;
    std::uint64_t lockCount = 0;
  };

  /// @brief One run mirroring AsynchReceiver volunteer-service under bufferMutex_.
  RunResult runOnce(const Options & opt)
  {
    const unsigned bufferCount =
      opt.buffers != 0 ? opt.buffers : std::max(8u, 8u * opt.producers);

    std::vector<std::shared_ptr<Communication::LinkedBuffer>> lifetimes;
    lifetimes.reserve(bufferCount);
    Communication::BufferCollection freePool;
    for(unsigned i = 0; i < bufferCount; ++i)
    {
      auto buf = std::make_shared<Communication::LinkedBuffer>(64);
      buf->setUsed(64);
      freePool.push(buf.get());
      lifetimes.push_back(std::move(buf));
    }

    std::mutex bufferMutex;
    std::condition_variable freeCv;
    Communication::SingleServerBufferQueue queue;
    MutexTiming timing;
    std::atomic<std::uint64_t> pushed{0};

    const auto wallStart = Clock::now();
    std::vector<std::thread> threads;
    threads.reserve(opt.producers);

    for(unsigned p = 0; p < opt.producers; ++p)
    {
      threads.emplace_back([&, ops = opt.opsPerProducer, serviceNs = opt.serviceNs]() {
        for(std::uint64_t n = 0; n < ops; ++n)
        {
          bool service = false;
          {
            TimedLock timed(bufferMutex, timing);
            auto & lock = timed.get();
            // Backpressure like a full idle pool: park on CV, do not spin the mutex.
            timed.wait(freeCv, [&]() { return !freePool.isEmpty(); });
            Communication::LinkedBuffer * buffer = freePool.pop();
            if(queue.push(buffer, lock))
            {
              service = queue.startService(lock);
            }
          }
          pushed.fetch_add(1, std::memory_order_relaxed);

          while(service)
          {
            Communication::BufferCollection idle;
            for(;;)
            {
              Communication::LinkedBuffer * buffer = queue.serviceNext();
              if(buffer == nullptr)
              {
                break;
              }
              busyWaitNs(serviceNs);
              idle.push(buffer);
            }

            TimedLock timed(bufferMutex, timing);
            auto & lock = timed.get();
            if(!idle.isEmpty())
            {
              freePool.push(idle);
              freeCv.notify_all();
            }
            service = queue.endService(true, lock);
          }
        }
      });
    }

    for(auto & t : threads)
    {
      t.join();
    }
    const auto wallEnd = Clock::now();

    RunResult r;
    r.producers = opt.producers;
    r.totalOps = pushed.load(std::memory_order_relaxed);
    r.wallSec = std::chrono::duration<double>(wallEnd - wallStart).count();
    r.waitNs = timing.waitNs.load(std::memory_order_relaxed);
    r.holdNs = timing.holdNs.load(std::memory_order_relaxed);
    r.parkNs = timing.parkNs.load(std::memory_order_relaxed);
    r.lockCount = timing.lockCount.load(std::memory_order_relaxed);
    return r;
  }

  void printHeader()
  {
    std::cout
      << std::left
      << std::setw(10) << "producers"
      << std::setw(12) << "ops"
      << std::setw(12) << "wall_s"
      << std::setw(14) << "ops_per_s"
      << std::setw(12) << "locks"
      << std::setw(14) << "avg_wait_ns"
      << std::setw(14) << "avg_hold_ns"
      << std::setw(12) << "wait_pct"
      << std::setw(12) << "park_pct"
      << '\n';
  }

  void printRow(const RunResult & r)
  {
    const double avgWait =
      r.lockCount ? static_cast<double>(r.waitNs) / static_cast<double>(r.lockCount) : 0.0;
    const double avgHold =
      r.lockCount ? static_cast<double>(r.holdNs) / static_cast<double>(r.lockCount) : 0.0;
    // Fraction of aggregate thread-time spent waiting on the mutex / parked for buffers.
    const double threadSec = r.wallSec * static_cast<double>(r.producers);
    const double waitPct =
      threadSec > 0.0
        ? (100.0 * (static_cast<double>(r.waitNs) / 1e9) / threadSec)
        : 0.0;
    const double parkPct =
      threadSec > 0.0
        ? (100.0 * (static_cast<double>(r.parkNs) / 1e9) / threadSec)
        : 0.0;
    const double opsPerSec =
      r.wallSec > 0.0 ? static_cast<double>(r.totalOps) / r.wallSec : 0.0;

    std::cout
      << std::left
      << std::setw(10) << r.producers
      << std::setw(12) << r.totalOps
      << std::setw(12) << std::fixed << std::setprecision(4) << r.wallSec
      << std::setw(14) << std::setprecision(0) << opsPerSec
      << std::setw(12) << r.lockCount
      << std::setw(14) << std::setprecision(1) << avgWait
      << std::setw(14) << avgHold
      << std::setw(12) << std::setprecision(2) << waitPct
      << std::setw(12) << parkPct
      << '\n';
  }
}

int main(int argc, char * argv[])
{
  Options opt;
  if(!parseArgs(argc, argv, opt))
  {
    return 1;
  }

  std::cout
    << "BufferQueueBench: SingleServerBufferQueue under one mutex "
       "(Receiver::bufferMutex_ pattern)\n"
    << "service_ns=" << opt.serviceNs
    << " ops_per_producer=" << opt.opsPerProducer
    << '\n';
  printHeader();

  if(opt.sweep)
  {
    const unsigned levels[] = {1, 2, 4, 8};
    for(unsigned n : levels)
    {
      Options one = opt;
      one.producers = n;
      one.sweep = false;
      printRow(runOnce(one));
    }
  }
  else
  {
    printRow(runOnce(opt));
  }
  return 0;
}
