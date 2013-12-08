#include "bundle-hasher-impl.h"

#include <string>
#include <vector>

#include "macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace polar_express {

class BundleHasherImplTest : public testing::Test {
 protected:
  void HashData(const vector<const vector<byte>*>& sequential_data,
                string* sha256_linear_digest,
                string* sha256_tree_digest) {
    bundle_hasher_impl_.HashData(sequential_data, sha256_linear_digest,
                                 sha256_tree_digest);
  }

  void HashData() {
    HashData({ &data_ }, &sha256_linear_digest_, &sha256_tree_digest_);
  }

  vector<byte> data_;
  string sha256_linear_digest_;
  string sha256_tree_digest_;
  BundleHasherImpl bundle_hasher_impl_;
};

namespace {

const byte kTestData[] =
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum.";

TEST_F(BundleHasherImplTest, HashDataUnderOneMegabyte) {
  data_.assign(kTestData, kTestData + sizeof(kTestData));
  HashData();

  EXPECT_EQ("CE80D3FC3B705B3CC4A15F596F81E55A5AC1A87617209EB6C46A37FF03FC1D40",
            sha256_linear_digest_);
  EXPECT_EQ("CE80D3FC3B705B3CC4A15F596F81E55A5AC1A87617209EB6C46A37FF03FC1D40",
            sha256_tree_digest_);
}

TEST_F(BundleHasherImplTest, HashDataOverOneMegabyte) {
  // This makes a payload of slightly more than 5MB, so six 1MB blocks.
  for (int i = 0; i < 12000; ++i) {
    data_.insert(data_.end(), kTestData, kTestData + sizeof(kTestData));
  }
  HashData();

  EXPECT_EQ("34C28E6AB5A010D247B0E833548EC75BF4D0DDBE58AF8B4B8EB098D7F046C05D",
            sha256_linear_digest_);
  EXPECT_EQ("08C2957765E7137B37A209402B4E0A6704BD78E01F8153B385FB079AC6F46264",
            sha256_tree_digest_);
}

TEST_F(BundleHasherImplTest, HashDataOverOneMegabyteOddNumberOfBlocks) {
  // This makes a payload of slightly more than 6MB, so seven 1MB blocks.
  for (int i = 0; i < 15000; ++i) {
    data_.insert(data_.end(), kTestData, kTestData + sizeof(kTestData));
  }
  HashData();

  EXPECT_EQ("27A0525680BEB5E3B65EFFA8F61A4C097E5418613C4AA7F9E30483D8490324BE",
            sha256_linear_digest_);
  EXPECT_EQ("94497490CCB052FFEB81DD4300374EC5618B459DC8083F441383E95693987CDD",
            sha256_tree_digest_);
}

}  // namespace
}  // namespace polar_express

