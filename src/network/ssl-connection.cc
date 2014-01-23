#include "network/ssl-connection.h"

#include <openssl/ssl.h>

namespace polar_express {

SslConnection::SslConnection()
    : StreamConnectionTmpl<asio::ssl::stream<asio::ip::tcp::socket> >(
          true /* secure */) {
}

SslConnection::~SslConnection() {
}

unique_ptr<asio::ssl::stream<asio::ip::tcp::socket> >
SslConnection::StreamConstruct(asio::io_service& io_service) {
  ssl_context_.reset(new asio::ssl::context(boost::asio::ssl::context::sslv23));
  ssl_context_->set_default_verify_paths();  // Use system certificate store.

  unique_ptr<asio::ssl::stream<asio::ip::tcp::socket> > stream(
      new asio::ssl::stream<asio::ip::tcp::socket>(io_service, *ssl_context_));
  stream->set_verify_mode(asio::ssl::verify_peer);

  return std::move(stream);
}

void SslConnection::AfterConnect(Callback open_callback) {
  auto handler = MakeStrandCallbackWithArgs<
    const system::error_code&>(
        boost::bind(
            &SslConnection::HandleHandshake, this, _1, open_callback),
        asio::placeholders::error);

  stream().async_handshake(boost::asio::ssl::stream_base::client, handler);
}

void SslConnection::HandleHandshake(
    const system::error_code& err, Callback open_callback) {
  if (err) {
    Close();
  }
  open_callback();
}

}  // namespace polar_express
