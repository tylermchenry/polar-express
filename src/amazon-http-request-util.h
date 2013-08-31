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

  // Returns a canonical timestamp to use for a request, derived from
  // the current system time.
  virtual string GetCanonicalTimestamp() const;

  // Returns the canonical string describing the HTTP request, as
  // described by:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
  //
  // This canonical string is used for authenication. It is not
  // appropriate for actually making a HTTP request.
  virtual bool MakeCanonicalRequest(
      const HttpRequest& http_request, const string& payload_sha256_digest,
      string* canonical_http_request) const;

  // Returns the signing string for a canonical request, as described by:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html
  //
  // This method assumes that SHA256 hashes are being used. The
  // canonical timestamp provided must be the same one used elsewhere in
  // the request.
  virtual bool MakeSigningString(
      const string& aws_region_name, const string& aws_service_name,
      const string& canonical_timestamp, const string& canonical_request,
      string* signing_string) const;

 private:
  string UriEncode(const string& str) const;
  string TrimWhitespace(const string& str) const;
  string JoinKeyValuePairs(const map<string, string>& kv_pairs,
                           const string& internal_separator,
                           const string& external_separator) const;
  string NormalizePath(const string& path) const;
  string GenerateSha256Digest(const string& str) const;

  DISALLOW_COPY_AND_ASSIGN(AmazonHttpRequestUtil);
};

}  // namespace polar_express

#endif  // AMAZON_HTTP_REQUEST_UTIL
