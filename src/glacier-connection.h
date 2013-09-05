#ifndef GLACIER_CONNECTION
#define GLACIER_CONNECTION

#include <memory>
#include <string>

#include "crypto++/secblock.h"

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

class GlacierVaultDescription;
class GlacierVaultList;
class HttpConnection;

class GlacierConnection {
 public:
  GlacierConnection();
  virtual ~GlacierConnection();

  virtual bool is_opening() const;
  virtual bool is_open() const;
  virtual AsioDispatcher::NetworkUsageType network_usage_type() const;
  virtual const string& aws_region() const;
  virtual const string& aws_access_key() const;
  virtual const CryptoPP::SecByteBlock& aws_secret_key() const;
  virtual bool last_operation_succeeded() const;

  virtual bool Open(
      AsioDispatcher::NetworkUsageType network_usage_type,
      const string& aws_region, const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      Callback callback);

  virtual void Close();

  virtual bool CreateVault(
      const string& vault_name, bool* vault_created,
      Callback callback);

  virtual bool DescribeVault(
      const string& vault_name, GlacierVaultDescription* vault_description,
      Callback callback);

  virtual bool ListVaults(
      GlacierVaultList* vault_list, Callback callback);

 private:
  string aws_region_;
  string aws_access_key_;
  CryptoPP::SecByteBlock aws_secret_key_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  const unique_ptr<HttpConnection> http_connection_;

  DISALLOW_COPY_AND_ASSIGN(GlacierConnection);
};

}  // namespace polar_express

#endif  // GLACIER_CONNECTION
