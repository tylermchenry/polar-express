#include "amazon-http-request-util.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <functional>

#include "boost/algorithm/string.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/format.hpp"
#include "crypto++/hex.h"
#include "crypto++/hmac.h"
#include "crypto++/sha.h"

#include "proto/http.pb.h"

namespace polar_express {
namespace {

const char kAmazonTerminationString[] = "aws4_request";

}  // namespace

AmazonHttpRequestUtil::AmazonHttpRequestUtil() {
}

AmazonHttpRequestUtil::~AmazonHttpRequestUtil() {
}

string AmazonHttpRequestUtil::GetCanonicalTimestamp() const {
  // Canonical timestamp uses ISO 8601 simple format with timezone,
  // which should always be UTC ("Z").
  return boost::posix_time::to_iso_string(
      boost::posix_time::second_clock::universal_time()) + "Z";
}

bool AmazonHttpRequestUtil::SignRequest(
    const CryptoPP::SecByteBlock& aws_secret_key,
    const string& aws_region_name,
    const string& aws_service_name,
    const HttpRequest& http_request,
    const string& payload_sha256_digest,
    string* signature) const {
  string canonical_timestamp;
  if (!GetCanonicalTimestampFromRequest(http_request, &canonical_timestamp)) {
    return false;
  }

  string canonical_http_request;
  if (!MakeCanonicalRequest(
          http_request, payload_sha256_digest, &canonical_http_request)) {
    return false;
  }

  string signing_string;
  if (!MakeSigningString(
          aws_region_name, aws_service_name, canonical_timestamp,
          canonical_http_request, &signing_string)) {
    return false;
  }

  CryptoPP::SecByteBlock derived_signing_key;
  if (!MakeDerivedSigningKey(
          aws_secret_key, aws_region_name, aws_service_name,
          canonical_timestamp, &derived_signing_key)) {
    return false;
  }

  *CHECK_NOTNULL(signature) =
      MakeSignature(derived_signing_key, signing_string);
  return true;
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

  string canonical_payload_sha256_digest =
      boost::algorithm::to_lower_copy(payload_sha256_digest);

  // Note the extra linebreak after the request headers. Technically
  // the "canonical request headers" portion of the canonical request
  // includes a trailing newline after the final header. Then there is
  // _another_ newline that separates the canonical request headers
  // portion from the signed headers portion.
  *canonical_http_request = boost::algorithm::join<vector<string> >(
      { HttpRequest_Method_Name(http_request.method()),
        NormalizePath(http_request.path()),
        JoinKeyValuePairs(canonical_query_parameters, "=", "&"),
        JoinKeyValuePairs(canonical_request_headers, ":", "\n") + "\n",
        boost::algorithm::join(signed_headers, ";"),
        canonical_payload_sha256_digest },
      "\n");

  return true;
}

bool AmazonHttpRequestUtil::MakeSigningString(
    const string& aws_region_name, const string& aws_service_name,
    const string& canonical_timestamp, const string& canonical_request,
    string* signing_string) const {
  const char kAmazonSha256AlgorithmId[] = "AWS4-HMAC-SHA256";
  CHECK_NOTNULL(signing_string)->clear();

  // Pull the date out of the canonical timestamp for use in the
  // credential scope.
  string canonical_date;
  if (!GetCanonicalDate(canonical_timestamp, &canonical_date)) {
    return false;
  }

  const string credential_scope =
      boost::algorithm::join<vector<string> >(
          { canonical_date, aws_region_name, aws_service_name,
            kAmazonTerminationString},
          "/");

  const string canonical_request_sha256_digest =
      boost::algorithm::to_lower_copy(GenerateSha256Digest(canonical_request));

  *signing_string = boost::algorithm::join<vector<string> >(
    { kAmazonSha256AlgorithmId, canonical_timestamp, credential_scope,
      canonical_request_sha256_digest },
    "\n");

  return true;
}

bool AmazonHttpRequestUtil::MakeDerivedSigningKey(
    const CryptoPP::SecByteBlock& aws_secret_key,
    const string& aws_region_name, const string& aws_service_name,
    const string& canonical_timestamp,
    CryptoPP::SecByteBlock* derived_signing_key) const {
  const byte kAmazonAwsSecretKeyPrefix[] = "AWS4";

  // Be sure not to include the trailing NUL in the block.
  const CryptoPP::SecByteBlock kAmazonAwsSecretKeyPrefixBlock =
      CryptoPP::SecByteBlock(
          kAmazonAwsSecretKeyPrefix, sizeof(kAmazonAwsSecretKeyPrefix) - 1);

  string canonical_date;
  if (!GetCanonicalDate(canonical_timestamp, &canonical_date)) {
    return false;
  }

  // The derived signing key is a successive application of a SHA256
  // HMAC to several inputs. The first key is the actual secret key
  // with a prefix applied, and then at each subsequent step, the
  // output from the previous step is the key for the next step.
  *CHECK_NOTNULL(derived_signing_key) = kAmazonAwsSecretKeyPrefixBlock;
  *derived_signing_key += aws_secret_key;
  CryptoPP::SecByteBlock sha256_hmac;
  const vector<string> inputs =
      { canonical_date, aws_region_name, aws_service_name,
        kAmazonTerminationString };
  for (const string& input : inputs) {
    GenerateSha256Hmac(*derived_signing_key, input, &sha256_hmac);
    *derived_signing_key = sha256_hmac;
  }

  return true;
}

string AmazonHttpRequestUtil::MakeSignature(
    const CryptoPP::SecByteBlock& derived_signing_key,
    const string& signing_string) const {
  vector<byte> binary_signature;
  GenerateSha256Hmac(derived_signing_key, signing_string, &binary_signature);
  return boost::algorithm::to_lower_copy(HexEncode(binary_signature));
}

string AmazonHttpRequestUtil::UriEncode(const string& str) const {
  // Can't find a boost algorithm that does URI encoding out of the
  // box. Besides, this needs to be the exact form of encoding
  // described by Amazon (e.g. "%20" not "+").
  string encoded_str;

  // Safe ranges are A-Z, a-z, 0-9, plus the characters: - _ . ~
  for (const unsigned char c : str) {
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded_str += c;
    } else {
      encoded_str += boost::str(boost::format("%%%02X") %
                                static_cast<unsigned int>(c));
    }
  }

  return encoded_str;
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

bool AmazonHttpRequestUtil::GetCanonicalDate(
    const string& canonical_timestamp, string* canonical_date) const {
  boost::posix_time::ptime canonical_ptime =
      boost::posix_time::from_iso_string(canonical_timestamp);
  if (canonical_ptime.is_not_a_date_time()) {
    return false;
  }
  *CHECK_NOTNULL(canonical_date) =
      boost::gregorian::to_iso_string(canonical_ptime.date());
  return true;
}

string AmazonHttpRequestUtil::GenerateSha256Digest(const string& str) const {
  CryptoPP::SHA256 sha256_engine;
  vector<byte> raw_digest(CryptoPP::SHA256::DIGESTSIZE);
  sha256_engine.CalculateDigest(
      raw_digest.data(),
      reinterpret_cast<const byte*>(str.data()),
      str.size());
  return HexEncode(raw_digest);
}

void AmazonHttpRequestUtil::GenerateSha256Hmac(
    const CryptoPP::SecByteBlock& key, const string& str,
    CryptoPP::SecByteBlock* sha256_hmac) const {
  CryptoPP::HMAC<CryptoPP::SHA256> sha256_hmac_engine(key.data(), key.size());
  CHECK_NOTNULL(sha256_hmac)->resize(sha256_hmac_engine.DigestSize());
  sha256_hmac_engine.CalculateDigest(
      sha256_hmac->data(),
      reinterpret_cast<const byte*>(str.data()),
      str.size());
}

void AmazonHttpRequestUtil::GenerateSha256Hmac(
    const CryptoPP::SecByteBlock& key, const string& str,
    vector<byte>* sha256_hmac) const {
  CryptoPP::SecByteBlock sha256_hmac_block;
  GenerateSha256Hmac(key, str, &sha256_hmac_block);
  std::copy(sha256_hmac_block.begin(), sha256_hmac_block.end(),
            std::back_inserter(*CHECK_NOTNULL(sha256_hmac)));
}

string AmazonHttpRequestUtil::HexEncode(const vector<byte>& data) const {
  string digest_str;
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(digest_str));
  encoder.Put(data.data(), data.size());
  encoder.MessageEnd();
  return digest_str;
}

bool AmazonHttpRequestUtil::GetCanonicalTimestampFromRequest(
    const HttpRequest& request, string* canonical_timestamp) const {
  const char kAmazonCanonicalTimestampHeaderKey[] = "x-amz-date";
  for (const auto& header : request.request_headers()) {
    if (header.key() == kAmazonCanonicalTimestampHeaderKey) {
      *CHECK_NOTNULL(canonical_timestamp) = header.value();
      return true;
    }
  }
  return false;
}

}  // namespace polar_express
