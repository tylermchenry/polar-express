#include "glacier-connection.h"

#include <iostream>
#include <sstream>

#include "boost/algorithm/string.hpp"
#include "boost/bind.hpp"
#include "boost/lexical_cast.hpp"
#include "json_spirit.h"

#include "http-connection.h"
#include "proto/glacier.pb.h"
#include "proto/http.pb.h"

#define TMP_VALUE(field_name) field_name ## _value_tmp

#define SET_FIELD_METHOD(field_name) set_ ## field_name

#define GET_TYPE_METHOD(json_type) get_ ## json_type

#define COPY_VALUE_IF_NOT_NULL(json_name, json_type, protobuf, field_name) \
  do {                                                                     \
    auto TMP_VALUE(field_name) =                                           \
        json_spirit::find_value(json_obj, #json_name);                     \
    if (!TMP_VALUE(field_name).is_null()) {                                \
      (protobuf)->SET_FIELD_METHOD(field_name)(                            \
          TMP_VALUE(field_name).GET_TYPE_METHOD(json_type)());             \
    }                                                                      \
  } while (false)

namespace polar_express {
namespace {

const char kAwsDomain[] = "amazonaws.com";
const char kAwsGlacierServiceName[] = "glacier";
const char kAwsGlacierVersionHeaderKey[] = "x-amz-glacier-version";
const char kAwsGlacierVersion[] = "2012-06-01";
const char kAwsGlacierVaultPathPrefix[] = "/-/vaults/";
const char kAwsGlacierArchivesDirectory[] = "archives";
const char kSha256DigestOfEmptyString[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

const size_t kUploadArchivePayloadDescriptionMaxLength = 1024;
const char kUploadArchivePayloadSha256LinearDigestHeaderKey[] =
    "x-amz-content-sha256";
const char kUploadArchivePayloadSha256TreeDigestHeaderKey[] =
    "x-amz-sha256-tree-hash";
const char kUploadArchivePayloadDescriptionHeaderKey[] =
    "x-amz-archive-description";
const char kUploadArchiveArchiveIdHeaderKey[] = "x-amz-archive-id";

const char kListVaultsMaxVaultsKey[] = "limit";
const char kListVaultsStartMarkerKey[] = "marker";

// Wraps a byte vector as a char streambuf.
class vector_byte_streambuf : public std::basic_streambuf<char> {
 public:
  vector_byte_streambuf(std::vector<byte>& vec) {
    char* char_data = reinterpret_cast<char*>(vec.data());
    setg(char_data, char_data, char_data + vec.size());
  }
};

// Not implemented as a class private member because the json_spirit
// types are typedefs and cannot be forward-declared.
bool ParseJsonVaultDescription(
    const json_spirit::Object& json_obj,
    GlacierVaultDescription* vault_description) {
  assert(vault_description != nullptr);
  try {
    COPY_VALUE_IF_NOT_NULL(CreationDate, str, vault_description, creation_date);
    COPY_VALUE_IF_NOT_NULL(LastInventoryDate, str, vault_description,
                           last_inventory_date);
    COPY_VALUE_IF_NOT_NULL(NumberOfArchives, int64, vault_description,
                           number_of_archives);
    COPY_VALUE_IF_NOT_NULL(SizeInBytes, int64, vault_description,
                           size_in_bytes);
    COPY_VALUE_IF_NOT_NULL(VaultARN, str, vault_description, vault_arn);
    COPY_VALUE_IF_NOT_NULL(VaultName, str, vault_description, vault_name);
  } catch (std::runtime_error&) {
    // json_spirit throws std::runtime_error when the value is not the
    // expected type.
    // TODO: log/report this somehow.
    return false;
  }
  return true;
}

}  // namespace

GlacierConnection::GlacierConnection()
    : http_connection_(new HttpConnection),
      amazon_http_request_util_(new AmazonHttpRequestUtil),
      operation_pending_(false),
      last_operation_succeeded_(false) {
}

GlacierConnection::~GlacierConnection() {
}

bool GlacierConnection::is_opening() const {
  return http_connection_->is_opening();
}

bool GlacierConnection::is_open() const {
  return http_connection_->is_open();
}

AsioDispatcher::NetworkUsageType GlacierConnection::network_usage_type() const {
  return http_connection_->network_usage_type();
}

const string& GlacierConnection::aws_region_name() const {
  return aws_region_name_;
}

const string& GlacierConnection::aws_access_key() const {
  return aws_access_key_;
}

const CryptoPP::SecByteBlock& GlacierConnection::aws_secret_key() const {
  return aws_secret_key_;
}

bool GlacierConnection::last_operation_succeeded() const {
  return http_connection_->last_request_succeeded();
}

bool GlacierConnection::Open(
    AsioDispatcher::NetworkUsageType network_usage_type,
    const string& aws_region_name, const string& aws_access_key,
    const CryptoPP::SecByteBlock& aws_secret_key,
    Callback callback) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
  aws_region_name_ = aws_region_name;
  aws_access_key_ = aws_access_key;
  aws_secret_key_ = aws_secret_key;

  return http_connection_->Open(
      network_usage_type,
      algorithm::join<vector<string> >(
          { kAwsGlacierServiceName, aws_region_name_, kAwsDomain }, "."),
      callback);
}

void GlacierConnection::Close() {
  http_connection_->Close();
  aws_region_name_.clear();
  aws_access_key_.clear();
  aws_secret_key_.CleanNew(0);
}

bool GlacierConnection::CreateVault(
    const string& vault_name, bool* vault_created,
    Callback callback) {
  if (!is_open() || operation_pending_) {
    return false;
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::PUT);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix + vault_name);

  SendRequest(
      request, {}, "",
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleCreateVault, this,
                      vault_created, callback)));

  return true;
}

bool GlacierConnection::DescribeVault(
    const string& vault_name, GlacierVaultDescription* vault_description,
    Callback callback) {
  if (!is_open() || operation_pending_) {
    return false;
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::GET);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix + vault_name);

  SendRequest(
      request, {}, "",
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleDescribeVault, this,
                      vault_description, callback)));

  return true;
}

bool GlacierConnection::ListVaults(
    int max_vaults, const string& start_marker,
    GlacierVaultList* vault_list, Callback callback) {
  if (!is_open() || operation_pending_ || max_vaults < 1 || max_vaults > 1000) {
    return false;
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::GET);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix);

  KeyValue* max_vaults_param = request.add_query_parameters();
  max_vaults_param->set_key(kListVaultsMaxVaultsKey);
  max_vaults_param->set_value(lexical_cast<string>(max_vaults));

  if (!start_marker.empty()) {
    KeyValue* start_marker_param = request.add_query_parameters();
    start_marker_param->set_key(kListVaultsStartMarkerKey);
    start_marker_param->set_value(start_marker);
  }

  SendRequest(
      request, {}, "",
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleListVaults, this,
                      vault_list, callback)));

  return true;
}

bool GlacierConnection::DeleteVault(
    const string& vault_name, bool* vault_deleted,
    Callback callback) {
  if (!is_open() || operation_pending_ || vault_name.empty()) {
    return false;
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::DELETE);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix + vault_name);

  SendRequest(
      request, {}, "",
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleDeleteVault, this,
                      vault_deleted, callback)));

  return true;
}

bool GlacierConnection::UploadArchive(
    const string& vault_name, const vector<byte>* payload,
    const string& payload_sha256_linear_digest,
    const string& payload_sha256_tree_digest, const string& payload_description,
    string* archive_id, Callback callback) {
  return UploadArchive(vault_name, {payload}, payload_sha256_linear_digest,
                       payload_sha256_tree_digest, payload_description,
                       archive_id, callback);
}

bool GlacierConnection::UploadArchive(
    const string& vault_name,
    const vector<const vector<byte>*>& sequential_payload,
    const string& payload_sha256_linear_digest,
    const string& payload_sha256_tree_digest, const string& payload_description,
    string* archive_id, Callback callback) {
  if (!is_open() || operation_pending_ ||
      sequential_payload.empty() ||
      payload_sha256_linear_digest.empty() ||
      payload_sha256_tree_digest.empty() ||
      payload_description.size() > kUploadArchivePayloadDescriptionMaxLength) {
    return false;
  }

  // Check restrictions on characters in description.
  for (const char c : payload_description) {
    if (c < ' ' || c > '~') {
      return false;
    }
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::POST);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix + vault_name + '/' +
                   kAwsGlacierArchivesDirectory);

  KeyValue* kv = request.add_request_headers();
  kv->set_key(kUploadArchivePayloadSha256LinearDigestHeaderKey);
  kv->set_value(payload_sha256_linear_digest);

  kv = request.add_request_headers();
  kv->set_key(kUploadArchivePayloadSha256TreeDigestHeaderKey);
  kv->set_value(payload_sha256_tree_digest);

  if (!payload_description.empty()) {
    kv = request.add_request_headers();
    kv->set_key(kUploadArchivePayloadDescriptionHeaderKey);
    kv->set_value(payload_description);
  }

  SendRequest(
      request, sequential_payload, payload_sha256_linear_digest,
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleUploadArchive, this,
                      archive_id, callback)));
  return true;
}

bool GlacierConnection::DeleteArchive(const string& vault_name,
                                      const string& archive_id,
                                      bool* archive_deleted,
                                      Callback callback) {
  if (!is_open() || operation_pending_ || vault_name.empty() ||
      archive_id.empty()) {
    return false;
  }

  operation_pending_ = true;

  HttpRequest request;
  request.set_method(HttpRequest::DELETE);
  request.set_hostname(http_connection_->hostname());
  request.set_path(kAwsGlacierVaultPathPrefix + vault_name + '/' +
                   kAwsGlacierArchivesDirectory + '/' + archive_id);

  SendRequest(
      request, {}, "",
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&GlacierConnection::HandleDeleteArchive, this,
                      archive_deleted, callback)));

  return true;
}

bool GlacierConnection::SendRequest(
    const HttpRequest& request,
    const vector<const vector<byte>*>& sequential_payload,
    const string& payload_sha256_digest, Callback callback) {
  HttpRequest authorized_request(request);
  KeyValue* kv = authorized_request.add_request_headers();
  kv->set_key(kAwsGlacierVersionHeaderKey);
  kv->set_value(kAwsGlacierVersion);

  bool is_empty_payload = true;
  for (const auto* buf : sequential_payload) {
    if (buf != nullptr && !buf->empty()) {
      is_empty_payload = false;
      break;
    }
  }

  if (amazon_http_request_util_->AuthorizeRequest(
          aws_secret_key_,
          aws_access_key_,
          aws_region_name_,
          kAwsGlacierServiceName,
          (is_empty_payload ?
           kSha256DigestOfEmptyString : payload_sha256_digest),
          &authorized_request)) {
    response_.reset(new HttpResponse);
    response_payload_.reset(new vector<byte>);
    return http_connection_->SendRequest(authorized_request, sequential_payload,
                                         response_.get(),
                                         response_payload_.get(), callback);
  }

  return false;
}

void GlacierConnection::HandleCreateVault(
    bool* vault_created,
    Callback create_vault_callback) {
  // Note that AWS returns "201 Created", NOT "200 OK" for success.
  *CHECK_NOTNULL(vault_created) =
      http_connection_->last_request_succeeded() &&
      response_->status_code() == 201;

  last_operation_succeeded_ = *vault_created;
  CleanUpRequestState();
  create_vault_callback();
}

void GlacierConnection::HandleDescribeVault(
    GlacierVaultDescription* vault_description,
    Callback describe_vault_callback) {
  assert(vault_description != nullptr);

  if (!http_connection_->last_request_succeeded() &&
      response_->status_code() != 200) {
    HandleOperationError(describe_vault_callback);
    return;
  }

  vector_byte_streambuf response_payload_streambuf(*response_payload_);
  istream response_payload_istream(&response_payload_streambuf);

  // Would prefer to use boost::json_parser here, but the version of
  // this boost library on Raring seems borked.
  json_spirit::Value json_value;
  if (!json_spirit::read(response_payload_istream, json_value) ||
      json_value.type() != json_spirit::obj_type) {
    HandleOperationError(describe_vault_callback);
    return;
  }

  const json_spirit::Object& json_obj = json_value.get_obj();
  last_operation_succeeded_ =
      ParseJsonVaultDescription(json_obj, vault_description);

  CleanUpRequestState();
  describe_vault_callback();
}

void GlacierConnection::HandleListVaults(
    GlacierVaultList* vault_list,
    Callback list_vaults_callback) {
  assert(vault_list != nullptr);

  if (!http_connection_->last_request_succeeded() &&
      response_->status_code() != 200) {
    HandleOperationError(list_vaults_callback);
    return;
  }

  vector_byte_streambuf response_payload_streambuf(*response_payload_);
  istream response_payload_istream(&response_payload_streambuf);

  json_spirit::Value json_value;
  if (!json_spirit::read(response_payload_istream, json_value) ||
      json_value.type() != json_spirit::obj_type) {
    HandleOperationError(list_vaults_callback);
    return;
  }

  try {
    const json_spirit::Object& json_obj = json_value.get_obj();

    const auto& json_marker_obj =
        json_spirit::find_value(json_obj, "Marker");
    if (!json_marker_obj.is_null()) {
      vault_list->set_next_start_marker(json_marker_obj.get_str());
    }

    const json_spirit::Array& json_arr =
        json_spirit::find_value(json_obj, "VaultList").get_array();
    for (const auto& json_arr_value : json_arr) {
      if (json_arr_value.type() != json_spirit::obj_type ||
          !ParseJsonVaultDescription(
              json_arr_value.get_obj(), vault_list->add_vault_descriptions())) {
        HandleOperationError(list_vaults_callback);
        return;
      }
    }
    last_operation_succeeded_ = true;
  } catch (std::runtime_error& ex) {
    // TODO: log/report this somehow.
    last_operation_succeeded_ = false;
  }

  CleanUpRequestState();
  list_vaults_callback();
}

void GlacierConnection::HandleDeleteVault(
    bool* vault_deleted,
    Callback delete_vault_callback) {
  // Note that AWS returns "204 No Content", NOT "200 OK" for success.
  *CHECK_NOTNULL(vault_deleted) =
      http_connection_->last_request_succeeded() &&
      response_->status_code() == 204;

  last_operation_succeeded_ = *vault_deleted;
  CleanUpRequestState();
  delete_vault_callback();
}

void GlacierConnection::HandleUploadArchive(
    string* archive_id,
    Callback upload_archive_callback) {
  *CHECK_NOTNULL(archive_id) = "";

  // Note that AWS returns "201 Created", NOT "200 OK" for success.
  if (!http_connection_->last_request_succeeded() ||
      response_->status_code() != 201) {
    HandleOperationError(upload_archive_callback);
    return;
  }

  for (const auto& kv : response_->response_headers()) {
    if (kv.key() == kUploadArchiveArchiveIdHeaderKey) {
      *archive_id = kv.value();
      break;
    }
  }

  last_operation_succeeded_ = !archive_id->empty();
  CleanUpRequestState();
  upload_archive_callback();
}

void GlacierConnection::HandleDeleteArchive(
    bool* archive_deleted,
    Callback delete_archive_callback) {
  // Note that AWS returns "204 No Content", NOT "200 OK" for success.
  *CHECK_NOTNULL(archive_deleted) =
      http_connection_->last_request_succeeded() &&
      response_->status_code() == 204;

  last_operation_succeeded_ = *archive_deleted;
  CleanUpRequestState();
  delete_archive_callback();
}

void GlacierConnection::HandleOperationError(Callback callback) {
  last_operation_succeeded_ = false;
  CleanUpRequestState();
  callback();
}

void GlacierConnection::CleanUpRequestState() {
  response_.reset();
  response_payload_.reset();
  operation_pending_ = false;
}

}  // namespace polar_express
