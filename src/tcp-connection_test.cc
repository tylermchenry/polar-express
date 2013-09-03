#include "tcp-connection.h"

#include <memory>

#include "boost/bind/bind.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "asio-dispatcher.h"

namespace polar_express {
namespace {

class TcpConnectionTest : public testing::Test {
 public:
  TcpConnectionTest()
      : callback_invoked_(false),
        expect_open_(false),
        tcp_connection_(new TcpConnection) {
  }

  virtual void SetUp() {
    AsioDispatcher::GetInstance()->Start();
  }

 protected:
  void WaitForFinish() {
    AsioDispatcher::GetInstance()->WaitForFinish();
  }

  Callback GetCallback() {
    return boost::bind(&TcpConnectionTest::callback_function, this);
  }

  bool callback_invoked_;
  bool expect_open_;
  unique_ptr<TcpConnection> tcp_connection_;

 private:
  void callback_function() {
    callback_invoked_ = true;

    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    EXPECT_FALSE(tcp_connection_->is_opening());
    EXPECT_EQ(expect_open_, tcp_connection_->is_open());

    tcp_connection_.reset();
  }
};

TEST_F(TcpConnectionTest, Open) {
  expect_open_ = true;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http", GetCallback()));

  WaitForFinish();
  EXPECT_TRUE(callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenBadUsageType) {
  expect_open_ = true;

  EXPECT_FALSE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kInvalid,
      "www.google.com", "http", GetCallback()));

  EXPECT_FALSE(tcp_connection_->is_opening());
  EXPECT_FALSE(tcp_connection_->is_open());
  tcp_connection_.reset();

  WaitForFinish();
  EXPECT_FALSE(callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenResolutionFailure) {
  expect_open_ = false;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "foo.bar.baz", "http", GetCallback()));

  WaitForFinish();
  EXPECT_TRUE(callback_invoked_);
}

}  // namespace
}  // namespace polar_express
