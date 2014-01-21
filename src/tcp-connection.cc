#include "tcp-connection.h"

#include "make-unique.h"

namespace polar_express {

TcpConnection::TcpConnection()
    : StreamConnectionTmpl<asio::ip::tcp::socket>(false  /* not secure */) {
}

TcpConnection::~TcpConnection() {
}

unique_ptr<asio::ip::tcp::socket> TcpConnection::StreamConstruct(
    asio::io_service& io_service) {
  return make_unique<asio::ip::tcp::socket>(io_service);
}

}  // namespace polar_express
