#include "glacier-connection.h"

#include "http-connection.h"
#include "proto/glacier.pb.h"

namespace polar_express {

GlacierConnection::GlacierConnection()
    : http_connection_(new HttpConnection) {
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

const string& GlacierConnection::aws_region() const {
  return aws_region_;
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
    const string& aws_region, const string& aws_access_key,
    const CryptoPP::SecByteBlock& aws_secret_key,
    Callback callback) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
  aws_region_ = aws_region;
  aws_access_key_ = aws_access_key;
  aws_secret_key_ = aws_secret_key;

  return http_connection_->Open(
      network_usage_type, "glacier." + aws_region_ + "amazonaws.com", callback);
}

void GlacierConnection::Close() {
  http_connection_->Close();
  aws_region_.clear();
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

}  // namespace polar_express
