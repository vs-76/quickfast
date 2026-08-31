// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
// Helper for unit tests that need a scratch file on disk.
#ifndef QUICKFAST_TEMPFILE_H
#define QUICKFAST_TEMPFILE_H

#include <gtest/gtest.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>

namespace QuickFAST {
namespace TestPaths {

/// @brief A scratch file that cleans itself up, under a name no other test uses.
///
/// gtest_discover_tests registers every test as its own ctest entry, so
/// `ctest -j` runs them as concurrent processes that share a working directory
/// and a system temp directory. A fixed file name therefore lets one test's
/// cleanup delete the file another test is still reading, and a per-process
/// counter does not help because every process starts counting at zero.
///
/// The name mixes in the test name, a launch timestamp and a counter, so it is
/// unique across concurrent processes, across tests within a process, and
/// across several files within one test. The file lives in the system temp
/// directory rather than the working directory, which keeps build trees clean.
class TemporaryFile
{
public:
  explicit TemporaryFile(const std::string & contents,
                         const char * extension = ".dat")
    : path_(std::filesystem::temp_directory_path() / uniqueName(extension))
  {
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    out.write(contents.data(), std::streamsize(contents.size()));
  }

  ~TemporaryFile()
  {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  TemporaryFile(const TemporaryFile &) = delete;
  TemporaryFile & operator=(const TemporaryFile &) = delete;

  /// @brief The absolute path of the file.
  std::string name() const
  {
    return path_.string();
  }

private:
  static std::string uniqueName(const char * extension)
  {
    static std::atomic<unsigned> counter{0};

    const ::testing::TestInfo * info =
      ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test = (info == 0) ? std::string("unknown") : info->name();
    // Parameterized and typed test names carry '/' and '<'.
    for(char & character : test)
    {
      if(std::isalnum(static_cast<unsigned char>(character)) == 0)
      {
        character = '_';
      }
    }

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "quickfast-" + test + "-" + std::to_string(now) + "-"
      + std::to_string(counter++) + extension;
  }

  std::filesystem::path path_;
};

} // namespace TestPaths
} // namespace QuickFAST

#endif // QUICKFAST_TEMPFILE_H
