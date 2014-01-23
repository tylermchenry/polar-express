#include "network/tcp-connection.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include <boost/bind/bind.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/asio-dispatcher.h"

namespace polar_express {
namespace {

class TcpConnectionTest : public testing::Test {
 public:
  TcpConnectionTest()
      : open_callback_invoked_(false),
        expect_open_(false),
        write_callback_invoked_(false),
        expect_write_success_(false),
        read_callback_invoked_(false),
        expect_read_success_(false),
        tcp_connection_(new TcpConnection) {
  }

  virtual void SetUp() {
    AsioDispatcher::GetInstance()->Start();
  }

 protected:
  void WaitForFinish() {
    AsioDispatcher::GetInstance()->WaitForFinish();
  }

  void DestroyConnection() {
      tcp_connection_.reset();
  }

  void WriteData(
      const vector<byte>* data, bool expect_success, Callback next_action) {
    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    ASSERT_TRUE(data != nullptr);

    bool success = tcp_connection_->Write(data, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void WriteAllData(
      const vector<const vector<byte>*>* sequential_data,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    ASSERT_TRUE(sequential_data != nullptr);

    bool success = tcp_connection_->WriteAll(*sequential_data, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void ReadUntilData(
      const vector<byte>& terminator_bytes, vector<byte>* data,
      bool expect_success, Callback next_action) {
    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    ASSERT_TRUE(data != nullptr);

    bool success = tcp_connection_->ReadUntil(
        terminator_bytes, data, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void ReadSizeData(
      size_t max_data_size, vector<byte>* data, bool expect_success,
      Callback next_action) {
    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    ASSERT_TRUE(data != nullptr);

    bool success = tcp_connection_->ReadSize(max_data_size, data, next_action);
    EXPECT_EQ(expect_success, success);

    if (!success) {
      next_action();
    }
  }

  void Close(bool expect_success, Callback next_action) {
    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    EXPECT_EQ(expect_success, tcp_connection_->Close());
    if (expect_success) {
      EXPECT_FALSE(tcp_connection_->is_opening());
      EXPECT_FALSE(tcp_connection_->is_open());
    }
    next_action();
  }

  Callback GetOpenCallback(Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::open_callback_function, this, next_action);
  }

  Callback GetWriteCallback(Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::write_callback_function, this, next_action);
  }

  Callback GetReadCallback(Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::read_callback_function, this, next_action);
  }

  Callback GetDestroyConnectionAction() {
    return boost::bind(&TcpConnectionTest::DestroyConnection, this);
  }

  Callback GetWriteDataAction(
      const vector<byte>* data, bool expect_success, Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::WriteData, this, data, expect_success, next_action);
  }

  Callback GetWriteAllDataAction(
      const vector<const vector<byte>*>* sequential_data,
      bool expect_success, Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::WriteAllData,
        this, sequential_data, expect_success, next_action);
  }

  Callback GetReadUntilDataAction(
      const vector<byte>& terminator_bytes, vector<byte>* data,
      bool expect_success, Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::ReadUntilData,
        this, terminator_bytes, data, expect_success, next_action);
  }

  Callback GetReadSizeDataAction(
      size_t max_data_size, vector<byte>* data, bool expect_success,
      Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::ReadSizeData,
        this, max_data_size, data, expect_success, next_action);
  }

  Callback GetCloseAction(bool expect_success, Callback next_action) {
    return boost::bind(
        &TcpConnectionTest::Close, this, expect_success, next_action);
  }

  bool open_callback_invoked_;
  bool expect_open_;

  bool write_callback_invoked_;
  bool expect_write_success_;

  bool read_callback_invoked_;
  bool expect_read_success_;

  unique_ptr<TcpConnection> tcp_connection_;

 private:
  void open_callback_function(Callback next_action) {
    EXPECT_FALSE(open_callback_invoked_);
    open_callback_invoked_ = true;

    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    EXPECT_FALSE(tcp_connection_->is_opening());
    EXPECT_EQ(expect_open_, tcp_connection_->is_open());

    next_action();
  }

  void write_callback_function(Callback next_action) {
    EXPECT_FALSE(write_callback_invoked_);
    write_callback_invoked_ = true;

    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    EXPECT_EQ(expect_write_success_, tcp_connection_->last_write_succeeded());

    next_action();
  }

  void read_callback_function(Callback next_action) {
    EXPECT_FALSE(read_callback_invoked_);
    read_callback_invoked_ = true;

    ASSERT_TRUE(tcp_connection_.get() != nullptr);
    EXPECT_EQ(expect_read_success_, tcp_connection_->last_read_succeeded());

    next_action();
  }
};

TEST_F(TcpConnectionTest, Open) {
  expect_open_ = true;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http", GetOpenCallback(GetDestroyConnectionAction())));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenBadUsageType) {
  expect_open_ = true;

  EXPECT_FALSE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kInvalid,
      "www.google.com", "http", GetOpenCallback(GetDestroyConnectionAction())));

  EXPECT_FALSE(tcp_connection_->is_opening());
  EXPECT_FALSE(tcp_connection_->is_open());
  tcp_connection_.reset();

  WaitForFinish();
  EXPECT_FALSE(open_callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenResolutionFailure) {
  expect_open_ = false;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "foo.bar.baz", "http", GetOpenCallback(GetDestroyConnectionAction())));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenAndWrite) {
  expect_open_ = true;
  expect_write_success_ = true;

  const char kRequest[] = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
  vector<byte> request_data(kRequest, kRequest + sizeof(kRequest) - 1);

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http",
      GetOpenCallback(
          GetWriteDataAction(
              &request_data, true,
              GetWriteCallback(GetDestroyConnectionAction())))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(write_callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenAndWriteAll) {
  expect_open_ = true;
  expect_write_success_ = true;

  const char kRequest1[] = "GET / HTTP/1.1\r\n";
  const char kRequest2[] = "Host: www.google.com\r\n";
  const char kRequest3[] = "\r\n";

  vector<byte> request_data1(kRequest1, kRequest1 + sizeof(kRequest1) - 1);
  vector<byte> request_data2(kRequest2, kRequest2 + sizeof(kRequest2) - 1);
  vector<byte> request_data3(kRequest3, kRequest3 + sizeof(kRequest3) - 1);

  vector<const vector<byte>*> sequential_request_data = {
    &request_data1, &request_data2, &request_data3 };

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http",
      GetOpenCallback(
          GetWriteAllDataAction(
              &sequential_request_data, true,
              GetWriteCallback(GetDestroyConnectionAction())))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(write_callback_invoked_);
}

TEST_F(TcpConnectionTest, OpenAndWriteAndReadUntil) {
  expect_open_ = true;
  expect_write_success_ = true;
  expect_read_success_ = true;

  const char kRequest[] =
      "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
  vector<byte> request_data(kRequest, kRequest + sizeof(kRequest) - 1);

  vector<byte> termination_bytes = { '\r', '\n', '\r', '\n' };
  vector<byte> response_data;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http",
      GetOpenCallback(
          GetWriteDataAction(
              &request_data, true,
              GetWriteCallback(
                  GetReadUntilDataAction(
                      termination_bytes, &response_data, true,
                      GetReadCallback(GetDestroyConnectionAction())))))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(write_callback_invoked_);
  EXPECT_TRUE(read_callback_invoked_);

  string response_data_str;
  std::copy(response_data.begin(), response_data.end(),
            std::back_inserter(response_data_str));
  EXPECT_TRUE(response_data_str.find("HTTP/1.1 200 OK") != string::npos);
}

TEST_F(TcpConnectionTest, OpenAndWriteAndReadSize) {
  expect_open_ = true;
  expect_write_success_ = true;
  expect_read_success_ = false;  // Because we won't fill the whole buffer.

  const char kRequest[] =
      "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n";
  vector<byte> request_data(kRequest, kRequest + sizeof(kRequest) - 1);

  const size_t kMaxResponseSize = (1 << 20) * 5;  // 5 MiB
  vector<byte> response_data;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http",
      GetOpenCallback(
          GetWriteDataAction(
              &request_data, true,
              GetWriteCallback(
                  GetReadSizeDataAction(
                      kMaxResponseSize, &response_data, true,
                      GetReadCallback(GetDestroyConnectionAction())))))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
  EXPECT_TRUE(write_callback_invoked_);
  EXPECT_TRUE(read_callback_invoked_);

  string response_data_str;
  std::copy(response_data.begin(), response_data.end(),
            std::back_inserter(response_data_str));
  EXPECT_TRUE(response_data_str.find("HTTP/1.1 200 OK") != string::npos);
}

TEST_F(TcpConnectionTest, OpenAndClose) {
  expect_open_ = true;

  EXPECT_TRUE(tcp_connection_->Open(
      AsioDispatcher::NetworkUsageType::kDownlinkBound,
      "www.google.com", "http",
      GetOpenCallback(GetCloseAction(true, GetDestroyConnectionAction()))));

  WaitForFinish();
  EXPECT_TRUE(open_callback_invoked_);
}

TEST_F(TcpConnectionTest, Close) {
  // Close without open should be fine.
  Close(true, GetDestroyConnectionAction());
  WaitForFinish();
}

}  // namespace
}  // namespace polar_express
