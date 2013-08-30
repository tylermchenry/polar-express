#include "amazon-http-request-util.h"

#include <iomanip>
#include <functional>
#include <map>
#include <sstream>
#include <vector>

#include "boost/algorithm/string.hpp"

#include "proto/http.pb.h"

namespace polar_express {

AmazonHttpRequestUtil::AmazonHttpRequestUtil() {
}

AmazonHttpRequestUtil::~AmazonHttpRequestUtil() {
}

bool AmazonHttpRequestUtil::MakeCanonicalRequest(
    const HttpRequest& http_request, const string& payload_sha256_digest,
    string* canonical_http_request) const {
  CHECK_NOTNULL(canonical_http_request)->clear();

  // The canonical URI must have query parameters in strict ASCII
  // order by key. The map structure implicitly provides this
  // ordering. Amazon does not specify a way to break ties between
  // identical keys, so return false if a key is duplicated.
  map<string, string> canonical_query_parameters;
  for (const auto& kv : http_request.query_parameters()) {
    if (!canonical_query_parameters.insert(
            make_pair(UriEncode(kv.key()), UriEncode(kv.value()))).second) {
      return false;
    }
  }

  // Same ASCII-ordering requirements on request headers as on query
  // parameters.
  map<string, string> canonical_request_headers;
  for (const auto& kv : http_request.request_headers()) {
    if (!canonical_request_headers.insert(
            make_pair(boost::algorithm::to_lower_copy(kv.key()),
                      TrimWhitespace(kv.value()))).second) {
      return false;
    }
  }
  if (!canonical_request_headers.insert(
          make_pair("host", TrimWhitespace(http_request.hostname()))).second) {
    return false;
  }

  vector<string> signed_headers;
  for (const auto& kv : canonical_request_headers) {
    signed_headers.push_back(kv.first);
  }

  string canonical_payload_sha256_digest = payload_sha256_digest;
  boost::algorithm::to_lower(canonical_payload_sha256_digest);

  // Note the two linebreaks after the request headers. Technically
  // the "canonical request headers" portion of the canonical request
  // includes a trailing newline after the final header. Then there is
  // _another_ newline that separates the canonical request headers
  // portion from the signed headers portion.
  *canonical_http_request =
      HttpRequest_Method_Name(http_request.method()) + "\n" +
      NormalizePath(http_request.path()) + "\n" +
      JoinKeyValuePairs(canonical_query_parameters, "=", "&") + "\n" +
      JoinKeyValuePairs(canonical_request_headers, ":", "\n") + "\n\n" +
      UriEncode(boost::algorithm::join(signed_headers, ";")) + "\n" +
      canonical_payload_sha256_digest;
  return true;
}

string AmazonHttpRequestUtil::UriEncode(const string& str) const {
  // Can't find a boost algorithm that does URI encoding out of the
  // box. Besides, this needs to be the exact form of encoding
  // described by Amazon (e.g. "%20" not "+").
  string encoded_str;

  // Safe ranges are A-Z, a-z, 0-9, plus the characters: - _ . ~
  for (const char c : str) {
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded_str += c;
    } else {
      ostringstream osstr;
      osstr << "%" << ios::hex << setw(2) << static_cast<int>(c);
      encoded_str += osstr.str();
    }
  }

  return str;
}

string AmazonHttpRequestUtil::TrimWhitespace(const string& str) const {
  string trimmed_str = boost::algorithm::trim_copy(str);

  // If after trimming leading/trailing whitespace, the remaining
  // string is not quoted, then also trim inner whitepsace (collapse
  // consecutive whitespace).
  if (!trimmed_str.empty() &&
      (*trimmed_str.begin() == '\"' && *trimmed_str.rbegin() != '\"')) {
    // TODO(tylermchenry): Want to call boost::algorithm::trim_all
    // here. Looks like I need a newer version of boost...
    return trimmed_str;
  }

  return trimmed_str;
}

string AmazonHttpRequestUtil::JoinKeyValuePairs(
    const map<string, string>& kv_pairs,
    const string& internal_separator,
    const string& external_separator) const {
  string joined_str;
  for (const auto& kv : kv_pairs) {
    if (!joined_str.empty()) {
      joined_str += external_separator;
    }
    joined_str += kv.first + internal_separator + kv.second;
  }
  return joined_str;
}

string AmazonHttpRequestUtil::NormalizePath(const string& path) const {
  // Normalizes a URI path into a sequence of URI-encoded segments,
  // joined by '/', per RFC3986. According to the Amazon API, we must
  // also remove redundant . and .. segments, but this is not yet
  // implemented.
  //
  // TODO(tylermchenry): Remove redundant . and .. segments.
  vector<string> segments;
  boost::algorithm::split(segments, path, boost::is_any_of("/"));
  if (segments.empty()) {
    return "/";
  }
  for (auto itr = segments.begin(); itr != segments.end(); ++itr) {
    *itr = UriEncode(*itr);
  }
  return boost::algorithm::join(segments, "/");
}

}  // namespace polar_express
