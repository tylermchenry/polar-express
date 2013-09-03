#include "http-connection.h"

#include <algorithm>
#include <sstream>

#include "boost/algorithm/string.hpp"
#include "boost/bind.hpp"
#include "curl/curl.h"

#include "tcp-connection.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

const char kHttpProtocol[] = "http";

}  // namespace

HttpConnection::HttpConnection()
    : request_pending_(false),
      last_request_succeeded_(false),
      tcp_connection_(new TcpConnection),
      curl_(curl_easy_init()) {
}

HttpConnection::~HttpConnection() {
  curl_easy_cleanup(curl_);
}

bool HttpConnection::is_opening() const {
  return tcp_connection_->is_opening();
}

bool HttpConnection::is_open() const {
  return tcp_connection_->is_open();
}

AsioDispatcher::NetworkUsageType HttpConnection::network_usage_type() const {
  return tcp_connection_->network_usage_type();
}

const string& HttpConnection::hostname() const {
  return tcp_connection_->hostname();
}

const string& HttpConnection::protocol() const {
  return tcp_connection_->protocol();
}

bool HttpConnection::last_request_succeeded() const {
  return last_request_succeeded_;
}

bool HttpConnection::Open(
    AsioDispatcher::NetworkUsageType network_usage_type,
    const string& hostname, Callback callback) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
  return tcp_connection_->Open(
      network_usage_type, hostname, kHttpProtocol, callback);
}

bool HttpConnection::Close() {
  return tcp_connection_->Close();
}

bool HttpConnection::SendRequest(
    const HttpRequest& request, const vector<byte>& request_payload,
    HttpResponse* response, vector<byte>* response_payload,
    Callback callback) {
  if (!is_open() || request_pending_) {
    return false;
  }

  request_pending_ = true;
  SerializeRequest(request, request_payload.size());

  if (response_payload == nullptr) {
    tmp_response_payload_.reset(new vector<byte>);
    response_payload = tmp_response_payload_.get();
  }


  Callback request_sent_callback =
      strand_dispatcher_->CreateStrandCallback(
          boost::bind(&HttpConnection::RequestSent, this,
                      response, response_payload, callback));
  if (tcp_connection_->WriteAll(
          { serialized_request_.get(), &request_payload },
          request_sent_callback)) {
    return true;
  }

  CleanUpRequestState();
  return false;
}

void HttpConnection::SerializeRequest(
    const HttpRequest& request, size_t payload_size) {
  const char* const method =
      request.method() == HttpRequest::POST ? "POST" : "GET";
  const string& hostname =
      request.hostname().empty() ? this->hostname() : request.hostname();

  string query_parameters;
  BuildQueryParameters(request, &query_parameters);

  string request_headers;
  BuildRequestHeaders(request, &request_headers);

  ostringstream serialized_request_sstr;
  serialized_request_sstr << method << " " << request.path();
  if (!query_parameters.empty()) {
    serialized_request_sstr << "?" << query_parameters;
  }
  serialized_request_sstr << " HTTP/1.1\r\n"
                          << " Host: " << hostname << "\r\n";
  if (!request_headers.empty()) {
    serialized_request_sstr << request_headers;
  }
  if (payload_size > 0) {
    serialized_request_sstr << "Content-Length: " << payload_size << "\r\n";
  }
  serialized_request_sstr << "\r\n";

  const string& serialized_request_str = serialized_request_sstr.str();
  serialized_request_.reset(new vector<byte>(
      serialized_request_str.begin(), serialized_request_str.end()));
}

void HttpConnection::BuildQueryParameters(
    const HttpRequest& request, string* query_parameters) const {
  vector<string> parameter_pairs;
  for (const auto& kv : request.request_headers()) {
    parameter_pairs.push_back(
        UriEncode(kv.key()) + "=" + UriEncode(kv.value()));
  }

  *CHECK_NOTNULL(query_parameters) = algorithm::join(parameter_pairs, "&");
}

void HttpConnection::BuildRequestHeaders(
    const HttpRequest& request, string* request_headers) const {
  vector<string> header_lines;
  for (const auto& kv : request.request_headers()) {
    if (iequals(kv.key(), "Host") || iequals(kv.key(), "Content-Length")) {
      continue;
    }
    header_lines.push_back(kv.key() + ": " + kv.value());
  }

  *CHECK_NOTNULL(request_headers) = algorithm::join(header_lines, "\r\n");
}

string HttpConnection::UriEncode(const string& str) const {
  string encoded_str;
  char* curl_encoded_str = curl_easy_escape(curl_, str.c_str(), str.size());
  encoded_str.assign(curl_encoded_str);
  curl_free(curl_encoded_str);
  return encoded_str;
}

bool HttpConnection::DeserializeResponse(HttpResponse* response) {
  // TODO: Implement
  return false;
}

void HttpConnection::RequestSent(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  bool still_ok = false;
  if (tcp_connection_->last_write_succeeded()) {
    Callback response_received_callback =
        strand_dispatcher_->CreateStrandCallback(
            boost::bind(&HttpConnection::ResponseReceived, this,
                        response, response_payload, send_request_callback));
    serialized_response_.reset(new vector<byte>);
    if (tcp_connection_->ReadUntil(
            { '\r', '\n', '\r', '\n' }, serialized_response_.get(),
            response_received_callback)) {
      still_ok = true;
    }
  }

  if (!still_ok) {
    last_request_succeeded_ = false;
    CleanUpRequestState();
    Close();
    send_request_callback();
  }
}

void HttpConnection::ResponseReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
    bool still_ok = false;
  if (tcp_connection_->last_read_succeeded() &&
      DeserializeResponse(response)) {
    Callback response_payload_received_callback =
        strand_dispatcher_->CreateStrandCallback(
            boost::bind(&HttpConnection::ResponsePayloadReceived, this,
                        send_request_callback));

    // TODO: Figure out how many bytes long the response is; deal with
    // chunked mode.
    if (tcp_connection_->ReadSize(
            0, response_payload, response_payload_received_callback)) {
      still_ok = true;
    }
  }

  if (!still_ok) {
    last_request_succeeded_ = false;
    CleanUpRequestState();
    Close();
    send_request_callback();
  }
}

void HttpConnection::ResponsePayloadReceived(
    Callback send_request_callback) {
  last_request_succeeded_ = tcp_connection_->last_read_succeeded();
  CleanUpRequestState();
  if (!last_request_succeeded_) {
     Close();
  }
  send_request_callback();
}

void HttpConnection::CleanUpRequestState() {
  serialized_request_.reset();
  serialized_response_.reset();
  tmp_response_payload_.reset();
  request_pending_ = false;
}

}  // namespace polar_express
