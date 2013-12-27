#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <memory>
#include <string>
#include <vector>

#include "boost/asio.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

// This class wraps an ASIO-based TCP connection in a way that will
// integrate well with the ASIO dispatcher mechanism and run the IO on
// the appropriate threads. As long as a connection is held open, the
// ASIO dispatcher master thread will not terminate.
class TcpConnection {
 public:
  TcpConnection();
  virtual ~TcpConnection();

  // These methods are synchronous but not internally synchronized.
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

 private:
  class MatchByteSequenceCondition;

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
  unique_ptr<asio::ip::tcp::socket> socket_;

  vector<asio::const_buffer> write_buffers_;
  unique_ptr<vector<byte> > read_buffer_;  // For ReadSize
  unique_ptr<asio::streambuf> read_streambuf_;  // For ReadUntil

  DISALLOW_COPY_AND_ASSIGN(TcpConnection);
};

}  // namespace polar_express

#endif  // TCP_CONNECTION
