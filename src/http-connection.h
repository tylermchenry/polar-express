#ifndef HTTP_CONNECTION
#define HTTP_CONNECTION

#include <memory>
#include <vector>

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

class HttpRequest;
class HttpResponse;
class TcpConnection;

class HttpConnection {
 public:
  HttpConnection();
  virtual ~HttpConnection();

  // These methods are synchronous but not internally synchronized.
  virtual bool is_opening() const;
  virtual bool is_open() const;
  virtual AsioDispatcher::NetworkUsageType network_usage_type() const;
  virtual const string& hostname() const;
  virtual const string& protocol() const;
  virtual bool last_request_succeeded() const;

  virtual bool Open(
      AsioDispatcher::NetworkUsageType network_usage_type,
      const string& hostname, Callback callback);

  virtual bool Close();

  // Returns false if the connection is not open, or if a request is
  // already pending. response must not be null, but response_payload
  // may be null. It is not necessary that the 'host' field in the
  // request match the hostname used for the Open() call, but if host is
  // omitted from the request, the opened hostname will be used.
  virtual bool SendRequest(
      const HttpRequest& request, const vector<byte>& request_payload,
      HttpResponse* response, vector<byte>* response_payload,
      Callback callback);

 private:
  void SerializeRequest(const HttpRequest& request);

  bool DeserializeResponse(HttpResponse* response);

  void RequestSent(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponseReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadReceived(
      Callback send_request_callback);

  void CleanUpRequestState();

  bool request_pending_;
  bool last_request_succeeded_;
  unique_ptr<vector<byte> > serialized_request_;
  unique_ptr<vector<byte> > serialized_response_;
  unique_ptr<vector<byte> > tmp_response_payload_;

  const unique_ptr<TcpConnection> tcp_connection_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace polar_express

#endif  // HTTP_CONNECTION
