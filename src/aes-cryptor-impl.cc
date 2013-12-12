#include "aes-cryptor-impl.h"

#include "crypto++/aes.h"
#include "crypto++/osrng.h"
#include "crypto++/pwdbased.h"
#include "crypto++/sha.h"

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
  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  encrypted_file_headers_.reset(new EncryptedFileHeaders);

  if (!keying_data.secret_key.empty()) {
    encryption_key_.reset(new CryptoPP::SecByteBlock(keying_data.secret_key));
  } else {
    vector<byte> salt(kKeyDerivationSaltLength);
    rng.GenerateBlock(salt.data(), salt.size());
    DeriveKeyFromPassphrase(keying_data.passphrase, salt);
  }

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

void AesCryptorImpl::DeriveKeyFromPassphrase(
    const CryptoPP::SecByteBlock& passphrase, const vector<byte>& salt) {
  const uint8_t kPbkdf2IterationsExponent = 17;
  const int kPbkdf2Iterations = 1 << kPbkdf2IterationsExponent;  // About 13k
  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

  encryption_key_.reset(new CryptoPP::SecByteBlock(kAesKeyLength));
  pbkdf2.DeriveKey(
      *encryption_key_, encryption_key_->size(),
      0x00,  // not used
      passphrase.data(), passphrase.size(),
      salt.data(), salt.size(),
      kPbkdf2Iterations);

  encrypted_file_headers_->SetKeyDerivationPbkdf2(
      kPbkdf2IterationsExponent, salt, vector<byte>(salt.size(), '\0'));
}

}  // namespace polar_express
