#include "network/http-server-connection.h"

#include <iostream>
#include <deque>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include "base/make-unique.h"
#include "network/stream-connection.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

bool GetMethod(const string& method_str, HttpRequest::Method* method) {
  assert(method != nullptr);
  if (boost::iequals(method_str, "GET")) {
    *method = HttpRequest::GET;
    return true;
  } else if (boost::iequals(method_str, "PUT")) {
    *method = HttpRequest::PUT;
    return true;
  } else if (boost::iequals(method_str, "POST")) {
    *method = HttpRequest::POST;
    return true;
  } else if (boost::iequals(method_str, "DELETE")) {
    *method = HttpRequest::DELETE;
    return true;
  }
  return false;
}

}  // namespace

HttpServerConnection::HttpServerConnection(
      unique_ptr<StreamConnection>&& stream_connection)
    : HttpConnection(std::move(stream_connection)),
      waiting_for_request_(false),
      last_operation_succeeded_(false) {
}

HttpServerConnection::~HttpServerConnection() {
}

bool HttpServerConnection::last_operation_succeeded() const {
  return last_operation_succeeded_;
}

bool HttpServerConnection::ReceiveRequest(
    HttpRequest* request,
    vector<byte>* request_payload, Callback callback) {
  if (waiting_for_request_) {
    return false;
  }

  DLOG(std::cerr << "HTTP Server Waiting to receive." << std::endl);

  Callback request_received_callback = CreateStrandCallback(
      boost::bind(&HttpServerConnection::RequestReceived, this,
                  request, request_payload, callback));
  serialized_request_ = make_unique<vector<byte> >();

  if (ReadNextMessage(serialized_request_.get(), request_received_callback)) {
    waiting_for_request_ = true;
    return true;
  }
  return false;
}

bool HttpServerConnection::SendResponse(
    const HttpResponse& response,
    const vector<byte>* response_payload, Callback callback) {
  return SendResponse(response, vector<const vector<byte>*>({response_payload}),
                      callback);
}

bool HttpServerConnection::SendResponse(
    const HttpResponse& response,
    const vector<const vector<byte>*>& response_sequential_payload,
    Callback callback) {
  if (!is_open()) {
    return false;
  }

  // Refuse to send responses specifying a secure connection over an insecure
  // connection (but the reverse is fine).
  if (response.is_secure() && !is_secure()) {
    return false;
  }

  // sequential_data will hold the headers generated by serializing the HTTP
  // Response, followed by the response sequential payload.
  vector<const vector<byte>*> sequential_data;
  sequential_data.reserve(response_sequential_payload.size() + 1);

  // Insert a placeholder for the serialized response (generated below);
  sequential_data.push_back(nullptr);

  size_t total_payload_size = 0;
  for (const auto* buf : response_sequential_payload) {
    if (buf != nullptr) {
      sequential_data.push_back(buf);
      total_payload_size += buf->size();
    }
  }

  boost::shared_ptr<vector<byte> > serialized_response =
      SerializeResponse(response, total_payload_size);
  sequential_data[0] = serialized_response.get();

  Callback response_sent_callback =
      CreateStrandCallback(boost::bind(&HttpServerConnection::ResponseSent,
                                       this, serialized_response, callback));
  if (stream_connection().WriteAll(sequential_data, response_sent_callback)) {
    return true;
  }

  return false;
}

bool HttpServerConnection::IsRequestPayloadChunked(
    const HttpRequest& request) const {
  return IsPayloadChunked(request.request_headers());
}

size_t HttpServerConnection::GetRequestPayloadSize(
    const HttpRequest& request) const {
  return GetPayloadSize(request.request_headers());
}

bool HttpServerConnection::ParseRequestLine(const string& request_line,
                                            HttpRequest* request) const {
  assert(request != nullptr);
  istringstream request_line_sstr(request_line);
  string method_str;
  string path_and_query;
  string http_version;

  request_line_sstr >> method_str >> path_and_query >> http_version;

  HttpRequest::Method method;
  if (GetMethod(method_str, &method)) {
    request->set_method(method);
  } else {
    return false;
  }

  vector<string> path_and_query_parts;
  split(path_and_query_parts, path_and_query, is_any_of("?"));
  if (!path_and_query_parts.empty()) {
    request->set_path(path_and_query_parts[0]);
    if ((path_and_query_parts.size() == 2 &&
         !ParseRequestQueryParameters(path_and_query_parts[1],
                                      request->mutable_query_parameters())) ||
        path_and_query_parts.size() > 2) {
      return false;
    }
  } else {
    return false;
  }

  vector<string> version_parts;
  split(version_parts, http_version, is_any_of("/"));
  if (version_parts.size() == 2) {
    request->set_http_version(version_parts[1]);
  } else {
    return false;
  }

  return true;
}

bool HttpServerConnection::ParseRequestQueryParameters(
    const string& query_parameters_str,
    google::protobuf::RepeatedPtrField<KeyValue>* query_parameters) const {
  assert(query_parameters != nullptr);

  vector<string> query_parameters_pairs;
  split(query_parameters_pairs, query_parameters_str, is_any_of("&"));

  for (const string& param_pair : query_parameters_pairs) {
    string decoded_param_pair = UriDecode(param_pair);
    deque<string> param_pair_parts;
    split(param_pair_parts, decoded_param_pair, is_any_of("="));
    if (!param_pair_parts.empty()) {
      auto* kv = query_parameters->Add();
      kv->set_key(param_pair_parts[0]);
      param_pair_parts.erase(param_pair_parts.begin());
      if (!param_pair_parts.empty()) {
        // It is possible and legal for the param value to contain a literal
        // '=', so it might have split further on that.
        kv->set_value(join(param_pair_parts, "="));
      }
    }
  }

  return true;
}

void HttpServerConnection::ParseRequestHeaders(
    vector<byte>::const_iterator begin,
    vector<byte>::const_iterator end,
    HttpRequest* request) const {
  DeserializeHeadersFromData(
      begin, end, CHECK_NOTNULL(request)->mutable_request_headers());

  // Remove the "Host:" header, if present, and put the value in the hostname
  // proto field instead.
  for (int i = 0; i < request->request_headers_size(); ++i) {
    const auto& kv = request->request_headers(i);
    if (boost::iequals(kv.key(), "host")) {
      request->set_hostname(kv.value());

      // TODO: Replace with simpler DeleteSubrange call when upgraded to a
      // version of protobuf that implements it. Note that a single Swap to end
      // and then RemoveLast is not sufficient, since this would change the
      // header order.
      for (int j = i + 1; j < request->request_headers_size(); ++i, ++j) {
        request->mutable_request_headers()->SwapElements(i, j);
      }
      request->mutable_request_headers()->RemoveLast();
      break;
    }
  }
}

bool HttpServerConnection::DeserializeRequest(HttpRequest* request) const {
  assert(request != nullptr);
  const vector<byte>::const_iterator serialized_request_end_itr =
      serialized_request_->end();
  vector<byte>::const_iterator serialized_request_itr =
      serialized_request_->begin();

  request->set_transport_succeeded(true);
  request->set_is_secure(is_secure());

  string request_line;
  serialized_request_itr = GetTextLineFromData(
      serialized_request_itr, serialized_request_end_itr, &request_line);
  if (!ParseRequestLine(request_line, request)) {
    return false;
  }

  ParseRequestHeaders(serialized_request_itr, serialized_request_end_itr,
                      request);
  return true;
}

boost::shared_ptr<vector<byte> > HttpServerConnection::SerializeResponse(
      const HttpResponse& response, const size_t payload_size) const {
  ostringstream serialized_response_sstr;
  serialized_response_sstr << "HTTP/" << response.http_version() << " "
                           << response.status_code();
  if (!response.status_phrase().empty()) {
    serialized_response_sstr << " " << response.status_phrase();
  }
  serialized_response_sstr << "\r\n\r\n";
  const string serialized_response_str = serialized_response_sstr.str();
  return boost::shared_ptr<vector<byte> >(new vector<byte>(
      serialized_response_str.begin(), serialized_response_str.end()));
}

void HttpServerConnection::RequestReceived(
    HttpRequest* request,
    vector<byte>* request_payload,
    Callback receive_request_callback) {
  bool still_ok = false;
  DLOG(std::cerr << "HTTP server received " << serialized_request_->size()
                 << "bytes." << std::endl);
  if (stream_connection().last_read_succeeded() &&
      DeserializeRequest(request)) {
    DLOG(std::cerr << "HTTP server got request:\n" << request->DebugString()
                   << std::endl);
    if (IsRequestPayloadChunked(*request)) {
      // TODO: Handle chunked request payloads. Right now this will fall through
      // to failure with still_ok = false.
    } else {
      Callback request_payload_received_callback = CreateStrandCallback(
          boost::bind(&HttpServerConnection::RequestPayloadReceived, this,
                      request, request_payload, receive_request_callback));

      // Ask to read, even if the payload size is zero, to simplify
      // callback handling (so that RequestPayloadRecieved is always
      // the last callback in a successful request.)
      still_ok = stream_connection().ReadSize(
          GetRequestPayloadSize(*request),
          request_payload, request_payload_received_callback);
    }
  }

  if (!still_ok) {
    HandleRequestError(request, receive_request_callback);
  }
}

void HttpServerConnection::RequestPayloadReceived(
    HttpRequest* request,
    vector<byte>* request_payload,
    Callback receive_request_callback) {
  last_operation_succeeded_ = stream_connection().last_read_succeeded();
  CleanUpRequestState();
  if (!last_operation_succeeded_) {
    CHECK_NOTNULL(request)->set_transport_succeeded(false);
    Close();
  }
  receive_request_callback();
}

void HttpServerConnection::HandleRequestError(
    HttpRequest* request,
    Callback receive_request_callback) {
  last_operation_succeeded_ = false;
  CHECK_NOTNULL(request)->set_transport_succeeded(false);
  CleanUpRequestState();
  Close();
  receive_request_callback();
}

void HttpServerConnection::CleanUpRequestState() {
  serialized_request_.reset();
  tmp_request_payload_.reset();
  waiting_for_request_ = false;
}

void HttpServerConnection::ResponseSent(
    boost::shared_ptr<vector<byte> > serialized_response,
    Callback response_sent_callback) {
  last_operation_succeeded_ = stream_connection().last_write_succeeded();
  serialized_response.reset();
  response_sent_callback();
}

}  // namespace polar_express