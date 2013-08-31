#include "amazon-http-request-util.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "crypto++/hex.h"
#include "crypto++/secblock.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "proto/http.pb.h"

using testing::ContainerEq;

namespace polar_express {
namespace {

// These are the canonical request, signing string, and derived
// signing key used for the examples in Amazon's documentation. They
// are used in test cases below that are derived from examples in the
// documentation.
const char kAmazonDocsExpectedCanonicalHttpRequest[] =
    "POST\n"
    "/\n"
    "\n"
    "content-type:application/x-www-form-urlencoded; charset=utf-8\n"
    "host:iam.amazonaws.com\n"
    "x-amz-date:20110909T233600Z\n"
    "\n"
    "content-type;host;x-amz-date\n"
    "b6359072c78d70ebee1e81adcbab4f01bf2c23245fa365ef83fe8f1f955085e2";

const char kAmazonDocsExpectedSigningString[] =
    "AWS4-HMAC-SHA256\n"
    "20110909T233600Z\n"
    "20110909/us-east-1/iam/aws4_request\n"
    "3511de7e95d28ecd39e9513b642aee07e54f4941150d8df8bf94b328ef7e55e2";

const byte kAmazonDocsExpectedDerivedSigningKey[] = {
  0x98u, 0xF1u, 0xD8u, 0x89u, 0xFEu, 0xC4u, 0xF4u, 0x42u,
  0x1Au, 0xDCu, 0x52u, 0x2Bu, 0xABu, 0x0Cu, 0xE1u, 0xF8u,
  0x2Eu, 0x69u, 0x29u, 0xC2u, 0x62u, 0xEDu, 0x15u, 0xE5u,
  0xA9u, 0x4Cu, 0x90u, 0xEFu, 0xD1u, 0xE3u, 0xB0u, 0xE7u
};

string HexEncode(const vector<byte>& data) {
  string digest_str;
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(digest_str));
  encoder.Put(data.data(), data.size());
  encoder.MessageEnd();
  return digest_str;
}

class AmazonHttpRequestUtilTest : public testing::Test {
 public:
  AmazonHttpRequestUtilTest()
      : amazon_docs_expected_derived_signing_key_vec_(
          kAmazonDocsExpectedDerivedSigningKey,
          kAmazonDocsExpectedDerivedSigningKey +
          sizeof(kAmazonDocsExpectedDerivedSigningKey)) {
  }

 protected:
  const AmazonHttpRequestUtil amazon_http_request_util_;
  const vector<byte> amazon_docs_expected_derived_signing_key_vec_;
};

TEST_F(AmazonHttpRequestUtilTest, MakeCanonicalRequest) {
  // The test input and expected output is taken from the
  // Amazon documentation:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html

  HttpRequest http_request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "method: POST                                                          "
      "hostname: 'iam.amazonaws.com'                                         "
      "path: '/'                                                             "
      "request_headers <                                                     "
      "  key: 'Content-type'                                                 "
      "  value: 'application/x-www-form-urlencoded; charset=utf-8'           "
      ">                                                                     "
      "request_headers <                                                     "
      "  key: 'x-amz-date'                                                   "
      "  value: '20110909T233600Z'                                           "
      ">                                                                     ",
      &http_request));

  // Intentionally in uppercase to test that it is converted to lowercase.
  const char payload_sha256_digest[] =
      "B6359072C78D70EBEE1E81ADCBAB4F01BF2C23245FA365EF83FE8F1F955085E2";

  string canonical_http_request;
  EXPECT_TRUE(amazon_http_request_util_.MakeCanonicalRequest(
      http_request, payload_sha256_digest, &canonical_http_request));

  EXPECT_EQ(kAmazonDocsExpectedCanonicalHttpRequest, canonical_http_request);
}

TEST_F(AmazonHttpRequestUtilTest, MakeCanonicalRequestWithPathAndParameters) {
  // An extension of the Amazon-provided test above, including a
  // nontrivial path and some query parameters. The expected value is
  // what I think it should be based on Amazon's docs. Sadly Amazon
  // does not provide a complete test case.

  HttpRequest http_request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "method: POST                                                          "
      "hostname: 'iam.amazonaws.com'                                         "
      "path: '/some/path/to/a/file.ext'                                      "
      "query_parameters <                                                    "
      "  key: 'foo'                                                          "
      "  value: 'bar'                                                        "
      ">                                                                     "
      "query_parameters <                                                    "
      "  key: 'Complicated $KEY$'                                            "
      "  value: 'More*** (complicated!) +=%40PArAmeter^^^;&&???\\012\\t1~23' "
      ">                                                                     "
      "request_headers <                                                     "
      "  key: 'Content-type'                                                 "
      "  value: 'application/x-www-form-urlencoded; charset=utf-8'           "
      ">                                                                     "
      "request_headers <                                                     "
      "  key: 'x-amz-date'                                                   "
      "  value: '20110909T233600Z'                                           "
      ">                                                                     ",
      &http_request));

  // Intentionally in uppercase to test that it is converted to lowercase.
  const char payload_sha256_digest[] =
      "B6359072C78D70EBEE1E81ADCBAB4F01BF2C23245FA365EF83FE8F1F955085E2";

  string canonical_http_request;
  EXPECT_TRUE(amazon_http_request_util_.MakeCanonicalRequest(
      http_request, payload_sha256_digest, &canonical_http_request));

  EXPECT_EQ(
      "POST\n"
      "/some/path/to/a/file.ext\n"
      "Complicated%20%24KEY%24=More%2A%2A%2A%20%28complicated%21%29%20%2B%3D%25"
      "40PArAmeter%5E%5E%5E%3B%26%26%3F%3F%3F%0A%091~23&foo=bar\n"
      "content-type:application/x-www-form-urlencoded; charset=utf-8\n"
      "host:iam.amazonaws.com\n"
      "x-amz-date:20110909T233600Z\n"
      "\n"
      "content-type;host;x-amz-date\n"
      "b6359072c78d70ebee1e81adcbab4f01bf2c23245fa365ef83fe8f1f955085e2",
      canonical_http_request);
}

TEST_F(AmazonHttpRequestUtilTest, MakeSigningString) {
  // Test case is from Amazon's documentation at:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html

  string signing_string;
  EXPECT_TRUE(amazon_http_request_util_.MakeSigningString(
      "us-east-1", "iam", "20110909T233600Z",
      kAmazonDocsExpectedCanonicalHttpRequest, &signing_string));

  EXPECT_EQ(kAmazonDocsExpectedSigningString, signing_string);
}

TEST_F(AmazonHttpRequestUtilTest, MakeDerivedSigningKey) {
  // Test case is from Amazon's documentation at:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  const byte kAwsSecretKey[] = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";

  // Be sure not to include the trailing NUL in the block.
  const CryptoPP::SecByteBlock kAwsSecretKeyBlock =
      CryptoPP::SecByteBlock(kAwsSecretKey, sizeof(kAwsSecretKey) - 1);

  CryptoPP::SecByteBlock derived_signing_key;
  EXPECT_TRUE(amazon_http_request_util_.MakeDerivedSigningKey(
      kAwsSecretKeyBlock, "us-east-1", "iam", "20110909T233600Z",
      &derived_signing_key));

  vector<byte> derived_signing_key_vec;
  std::copy(derived_signing_key.begin(), derived_signing_key.end(),
            std::back_inserter(derived_signing_key_vec));
  EXPECT_THAT(derived_signing_key_vec,
              ContainerEq(amazon_docs_expected_derived_signing_key_vec_))
      << "HexEncoded Actual: " << HexEncode(derived_signing_key_vec) << "\n"
      << "HexEncoded Expected: "
      << HexEncode(amazon_docs_expected_derived_signing_key_vec_);
}

TEST_F(AmazonHttpRequestUtilTest, MakeSignature) {
  // Test case is from Amazon's documentation at:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  const CryptoPP::SecByteBlock kDerivedSigningKeyBlock =
      CryptoPP::SecByteBlock(
          amazon_docs_expected_derived_signing_key_vec_.data(),
          amazon_docs_expected_derived_signing_key_vec_.size());
  EXPECT_EQ(
      "ced6826de92d2bdeed8f846f0bf508e8559e98e4b0199114b84c54174deb456c",
      amazon_http_request_util_.MakeSignature(
          kDerivedSigningKeyBlock, kAmazonDocsExpectedSigningString));
}

}  // namespace
}  // namespace polar_express
