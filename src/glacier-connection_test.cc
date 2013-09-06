#include "glacier-connection.h"

#include <memory>
#include <vector>

#include "boost/bind/bind.hpp"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "asio-dispatcher.h"
#include "macros.h"
#include "callback.h"
#include "proto/glacier.pb.h"

using testing::EndsWith;
using testing::HasSubstr;
using testing::StartsWith;

namespace polar_express {
namespace {

// Fill in credentials to run E2E tests.
const char kAwsRegionName[] = "";
const char kAwsAccessKey[] = "";
const byte kAwsSecretKey[] = "";
const char kAwsGlacierVaultName[] = "";

// No friendly proto matcher in GoogleMock? Lame.
MATCHER_P(EqualsProto, message, "") {
  string expected_serialized;
  string actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class GlacierConnectionTest : public testing::Test {
 public:
  GlacierConnectionTest()
      : open_callback_invoked_(false),
        expect_open_(false),
        describe_callback_invoked_(false),
        expect_describe_success_(false),
        // Be sure not to include the trailing NUL in the key block.
        aws_secret_key_(kAwsSecretKey,  sizeof(kAwsSecretKey) - 1),
        glacier_connection_(new GlacierConnection) {
  }

  virtual void SetUp() {
    AsioDispatcher::GetInstance()->Start();
  }

 protected:
  void WaitForFinish() {
    AsioDispatcher::GetInstance()->WaitForFinish();
  }

  void DestroyConnection() {
      glacier_connection_.reset();
  }

  void DescribeVault(
      const string& vault_name, GlacierVaultDescription* vault_description,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->DescribeVault(
        vault_name, vault_description, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  Callback GetOpenCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::open_callback_function, this, next_action);
  }

  Callback GetDescribeCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::describe_callback_function, this, next_action);
  }

  Callback GetDescribeVaultAction(
      const string& vault_name, GlacierVaultDescription* vault_description,
      bool expect_success, Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::DescribeVault,
        this, vault_name, vault_description, expect_success, next_action);
  }

  Callback GetDestroyConnectionAction() {
    return boost::bind(&GlacierConnectionTest::DestroyConnection, this);
  }

  bool open_callback_invoked_;
  bool expect_open_;

  bool describe_callback_invoked_;
  bool expect_describe_success_;

  const CryptoPP::SecByteBlock aws_secret_key_;
  unique_ptr<GlacierConnection> glacier_connection_;

 private:
  void open_callback_function(Callback next_action) {
    EXPECT_FALSE(open_callback_invoked_);
    open_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_FALSE(glacier_connection_->is_opening());
    EXPECT_EQ(expect_open_, glacier_connection_->is_open());

    next_action();
  }

  void describe_callback_function(Callback next_action) {
    EXPECT_FALSE(describe_callback_invoked_);
    describe_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_describe_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_describe_success_, glacier_connection_->is_open());

    next_action();
  }
};

TEST_F(GlacierConnectionTest, Open) {
  expect_open_ = true;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_,
      GetOpenCallback(GetDestroyConnectionAction())));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_F(GlacierConnectionTest, OpenAndDescribeVault) {
  expect_open_ = true;
  expect_describe_success_ = true;

  GlacierVaultDescription vault_description;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_, GetOpenCallback(
          GetDescribeVaultAction(
              kAwsGlacierVaultName, &vault_description, true,
              GetDescribeCallback(GetDestroyConnectionAction())))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(describe_callback_invoked_);

  // No good way to know what the exact contents should be, if using a
  // live Amazon account for an E2E test.
  EXPECT_FALSE(vault_description.creation_date().empty());
  EXPECT_FALSE(vault_description.last_inventory_date().empty());
  EXPECT_TRUE(vault_description.has_number_of_archives());
  EXPECT_TRUE(vault_description.has_size_in_bytes());
  EXPECT_THAT(vault_description.vault_arn(), StartsWith("arn:aws:glacier"));
  EXPECT_THAT(vault_description.vault_arn(), HasSubstr(kAwsRegionName));
  EXPECT_THAT(vault_description.vault_arn(), EndsWith(kAwsGlacierVaultName));
  EXPECT_EQ(kAwsGlacierVaultName, vault_description.vault_name());
}

}  // namespace
}  // namespace polar_express
