#include "tcp-connection.h"

#include "boost/asio/buffer.hpp"
#include "boost/bind.hpp"
#include "boost/bind/protect.hpp"

using boost::asio::ip::tcp;

namespace polar_express {
namespace {

// The following is some template magic that allows us to create
// callbacks for the boost TCP object methods which will be run in the
// appropriate strand. This requires some work, because unlike all of
// the internal callbacks in polar express, whcih are zero-argument
// and fully bound, the callbacks invoked by the boost TCP objects
// need to take arguments.
//
// So the following templates allow us to create a callback that takes
// the arguments that boost wants to pass. When it is invoked, it
// creates a new, fully-bound callback using those arguments and posts
// it to the appropriate strand.
//
// TODO: If this turns out to be more generally useful, move these
// templates to asio-dispatcher.h.

template <typename T1>
void StrandCallbackWithArgs(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::function<void(T1)> callback_with_args,
    T1 arg1) {
  strand_dispatcher->Post(boost::bind(callback_with_args, arg1));
}

template <typename T1, typename T2>
void StrandCallbackWithArgs(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::function<void(T1, T2)> callback_with_args,
    T1 arg1, T2 arg2) {
  strand_dispatcher->Post(boost::bind(callback_with_args, arg1, arg2));
}

template <typename T1, typename PT1>
boost::function<void(T1)> MakeStrandCallbackWithArgs(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::function<void(T1)> callback_with_args,
    PT1 arg1_placeholder) {
  return boost::bind(
      &StrandCallbackWithArgs<T1>, strand_dispatcher, callback_with_args,
      arg1_placeholder);
}

template <typename T1, typename T2, typename PT1, typename PT2>
boost::function<void(T1, T2)> MakeStrandCallbackWithArgs(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::function<void(T1, T2)> callback_with_args,
    PT1 arg1_placeholder, PT2 arg2_placeholder) {
  return boost::bind(
      &StrandCallbackWithArgs<T1, T2>, strand_dispatcher, callback_with_args,
      arg1_placeholder, arg2_placeholder);
}

}  // namespace

TcpConnection::TcpConnection()
    : is_opening_(false),
      is_open_(false),
      is_writing_(false),
      is_reading_(false),
      network_usage_type_(AsioDispatcher::NetworkUsageType::kInvalid),
      last_write_succeeded_(false) {
}

TcpConnection::~TcpConnection() {
}

bool TcpConnection::is_opening() const {
  return is_opening_;
}

bool TcpConnection::is_open() const {
  return is_open_;
}

AsioDispatcher::NetworkUsageType TcpConnection::network_usage_type() const {
  return network_usage_type_;
}

const string& TcpConnection::hostname() const {
  return hostname_;
}

const string& TcpConnection::protocol() const {
  return protocol_;
}

bool TcpConnection::last_write_succeeded() const {
  return last_write_succeeded_;
}

bool TcpConnection::Open(
    AsioDispatcher::NetworkUsageType network_usage_type,
    const string& hostname, const string& protocol, Callback callback) {
  if (is_opening_ || is_open_ || !CreateNetworkingObjects(network_usage_type)) {
    return false;
  }
  is_opening_ = true;
  hostname_ = hostname;
  protocol_ = protocol;

  tcp::resolver::query query(hostname, protocol);
  auto handler = MakeStrandCallbackWithArgs<
      const system::error_code&, asio::ip::tcp::resolver::iterator>(
          strand_dispatcher_,
          boost::bind(&TcpConnection::HandleResolve, this, _1, _2, callback),
          asio::placeholders::error,
          asio::placeholders::iterator);
  resolver_->async_resolve(query, handler);

  return true;
}

bool TcpConnection::Close() {
  if (is_opening_) {
    return false;
  }

  // TODO: Implement

  DestroyNetworkingObjects();
  is_open_ = false;
  return true;
}

bool TcpConnection::Write(
    const vector<byte>& data, Callback callback) {
  return WriteAll({ &data }, callback);
}

bool TcpConnection::WriteAll(
    const vector<const vector<byte>*>& sequential_data,
    Callback callback) {
  if (!is_open_ || is_writing_) {
    return false;
  }

  is_writing_ = true;

  assert(write_buffers_.empty());
  for (const auto* data : sequential_data) {
    write_buffers_.push_back(asio::buffer(*CHECK_NOTNULL(data)));
  }

  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleWrite, this, _1, _2, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  asio::async_write(*socket_, write_buffers_, handler);

  return true;
}

bool TcpConnection::ReadUntil(
    const vector<byte>& terminator_bytes, vector<byte>* data,
    Callback callback) {
  // TODO: Implement
  return false;
}

bool TcpConnection::ReadSize(
    size_t data_size, vector<byte>* data, Callback callback) {
  // TODO: Implement
  return false;
}

bool TcpConnection::ReadAll(vector<byte>* data, Callback callback) {
  // TODO: Implement
  return false;
}

bool TcpConnection::CreateNetworkingObjects(
    AsioDispatcher::NetworkUsageType network_usage_type) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
  if (strand_dispatcher_.get() == nullptr) {
    DestroyNetworkingObjects();
    return false;
  }

  network_usage_type_ = network_usage_type;
  last_write_succeeded_ = false;

  io_service_work_ = strand_dispatcher_->make_work();
  resolver_.reset(new tcp::resolver(strand_dispatcher_->io_service()));
  socket_.reset(new tcp::socket(strand_dispatcher_->io_service()));
  return true;
}

void TcpConnection::DestroyNetworkingObjects() {
  socket_.reset();
  resolver_.reset();
  io_service_work_.reset();
  strand_dispatcher_.reset();
  network_usage_type_ = AsioDispatcher::NetworkUsageType::kInvalid;
  hostname_.clear();
  protocol_.clear();
}

void TcpConnection::TryConnect(
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    Callback open_callback) {
  auto handler = MakeStrandCallbackWithArgs<
    const system::error_code&, asio::ip::tcp::resolver::iterator>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleConnect, this, _1, _2, open_callback),
      asio::placeholders::error,
      asio::placeholders::iterator);

  boost::asio::async_connect(*socket_, endpoint_iterator, handler);
}

void TcpConnection::HandleResolve(
    const boost::system::error_code& err,
    tcp::resolver::iterator endpoint_iterator,
    Callback open_callback) {
  if (err) {
    is_opening_ = false;
    DestroyNetworkingObjects();
    open_callback();
    return;
  }

  // Attempt a connection to each endpoint in the list until we
  // successfully establish a connection.
  TryConnect(endpoint_iterator, open_callback);
}

void TcpConnection::HandleConnect(
    const boost::system::error_code& err,
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    Callback open_callback) {
  is_opening_ = false;

  if (err) {
    if (endpoint_iterator == tcp::resolver::iterator()) {
      DestroyNetworkingObjects();
    } else {
      // There is another endpoint to try...
      TryConnect(endpoint_iterator, open_callback);
      return;
    }
  } else {
    is_open_ = true;
  }

  open_callback();
}

void TcpConnection::HandleWrite(
    const system::error_code& err,
    size_t bytes_transferred,
    Callback write_callback) {
  is_writing_ = false;
  if (!err) {
    last_write_succeeded_ = true;
  }
  write_buffers_.clear();
  write_callback();
}

}  // namespace polar_express
