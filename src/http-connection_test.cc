#include "http-connection.h"

#include <memory>
#include <vector>

#include "boost/bind/bind.hpp"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "asio-dispatcher.h"
#include "macros.h"
#include "callback.h"
#include "proto/http.pb.h"

namespace polar_express {
namespace {

// TODO: Uncomment when MATCHER_P is unborked.
// // No friendly proto matcher in GoogleMock? Lame.
// MATCHER_P(EqualsProto, message, "") {
//   string expected_serialized;
//   string actual_serialized;
//   message.SerializeToString(&expected_serialized);
//   arg.SerializeToString(&actual_serialized);
//   return expected_serialized == actual_serialized;
// }

class HttpConnectionTest : public testing::TestWithParam<bool> {
 public:
  HttpConnectionTest()
      : open_callback_invoked_(false),
        expect_open_(false),
        request_callback_invoked_(false),
        expect_request_success_(false),
        http_connection_(
            GetParam() ? static_cast<HttpConnection*>(new HttpsConnection)
                       : static_cast<HttpConnection*>(new HttpConnection)) {}

  virtual void SetUp() {
    AsioDispatcher::GetInstance()->Start();
  }

 protected:
  void WaitForFinish() {
    AsioDispatcher::GetInstance()->WaitForFinish();
  }

  void DestroyConnection() {
      http_connection_.reset();
  }

  void SendRequest(bool expect_success, Callback next_action) {
    ASSERT_TRUE(http_connection_.get() != nullptr);

    bool success = http_connection_->SendRequest(
        request_, &request_payload_, &response_, &response_payload_,
        next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  Callback GetOpenCallback(Callback next_action) {
    return boost::bind(
        &HttpConnectionTest::open_callback_function, this, next_action);
  }

  Callback GetRequestCallback(Callback next_action) {
    return boost::bind(
        &HttpConnectionTest::request_callback_function, this, next_action);
  }

  Callback GetSendRequestAction(bool expect_success, Callback next_action) {
    return boost::bind(
        &HttpConnectionTest::SendRequest, this, expect_success, next_action);
  }

  Callback GetDestroyConnectionAction() {
    return boost::bind(&HttpConnectionTest::DestroyConnection, this);
  }

  bool open_callback_invoked_;
  bool expect_open_;

  bool request_callback_invoked_;
  bool expect_request_success_;

  HttpRequest request_;
  vector<byte> request_payload_;
  HttpResponse response_;
  vector<byte> response_payload_;

  unique_ptr<HttpConnection> http_connection_;

 private:
  void open_callback_function(Callback next_action) {
    EXPECT_FALSE(open_callback_invoked_);
    open_callback_invoked_ = true;

    ASSERT_TRUE(http_connection_.get() != nullptr);
    EXPECT_FALSE(http_connection_->is_opening());
    EXPECT_EQ(expect_open_, http_connection_->is_open());

    next_action();
  }

  void request_callback_function(Callback next_action) {
    EXPECT_FALSE(request_callback_invoked_);
    request_callback_invoked_ = true;

    ASSERT_TRUE(http_connection_.get() != nullptr);
    EXPECT_EQ(expect_request_success_,
              http_connection_->last_request_succeeded());
    EXPECT_EQ(expect_request_success_, http_connection_->is_open());

    next_action();
  }
};

TEST_P(HttpConnectionTest, Open) {
  expect_open_ = true;

  EXPECT_TRUE(http_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", GetOpenCallback(GetDestroyConnectionAction())));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_P(HttpConnectionTest, OpenAndSendRequest) {
  expect_open_ = true;
  expect_request_success_ = true;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "method: GET                 "
      "hostname: 'www.google.com'  "
      "path: '/'                   "
      "query_parameters <          "
      "  key: 'foo'                "
      "  value: 'bar'              "
      ">                           "
      "query_parameters <          "
      "  key: 'ba z'               "
      "  value: 'Qu~x!x$'          "
      ">                           "
      "request_headers <           "
      "  key: 'X-Fake-Header'      "
      "  value: 'some value'       "
      ">                           ",
      &request_));

  EXPECT_TRUE(http_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", GetOpenCallback(
          GetSendRequestAction(
              true,
              GetRequestCallback(GetDestroyConnectionAction())))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(request_callback_invoked_);

  HttpResponse response_without_headers = response_;
  response_without_headers.clear_response_headers();

  HttpResponse expected_response_without_headers;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "transport_succeeded: true   "
      "http_version: '1.1'         "
      "is_secure: false            "
      "status_code: 200            "
      "status_phrase: 'OK'         ",
      &expected_response_without_headers));

  // TODO: Uncomment when MATCHER_P is unborked.
  // EXPECT_THAT(
  //     response_without_headers,
  //     EqualsProto(expected_response_without_headers));
  EXPECT_FALSE(response_payload_.empty());
}

INSTANTIATE_TEST_CASE_P(SecureAndInsecureTest, HttpConnectionTest,
                        testing::Bool());

}  // namespace
}  // namespace polar_express
