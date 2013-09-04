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

  ParseResponseHeaders(
      serialized_response_itr, serialized_response_end_itr, response);

  return true;
}

void HttpConnection::ParseResponseHeaders(
    vector<byte>::const_iterator begin,
    vector<byte>::const_iterator end,
    HttpResponse* response) const {
  string header_line;
  while (begin != end) {
    begin = GetTextLineFromData(begin, end, &header_line);
    if (!header_line.empty()) {
      ParseResponseHeader(header_line, response);
    }
  }
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

const string& HttpConnection::GetResponseHeaderValue(
    const HttpResponse& response, const string& key) const {
  for (const auto& kv : response.response_headers()) {
    if (iequals(kv.key(), key)) {
      return kv.value();
    }
  }
  return KeyValue::default_instance().value();
}

bool HttpConnection::IsChunkedPayload(const HttpResponse& response) const {
  return iequals(
      GetResponseHeaderValue(response, "Transfer-Encoding"), "chunked");
}

size_t HttpConnection::GetResponsePayloadSize(
    const HttpResponse& response) const {
  try {
    return lexical_cast<size_t>(
        GetResponseHeaderValue(response, "Content-Length"));
  } catch (bad_lexical_cast&) {
    return 0;
  }
}

size_t HttpConnection::GetPayloadChunkSize(
    const vector<byte>& chunk_header) const {
  string chunk_header_str;
  const auto itr = find(chunk_header.begin(), chunk_header.end(), ';');
  try {
    // boost::lexical_cast doesn't support hexadecimal, and
    // strtoi/strtol require that we know what fundamental type size_t
    // maps to. So do it the roundabout way.
    istringstream sstr(string(chunk_header.begin(), itr));
    size_t chunk_size;
    sstr >> hex >> chunk_size;
    return chunk_size;
  } catch (...) {
    // Must use max instead of 0 or -1, since 0 is a meaningful value
    // and size_t is unsigned.
    return std::numeric_limits<size_t>::max();
  }
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
    still_ok = tcp_connection_->ReadUntil(
        { '\r', '\n', '\r', '\n' }, serialized_response_.get(),
        response_received_callback);
  }

  if (!still_ok) {
    HandleRequestError(response, send_request_callback);
  }
}

void HttpConnection::ResponseReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  bool still_ok = false;
  if (tcp_connection_->last_read_succeeded() &&
      DeserializeResponse(response)) {
    if (IsChunkedPayload(*response)) {
      Callback response_payload_chunk_header_received_callback =
          strand_dispatcher_->CreateStrandCallback(
              boost::bind(&HttpConnection::ResponsePayloadChunkHeaderReceived,
                          this, response, response_payload,
                          send_request_callback));
      response_payload_chunk_buffer_.reset(new vector<byte>);
      still_ok = tcp_connection_->ReadUntil(
          { '\r', '\n' }, response_payload_chunk_buffer_.get(),
          response_payload_chunk_header_received_callback);
    } else {
      Callback response_payload_received_callback =
          strand_dispatcher_->CreateStrandCallback(
              boost::bind(&HttpConnection::ResponsePayloadReceived, this,
                          response, response_payload, send_request_callback));

      // Ask to read, even if the payload size is zero, to simplify
      // callback handling (so that ResponsePayloadRecieved is always
      // the last callback in a successful response.)
      still_ok = tcp_connection_->ReadSize(
          GetResponsePayloadSize(*response),
          response_payload, response_payload_received_callback);
    }
  }

  if (!still_ok) {
    HandleRequestError(response, send_request_callback);
  }
}

void HttpConnection::ResponsePayloadChunkHeaderReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  bool still_ok = false;
  if (tcp_connection_->last_read_succeeded()) {
    size_t chunk_size = GetPayloadChunkSize(*response_payload_chunk_buffer_);

    // A chunk size of 0 indicates that the payload has finished, but
    // there may be more headers suffixed to the message. A chunk size
    // of size_t-max is an error value from GetPayloadChunkSize
    // indicating that the size could not be parsed.
    if (chunk_size > 0 && chunk_size < std::numeric_limits<size_t>::max()) {
      Callback response_payload_chunk_received_callback =
          strand_dispatcher_->CreateStrandCallback(
              boost::bind(&HttpConnection::ResponsePayloadChunkReceived,
                          this, response, response_payload,
                          send_request_callback));
      still_ok = tcp_connection_->ReadSize(
          chunk_size, response_payload_chunk_buffer_.get(),
          response_payload_chunk_received_callback);
    } else if (chunk_size == 0) {
      Callback response_payload_post_chunk_header_received_callback =
          strand_dispatcher_->CreateStrandCallback(
              boost::bind(
                  &HttpConnection::ResponsePayloadPostChunkHeaderReceived,
                  this, response, response_payload, send_request_callback));
      still_ok = tcp_connection_->ReadUntil(
          { '\r', '\n', }, response_payload_chunk_buffer_.get(),
          response_payload_post_chunk_header_received_callback);
    }
  }

  if (!still_ok) {
    HandleRequestError(response, send_request_callback);
  }
}

void HttpConnection::ResponsePayloadChunkSeparatorReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  bool still_ok = false;
  if (tcp_connection_->last_read_succeeded()) {
    Callback response_payload_chunk_header_received_callback =
        strand_dispatcher_->CreateStrandCallback(
            boost::bind(&HttpConnection::ResponsePayloadChunkHeaderReceived,
                        this, response, response_payload,
                        send_request_callback));
    still_ok = tcp_connection_->ReadUntil(
        { '\r', '\n' }, response_payload_chunk_buffer_.get(),
        response_payload_chunk_header_received_callback);
  }

  if (!still_ok) {
    HandleRequestError(response, send_request_callback);
  }
}

void HttpConnection::ResponsePayloadChunkReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  bool still_ok = false;

  // Copy partial payload even if the read did not succeed.
  response_payload->insert(response_payload->end(),
                           response_payload_chunk_buffer_->begin(),
                           response_payload_chunk_buffer_->end());

  if (tcp_connection_->last_read_succeeded()) {
    // There is a line terminator ("\r\n") at the end of each chunk
    // which is not part of the chunk and not counted in the chunk
    // size. Consume this "separator" before attempting to consume the
    // next chunk header.
    Callback response_payload_chunk_separator_received_callback =
        strand_dispatcher_->CreateStrandCallback(
            boost::bind(&HttpConnection::ResponsePayloadChunkSeparatorReceived,
                        this, response, response_payload,
                        send_request_callback));
    still_ok = tcp_connection_->ReadUntil(
        { '\r', '\n' }, serialized_response_.get(),
        response_payload_chunk_separator_received_callback);
  }

  if (!still_ok) {
    HandleRequestError(response, send_request_callback);
  }
}

void HttpConnection::ResponsePayloadPostChunkHeaderReceived(
    HttpResponse* response, vector<byte>* response_payload,
    Callback send_request_callback) {
  // If the header is blank (consists only of "\r\n") then this is the
  // end of the transmission. Else, parse this header and look for the
  // next one.
  if (response_payload_chunk_buffer_->size() <= 2) {
    ResponsePayloadReceived(response, response_payload, send_request_callback);
    return;
  } else {
    ParseResponseHeaders(response_payload_chunk_buffer_->begin(),
                         response_payload_chunk_buffer_->end(),
                         response);
    Callback response_payload_post_chunk_header_received_callback =
        strand_dispatcher_->CreateStrandCallback(
            boost::bind(
                &HttpConnection::ResponsePayloadPostChunkHeaderReceived,
                this, response, response_payload, send_request_callback));
    if (!tcp_connection_->ReadUntil(
            { '\r', '\n', }, response_payload_chunk_buffer_.get(),
            response_payload_post_chunk_header_received_callback)) {
      HandleRequestError(response, send_request_callback);
    }
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

void HttpConnection::HandleRequestError(
    HttpResponse* response, Callback send_request_callback) {
  last_request_succeeded_ = false;
  CHECK_NOTNULL(response)->set_transport_succeeded(false);
  CleanUpRequestState();
  Close();
  send_request_callback();
}

void HttpConnection::CleanUpRequestState() {
  serialized_request_.reset();
  serialized_response_.reset();
  tmp_response_payload_.reset();
  response_payload_chunk_buffer_.reset();
  request_pending_ = false;
}

}  // namespace polar_express
