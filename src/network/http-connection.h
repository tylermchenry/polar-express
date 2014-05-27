#ifndef HTTP_CONNECTION
#define HTTP_CONNECTION

#include <memory>
#include <set>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "util/container-util.h"

namespace polar_express {

class KeyValue;
class StreamConnection;

// Class that embodies the logic for speaking HTTP over an artibrary stream
// connection (will use a new TCP stream by default).
class HttpConnection {
 public:
  virtual ~HttpConnection();

  // These methods are synchronous but not internally synchronized.
  virtual bool is_secure() const;
  virtual bool is_opening() const;
  virtual bool is_open() const;
  virtual AsioDispatcher::NetworkUsageType network_usage_type() const;

  virtual bool Close();

 protected:
  explicit HttpConnection(unique_ptr<StreamConnection>&& stream_connection);

  string UriEncode(const string& str) const;

  string UriDecode(const string& str) const;

  // Reads data from the stream connection up to and including the next message
  // delimiter (two consecutive "\r\n" sequences).
  bool ReadNextMessage(vector<byte>* data_buffer, Callback callback);

  // Extracts a line of text from a data buffer, consisting of all
  // bytes from begin up to encountering an "\r\n" sequence, or end,
  // whichever comes first. Returns an iterator to one byte past the
  // end of the line. The "\r\n" sequence is not included in
  // text_line, but the returned iterator points beyond it.
  vector<byte>::const_iterator GetTextLineFromData(
      vector<byte>::const_iterator begin,
      vector<byte>::const_iterator end,
      string* text_line) const;

  // Serializes headers from a protobuf to a string, delimited by "\r\n"s. Does
  // NOT append an empty line at the end of the serialized output. Ignores any
  // inputs headers whose keys match the blacklist.
  void SerializeHeaders(
      const google::protobuf::RepeatedPtrField<KeyValue>& headers,
      const container_util::CaseInsensitiveStringSet& blacklisted_keys,
      string* serialized_headers) const;

  // Deserializes headers from an incoming HTTP data stream into a protobuf.
  // Starts looking for headers at 'begin', and stops either when 'end' is
  // reached, or when a blank line is encountered.
  void DeserializeHeadersFromData(
      vector<byte>::const_iterator begin,
      vector<byte>::const_iterator end,
      google::protobuf::RepeatedPtrField<KeyValue>* headers) const;

  const string& GetHeaderValue(
      const google::protobuf::RepeatedPtrField<KeyValue>& headers,
      const string& key) const;

  bool IsPayloadChunked(
      const google::protobuf::RepeatedPtrField<KeyValue>& headers) const;

  size_t GetPayloadSize(
      const google::protobuf::RepeatedPtrField<KeyValue>& headers) const;

  size_t GetPayloadChunkSize(const vector<byte>& chunk_header) const;

  void ResetStrandDispatcher(
      AsioDispatcher::NetworkUsageType network_usage_type);

  Callback CreateStrandCallback(Callback callback);

  StreamConnection& stream_connection() const;

 private:
  void DeserializeHeader(
      const string& serialized_header_line,
      google::protobuf::RepeatedPtrField<KeyValue>* headers) const;

  const unique_ptr<StreamConnection> stream_connection_;
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  void* curl_;  // Owned, but destructed specially.

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace polar_express

#endif  // HTTP_CONNECTION
