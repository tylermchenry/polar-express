#ifndef HTTP_SERVER_CONNECTION
#define HTTP_SERVER_CONNECTION

#include <memory>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "network/http-connection.h"

namespace polar_express {

class HttpRequest;
class HttpResponse;
class KeyValue;
class StreamConnection;

class HttpServerConnection : public HttpConnection {
 public:
  explicit HttpServerConnection(
      unique_ptr<StreamConnection>&& stream_connection);
  virtual ~HttpServerConnection();

  virtual bool last_operation_succeeded() const;

  // Receives an incoming request from a server, and invokes callback when the
  // request is completely recieved, including payload. Returns false if another
  // ReceiveRequest call is already pending.
  virtual bool ReceiveRequest(
      HttpRequest* request,
      vector<byte>* request_payload, Callback callback);

  // Sends a response to a server, and invokes callback when the response is
  // finished being sent.
  virtual bool SendResponse(
      const HttpResponse& response,
      const vector<byte>* response_payload, Callback callback);

  // Version of SendResponse that accepts a series of sequential byte buffers
  // for the payload.
  virtual bool SendResponse(
      const HttpResponse& response,
      const vector<const vector<byte>*>& response_sequential_payload,
      Callback callback);

 private:
  bool IsRequestPayloadChunked(const HttpRequest& request) const;

  size_t GetRequestPayloadSize(const HttpRequest& request) const;

  bool ParseRequestLine(const string& request_line, HttpRequest* request) const;

  bool ParseRequestQueryParameters(
      const string& query_parameters_str,
      google::protobuf::RepeatedPtrField<KeyValue>* query_parameters) const;

  void ParseRequestHeaders(
      vector<byte>::const_iterator begin,
      vector<byte>::const_iterator end,
      HttpRequest* request) const;

  bool DeserializeRequest(HttpRequest* request) const;

  boost::shared_ptr<vector<byte> > SerializeResponse(
      const HttpResponse& response, size_t payload_size) const;

  void RequestReceived(
      HttpRequest* request,
      vector<byte>* request_payload,
      Callback receive_request_callback);

  void RequestPayloadReceived(
      HttpRequest* request,
      vector<byte>* request_payload,
      Callback receive_request_callback);

  void HandleRequestError(
      HttpRequest* request,
      Callback receive_request_callback);

  void CleanUpRequestState();

  void ResponseSent(
      boost::shared_ptr<vector<byte> > serialized_response,
      Callback response_sent_callback);

  bool waiting_for_request_;
  bool last_operation_succeeded_;
  unique_ptr<vector<byte> > serialized_request_;
  unique_ptr<vector<byte> > tmp_request_payload_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerConnection);
};

}  // namespace polar_express

#endif  // HTTP_SERVER_CONNECTION
