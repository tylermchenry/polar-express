#ifndef STREAM_CONNECTION
#define STREAM_CONNECTION

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

// Non-template parts of StreamConnection (below).
//
// These classes wrap an ASIO-based stream connection in a way that will
// integrate well with the ASIO dispatcher mechanism and run the IO on
// the appropriate threads. As long as a connection is held open, the
// ASIO dispatcher master thread will not terminate.
class StreamConnectionBase {
 public:
  virtual ~StreamConnectionBase();

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
  explicit StreamConnectionBase(bool is_secure);

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

  DISALLOW_COPY_AND_ASSIGN(StreamConnectionBase);
};

}  // namespace polar_express

namespace boost {
namespace asio {
template <>
struct is_match_condition<
  polar_express::StreamConnectionBase::MatchByteSequenceCondition>
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
class StreamConnection : public StreamConnectionBase {
 public:
  virtual ~StreamConnection();

 protected:
  explicit StreamConnection(bool is_secure);

  // Subclasses must provide for creation of the underlying stream object.
  virtual unique_ptr<AsyncStreamT> StreamConstruct(
      asio::io_service& io_service) const = 0;

  virtual void StreamShutdown(
      AsyncStreamT* stream, system::error_code& error_code) const = 0;

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

  DISALLOW_COPY_AND_ASSIGN(StreamConnection);
};

template <typename AsyncStreamT>
StreamConnection<AsyncStreamT>::StreamConnection(bool is_secure)
  : StreamConnectionBase(is_secure) {
}

template <typename AsyncStreamT>
StreamConnection<AsyncStreamT>::~StreamConnection() {
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamCreate(
    asio::io_service& io_service) {
  stream_ = StreamConstruct(io_service);
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamDestroy() {
  stream_.reset();
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamClose() {
  if (stream_) {
    auto error_code =
        asio::error::make_error_code(asio::error::connection_aborted);
    StreamShutdown(stream_.get(), error_code);
    stream_->close(error_code);
  }
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamAsyncConnect(
    asio::ip::tcp::resolver::iterator endpoint_iterator,
    const boost::function<void(const system::error_code&,
                         asio::ip::tcp::resolver::iterator)>& handler) {
  asio::async_connect(*CHECK_NOTNULL(stream_), endpoint_iterator, handler);
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamAsyncWrite(
    const vector<asio::const_buffer>& write_buffers,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_write(*CHECK_NOTNULL(stream_), write_buffers, handler);
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamAsyncRead(
    vector<byte>* read_buffer,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_read(*CHECK_NOTNULL(stream_),
                   asio::buffer(*CHECK_NOTNULL(read_buffer)), handler);
}

template <typename AsyncStreamT>
void StreamConnection<AsyncStreamT>::StreamAsyncReadUntil(
    asio::streambuf* read_streambuf,
    const MatchByteSequenceCondition& termination_condition,
    const boost::function<void(const system::error_code&, size_t)>& handler) {
  asio::async_read_until(*CHECK_NOTNULL(stream_),
                         *CHECK_NOTNULL(read_streambuf), termination_condition,
                         handler);
}

}  // namespace polar_express

#endif  // STREAM_CONNECTION
