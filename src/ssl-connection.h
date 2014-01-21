#ifndef SSL_CONNECTION
#define SSL_CONNECTION

#include <memory>

#include "boost/asio/ssl.hpp"

#include "callback.h"
#include "macros.h"
#include "stream-connection.h"

namespace polar_express {

class SslConnection
    : public StreamConnectionTmpl<asio::ssl::stream<asio::ip::tcp::socket> > {
 public:
  SslConnection();
  virtual ~SslConnection();

 private:
  virtual unique_ptr<asio::ssl::stream<asio::ip::tcp::socket> > StreamConstruct(
      asio::io_service& io_service);

  virtual void AfterConnect(Callback open_callback);

  void HandleHandshake(const system::error_code& err, Callback open_callback);

  unique_ptr<asio::ssl::context> ssl_context_;

  DISALLOW_COPY_AND_ASSIGN(SslConnection);
};

}  // namespace polar_express

#endif  // SSL_CONNECTION
