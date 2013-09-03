#include "tcp-connection.h"

#include <algorithm>
#include <utility>

#include "boost/asio/buffer.hpp"
#include "boost/bind.hpp"

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

template <typename T1, typename T2>
void StrandCallbackWithArgs(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::function<void(T1, T2)> callback_with_args,
    T1 arg1, T2 arg2) {
  strand_dispatcher->Post(boost::bind(callback_with_args, arg1, arg2));
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

typedef boost::asio::buffers_iterator<
    boost::asio::streambuf::const_buffers_type> boost_streambuf_iterator;

class MatchByteSequenceCondition {
 public:
  explicit MatchByteSequenceCondition(const vector<byte>& byte_sequence)
      : byte_sequence_(byte_sequence) {
  }

  pair<boost_streambuf_iterator, bool> operator() (
    boost_streambuf_iterator begin, boost_streambuf_iterator end) const {
    boost_streambuf_iterator pos = search(
        begin, end, byte_sequence_.begin(), byte_sequence_.end());
    return make_pair(pos, pos != end);
  }

 private:
  vector<byte> byte_sequence_;
};

}  // namespace
}  // namespace polar_express

namespace boost {
namespace asio {
template <> struct is_match_condition<polar_express::MatchByteSequenceCondition>
    : public boost::true_type {};
}  // namespace asio
}  // namespace boost

namespace polar_express {

TcpConnection::TcpConnection()
    : is_opening_(false),
      is_open_(false),
      is_writing_(false),
      is_reading_(false),
      network_usage_type_(AsioDispatcher::NetworkUsageType::kInvalid),
      last_write_succeeded_(false),
      last_read_succeeded_(false),
      read_data_(nullptr) {
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

bool TcpConnection::last_read_succeeded() const {
  return last_read_succeeded_;
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

  if (socket_.get()) {
    auto error_code =
        asio::error::make_error_code(asio::error::connection_aborted);
    socket_->shutdown(tcp::socket::shutdown_both, error_code);
    socket_->close(error_code);
  }

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
  if (!is_open_ || is_reading_) {
    return false;
  }

  is_reading_ = true;

  assert(read_data_ == nullptr);
  assert(read_streambuf_ == nullptr);
  read_data_ = CHECK_NOTNULL(data);
  read_streambuf_.reset(new asio::streambuf);

  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleRead, this, _1, _2, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  asio::async_read_until(
      *socket_, *read_streambuf_, MatchByteSequenceCondition(terminator_bytes),
      handler);

  return true;
}

bool TcpConnection::ReadSize(
    size_t max_data_size, vector<byte>* data, Callback callback) {
  if (!is_open_ || is_reading_) {
    return false;
  }

  is_reading_ = true;

  assert(read_data_ == nullptr);
  assert(read_streambuf_ == nullptr);
  read_data_ = CHECK_NOTNULL(data);

  if (read_data_->size() < max_data_size) {
    read_data_->resize(max_data_size);
  }

  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleRead, this, _1, _2, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  asio::async_read(*socket_, asio::buffer(*read_data_), handler);

  return true;
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
  last_read_succeeded_ = false;

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
  write_buffers_.clear();

  is_writing_ = false;
  if (!err) {
    last_write_succeeded_ = true;
  }

  write_callback();
}

void TcpConnection::HandleRead(
    const system::error_code& err,
    size_t bytes_transferred,
    Callback read_callback) {
  // If ReadUntil was used, then the data is actually in
  // read_streambuf_. Otherwise the data went directly into
  // read_data_, but may have stopped short of filling the entire
  // buffer.
  CHECK_NOTNULL(read_data_)->resize(bytes_transferred);
  if (read_streambuf_.get()) {
    copy(asio::buffers_begin(read_streambuf_->data()),
         asio::buffers_begin(read_streambuf_->data()) + bytes_transferred,
         read_data_->begin());
    read_streambuf_.reset();
  }
  read_data_ = nullptr;

  is_reading_ = false;
  if (!err) {
    last_read_succeeded_ = true;
  }

  read_callback();
}

}  // namespace polar_express
