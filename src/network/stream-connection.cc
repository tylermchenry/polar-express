#include "network/stream-connection.h"

#include <algorithm>
#include <utility>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace polar_express {

StreamConnection::StreamConnection(bool is_secure)
    : StreamConnection(is_secure, false, ConnectionProperties()) {
}

StreamConnection::StreamConnection(
    bool is_secure, bool is_open, const ConnectionProperties& properties)
    : is_secure_(is_secure),
      is_opening_(false),
      is_open_(is_open),
      is_writing_(false),
      is_reading_(false),
      properties_(properties),
      last_write_succeeded_(false),
      last_read_succeeded_(false) {
  if (is_open_) {
    CreateNetworkingObjects(properties_.network_usage_type_);
  }
}

StreamConnection::~StreamConnection() {
}

bool StreamConnection::is_secure() const {
  return is_secure_;
}

bool StreamConnection::is_opening() const {
  return is_opening_;
}

bool StreamConnection::is_open() const {
  return is_open_;
}

AsioDispatcher::NetworkUsageType StreamConnection::network_usage_type()
    const {
  return properties_.network_usage_type_;
}

const string& StreamConnection::hostname() const {
  return properties_.hostname_;
}

const string& StreamConnection::protocol() const {
  return properties_.protocol_;
}

bool StreamConnection::last_write_succeeded() const {
  return last_write_succeeded_;
}

bool StreamConnection::last_read_succeeded() const {
  return last_read_succeeded_;
}

bool StreamConnection::Open(
    AsioDispatcher::NetworkUsageType network_usage_type,
    const string& hostname, const string& protocol, Callback callback) {
  return Open(ConnectionProperties(network_usage_type, hostname, protocol),
              callback);
}

bool StreamConnection::Open(
    const ConnectionProperties& properties, Callback callback) {
  if (is_opening_ || is_open_ ||
      !CreateNetworkingObjects(properties.network_usage_type_)) {
    return false;
  }
  is_opening_ = true;
  properties_ = properties;

  asio::ip::tcp::resolver::query query(properties_.hostname_,
                                       properties_.protocol_);
  auto handler = MakeStrandCallbackWithArgs<
      const system::error_code&, asio::ip::tcp::resolver::iterator>(
          boost::bind(
              &StreamConnection::HandleResolve, this, _1, _2, callback),
          asio::placeholders::error,
          asio::placeholders::iterator);
  resolver_->async_resolve(query, handler);

  return true;
}

bool StreamConnection::Reopen(Callback callback) {
  if (is_opening_ || is_open_) {
    return false;
  }
  return Open(properties_, callback);
}

bool StreamConnection::Close() {
  if (is_opening_) {
    return false;
  }

  StreamClose();
  DestroyNetworkingObjects();
  is_open_ = false;
  return true;
}

bool StreamConnection::Write(
    const vector<byte>* data, Callback callback) {
  return WriteAll({ data }, callback);
}

bool StreamConnection::WriteAll(
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
      boost::bind(&StreamConnection::HandleWrite, this, _1, _2, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  StreamAsyncWrite(write_buffers_, handler);
  return true;
}

bool StreamConnection::ReadUntil(
    const vector<byte>& terminator_bytes, vector<byte>* data,
    Callback callback) {
  if (!is_open_ || is_reading_) {
    return false;
  }

  is_reading_ = true;

  auto termination_condition = MatchByteSequenceCondition(terminator_bytes);
  auto handler = MakeStrandCallbackWithArgs<const system::error_code&, size_t>(
      boost::bind(&StreamConnection::HandleReadUntil, this, _1, _2,
                  termination_condition, data, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  StreamAsyncReadUntil(read_streambuf_.get(), termination_condition, handler);
  return true;
}

bool StreamConnection::ReadSize(
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
      boost::bind(
          &StreamConnection::HandleReadSize, this, _1, _2, data, callback),
      asio::placeholders::error,
      asio::placeholders::bytes_transferred);

  // Read the amount of data necessary to reach max_data_size. It is
  // possible that the existing output from read_streambuf_ has
  // already completely satisfied this, but call async_read anyway; it
  // will simply invoke the callback without doing anything.
  read_buffer_.reset(new vector<byte>(max_data_size - existing_output_size));
  StreamAsyncRead(read_buffer_.get(), handler);
  return true;
}

void StreamConnection::AfterConnect(Callback open_callback) {
  open_callback();
}

bool StreamConnection::CreateNetworkingObjects(
    AsioDispatcher::NetworkUsageType network_usage_type) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
  if (strand_dispatcher_.get() == nullptr) {
    DestroyNetworkingObjects();
    return false;
  }

  properties_.network_usage_type_ = network_usage_type;
  last_write_succeeded_ = false;
  last_read_succeeded_ = false;

  io_service_work_ = strand_dispatcher_->make_work();
  resolver_.reset(
      new asio::ip::tcp::resolver(strand_dispatcher_->io_service()));
  read_streambuf_.reset(new asio::streambuf);

  StreamCreate(strand_dispatcher_->io_service());
  return true;
}

void StreamConnection::DestroyNetworkingObjects() {
  StreamDestroy();

  resolver_.reset();
  io_service_work_.reset();
  strand_dispatcher_.reset();
  read_streambuf_.reset();
}

void StreamConnection::TryConnect(
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    Callback open_callback) {
  auto handler = MakeStrandCallbackWithArgs<
    const system::error_code&, asio::ip::tcp::resolver::iterator>(
        boost::bind(
            &StreamConnection::HandleConnect, this, _1, _2, open_callback),
        asio::placeholders::error,
        asio::placeholders::iterator);

  StreamAsyncConnect(endpoint_iterator, handler);
}

void StreamConnection::HandleResolve(
    const boost::system::error_code& err,
    asio::ip::tcp::resolver::iterator endpoint_iterator,
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

void StreamConnection::HandleConnect(
    const boost::system::error_code& err,
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    Callback open_callback) {
  is_opening_ = false;

  if (err) {
    if (endpoint_iterator == asio::ip::tcp::resolver::iterator()) {
      DestroyNetworkingObjects();
    } else {
      // There is another endpoint to try...
      TryConnect(endpoint_iterator, open_callback);
      return;
    }
  } else {
    is_open_ = true;
  }

  AfterConnect(open_callback);
}

void StreamConnection::HandleWrite(
    const system::error_code& err,
    size_t bytes_transferred,
    Callback write_callback) {
  write_buffers_.clear();

  is_writing_ = false;
  last_write_succeeded_ = !err;

  write_callback();
}

void StreamConnection::HandleReadSize(
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

void StreamConnection::HandleReadUntil(
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

StreamConnection::MatchByteSequenceCondition::MatchByteSequenceCondition(
    const vector<byte>& byte_sequence)
    : byte_sequence_(byte_sequence) {
}

pair<StreamConnection::boost_streambuf_iterator, bool>
StreamConnection::MatchByteSequenceCondition::operator()(
    boost_streambuf_iterator begin, boost_streambuf_iterator end) const {
  boost_streambuf_iterator pos =
      search(begin, end, byte_sequence_.begin(), byte_sequence_.end());
  return make_pair(pos, pos != end);
}

size_t StreamConnection::MatchByteSequenceCondition::sequence_length()
    const {
  return byte_sequence_.size();
}

}  // namespace polar_express
