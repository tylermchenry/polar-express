#ifndef HTTP_CLIENT_CONNECTION
#define HTTP_CLIENT_CONNECTION

#include <memory>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "network/http-connection.h"

namespace polar_express {

class HttpRequest;
class HttpResponse;
class StreamConnection;

class HttpClientConnection : public HttpConnection {
 public:
  HttpClientConnection();
  virtual ~HttpClientConnection();

  virtual const string& hostname() const;
  virtual const string& protocol() const;
  virtual bool last_request_succeeded() const;

  virtual bool Open(
      AsioDispatcher::NetworkUsageType network_usage_type,
      const string& hostname, Callback callback);

  virtual bool Reopen(Callback callback);

  // Returns false if the connection is not open, or if a request is
  // already pending. response must not be null, but response_payload
  // may be null.
  //
  // The request's http_version is ignored (1.1 is always used). It is
  // not necessary that the 'host' field in the request match the
  // hostname used for the Open() call, but if host is omitted from
  // the request, the opened hostname will be used. The Content-Length
  // header will be forced to match the length of the request payload. No
  // Content-Type header will be added, so the caller is responsible for
  // this.
  //
  // If any part of the request fails, the connection will be closed,
  // so as not to leave stray data behind in the stream.
  virtual bool SendRequest(
      const HttpRequest& request, const vector<byte>* request_payload,
      HttpResponse* response, vector<byte>* response_payload,
      Callback callback);

  // Version of SendRequest that accepts a series of sequential byte buffers
  // for the payload.
  virtual bool SendRequest(
      const HttpRequest& request,
      const vector<const vector<byte>*>& request_sequential_payload,
      HttpResponse* response, vector<byte>* response_payload,
      Callback callback);

 protected:
  explicit HttpClientConnection(
      unique_ptr<StreamConnection>&& stream_connection);

 private:
  void SerializeRequest(const HttpRequest& request, size_t payload_size);

  void BuildQueryParameters(
      const HttpRequest& request, string* query_parameters) const;

  void BuildRequestHeaders(
      const HttpRequest& request, string* request_headers) const;

  bool DeserializeResponse(HttpResponse* response);

  void ParseResponseHeaders(
      vector<byte>::const_iterator begin,
      vector<byte>::const_iterator end,
      HttpResponse* response) const;

  void ParseResponseStatus(
      const string& status_line, HttpResponse* response) const;

  const string& GetResponseHeaderValue(
      const HttpResponse& response, const string& key) const;

  bool IsResponsePayloadChunked(const HttpResponse& response) const;

  size_t GetResponsePayloadSize(const HttpResponse& response) const;

  void RequestSent(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponseReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadChunkHeaderReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadChunkSeparatorReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadChunkReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadPostChunkHeaderReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void ResponsePayloadReceived(
      HttpResponse* response, vector<byte>* response_payload,
      Callback send_request_callback);

  void HandleRequestError(
      HttpResponse* response, Callback send_response_callback);

  void CleanUpRequestState();

  bool request_pending_;
  bool last_request_succeeded_;
  unique_ptr<vector<byte> > serialized_request_;
  unique_ptr<vector<byte> > serialized_response_;
  unique_ptr<vector<byte> > tmp_response_payload_;
  unique_ptr<vector<byte> > response_payload_chunk_buffer_;

  DISALLOW_COPY_AND_ASSIGN(HttpClientConnection);
};

class HttpsClientConnection : public HttpClientConnection {
 public:
  HttpsClientConnection();
  virtual ~HttpsClientConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpsClientConnection);
};

}  // namespace polar_express

#endif  // HTTP_CLIENT_CONNECTION

