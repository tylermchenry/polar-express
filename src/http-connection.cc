#include "http-connection.h"

#include <algorithm>
#include <sstream>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/regex.hpp"
#include "boost/bind.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/regex.hpp"
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

  CHECK_NOTNULL(response)->Clear();
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
                          << "Host: " << hostname << "\r\n";
  if (!request_headers.empty()) {
    serialized_request_sstr << request_headers << "\r\n";
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
  assert(response != nullptr);
  const vector<byte>::const_iterator serialized_response_end_itr =
      serialized_response_->end();
  vector<byte>::const_iterator serialized_response_itr =
      serialized_response_->begin();

  // TODO: Support HTTPS
  response->set_transport_succeeded(true);
  response->set_is_secure(false);

  string status_line;
  serialized_response_itr = GetTextLineFromData(
      serialized_response_itr, serialized_response_end_itr, &status_line);
  ParseResponseStatus(status_line, response);

  string header_line;
  while (serialized_response_itr != serialized_response_end_itr) {
    serialized_response_itr = GetTextLineFromData(
        serialized_response_itr, serialized_response_end_itr, &header_line);
    if (!header_line.empty()) {
      ParseResponseHeader(header_line, response);
    }
  }

  return true;
}

vector<byte>::const_iterator HttpConnection::GetTextLineFromData(
    vector<byte>::const_iterator begin,
    vector<byte>::const_iterator end,
    string* text_line) const {
  CHECK_NOTNULL(text_line)->clear();
  while (begin != end) {
    *text_line += *begin++;
    if (text_line->size() >= 2 &&
        (*text_line)[text_line->size() - 2] == '\r' &&
        (*text_line)[text_line->size() - 1] == '\n') {
      text_line->resize(text_line->size() - 2);
      break;
    }
  }
  return begin;
}

void HttpConnection::ParseResponseStatus(
    const string& status_line, HttpResponse* response) const {
  assert(response != nullptr);
  istringstream status_line_sstr(status_line);
  string http_version;
  int status_code = 0;

  // TODO: Error handling?
  status_line_sstr >> http_version >> status_code;

  vector<string> version_parts;
  split(version_parts, http_version, is_any_of("/"));
  if (version_parts.size() >= 2) {
    response->set_http_version(version_parts[1]);
  }

  response->set_status_code(status_code);
  response->set_status_phrase(
      status_line_sstr.eof()  ?  "" :
      algorithm::trim_left_copy(
          status_line_sstr.str().substr(status_line_sstr.tellg())));
}

void HttpConnection::ParseResponseHeader(
    const string& header_line, HttpResponse* response) const {
  // TODO: Regex seems heavyweight for this. Is there no simpler way
  // to have a multi-character delimiter?
  vector<string> header_parts;
  split_regex(header_parts, header_line, regex(": "));

  KeyValue* kv = CHECK_NOTNULL(response)->add_response_headers();
  kv->set_key(header_parts[0]);
  if (header_parts.size() >= 2) {
    kv->set_value(header_parts[1]);
  }
}

size_t HttpConnection::GetResponsePayloadSize(
    const HttpResponse& response) const {
  for (const auto& kv : response.response_headers()) {
    if (iequals(kv.key(), "Content-Length: ")) {
      try {
        return lexical_cast<int>(kv.value());
      } catch (bad_lexical_cast&) {
        // Continue. There might be another, valid Content-Length header?
      }
    }
  }
  return 0;
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
    CHECK_NOTNULL(response)->set_transport_succeeded(false);
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
                        response, response_payload, send_request_callback));

    // Ask to read, even if the payload size is zero, to simplify
    // callback handling (so that ResponsePayloadRecieved is always
    // the last callback in a successful response.)
    if (tcp_connection_->ReadSize(
             GetResponsePayloadSize(*response),
             response_payload, response_payload_received_callback)) {
      still_ok = true;
    }
  }

  if (!still_ok) {
    last_request_succeeded_ = false;
    CHECK_NOTNULL(response)->set_transport_succeeded(false);
    CleanUpRequestState();
    Close();
    send_request_callback();
  }
}

void HttpConnection::ResponsePayloadReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  last_request_succeeded_ = tcp_connection_->last_read_succeeded();
  CleanUpRequestState();
  if (!last_request_succeeded_) {
    CHECK_NOTNULL(response)->set_transport_succeeded(false);
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
