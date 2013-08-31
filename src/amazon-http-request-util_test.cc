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

// These are the AWS secret key, access key, request, canonical
// request, services, timestamp, signing string, derived signing key,
// and signature used for the examples in Amazon's documentation. They
// are used in test cases below that are derived from examples in the
// documentation.
const byte kAmazonDocsExampleAwsSecretKey[] =
    "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";

const char kAmazonDocsExampleAwsAccessKey[] = "AKIDEXAMPLE";

const char kAmazonDocsExampleRequestAsTextProto[] =
    "method: POST                                                "
    "hostname: 'iam.amazonaws.com'                               "
    "path: '/'                                                   "
    "request_headers <                                           "
    "  key: 'Content-type'                                       "
    "  value: 'application/x-www-form-urlencoded; charset=utf-8' "
    ">                                                           ";

// Intentionally in uppercase to test that it is converted to lowercase.
const char kAmazonDocsExamplePayloadSha256Digest[] =
    "B6359072C78D70EBEE1E81ADCBAB4F01BF2C23245FA365EF83FE8F1F955085E2";

const char kAmazonDocsExampleCanonicalTimestamp[] = "20110909T233600Z";

const char kAmazonDocsExampleAwsRegionName[] = "us-east-1";

const char kAmazonDocsExampleAwsServiceName[] = "iam";

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

const char kAmazonDocsExpectedSignature[] =
    "ced6826de92d2bdeed8f846f0bf508e8559e98e4b0199114b84c54174deb456c";

const char kAmazonDocsExpectedAuthorizationHeaderValue[] =
    "AWS4-HMAC-SHA256 "
    "Credential=AKIDEXAMPLE/20110909/us-east-1/iam/aws4_request, "
    "SignedHeaders=content-type;host;x-amz-date, "
    "Signature="
    "ced6826de92d2bdeed8f846f0bf508e8559e98e4b0199114b84c54174deb456c";

// No friendly proto matcher in GoogleMock? Lame.
MATCHER_P(EqualsProto, message, "") {
  string expected_serialized;
  string actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// A partial fake of the request util that provides the documentation
// example canonical timestamp instead of one for the current time.
class FakeTimestampAmazonHttpRequestUtil : public AmazonHttpRequestUtil {
 public:
  FakeTimestampAmazonHttpRequestUtil() {}
  virtual ~FakeTimestampAmazonHttpRequestUtil() {}

  virtual string GetCanonicalTimestamp() const {
    return kAmazonDocsExampleCanonicalTimestamp;
  }
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
      :  // Be sure not to include the trailing NUL in the key block.
      amazon_docs_example_aws_secret_key_block_(
          kAmazonDocsExampleAwsSecretKey,
          sizeof(kAmazonDocsExampleAwsSecretKey) - 1),
      amazon_docs_expected_derived_signing_key_vec_(
          kAmazonDocsExpectedDerivedSigningKey,
          kAmazonDocsExpectedDerivedSigningKey +
          sizeof(kAmazonDocsExpectedDerivedSigningKey)) {
  }

 protected:
  const FakeTimestampAmazonHttpRequestUtil amazon_http_request_util_;
  const CryptoPP::SecByteBlock amazon_docs_example_aws_secret_key_block_;
  const vector<byte> amazon_docs_expected_derived_signing_key_vec_;
};

TEST_F(AmazonHttpRequestUtilTest, MakeCanonicalRequest) {
  // The test input and expected output is taken from the
  // Amazon documentation:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html

  HttpRequest http_request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kAmazonDocsExampleRequestAsTextProto, &http_request));

  auto* timestamp_header = http_request.add_request_headers();
  timestamp_header->set_key("x-amz-date");
  timestamp_header->set_value(kAmazonDocsExampleCanonicalTimestamp);

  string canonical_http_request;
  EXPECT_TRUE(amazon_http_request_util_.MakeCanonicalRequest(
      http_request, kAmazonDocsExamplePayloadSha256Digest,
      &canonical_http_request));

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

  string canonical_http_request;
  EXPECT_TRUE(amazon_http_request_util_.MakeCanonicalRequest(
      http_request, kAmazonDocsExamplePayloadSha256Digest,
      &canonical_http_request));

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
      kAmazonDocsExampleAwsRegionName,
      kAmazonDocsExampleAwsServiceName,
      kAmazonDocsExampleCanonicalTimestamp,
      kAmazonDocsExpectedCanonicalHttpRequest,
      &signing_string));

  EXPECT_EQ(kAmazonDocsExpectedSigningString, signing_string);
}

TEST_F(AmazonHttpRequestUtilTest, MakeDerivedSigningKey) {
  // Test case is from Amazon's documentation at:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  CryptoPP::SecByteBlock derived_signing_key;
  EXPECT_TRUE(amazon_http_request_util_.MakeDerivedSigningKey(
      amazon_docs_example_aws_secret_key_block_,
      kAmazonDocsExampleAwsRegionName,
      kAmazonDocsExampleAwsServiceName,
      kAmazonDocsExampleCanonicalTimestamp,
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
      kAmazonDocsExpectedSignature,
      amazon_http_request_util_.MakeSignature(
          kDerivedSigningKeyBlock, kAmazonDocsExpectedSigningString));
}

TEST_F(AmazonHttpRequestUtilTest, AuthorizeRequest) {
  // This test case is from Amazon's documentation at:
  // http://docs.aws.amazon.com/general/latest/gr/sigv4-signed-request-examples.html

  HttpRequest http_request;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kAmazonDocsExampleRequestAsTextProto, &http_request));

  HttpRequest expected_authorized_http_request = http_request;

  auto* timestamp_header =
      expected_authorized_http_request.add_request_headers();
  timestamp_header->set_key("x-amz-date");
  timestamp_header->set_value(kAmazonDocsExampleCanonicalTimestamp);

  auto* authorized_header =
      expected_authorized_http_request.add_request_headers();
  authorized_header->set_key("Authorization");
  authorized_header->set_value(kAmazonDocsExpectedAuthorizationHeaderValue);

  EXPECT_TRUE(amazon_http_request_util_.AuthorizeRequest(
      amazon_docs_example_aws_secret_key_block_,
      kAmazonDocsExampleAwsAccessKey, kAmazonDocsExampleAwsRegionName,
      kAmazonDocsExampleAwsServiceName, kAmazonDocsExamplePayloadSha256Digest,
      &http_request));

  EXPECT_THAT(http_request, EqualsProto(expected_authorized_http_request))
      << "Actual Proto:\n" << http_request.DebugString()
      << "Expected Proto:\n" << expected_authorized_http_request.DebugString();
}

}  // namespace
}  // namespace polar_express
