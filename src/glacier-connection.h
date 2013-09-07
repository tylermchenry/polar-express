#ifndef GLACIER_CONNECTION
#define GLACIER_CONNECTION

#include <memory>
#include <string>
#include <vector>

#include "crypto++/secblock.h"

#include "amazon-http-request-util.h"
#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

class GlacierVaultDescription;
class GlacierVaultList;
class HttpConnection;
class HttpRequest;
class HttpResponse;

class GlacierConnection {
 public:
  GlacierConnection();
  virtual ~GlacierConnection();

  virtual bool is_opening() const;
  virtual bool is_open() const;
  virtual AsioDispatcher::NetworkUsageType network_usage_type() const;
  virtual const string& aws_region_name() const;
  virtual const string& aws_access_key() const;
  virtual const CryptoPP::SecByteBlock& aws_secret_key() const;
  virtual bool last_operation_succeeded() const;

  virtual bool Open(
      AsioDispatcher::NetworkUsageType network_usage_type,
      const string& aws_region_name, const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      Callback callback);

  virtual void Close();

  virtual bool CreateVault(
      const string& vault_name, bool* vault_created,
      Callback callback);

  virtual bool DescribeVault(
      const string& vault_name, GlacierVaultDescription* vault_description,
      Callback callback);

  // Lists up to max_vaults vaults starting from the position denoted
  // by start_marker (normally obtained through a previous ListVaults
  // request). max_vaults must be between 1 and 1000 inclusive (or
  // false will be returned). start_marker may be empty, in which case the
  // listing will start from the beginning.
  virtual bool ListVaults(
      int max_vaults, const string& start_marker,
      GlacierVaultList* vault_list, Callback callback);

 private:
  bool SendRequest(
      const HttpRequest& request, const vector<byte>& payload,
      const string& payload_sha256_digest, Callback callback);

  void HandleDescribeVault(
      GlacierVaultDescription* vault_description,
      Callback describe_vault_callback);

  void HandleListVaults(
      GlacierVaultList* vault_list,
      Callback list_vaults_callback);

  void HandleOperationError(Callback callback);

  void CleanUpRequestState();

  string aws_region_name_;
  string aws_access_key_;
  CryptoPP::SecByteBlock aws_secret_key_;
  bool operation_pending_;
  bool last_operation_succeeded_;

  unique_ptr<HttpResponse> response_;
  unique_ptr<vector<byte> > response_payload_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  const unique_ptr<HttpConnection> http_connection_;
  const unique_ptr<AmazonHttpRequestUtil> amazon_http_request_util_;

  DISALLOW_COPY_AND_ASSIGN(GlacierConnection);
};

}  // namespace polar_express

#endif  // GLACIER_CONNECTION
