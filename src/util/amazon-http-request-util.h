#ifndef AMAZON_HTTP_REQUEST_UTIL
#define AMAZON_HTTP_REQUEST_UTIL

#include <map>
#include <string>
#include <vector>

#include <crypto++/secblock.h>

#include "base/macros.h"

namespace polar_express {

class HttpRequest;

// A class that performs various operations specific to the Amazon AWS
// HTTP-based protocols.
class AmazonHttpRequestUtil {
 public:
  AmazonHttpRequestUtil();
  virtual ~AmazonHttpRequestUtil();

  // Adds the 'x-amz-date' and 'Authorization' header to the
  // given request, using the given keys and other data. If any error
  // occurs, false is returned, and the request may be modified. Uses
  // the current time for the canonical timestamp.
  //
  // This implements Version 4 of the AWS signature algorithm.
  virtual bool AuthorizeRequest(
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& aws_access_key,
      const string& aws_region_name,
      const string& aws_service_name,
      const string& payload_sha256_digest,
      HttpRequest* http_request) const;

  // The below methods are the various parts of AuthorizeRequest,
  // which may be useful separately:

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

  // Generates a derived key for signing a request with a particular
  // credential scope. As described by:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  //
  // The canonical timestamp provided must be the same one used elsewhere in
  // the request.
  virtual bool MakeDerivedSigningKey(
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& aws_region_name, const string& aws_service_name,
      const string& canonical_timestamp,
      CryptoPP::SecByteBlock* derived_signing_key) const;

  // Returns the signature for the given secret key and signing
  // string, as described by:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  virtual string MakeSignature(
      const CryptoPP::SecByteBlock& derived_signing_key,
      const string& signing_string) const;

 private:
  // Adds the specified header to the given request. Does not alter
  // key or value and does not check for duplicates.
  void AddHeaderToRequest(
      const string& key, const string& value, HttpRequest* http_request) const;

  // Produces a URI-encoded string from str, according to Amazon's
  // specifications about which characters need to be escaped and how.
  string UriEncode(const string& str) const;

  // Removes leading and trailing whitespace from a string, as well as
  // collapsing repeated inner whitespace if the string is not quoted.
  string TrimWhitespace(const string& str) const;

  // Given a map of key-value pairs, produces a string where the
  // associated key-values are first joined with internal_separator, and
  // then the resulting strings are joined together with
  // external_separator.
  string JoinKeyValuePairs(const map<string, string>& kv_pairs,
                           const string& internal_separator,
                           const string& external_separator) const;

  // Normalizes a URI path into a sequence of URI-encoded segments,
  // joined by forward slashes, per RFC3986.
  string NormalizePath(const string& path) const;

  // Produces the date portion of a canonical timestamp. Returns false
  // if the canonical timestamp cannot be parsed as ISO8601.
  bool GetCanonicalDate(
      const string& canonical_timestamp, string* canonical_date) const;

  // Synchronously generates a simple hex-encoded SHA256 digest of the
  // provided string.
  string GenerateSha256Digest(const string& str) const;

  // Synchronously generates a binary hashed MAC from the provided
  // key and string using SHA256. The only reason that the output is
  // a secure block is due to how this method is used in
  // GenerateDerivedSigningKey.
  void GenerateSha256Hmac(
      const CryptoPP::SecByteBlock& key, const string& str,
      CryptoPP::SecByteBlock* sha256_hmac) const;

  // Convenience version of the above where the output does not need
  // to be secure.
  void GenerateSha256Hmac(
      const CryptoPP::SecByteBlock& key, const string& str,
      vector<byte>* sha256_hmac) const;

  // Returns the hex encoding of the given binary data.
  string HexEncode(const vector<byte>& data) const;

  // Generates the value for the Authorization header. Assumes that
  // all headers present in the request were part of the signature
  // provided.
  string GenerateAuthorizationHeaderValue(
      const string& aws_access_key,
      const string& aws_region_name,
      const string& aws_service_name,
      const HttpRequest& http_request,
      const string& canonical_date,
      const string& signature) const;

  void* curl_;  // Owned, but destructed specially.

  DISALLOW_COPY_AND_ASSIGN(AmazonHttpRequestUtil);
};

}  // namespace polar_express

#endif  // AMAZON_HTTP_REQUEST_UTIL
