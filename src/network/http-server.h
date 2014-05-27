#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

class HttpRequest;
class HttpResponse;
class HttpServerConnection;
class TcpConnection;

// Bare-bones HTTP server to support things like status and configuration
// interfaces. Nothing fancy.
class HttpServer {
 public:
  HttpServer();
  virtual ~HttpServer();

  class Handler {
   public:
    // If false is returned, this indicates that the connection should be closed
    // after the response is sent.
    //
    // Certain parts of the response will be set/overridden by the HttpServer
    // object before the response is sent, in particular:
    //  - http_version (will be forced to actual version being used)
    //  - status_phrase (will be forced to match status_code)
    //  - Content-Length header (will be forced to equal payload length)
    //
    // If the handler makes certain errors, a generic 500 error response will be
    // returned instead. In particular this will happen:
    //  - If the handler sets an invalid status code.
    //  - If the handler specifies is_secure=true on a non-secure connection.
    virtual bool HandleHttpRequest(const HttpRequest& request,
                                   const vector<byte>& request_payload,
                                   HttpResponse* response,
                                   ostream* response_payload_stream) = 0;
  };

  // Two handlers cannot have the same prefix. Returns false if there is already
  // a handler registered for this prefix. If two handlers have prefixes that
  // nest, the most specific handler is called. E.g. if one handler handles the
  // prefix 'a' and another handles the prefix 'a/b', the requests for 'a/b',
  // 'a/b/c', etc. will hit the 'a/b' handler, but requests for 'a', 'a/d',
  // 'a/e', etc. will hit the 'a' handler.
  bool RegisterHandler(const string& path_prefix,
                       boost::shared_ptr<Handler> handler);

  void UnregisterHandler(const string& path_prefix);

  // While running, it is not legal to register or unregister handlers.
  bool Run(uint16_t port);
  void Stop();
  bool is_running() const;

 private:
  boost::shared_ptr<Handler> FindHandler(const string& path) const;

  void CreateNetworkingObjects();
  void DestroyNetworkingObjects();

  void AsyncAccept();
  void HandleAccept(const boost::system::error_code& err);

  void AsyncReceiveRequest(
      boost::shared_ptr<HttpServerConnection> server_connection);
  void HandleReceiveRequest(
      boost::shared_ptr<HttpServerConnection> server_connection);

  void AsyncSendResponse(
      boost::shared_ptr<HttpServerConnection> server_connection);
  void HandleSendResponse(
      boost::shared_ptr<HttpServerConnection> server_connection);

  void Close(boost::shared_ptr<HttpServerConnection> server_connection);

  bool is_running_;
  map<string, boost::shared_ptr<Handler> > handlers_;

  // Networking objects managed by Create/Destroy methods above.
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  unique_ptr<asio::io_service::work> io_service_work_;
  unique_ptr<asio::ip::tcp::resolver> resolver_;
  unique_ptr<asio::ip::tcp::acceptor> acceptor_;

  unique_ptr<asio::ip::tcp::socket> next_socket_;

  struct ConnectionContext;
  map<boost::shared_ptr<HttpServerConnection>, ConnectionContext>
      connection_contexts_;

  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

}  // namespace polar_express

#endif  // HTTP_SERVER

