#include "tcp-connection.h"

#include "make-unique.h"

namespace polar_express {

TcpConnection::TcpConnection()
    : StreamConnection<asio::ip::tcp::socket>(false  /* not secure */) {
}

TcpConnection::~TcpConnection() {
}

unique_ptr<asio::ip::tcp::socket> TcpConnection::StreamConstruct(
    asio::io_service& io_service) const {
  return make_unique<asio::ip::tcp::socket>(io_service);
}

void TcpConnection::StreamShutdown(
    asio::ip::tcp::socket* stream, system::error_code& error_code) const {
  CHECK_NOTNULL(stream)->shutdown(
      asio::ip::tcp::socket::shutdown_both, error_code);
}

}  // namespace polar_express
