#ifndef AMAZON_HTTP_REQUEST_UTIL
#define AMAZON_HTTP_REQUEST_UTIL

#include <map>
#include <string>

#include "macros.h"

namespace polar_express {

class HttpRequest;

class AmazonHttpRequestUtil {
 public:
  AmazonHttpRequestUtil();
  virtual ~AmazonHttpRequestUtil();

  // Returns the canonical string describing the HTTP request, as
  // described by:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
  //
  // This canonical string is used for authenication. It is not
  // appropriate for actually making a HTTP request.
  virtual bool MakeCanonicalRequest(
      const HttpRequest& http_request, const string& payload_sha256_digest,
      string* canonical_http_request) const;

 private:
  string UriEncode(const string& str) const;
  string TrimWhitespace(const string& str) const;
  string JoinKeyValuePairs(const map<string, string>& kv_pairs,
                           const string& internal_separator,
                           const string& external_separator) const;
  string NormalizePath(const string& path) const;

  DISALLOW_COPY_AND_ASSIGN(AmazonHttpRequestUtil);
};

}  // namespace polar_express

#endif  // AMAZON_HTTP_REQUEST_UTIL
