#include "http-connection.h"

#include "boost/bind.hpp"

#include "tcp-connection.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

const char kHttpProtocol[] = "http";

}  // namespace

HttpConnection::HttpConnection()
    : request_pending_(false),
      last_request_succeeded_(false),
      tcp_connection_(new TcpConnection) {
}

HttpConnection::~HttpConnection() {
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
  if (request_pending_) {
    return false;
  }

  request_pending_ = true;
  SerializeRequest(request);

  if (response_payload == nullptr) {
    tmp_response_payload_.reset(new vector<byte>);
    response_payload = tmp_response_payload_.get();
  }

  // TODO: Force appropriate thread
  Callback request_sent_callback =
      boost::bind(&HttpConnection::RequestSent, this,
           response, response_payload, callback);
  if (tcp_connection_->WriteAll(
          { serialized_request_.get(), &request_payload },
          request_sent_callback)) {
    return true;
  }

  CleanUpRequestState();
  return false;
}

void HttpConnection::SerializeRequest(const HttpRequest& request) {
  // TODO: Implement
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
    // TODO: Force appropriate thread
    Callback response_received_callback =
        boost::bind(&HttpConnection::ResponseReceived, this,
             response, response_payload, send_request_callback);
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
    // TODO: Force appropriate thread
    Callback response_payload_received_callback =
        boost::bind(&HttpConnection::ResponsePayloadReceived, this,
             send_request_callback);

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
