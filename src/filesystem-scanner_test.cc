#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filesystem-scanner.h"
#include "thread-launcher_mock.h"

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
};

TEST_F(FilesystemScannerTest, InitiallyNotScanning) {
  EXPECT_FALSE(scanner_.is_scanning());
}

}  // namespace polar_express
