#include "network/http-connection.h"

#include <curl/curl.h>

#include "network/stream-connection.h"

namespace polar_express {

HttpConnection::HttpConnection(unique_ptr<StreamConnection>&& stream_connection)
    : stream_connection_(std::move(CHECK_NOTNULL(stream_connection))),
      curl_(curl_easy_init()) {
}

HttpConnection::~HttpConnection() {
  curl_easy_cleanup(curl_);
}

bool HttpConnection::is_secure() const {
  return stream_connection_->is_secure();
}

bool HttpConnection::is_opening() const {
  return stream_connection_->is_opening();
}

bool HttpConnection::is_open() const {
  return stream_connection_->is_open();
}

AsioDispatcher::NetworkUsageType HttpConnection::network_usage_type() const {
  return stream_connection_->network_usage_type();
}

bool HttpConnection::Close() {
  return stream_connection_->Close();
}

string HttpConnection::UriEncode(const string& str) const {
  string encoded_str;
  char* curl_encoded_str = curl_easy_escape(curl_, str.c_str(), str.size());
  encoded_str.assign(curl_encoded_str);
  curl_free(curl_encoded_str);
  return encoded_str;
}

vector<byte>::const_iterator HttpConnection::GetTextLineFromData(
    vector<byte>::const_iterator begin,
    vector<byte>::const_iterator end,
    string* text_line) const {
  CHECK_NOTNULL(text_line)->clear();
  while (begin != end) {
    *text_line += *begin++;
    if (text_line->size() >= 2 &&
        (*text_line)[text_line->size() - 2] == '\r' &&
        (*text_line)[text_line->size() - 1] == '\n') {
      text_line->resize(text_line->size() - 2);
      break;
    }
  }
  return begin;
}

void HttpConnection::ResetStrandDispatcher(
    AsioDispatcher::NetworkUsageType network_usage_type) {
  strand_dispatcher_ =
      AsioDispatcher::GetInstance()->NewStrandDispatcherNetworkBound(
          network_usage_type);
}

Callback HttpConnection::CreateStrandCallback(Callback callback) {
  return strand_dispatcher_->CreateStrandCallback(callback);
}

StreamConnection& HttpConnection::stream_connection() const {
  return *stream_connection_;
}

}  // namespace polar_express
