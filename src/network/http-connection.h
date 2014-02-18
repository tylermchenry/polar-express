#ifndef HTTP_CONNECTION
#define HTTP_CONNECTION

#include <memory>
#include <vector>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

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

  // Extracts a line of text from a data buffer, consisting of all
  // bytes from begin up to encountering an "\r\n" sequence, or end,
  // whichever comes first. Returns an iterator to one byte past the
  // end of the line. The "\r\n" sequence is not included in
  // text_line, but the returned iterator points beyond it.
  vector<byte>::const_iterator GetTextLineFromData(
      vector<byte>::const_iterator begin,
      vector<byte>::const_iterator end,
      string* text_line) const;

  void ResetStrandDispatcher(
      AsioDispatcher::NetworkUsageType network_usage_type);

  Callback CreateStrandCallback(Callback callback);

  StreamConnection& stream_connection() const;

 private:
  const unique_ptr<StreamConnection> stream_connection_;
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  void* curl_;  // Owned, but destructed specially.

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace polar_express

#endif  // HTTP_CONNECTION
