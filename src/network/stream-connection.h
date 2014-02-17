#ifndef STREAM_CONNECTION
#define STREAM_CONNECTION

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

// Non-template parts of StreamConnectionTmpl (below).
//
// These classes wrap an ASIO-based stream connection in a way that will
// integrate well with the ASIO dispatcher mechanism and run the IO on
// the appropriate threads. As long as a connection is held open, the
// ASIO dispatcher master thread will not terminate.
class StreamConnection {
 public:
  virtual ~StreamConnection();

  // These methods are synchronous but not internally synchronized.
  virtual bool is_secure() const;
  virtual bool is_opening() const;
  virtual bool is_open() const;
  virtual AsioDispatcher::NetworkUsageType network_usage_type() const;
  virtual const string& hostname() const;
  virtual const string& protocol() const;
  virtual bool last_write_succeeded() const;
  virtual bool last_read_succeeded() const;

  // These methods immediately return false if the connection is
  // already open (or in the process of opening). In these cases callback
  // will never be called.
  virtual bool Open(
      AsioDispatcher::NetworkUsageType network_usage_type,
      const string& hostname, const string& protocol, Callback callback);

  // Re-opens with the same parameters as the previous call to Open. Returns
  // false and does nothing of Open has never been called.
  virtual bool Reopen(Callback callback);

  // Close can return false if the connection is in the process of
  // opening but has not yet opened. If the connection is already closed,
  // this is a no-op.
  virtual bool Close();

  // Write methods immediately return false if the connection is not
  // open or if another write is still in progress.

  virtual bool Write(const vector<byte>* data, Callback callback);

  virtual bool WriteAll(
      const vector<const vector<byte>*>& sequential_data,
      Callback callback);

  // Read methods immediately return false if the connection is not
  // open, or if another read is still in progress.

  // Reads until the terminator_bytes byte sequence is
  // encountered. The termination sequence IS included in the read data
  // (to distinguish from a prematurely closed connection). Will
  // resize the data vector as appropriate.
  virtual bool ReadUntil(
      const vector<byte>& terminator_bytes, vector<byte>* data,
      Callback callback);

  // Reads until data_size bytes have been read. May read fewer bytes
  // if the connection is closed prematurely. Will resize the data
  // vector as necessary.
  virtual bool ReadSize(
      size_t max_data_size, vector<byte>* data, Callback callback);

 protected:
  StreamConnection(bool is_secure, bool is_open);

  typedef asio::buffers_iterator<asio::streambuf::const_buffers_type>
    boost_streambuf_iterator;

  class MatchByteSequenceCondition {
   public:
    explicit MatchByteSequenceCondition(const vector<byte>& byte_sequence);

    pair<boost_streambuf_iterator, bool> operator()(
        boost_streambuf_iterator begin, boost_streambuf_iterator end) const;

    size_t sequence_length() const;

   private:
    vector<byte> byte_sequence_;
  };

  // Operations on the templated stream type that subclasses must implement:
  virtual void StreamCreate(asio::io_service& io_service) = 0;

  virtual void StreamDestroy() = 0;

  virtual void StreamClose() = 0;

  virtual void StreamAsyncConnect(
      asio::ip::tcp::resolver::iterator endpoint_iterator,
      const boost::function<
          void(const system::error_code&, asio::ip::tcp::resolver::iterator)>&
          handler) = 0;

  virtual void StreamAsyncWrite(
      const vector<asio::const_buffer>& write_buffers,
      const boost::function<void(const system::error_code&, size_t)>&
          handler) = 0;

  virtual void StreamAsyncRead(
      vector<byte>* read_buffer,
      const boost::function<void(const system::error_code&, size_t)>&
          handler) = 0;

  virtual void StreamAsyncReadUntil(
      asio::streambuf* read_streambuf,
      const MatchByteSequenceCondition& termination_condition,
      const boost::function<void(const system::error_code&, size_t)>&
          handler) = 0;

  // Subclasses may override this for extra behavior after a connection is
  // successfully established. By default, just invokes open_callback.
  virtual void AfterConnect(Callback open_callback);

  template <typename T1, typename PT1>
  boost::function<void(T1)> MakeStrandCallbackWithArgs(
      boost::function<void(T1)> callback_with_args, PT1 arg1_placeholder) {
    return strand_dispatcher_->MakeStrandCallbackWithArgs(
        callback_with_args, arg1_placeholder);
  }

  template <typename T1, typename T2, typename PT1, typename PT2>
  boost::function<void(T1, T2)> MakeStrandCallbackWithArgs(
      boost::function<void(T1, T2)> callback_with_args, PT1 arg1_placeholder,
      PT2 arg2_placeholder) {
    return strand_dispatcher_->MakeStrandCallbackWithArgs(
        callback_with_args, arg1_placeholder, arg2_placeholder);
  }

 private:
  bool CreateNetworkingObjects(
      AsioDispatcher::NetworkUsageType network_usage_type);
  void DestroyNetworkingObjects();

  void TryConnect(asio::ip::tcp::resolver::iterator endpoint_iterator,
                  Callback open_callback);

  void HandleResolve(
      const system::error_code& err,
      asio::ip::tcp::resolver::iterator endpoint_iterator,
      Callback open_callback);

  void HandleConnect(
      const system::error_code& err,
      asio::ip::tcp::resolver::iterator endpoint_iterator,
      Callback open_callback);

  void HandleWrite(
      const system::error_code& err, size_t bytes_transferred,
      Callback write_callback);

  void HandleReadSize(
      const system::error_code& err, size_t bytes_transferred,
      vector<byte>* read_data, Callback read_callback);

  void HandleReadUntil(
      const system::error_code& err, size_t bytes_transferred,
      const MatchByteSequenceCondition& termination_condition,
      vector<byte>* read_data, Callback read_callback);

  const bool is_secure_;
  bool is_opening_;
  bool is_open_;
  bool is_writing_;
  bool is_reading_;
  AsioDispatcher::NetworkUsageType network_usage_type_;
  string hostname_;
  string protocol_;
  bool last_write_succeeded_;
  bool last_read_succeeded_;

  // Networking objects managed by Create/Destroy methods above.
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  unique_ptr<asio::io_service::work> io_service_work_;
  unique_ptr<asio::ip::tcp::resolver> resolver_;

  vector<asio::const_buffer> write_buffers_;
  unique_ptr<vector<byte> > read_buffer_;  // For ReadSize
  unique_ptr<asio::streambuf> read_streambuf_;  // For ReadUntil

  DISALLOW_COPY_AND_ASSIGN(StreamConnection);
};

}  // namespace polar_express

namespace boost {
namespace asio {
template <>
struct is_match_condition<
  polar_express::StreamConnection::MatchByteSequenceCondition>
    : public boost::true_type {};
}  // namespace asio
}  // namespace boost

namespace polar_express {

// Abstract template base for stream-oriented connections.
//
// This class assumes that everything is TCP-based (which is a fair assumption
// for this application). So the TCP resolver is used to resolve a hostname and
// protocol to an ip and port. But the stream type is left templated since it
// may be a raw TCP socket or an SSL stream on top of a TCP socket.
template <typename AsyncStreamT>
class StreamConnectionTmpl : public StreamConnection {
 public:
  virtual ~StreamConnectionTmpl();

 protected:
  explicit StreamConnectionTmpl(bool is_secure);

  StreamConnectionTmpl(bool is_secure,
                       unique_ptr<AsyncStreamT>&& connected_stream);

  // Subclasses must provide for creation of the underlying stream object.
  virtual unique_ptr<AsyncStreamT> StreamConstruct(
      asio::io_service& io_service) = 0;

  AsyncStreamT& stream();

 private:
  virtual void StreamCreate(asio::io_service& io_service);

  virtual void StreamDestroy();

  virtual void StreamClose();

  virtual void StreamAsyncConnect(
      asio::ip::tcp::resolver::iterator endpoint_iterator,
      const boost::function<void(const system::error_code&,
                           asio::ip::tcp::resolver::iterator)>& handler);

  virtual void StreamAsyncWrite(
      const vector<asio::const_buffer>& write_buffers,
      const boost::function<void(const system::error_code&, size_t)>& handler);

  virtual void StreamAsyncRead(
      vector<byte>* read_buffer,
      const boost::function<void(const system::error_code&, size_t)>& handler);

  virtual void StreamAsyncReadUntil(
      asio::streambuf* read_streambuf,
      const MatchByteSequenceCondition& termination_condition,
      const boost::function<void(const system::error_code&, size_t)>& handler);

  unique_ptr<AsyncStreamT> stream_;

  DISALLOW_COPY_AND_ASSIGN(StreamConnectionTmpl);
};

template <typename AsyncStreamT>
StreamConnectionTmpl<AsyncStreamT>::StreamConnectionTmpl(bool is_secure)
    : StreamConnection(is_secure, false /* is_open */) {
}

template <typename AsyncStreamT>
StreamConnectionTmpl<AsyncStreamT>::StreamConnectionTmpl(
    bool is_secure, unique_ptr<AsyncStreamT>&& connected_stream)
    : StreamConnection(is_secure, true /* is_open */),
      stream_(std::move(CHECK_NOTNULL(connected_stream))) {
}

template <typename AsyncStreamT>
StreamConnectionTmpl<AsyncStreamT>::~StreamConnectionTmpl() {
}

template <typename AsyncStreamT>
AsyncStreamT& StreamConnectionTmpl<AsyncStreamT>::stream() {
  return *CHECK_NOTNULL(stream_);
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamCreate(
    asio::io_service& io_service) {
  if (stream_.get() == nullptr) {
    stream_ = StreamConstruct(io_service);
  }
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamDestroy() {
  stream_.reset();
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamClose() {
  if (stream_) {
    auto error_code =
        asio::error::make_error_code(asio::error::connection_aborted);
    stream_->lowest_layer().shutdown(
        asio::ip::tcp::socket::shutdown_both, error_code);
    stream_->lowest_layer().close(error_code);
  }
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamAsyncConnect(
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    const boost::function<void(const system::error_code&,
                         asio::ip::tcp::resolver::iterator)>& handler) {
  asio::async_connect(stream().lowest_layer(), endpoint_iterator, handler);
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamAsyncWrite(
    const vector<asio::const_buffer>& write_buffers,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_write(stream(), write_buffers, handler);
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamAsyncRead(
    vector<byte>* read_buffer,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_read(stream(), asio::buffer(*CHECK_NOTNULL(read_buffer)),
                   handler);
}

template <typename AsyncStreamT>
void StreamConnectionTmpl<AsyncStreamT>::StreamAsyncReadUntil(
    asio::streambuf* read_streambuf,
    const MatchByteSequenceCondition& termination_condition,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_read_until(stream(), *CHECK_NOTNULL(read_streambuf),
                         termination_condition, handler);
}

}  // namespace polar_express

#endif  // STREAM_CONNECTION
