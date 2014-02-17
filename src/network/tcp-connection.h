#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "base/asio-dispatcher.h"
#include "base/macros.h"
#include "network/stream-connection.h"

namespace polar_express {

// Stream connection using a basic, unaugmented TCP socket.
class TcpConnection : public StreamConnectionTmpl<asio::ip::tcp::socket> {
 public:
  TcpConnection();
  TcpConnection(unique_ptr<asio::ip::tcp::socket>&& connected_socket,
                const ConnectionProperties& properties);
  virtual ~TcpConnection();

 private:
  virtual unique_ptr<asio::ip::tcp::socket> StreamConstruct(
      asio::io_service& io_service);

  DISALLOW_COPY_AND_ASSIGN(TcpConnection);
};

}  // namespace polar_express

#endif  // TCP_CONNECTION
