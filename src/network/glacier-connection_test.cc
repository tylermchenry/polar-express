#include "network/glacier-connection.h"

#include <memory>
#include <sstream>
#include <vector>

#include <boost/bind/bind.hpp>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "proto/glacier.pb.h"
#include "services/bundle-hasher.h"

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

const char kTestArchiveDescription[] = "Lorem Ipsum";
const byte kTestArchivePayloadData[] =
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum.";

class GlacierConnectionTest : public testing::TestWithParam<bool> {
 public:
  GlacierConnectionTest()
      : open_callback_invoked_(false),
        expect_open_(false),
        create_callback_invoked_(false),
        expect_create_success_(false),
        describe_callback_invoked_(false),
        expect_describe_success_(false),
        list_callback_invoked_(false),
        expect_list_success_(false),
        delete_callback_invoked_(false),
        expect_delete_success_(false),
        upload_archive_callback_invoked_(false),
        expect_upload_archive_success_(false),
        delete_archive_callback_invoked_(false),
        expect_delete_archive_success_(false),
        // Be sure not to include the trailing NUL in the key block.
        aws_secret_key_(kAwsSecretKey, sizeof(kAwsSecretKey) - 1),
        glacier_connection_(
            GetParam()
                ? static_cast<GlacierConnection*>(new SecureGlacierConnection)
                : static_cast<GlacierConnection*>(new GlacierConnection)) {
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

  void CreateVault(
      const string& vault_name, bool* vault_created,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->CreateVault(
        vault_name, vault_created, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
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

  void ListVaults(
      int max_vaults, const string& start_marker,
      GlacierVaultList* vault_list,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->ListVaults(
        max_vaults, start_marker, vault_list, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void DeleteVault(
      const string& vault_name, bool* vault_deleted,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->DeleteVault(
        vault_name, vault_deleted, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void HashArchive(const vector<byte>* payload,
                   string* payload_sha256_linear_digest,
                   string* payload_sha256_tree_digest,
                   Callback next_action) {
    bundle_hasher_.ComputeHashes(payload, payload_sha256_linear_digest,
                                 payload_sha256_tree_digest, next_action);
  }

  void UploadArchive(const string& vault_name, const vector<byte>* payload,
                     const string* payload_sha256_linear_digest,
                     const string* payload_sha256_tree_digest,
                     const string& payload_description, string* archive_id,
                     bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->UploadArchive(
        vault_name, payload,
        *CHECK_NOTNULL(payload_sha256_linear_digest),
        *CHECK_NOTNULL(payload_sha256_tree_digest),
        payload_description, archive_id,
        next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void DeleteArchive(
      const string& vault_name, const string* archive_id, bool* archive_deleted,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(glacier_connection_.get() != nullptr);

    bool success = glacier_connection_->DeleteArchive(
        vault_name, *CHECK_NOTNULL(archive_id), archive_deleted, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  Callback GetOpenCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::open_callback_function, this, next_action);
  }

  Callback GetCreateCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::create_callback_function, this, next_action);
  }

  Callback GetDescribeCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::describe_callback_function, this, next_action);
  }

  Callback GetListCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::list_callback_function, this, next_action);
  }

  Callback GetDeleteCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::delete_callback_function, this, next_action);
  }

  Callback GetHashArchiveCallback(Callback next_action) {
    return next_action;  // No special post-processing for this.
  }

  Callback GetUploadArchiveCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::upload_archive_callback_function,
        this, next_action);
  }

  Callback GetDeleteArchiveCallback(Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::delete_archive_callback_function,
        this, next_action);
  }

  Callback GetCreateVaultAction(const string& vault_name, bool* vault_created,
                                 bool expect_success, Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::CreateVault,
        this, vault_name, vault_created, expect_success, next_action);
  }

  Callback GetDescribeVaultAction(const string& vault_name,
                                  GlacierVaultDescription* vault_description,
                                  bool expect_success, Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::DescribeVault,
        this, vault_name, vault_description, expect_success, next_action);
  }

  Callback GetListVaultsAction(int max_vaults, const string& start_marker,
                               GlacierVaultList* vault_list,
                               bool expect_success, Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::ListVaults,
        this, max_vaults, start_marker, vault_list, expect_success,
        next_action);
  }

  Callback GetDeleteVaultAction(const string& vault_name, bool* vault_deleted,
                                 bool expect_success, Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::DeleteVault,
        this, vault_name, vault_deleted, expect_success, next_action);
  }

  Callback GetHashArchiveAction(const vector<byte>* payload,
                                string* payload_sha256_linear_digest,
                                string* payload_sha256_tree_digest,
                                Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::HashArchive,
        this, payload, payload_sha256_linear_digest,
        payload_sha256_tree_digest, next_action);
  }

  Callback GetUploadArchiveAction(const string& vault_name,
                                  const vector<byte>* payload,
                                  const string* payload_sha256_linear_digest,
                                  const string* payload_sha256_tree_digest,
                                  const string& payload_description,
                                  string* archive_id, bool expect_success,
                                  Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::UploadArchive,
        this, vault_name, payload, payload_sha256_linear_digest,
        payload_sha256_tree_digest, payload_description, archive_id,
        expect_success, next_action);
  }

  Callback GetDeleteArchiveAction(const string& vault_name,
                                  const string* archive_id,
                                  bool* archive_deleted, bool expect_success,
                                  Callback next_action) {
    return boost::bind(
        &GlacierConnectionTest::DeleteArchive,
        this, vault_name, archive_id, archive_deleted, expect_success,
        next_action);
  }


  Callback GetDestroyConnectionAction() {
    return boost::bind(&GlacierConnectionTest::DestroyConnection, this);
  }

  bool open_callback_invoked_;
  bool expect_open_;

  bool create_callback_invoked_;
  bool expect_create_success_;

  bool describe_callback_invoked_;
  bool expect_describe_success_;

  bool list_callback_invoked_;
  bool expect_list_success_;

  bool delete_callback_invoked_;
  bool expect_delete_success_;

  bool upload_archive_callback_invoked_;
  bool expect_upload_archive_success_;

  bool delete_archive_callback_invoked_;
  bool expect_delete_archive_success_;

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

  void create_callback_function(Callback next_action) {
    EXPECT_FALSE(create_callback_invoked_);
    create_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_create_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_create_success_, glacier_connection_->is_open());

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

  void list_callback_function(Callback next_action) {
    EXPECT_FALSE(list_callback_invoked_);
    list_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_list_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_list_success_, glacier_connection_->is_open());

    next_action();
  }

  void delete_callback_function(Callback next_action) {
    EXPECT_FALSE(delete_callback_invoked_);
    delete_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_delete_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_delete_success_, glacier_connection_->is_open());

    next_action();
  }

  void upload_archive_callback_function(Callback next_action) {
    EXPECT_FALSE(upload_archive_callback_invoked_);
    upload_archive_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_upload_archive_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_upload_archive_success_, glacier_connection_->is_open());

    next_action();
  }

  void delete_archive_callback_function(Callback next_action) {
    EXPECT_FALSE(delete_archive_callback_invoked_);
    delete_archive_callback_invoked_ = true;

    ASSERT_TRUE(glacier_connection_.get() != nullptr);
    EXPECT_EQ(expect_delete_archive_success_,
              glacier_connection_->last_operation_succeeded());
    EXPECT_EQ(expect_delete_archive_success_, glacier_connection_->is_open());

    next_action();
  }

  BundleHasher bundle_hasher_;
};

TEST_P(GlacierConnectionTest, Open) {
  expect_open_ = true;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_,
      GetOpenCallback(GetDestroyConnectionAction())));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_P(GlacierConnectionTest, OpenAndDescribeVault) {
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

TEST_P(GlacierConnectionTest, OpenAndListVaults) {
  expect_open_ = true;
  expect_list_success_ = true;

  GlacierVaultList vault_list;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_, GetOpenCallback(
          GetListVaultsAction(
              100, "", &vault_list, true,
              GetListCallback(GetDestroyConnectionAction())))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(list_callback_invoked_);

  // No good way to know what the exact contents should be, if using a
  // live Amazon account for an E2E test.
  EXPECT_LT(0, vault_list.vault_descriptions_size());
}

TEST_P(GlacierConnectionTest, CreateAndDeleteVault) {
  ostringstream test_vault_name;
  test_vault_name << "glacier_connection_test_vault_" << time(nullptr);

  const string& kTestVaultName = test_vault_name.str();

  expect_open_ = true;
  expect_create_success_ = true;
  expect_describe_success_ = true;
  expect_delete_success_ = true;

  bool vault_created = false;
  bool vault_deleted = false;

  GlacierVaultDescription vault_description;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_,
      GetOpenCallback(GetCreateVaultAction(
          kTestVaultName, &vault_created, true,
          GetCreateCallback(GetDescribeVaultAction(
              kTestVaultName, &vault_description, true,
              GetDescribeCallback(GetDeleteVaultAction(
                  kTestVaultName, &vault_deleted, true,
                  GetDeleteCallback(GetDestroyConnectionAction())))))))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(create_callback_invoked_);
  EXPECT_TRUE(describe_callback_invoked_);
  EXPECT_TRUE(delete_callback_invoked_);

  EXPECT_TRUE(vault_created);

  // TODO: Test that creation date is today.
  EXPECT_FALSE(vault_description.creation_date().empty());
  EXPECT_FALSE(vault_description.has_last_inventory_date());
  EXPECT_EQ(0, vault_description.number_of_archives());
  EXPECT_TRUE(vault_description.has_size_in_bytes());
  EXPECT_THAT(vault_description.vault_arn(), StartsWith("arn:aws:glacier"));
  EXPECT_THAT(vault_description.vault_arn(), HasSubstr(kAwsRegionName));
  EXPECT_THAT(vault_description.vault_arn(), EndsWith(kTestVaultName));
  EXPECT_EQ(kTestVaultName, vault_description.vault_name());

  EXPECT_TRUE(vault_deleted);

  // TODO: Now try to describe the deleted vault, which should fail.
}

// Change 0 to 1 to run this test. This test will leave an empty test vault on
// your test glacier account that you will have to remove manually. Glacier does
// not allow you to delete vaults immediately after writing to them, even if you
// have already deleted the archive that you wrote.
#if 0

TEST_P(GlacierConnectionTest, UploadAndDeleteArchive) {
  ostringstream test_vault_name;
  test_vault_name << "glacier_connection_test_vault_" << time(nullptr);

  const string& kTestVaultName = test_vault_name.str();

  vector<byte> kTestArchivePayload;

  for (int i = 0; i < 15000; ++i) {
    kTestArchivePayload.insert(
        kTestArchivePayload.end(), kTestArchivePayloadData,
        kTestArchivePayloadData + sizeof(kTestArchivePayloadData));
  }

  string payload_sha256_linear_digest;
  string payload_sha256_tree_digest;

  expect_open_ = true;
  expect_create_success_ = true;
  expect_upload_archive_success_ = true;
  expect_delete_archive_success_ = true;

  bool vault_created = false;
  string archive_id;
  bool archive_deleted = false;

  EXPECT_TRUE(glacier_connection_->Open(
      AsioDispatcher::NetworkUsageType::kUplinkBound, kAwsRegionName,
      kAwsAccessKey, aws_secret_key_,
      GetOpenCallback(GetCreateVaultAction(
          kTestVaultName, &vault_created, true,
          GetCreateCallback(GetHashArchiveAction(
              &kTestArchivePayload, &payload_sha256_linear_digest,
              &payload_sha256_tree_digest,
              GetHashArchiveCallback(GetUploadArchiveAction(
                  kTestVaultName, &kTestArchivePayload,
                  &payload_sha256_linear_digest, &payload_sha256_tree_digest,
                  kTestArchiveDescription, &archive_id, true,
                  GetUploadArchiveCallback(GetDeleteArchiveAction(
                      kTestVaultName, &archive_id, &archive_deleted, true,
                      GetDeleteArchiveCallback(
                          GetDestroyConnectionAction())))))))))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(create_callback_invoked_);
  EXPECT_TRUE(upload_archive_callback_invoked_);
  EXPECT_TRUE(delete_archive_callback_invoked_);

  EXPECT_TRUE(vault_created);
  EXPECT_FALSE(archive_id.empty());
  EXPECT_TRUE(archive_deleted);
}

#endif

INSTANTIATE_TEST_CASE_P(SecureAndInsecureTest, GlacierConnectionTest,
                        testing::Bool());

}  // namespace
}  // namespace polar_express
