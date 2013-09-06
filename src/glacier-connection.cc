#include "glacier-connection.h"

#include <iostream>
#include <sstream>

#include "boost/algorithm/string.hpp"
#include "boost/bind.hpp"
#include "json_spirit.h"

#include "http-connection.h"
#include "proto/glacier.pb.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

const char kAwsDomain[] = "amazonaws.com";
const char kAwsGlacierServiceName[] = "glacier";
const char kAwsGlacierVersionHeaderKey[] = "x-amz-glacier-version";
const char kAwsGlacierVersion[] = "2012-06-01";
const char kAwsGlacierVaultPathPrefix[] = "/-/vaults/";
const char kSha256DigestOfEmptyString[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

// Wraps a byte vector as a char streambuf.
class vector_byte_streambuf : public std::basic_streambuf<char> {
 public:
  vector_byte_streambuf(std::vector<byte>& vec) {
    char* char_data = reinterpret_cast<char*>(vec.data());
    setg(char_data, char_data, char_data + vec.size());
  }
};

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
  // TODO: Implement
  return false;
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
    GlacierVaultList* vault_list, Callback callback) {
  // TODO: Implement
  return false;
}

bool GlacierConnection::SendRequest(
    const HttpRequest& request, const vector<byte>& payload,
    const string& payload_sha256_digest, Callback callback) {
  HttpRequest authorized_request(request);
  KeyValue* kv = authorized_request.add_request_headers();
  kv->set_key(kAwsGlacierVersionHeaderKey);
  kv->set_value(kAwsGlacierVersion);

  if (amazon_http_request_util_->AuthorizeRequest(
          aws_secret_key_,
          aws_access_key_,
          aws_region_name_,
          kAwsGlacierServiceName,
          (payload.empty() ?
           kSha256DigestOfEmptyString : payload_sha256_digest),
          &authorized_request)) {
    response_.reset(new HttpResponse);
    response_payload_.reset(new vector<byte>);
    return http_connection_->SendRequest(
        authorized_request, payload, response_.get(), response_payload_.get(),
        callback);
  }

  return false;
}

void GlacierConnection::HandleDescribeVault(
    GlacierVaultDescription* vault_description,
    Callback describe_vault_callback) {
  assert(vault_description != nullptr);
  last_operation_succeeded_ = false;

  if (http_connection_->last_request_succeeded() &&
      response_->status_code() == 200) {
    vector_byte_streambuf response_payload_streambuf(*response_payload_);
    istream response_payload_istream(&response_payload_streambuf);

    // Would prefer to use boost::json_parser here, but the version of
    // this boost library on Raring seems borked.
    json_spirit::Value json_value;
    if (json_spirit::read(response_payload_istream, json_value) &&
        json_value.type() == json_spirit::obj_type) {
      const json_spirit::Object& json_obj = json_value.get_obj();
      vault_description->set_creation_date(
          json_spirit::find_value(json_obj, "CreationDate").get_str());
      vault_description->set_last_inventory_date(
          json_spirit::find_value(json_obj, "LastInventoryDate").get_str());
      vault_description->set_number_of_archives(
          json_spirit::find_value(json_obj, "NumberOfArchives").get_int());
      vault_description->set_size_in_bytes(
          json_spirit::find_value(json_obj, "SizeInBytes").get_int());
      vault_description->set_vault_arn(
          json_spirit::find_value(json_obj, "VaultARN").get_str());
      vault_description->set_vault_name(
          json_spirit::find_value(json_obj, "VaultName").get_str());
      last_operation_succeeded_ = true;
    }
  }

  CleanUpRequestState();
  describe_vault_callback();
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
