#include "glacier-connection.h"

#include "http-connection.h"
#include "proto/glacier.pb.h"
#include "proto/http.pb.h"

namespace polar_express {

GlacierConnection::GlacierConnection()
    : http_connection_(new HttpConnection),
      amazon_http_request_util_(new AmazonHttpRequestUtil) {
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
      network_usage_type, "glacier." + aws_region_name_ + "amazonaws.com", callback);
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
  // TODO: Implement
  return false;
}

bool GlacierConnection::ListVaults(
    GlacierVaultList* vault_list, Callback callback) {
  // TODO: Implement
  return false;
}

bool GlacierConnection::SendRequest(
    const HttpRequest& request, const vector<byte>& payload,
    const string& payload_sha256_digest, Callback callback) {
  const char kAwsGlacierServiceName[] = "glacier";
  const char kAwsGlacierVersionHeaderKey[] = "x-amz-glacier-version";
  const char kAwsGlacierVersion[] = "2012-06-01";
  const char kSha256DigestOfEmptyString[] =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

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
    return http_connection_->SendRequest(
        authorized_request, payload, response_.get(), response_payload_.get(),
        callback);
  }

  return false;
}

void GlacierConnection::CleanUpRequestState() {
  response_.reset();
  response_payload_.reset();
}

}  // namespace polar_express
