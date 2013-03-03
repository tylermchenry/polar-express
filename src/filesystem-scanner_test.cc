#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filesystem-scanner.h"

namespace polar_express {

class FilesystemScannerTest : public testing::Test {
 protected:
  FilesystemScanner scanner_;
};

TEST(FilesystemScannerTest, InitiallyNotScanning) {
  EXPECT_FALSE(scanner_.is_scanning());
}

}  // namespace polar_express
