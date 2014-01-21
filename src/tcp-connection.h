#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <memory>

#include "boost/asio.hpp"
#include "boost/asio/ip/tcp.hpp"

#include "asio-dispatcher.h"
#include "macros.h"
#include "stream-connection.h"

namespace polar_express {

// Stream connection using a basic, unaugmented TCP socket.
class TcpConnection : public StreamConnectionTmpl<asio::ip::tcp::socket> {
 public:
  TcpConnection();
  virtual ~TcpConnection();

 private:
  virtual unique_ptr<asio::ip::tcp::socket> StreamConstruct(
      asio::io_service& io_service);

  DISALLOW_COPY_AND_ASSIGN(TcpConnection);
};

}  // namespace polar_express

#endif  // TCP_CONNECTION
