#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filesystem-scanner.h"

namespace polar_express {

TEST(FilesystemScanner, InitiallyNotScanning) {
  FilesystemScanner scanner;
  EXPECT_FALSE(scanner.is_scanning());
}

}  // namespace polar_express
