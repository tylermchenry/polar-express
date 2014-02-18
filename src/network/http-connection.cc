#include "network/http-connection.h"

#include <limits>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <curl/curl.h>

#include "network/stream-connection.h"
#include "proto/http.pb.h"

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

void HttpConnection::SerializeHeaders(
    const google::protobuf::RepeatedPtrField<KeyValue>& headers,
    const container_util::CaseInsensitiveStringSet& blacklisted_keys,
    string* serialized_headers) const {
  vector<string> header_lines;
  for (const auto& kv : headers) {
    if (container_util::Contains(blacklisted_keys, kv.key())) {
      continue;
    }
    header_lines.push_back(kv.key() + ": " + kv.value());
  }

  *CHECK_NOTNULL(serialized_headers) = algorithm::join(header_lines, "\r\n");
}

void HttpConnection::DeserializeHeadersFromData(
    vector<byte>::const_iterator begin,
    vector<byte>::const_iterator end,
    google::protobuf::RepeatedPtrField<KeyValue>* headers) const {
  string header_line;
  while (begin != end) {
    begin = GetTextLineFromData(begin, end, &header_line);
    if (!header_line.empty()) {
      DeserializeHeader(header_line, headers);
    }
  }
}

const string& HttpConnection::GetHeaderValue(
    const google::protobuf::RepeatedPtrField<KeyValue>& headers,
    const string& key) const {
  for (const auto& kv : headers) {
    if (iequals(kv.key(), key)) {
      return kv.value();
    }
  }
  return KeyValue::default_instance().value();
}

bool HttpConnection::IsPayloadChunked(
    const google::protobuf::RepeatedPtrField<KeyValue>& headers) const {
  return iequals(GetHeaderValue(headers, "Transfer-Encoding"), "chunked");
}

size_t HttpConnection::GetPayloadSize(
    const google::protobuf::RepeatedPtrField<KeyValue>& headers) const {
  try {
    return lexical_cast<size_t>(GetHeaderValue(headers, "Content-Length"));
  } catch (bad_lexical_cast&) {
    return 0;
  }
}

size_t HttpConnection::GetPayloadChunkSize(
    const vector<byte>& chunk_header) const {
  string chunk_header_str;
  const auto itr = find(chunk_header.begin(), chunk_header.end(), ';');
  try {
    // boost::lexical_cast doesn't support hexadecimal, and
    // strtoi/strtol require that we know what fundamental type size_t
    // maps to. So do it the roundabout way.
    istringstream sstr(string(chunk_header.begin(), itr));
    size_t chunk_size;
    sstr >> hex >> chunk_size;
    return chunk_size;
  } catch (...) {
    // Must use max instead of 0 or -1, since 0 is a meaningful value
    // and size_t is unsigned.
    return std::numeric_limits<size_t>::max();
  }
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

void HttpConnection::DeserializeHeader(
    const string& serialized_header_line,
    google::protobuf::RepeatedPtrField<KeyValue>* headers) const {
  // TODO: Regex seems heavyweight for this. Is there no simpler way
  // to have a multi-character delimiter?
  vector<string> header_parts;
  split_regex(header_parts, serialized_header_line, regex(": "));

  KeyValue* kv = CHECK_NOTNULL(headers)->Add();
  kv->set_key(header_parts[0]);
  if (header_parts.size() >= 2) {
    kv->set_value(header_parts[1]);
  }
}

}  // namespace polar_express
