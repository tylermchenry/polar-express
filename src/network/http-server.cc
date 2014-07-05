#include "network/http-server.h"

#include <iostream>
#include <algorithm>
#include <iterator>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "base/make-unique.h"
#include "network/http-server-connection.h"
#include "network/tcp-connection.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

const char* GetPhraseForStatusCode(int code) {
  switch (code) {
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested Range NOt Satisfiable";
    case 417: return "Expectation Failed";
    case 500:
    default:
      return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
  }
}

void ProduceErrorResponse(int code, const HttpRequest& request,
                          HttpResponse* response,
                          ostream* response_payload_stream) {
  response->Clear();
  response->set_is_secure(request.is_secure());
  response->set_status_code(code);

  response_payload_stream->clear();
  *response_payload_stream << "<html><body><h1>" << code << " "
                           << GetPhraseForStatusCode(code)
                           << "</h1></body></html>";
}

class NotFoundHandler : public HttpServer::Handler {
 public:
  virtual bool HandleHttpRequest(const HttpRequest& request,
                                 const vector<byte>& request_payload,
                                 HttpResponse* response,
                                 ostream* response_payload_stream) {
    ProduceErrorResponse(404, request, response, response_payload_stream);
    return true;
  }
};

}  // namespace

struct HttpServer::ConnectionContext {
  HttpRequest request_;
  vector<byte> request_payload_;
  HttpResponse response_;
  ostringstream response_payload_stream_;
  vector<byte> response_payload_;
};

HttpServer::HttpServer()
    : is_running_(false) {
}

HttpServer::~HttpServer() {
  Stop();
}

bool HttpServer::RegisterHandler(const string& path_prefix,
                                 boost::shared_ptr<Handler> handler) {
  if (is_running_) {
    return false;
  }

  const string relative_path_prefix =
      filesystem::path(path_prefix).relative_path().string();
  if (handlers_.find(relative_path_prefix) != handlers_.end() ||
      handler.get() == nullptr) {
    return false;
  }
  handlers_.insert(make_pair(relative_path_prefix, handler));
  return true;
}

void HttpServer::UnregisterHandler(const string& path_prefix) {
  const string relative_path_prefix =
      filesystem::path(path_prefix).relative_path().string();
  handlers_.erase(relative_path_prefix);
}

bool HttpServer::Run(uint16_t port) {
  if (is_running_) {
    return false;
  }
  is_running_ = true;

  CreateNetworkingObjects();

  const asio::ip::tcp::resolver::query query("localhost",
                                             lexical_cast<string>(port));

  const asio::ip::tcp::resolver::iterator endpoint_itr =
      resolver_->resolve(query);
  if (endpoint_itr == asio::ip::tcp::resolver::iterator()) {
    // Resolution failed.
    return false;
  }

  const asio::ip::tcp::endpoint endpoint = *endpoint_itr;
  acceptor_->open(endpoint.protocol());
  acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_->bind(endpoint);
  acceptor_->listen();

  AsyncAccept();

  return true;
}

void HttpServer::Stop() {
  connection_contexts_.clear();
  DestroyNetworkingObjects();
  is_running_ = false;
}

bool HttpServer::is_running() const {
  return is_running_;
}

boost::shared_ptr<HttpServer::Handler> HttpServer::FindHandler(
    const string& path) const {
  filesystem::path fs_path(path);
  fs_path = fs_path.relative_path();
  const auto itr = handlers_.find(fs_path.string());
  if (itr != handlers_.end()) {
    return itr->second;
  }

  fs_path = fs_path.root_path();
  while (!fs_path.empty()) {
    const auto itr = handlers_.find(fs_path.string());
    if (itr != handlers_.end()) {
      return itr->second;
    }
    fs_path = fs_path.branch_path();
  }

  return boost::shared_ptr<Handler>(new NotFoundHandler);
}

void HttpServer::CreateNetworkingObjects() {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherUserInterface();

  io_service_work_ = strand_dispatcher_->make_work();
  resolver_ =
      make_unique<asio::ip::tcp::resolver>(strand_dispatcher_->io_service());
  acceptor_ =
      make_unique<asio::ip::tcp::acceptor>(strand_dispatcher_->io_service());
}

void HttpServer::DestroyNetworkingObjects() {
  acceptor_->close();
  acceptor_.reset();
  resolver_.reset();
  io_service_work_.reset();
  strand_dispatcher_.reset();
}

void HttpServer::AsyncAccept() {
  std::cerr << "Waiting to accept." << std::endl;
  auto handler = strand_dispatcher_->MakeStrandCallbackWithArgs<
    const system::error_code&>(
        boost::bind(&HttpServer::HandleAccept, this, _1),
        asio::placeholders::error);

  next_socket_ =
      make_unique<asio::ip::tcp::socket>(strand_dispatcher_->io_service());
  acceptor_->async_accept(*next_socket_, handler);
}

void HttpServer::HandleAccept(const boost::system::error_code& err) {
  std::cerr << "Trying to accept." << std::endl;
  if (!is_running_ || next_socket_.get() == nullptr || err) {
    AsyncAccept();
    return;
  }

  std::cerr << "Accepting." << std::endl;

  boost::shared_ptr<HttpServerConnection> server_connection(
      new HttpServerConnection(make_unique<TcpConnection>(
          std::move(next_socket_),
          StreamConnection::ConnectionProperties(
              AsioDispatcher::NetworkUsageType::kLocalhost, "localhost",
              "http"))));

  AsyncReceiveRequest(server_connection);
  AsyncAccept();
}

void HttpServer::AsyncReceiveRequest(
    boost::shared_ptr<HttpServerConnection> server_connection) {
  assert(server_connection != nullptr);
  ConnectionContext& context = connection_contexts_[server_connection];

  context.request_.Clear();
  context.request_payload_.clear();

  auto handler = strand_dispatcher_->CreateStrandCallback(
      boost::bind(&HttpServer::HandleReceiveRequest, this, server_connection));
  if (!server_connection->ReceiveRequest(&context.request_,
                                         &context.request_payload_, handler)) {
    Close(server_connection);
  }
}

void HttpServer::HandleReceiveRequest(
    boost::shared_ptr<HttpServerConnection> server_connection) {
  assert(server_connection != nullptr);
  if (!is_running_) {
    Close(server_connection);
  }

  ConnectionContext& context = connection_contexts_[server_connection];
  if (!server_connection->last_operation_succeeded() ||
      !context.request_.transport_succeeded()) {
    Close(server_connection);
    return;
  }

  context.response_.Clear();
  context.response_payload_stream_.str("");
  context.response_payload_stream_.clear();
  context.response_payload_.clear();

  boost::shared_ptr<HttpServer::Handler> handler =
      FindHandler(context.request_.path());
  if (!handler) {
    Close(server_connection);
    return;
  }

  if (!handler->HandleHttpRequest(context.request_, context.request_payload_,
                                  &context.response_,
                                  &context.response_payload_stream_)) {
    ProduceErrorResponse(500, context.request_, &context.response_,
                         &context.response_payload_stream_);
  }

  AsyncSendResponse(server_connection);
}

void HttpServer::AsyncSendResponse(
    boost::shared_ptr<HttpServerConnection> server_connection) {
  assert(server_connection != nullptr);
  ConnectionContext& context = connection_contexts_[server_connection];

  context.response_.set_http_version("1.1");
  context.response_.set_status_phrase(
      GetPhraseForStatusCode(context.response_.status_code()));

  // Kind of sucks that we have to copy this (twice!). Any way to have the
  // stream write directly into the vector?
  string response_payload_str = context.response_payload_stream_.str();
  context.response_payload_stream_.str("");
  context.response_payload_.resize(response_payload_str.size());
  copy(response_payload_str.begin(), response_payload_str.end(),
       context.response_payload_.begin());

  std::cerr << "Responding with body: " << response_payload_str << std::endl;
  response_payload_str.clear();
  std::cerr << "Body has " << context.response_payload_.size() << " bytes." << std::endl;

  auto handler = strand_dispatcher_->CreateStrandCallback(
      boost::bind(&HttpServer::HandleSendResponse, this, server_connection));
  if (!server_connection->SendResponse(context.response_,
                                       &context.response_payload_, handler)) {
    Close(server_connection);
  }
}

void HttpServer::HandleSendResponse(
    boost::shared_ptr<HttpServerConnection> server_connection) {
  assert(server_connection != nullptr);
  if (!is_running_) {
    Close(server_connection);
  }

  ConnectionContext& context = connection_contexts_[server_connection];

  if (!server_connection->last_operation_succeeded()) {
    Close(server_connection);
    return;
  }

  AsyncReceiveRequest(server_connection);
}

void HttpServer::Close(
    boost::shared_ptr<HttpServerConnection> server_connection) {
  std::cerr << "Closing." << std::endl;
  connection_contexts_.erase(server_connection);
}

}  // namespace polar_express
