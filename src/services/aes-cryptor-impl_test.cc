#include "services/aes-cryptor-impl.h"

#include <boost/bind/bind.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {
namespace {

const byte kKey[] = {0x4D, 0x9D, 0xF9, 0xFB, 0x1C, 0xF0, 0x9B, 0x2E, 0x0C, 0x7D,
                     0xF7, 0x86, 0x1B, 0x0D, 0xB3, 0x0D, 0x4E, 0x55, 0xEA, 0xE4,
                     0x74, 0x65, 0x0D, 0xF5, 0x65, 0xD8, 0x4C, 0x5C, 0x47, 0x24,
                     0xDD, 0xAF};

const byte kIv[] = {0xC9, 0xB1, 0x0E, 0xC5, 0x7A, 0xF7, 0x51, 0x8F, 0xEF, 0x2A,
                    0x01, 0x09, 0x1F, 0x1B, 0x54, 0xA6};

class AesCryptorImplTest : public testing::Test {
 public:
  void CryptorCallback() {}

 protected:
  virtual void SetUp() {
    data_.reset(new vector<byte>);
    keying_data_.encryption_key.reset(new CryptoPP::SecByteBlock);
    keying_data_.encryption_key->Assign(kKey, sizeof(kKey));
    keying_data_.key_derivation_type_id =
        EncryptedFileHeaders::kKeyDerivationTypeIdNone;
    iv_.assign(kIv, kIv + sizeof(kIv));
    aes_cryptor_.InitializeEncryptionWithInitializationVector(
        keying_data_, iv_);
    AsioDispatcher::GetInstance()->Start();
  }

  void WaitForFinish() {
    AsioDispatcher::GetInstance()->WaitForFinish();
  }

  // TODO: Class should support decryption.
  void DecryptData() {
    CryptoPP::GCM<CryptoPP::AES>::Decryption d;
    d.SetKeyWithIV(keying_data_.encryption_key->data(),
                   keying_data_.encryption_key->size(),
                   iv_.data(), iv_.size());
    d.ProcessData(data_->data(), data_->data(), data_->size());
  }

  AesCryptorImpl aes_cryptor_;
  boost::shared_ptr<vector<byte> > data_;

 private:
  Cryptor::KeyingData keying_data_;
  vector<byte> iv_;
};

TEST_F(AesCryptorImplTest, SimpleRoundTrip) {
  const char kPlaintext[] = "This is a test.";
  const byte kExpectedCiphertext[] = {
    0xa6, 0xfd, 0xfe, 0x16, 0xff, 0x37, 0xe5, 0x09,
    0x1f, 0x40, 0x60, 0x6f, 0x67, 0xde, 0x4d
  };

  data_->assign(kPlaintext, kPlaintext + sizeof(kPlaintext) - 1);
  aes_cryptor_.EncryptData(
      data_, boost::bind(&AesCryptorImplTest::CryptorCallback, this));
  WaitForFinish();

  vector<byte> expected_ciphertext;
  expected_ciphertext.assign(kExpectedCiphertext,
                             kExpectedCiphertext + sizeof(kExpectedCiphertext));
  EXPECT_THAT(*data_, testing::ContainerEq(expected_ciphertext));

  DecryptData();
  vector<byte> expected_plaintext;
  expected_plaintext.assign(kPlaintext, kPlaintext + sizeof(kPlaintext) - 1);
  EXPECT_THAT(*data_, testing::ContainerEq(expected_plaintext));
}

}  // namespace
}  // namespace polar_express
