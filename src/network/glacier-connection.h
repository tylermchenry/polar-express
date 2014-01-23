#ifndef GLACIER_CONNECTION
#define GLACIER_CONNECTION

#include <memory>
#include <string>
#include <vector>

#include <crypto++/secblock.h>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "util/amazon-http-request-util.h"

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

  virtual bool is_secure() const;
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

  virtual bool Reopen(Callback callback);

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

  virtual bool DeleteVault(
      const string& vault_name, bool* vault_deleted,
      Callback callback);

  // Uploads an archive into the named vault. The named vault must already
  // exist, and the caller must provide pre-computed SHA256 linear and tree
  // digests for the payload. The description may be left black. If specified,
  // it is restricted to at most 1024 characters of printable ASCII.
  //
  // When the upload completes successfully, the Amazon-assigned archive ID (not
  // the same as the description) is filled in to archive_id. If there is an
  // error, the archive ID will be set to the empty string.
  virtual bool UploadArchive(
      const string& vault_name,
      const vector<byte>* payload,
      const string& payload_sha256_linear_digest,
      const string& payload_sha256_tree_digest,
      const string& payload_description,
      string* archive_id, Callback callback);

  // Version of UploadArchive that accepts a series of sequential byte buffers
  // for the payload.
  virtual bool UploadArchive(
      const string& vault_name,
      const vector<const vector<byte>*>& sequential_payload,
      const string& payload_sha256_linear_digest,
      const string& payload_sha256_tree_digest,
      const string& payload_description,
      string* archive_id, Callback callback);

  virtual bool DeleteArchive(
      const string& vault_name, const string& archive_id,
      bool* archive_deleted, Callback callback);

 protected:
  explicit GlacierConnection(bool secure);

 private:
  bool SendRequest(
      const HttpRequest& request,
      const vector<const vector<byte>*>& sequential_payload,
      const string& payload_sha256_digest, Callback callback);

  void HandleCreateVault(
      bool* vault_created,
      Callback create_vault_callback);

  void HandleDescribeVault(
      GlacierVaultDescription* vault_description,
      Callback describe_vault_callback);

  void HandleListVaults(
      GlacierVaultList* vault_list,
      Callback list_vaults_callback);

  void HandleDeleteVault(
      bool* vault_deleted,
      Callback delete_vault_callback);

  void HandleUploadArchive(
      string* archive_id,
      Callback upload_archive_callback);

  void HandleDeleteArchive(
      bool* archive_deleted,
      Callback delete_archive_callback);

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
  const bool send_secure_requests_;

  DISALLOW_COPY_AND_ASSIGN(GlacierConnection);
};

class SecureGlacierConnection : public GlacierConnection {
 public:
  SecureGlacierConnection();
  virtual ~SecureGlacierConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureGlacierConnection);
};

}  // namespace polar_express

#endif  // GLACIER_CONNECTION
