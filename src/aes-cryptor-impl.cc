#include "aes-cryptor-impl.h"

#include "crypto++/aes.h"
#include "crypto++/osrng.h"

namespace polar_express {
namespace {

// 32 bytes = 256 bits
const size_t kAesKeyLength = 32;
const size_t kKeyDerivationSaltLength = 32;

}  // namespace

AesCryptorImpl::AesCryptorImpl() {
}

AesCryptorImpl::~AesCryptorImpl() {
}

Cryptor::EncryptionType AesCryptorImpl::encryption_type() const {
  return Cryptor::EncryptionType::kAES;
}

size_t AesCryptorImpl::key_length() const {
  return kAesKeyLength;
}

void AesCryptorImpl::InitializeEncryption(
    const Cryptor::KeyingData& keying_data) {
  encryption_key_ = keying_data.encryption_key;
  assert(encryption_key_ != nullptr);
  assert(encryption_key_->size() == key_length());

  encrypted_file_headers_.reset(new EncryptedFileHeaders);
  SetKeyDerivationHeaders(keying_data, encrypted_file_headers_.get());

  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  vector<byte> iv(key_length());
  rng.GenerateBlock(iv.data(), iv.size());
  encrypted_file_headers_->SetEncryptionAes256Gcm(iv);

  // MAC is not necessary since GCM is an authenticated encryption mode.
  encrypted_file_headers_->SetMacNone();

  // TODO: Also authenticate the non-encrypted header data.

  aes_gcm_encryption_.reset(new CryptoPP::GCM<CryptoPP::AES>::Encryption);
  aes_gcm_encryption_->SetKeyWithIV(
      *encryption_key_, encryption_key_->size(), iv.data());
}

void AesCryptorImpl::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  assert(aes_gcm_encryption_ != nullptr);
  assert(data != nullptr);

  aes_gcm_encryption_->ProcessData(data->data(), data->data(), data->size());

  callback();
}

void AesCryptorImpl::FinalizeEncryption(
    vector<byte>* encrypted_file_header_block,
    vector<byte>* message_authentication_code) {
  assert(encrypted_file_headers_ != nullptr);
  encrypted_file_headers_->GetHeaderBlock(encrypted_file_header_block);
}

}  // namespace polar_express
