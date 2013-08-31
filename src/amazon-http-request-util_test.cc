#include "amazon-http-request-util.h"

#include <string>

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "proto/http.pb.h"

namespace polar_express {
namespace {

// This is the canonical request used for the examples in Amazon's
// documentation. It is used in test cases below that are derived from
// examples in the documentation.
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


class AmazonHttpRequestUtilTest : public testing::Test {
 protected:
  AmazonHttpRequestUtil amazon_http_request_util_;
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

  EXPECT_EQ(
      "AWS4-HMAC-SHA256\n"
      "20110909T233600Z\n"
      "20110909/us-east-1/iam/aws4_request\n"
      "3511de7e95d28ecd39e9513b642aee07e54f4941150d8df8bf94b328ef7e55e2",
      signing_string);
}

}  // namespace
}  // namespace polar_express
