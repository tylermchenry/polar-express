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

}  // namespace

class TcpConnection::MatchByteSequenceCondition {
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

  size_t sequence_length() const {
    return byte_sequence_.size();
  }

 private:
  vector<byte> byte_sequence_;
};

}  // namespace polar_express

namespace boost {
namespace asio {
template <>
struct is_match_condition<
  polar_express::TcpConnection::MatchByteSequenceCondition>
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
      last_read_succeeded_(false) {
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
    const vector<byte>* data, Callback callback) {
  return WriteAll({ data }, callback);
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

  auto termination_condition = MatchByteSequenceCondition(terminator_bytes);
  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleReadUntil, this, _1, _2,
                  termination_condition, data, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  asio::async_read_until(
      *socket_, *read_streambuf_, termination_condition, handler);

  return true;
}

bool TcpConnection::ReadSize(
    size_t max_data_size, vector<byte>* data, Callback callback) {
  if (!is_open_ || is_reading_) {
    return false;
  }

  is_reading_ = true;

  // Consume as much data as possible from the read streambuf. There
  // may be data left over in this buffer from previous calls to
  // ReadUntil.
  size_t existing_output_size = min(max_data_size, read_streambuf_->size());
  data->resize(existing_output_size);
  copy(asio::buffers_begin(read_streambuf_->data()),
       asio::buffers_begin(read_streambuf_->data()) + existing_output_size,
       data->begin());
  read_streambuf_->consume(existing_output_size);

  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      strand_dispatcher_,
      boost::bind(&TcpConnection::HandleReadSize, this, _1, _2, data, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  // Read the amount of data necessary to reach max_data_size. It is
  // possible that the existing output from read_streambuf_ has
  // already completely satisfied this, but call async_read anyway; it
  // will simply invoke the callback without doing anything.
  read_buffer_.reset(new vector<byte>(max_data_size - existing_output_size));
  asio::async_read(*socket_, asio::buffer(*read_buffer_), handler);

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
  read_streambuf_.reset(new asio::streambuf);
  return true;
}

void TcpConnection::DestroyNetworkingObjects() {
  socket_.reset();
  resolver_.reset();
  io_service_work_.reset();
  strand_dispatcher_.reset();
  read_streambuf_.reset();
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
  last_write_succeeded_ = !err;

  write_callback();
}

void TcpConnection::HandleReadSize(
    const system::error_code& err,
    size_t bytes_transferred,
    vector<byte>* read_data,
    Callback read_callback) {
  assert(read_data != nullptr);
  read_buffer_->resize(bytes_transferred);

  if (read_data->empty()) {
    read_data->swap(*read_buffer_);
  } else {
    size_t existing_data_size = read_data->size();
    read_data->resize(existing_data_size + read_buffer_->size());
    copy(read_buffer_->begin(), read_buffer_->end(),
         read_data->begin() + existing_data_size);
  }

  read_buffer_.reset();
  is_reading_ = false;
  last_read_succeeded_ = !err;

  read_callback();
}

void TcpConnection::HandleReadUntil(
    const system::error_code& err,
    size_t bytes_transferred,
    const MatchByteSequenceCondition& termination_condition,
    vector<byte>* read_data,
    Callback read_callback) {
  // The read_streambuf contains data up to AT LEAST the terminator,
  // but may contain additional data beyond this. In addition, if
  // read_streambuf_ was not empty to begin with (e.g. multiple
  // ReadUntil calls in sequence), bytes_transferred only indicates
  // how much additional data was added to it with this call, which
  // may be zero if it already contained a terminator. So the only
  // reliable way to figure out where the output to should end is to
  // find the next terminator again here.
  pair<boost_streambuf_iterator, bool> termination_check =
      termination_condition(
          asio::buffers_begin(read_streambuf_->data()),
          asio::buffers_begin(read_streambuf_->data()) +
          read_streambuf_->size());

  if (termination_check.second) {
    boost_streambuf_iterator output_end =
        termination_check.first + termination_condition.sequence_length();
    size_t output_size =
        output_end - asio::buffers_begin(read_streambuf_->data());
    CHECK_NOTNULL(read_data)->resize(output_size);
    copy(asio::buffers_begin(read_streambuf_->data()), output_end,
         read_data->begin());
    read_streambuf_->consume(output_size);
  } else {
    CHECK_NOTNULL(read_data)->clear();
  }

  is_reading_ = false;
  last_read_succeeded_ = !err;

  read_callback();
}

}  // namespace polar_express
