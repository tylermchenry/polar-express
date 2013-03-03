#include <string>
#include <vector>

#include "boost/assign/list_of.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filesystem-scanner.h"
#include "thread-launcher_mock.h"

using boost::assign::list_of;
using testing::ContainerEq;
using testing::StrictMock;

namespace polar_express {

class FilesystemScannerTest : public testing::Test {
 public:
  virtual void SetUp() {
    scanner_.SetThreadLauncherForTesting(&thread_launcher_mock_);
  }
  
 protected:
  FilesystemScanner scanner_;
  StrictMock<ThreadLauncherMock> thread_launcher_mock_;
  vector<string> paths_;
};

TEST_F(FilesystemScannerTest, InitiallyNotScanningNoResults) {
  EXPECT_FALSE(scanner_.is_scanning());

  scanner_.GetFilePaths(&paths_);
  EXPECT_TRUE(paths_.empty());

  scanner_.GetFilePathsAndClear(&paths_);
  EXPECT_TRUE(paths_.empty());
}

TEST_F(FilesystemScannerTest, StartScan) {
  EXPECT_CALL(thread_launcher_mock_, ShouldLaunchThread());
  EXPECT_TRUE(scanner_.Scan("/foo/bar"));
  EXPECT_TRUE(scanner_.is_scanning());
}

TEST_F(FilesystemScannerTest, CannotStartTwice) {
  EXPECT_CALL(thread_launcher_mock_, ShouldLaunchThread()).Times(1);
  EXPECT_TRUE(scanner_.Scan("/foo/bar"));
  EXPECT_TRUE(scanner_.is_scanning());
  EXPECT_FALSE(scanner_.Scan("/baz/quux"));
  EXPECT_TRUE(scanner_.is_scanning());
}

TEST_F(FilesystemScannerTest, StopScan) {
  EXPECT_CALL(thread_launcher_mock_, ShouldLaunchThread());
  EXPECT_TRUE(scanner_.Scan("/foo/bar"));
  EXPECT_TRUE(scanner_.is_scanning());

  // TODO: Figure out how to mock boost::thread so that we can verify that
  // interrupt and join were actually called on the thread.
  scanner_.StopScan();
  EXPECT_FALSE(scanner_.is_scanning());
}

TEST_F(FilesystemScannerTest, GetFilePaths) {
  const vector<string> initial_paths =
      list_of("/foo/bar")("/baz/quux")("/weeble/wobble");

  scanner_.AddFilePaths(initial_paths);
  scanner_.GetFilePaths(&paths_);
  EXPECT_THAT(paths_, ContainerEq(initial_paths));

  // Should be able to repeat with the same result.
  paths_.clear();
  scanner_.GetFilePaths(&paths_);
  EXPECT_THAT(paths_, ContainerEq(initial_paths));
}

TEST_F(FilesystemScannerTest, GetFilePathsAndClear) {
  const vector<string> initial_paths =
      list_of("/foo/bar")("/baz/quux")("/weeble/wobble");

  scanner_.AddFilePaths(initial_paths);
  scanner_.GetFilePathsAndClear(&paths_);
  EXPECT_THAT(paths_, ContainerEq(initial_paths));

  // Now GetFilePaths should return nothing.
  paths_.clear();
  scanner_.GetFilePaths(&paths_);
  EXPECT_TRUE(paths_.empty());
}

// TODO: Figure out how to mock boost::filesystem and add some tests for the
// ScannerThread functor.

}  // namespace polar_express
