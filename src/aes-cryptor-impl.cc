#include "aes-cryptor-impl.h"

#include "crypto++/aes.h"
#include "crypto++/osrng.h"
#include "crypto++/pwdbased.h"
#include "crypto++/sha.h"

namespace polar_express {
namespace {

// 32 bytes = 256 bits
const size_t kAesKeyLength = 32;
const size_t kShaHmacKeyLength = 32;
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
  encrypted_file_headers_.reset(new EncryptedFileHeaders);

  vector<byte> encryption_salt;
  vector<byte> authentication_salt;
  GenerateRandomSalts(&encryption_salt, &authentication_salt);

  if (!keying_data.secret_key.empty()) {
    DeriveKeysFromSecretKey(keying_data.secret_key, encryption_salt,
                            authentication_salt);
  } else {
    DeriveKeysFromPassphrase(keying_data.passphrase, encryption_salt,
                             authentication_salt);
  }

  vector<byte> iv(key_length());
  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(iv.data(), iv.size());
  encrypted_file_headers_->SetEncryptionAes256Cbf(iv);

  aes_cfb_encryption_.reset(
      new CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption(
          *encryption_key_, encryption_key_->size(), iv.data()));
}

void AesCryptorImpl::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  assert(aes_cfb_encryption_ != nullptr);
  assert(data != nullptr);

  aes_cfb_encryption_->ProcessData(data->data(), data->data(), data->size());
  // TODO: Update HMAC.

  callback();
}

void AesCryptorImpl::FinalizeEncryption(
    vector<byte>* encrypted_file_header_block,
    vector<byte>* message_authentication_code) {
  assert(encrypted_file_headers_ != nullptr);

  encrypted_file_headers_->GetHeaderBlock(encrypted_file_header_block);
  // TODO: Produce HMAC.
}

void AesCryptorImpl::DeriveKeysFromSecretKey(
    const CryptoPP::SecByteBlock& secret_key,
    const vector<byte>& encryption_salt,
    const vector<byte>& authentication_salt) {
  encryption_key_.reset(new CryptoPP::SecByteBlock(kAesKeyLength));
  Sha256Hkdf(secret_key, encryption_salt, encryption_key_.get());

  authentication_key_.reset(new CryptoPP::SecByteBlock(kAesKeyLength));
  Sha256Hkdf(secret_key, authentication_salt, authentication_key_.get());

  encrypted_file_headers_->SetKeyDerivationPexSha256Hkdf(
      encryption_salt, authentication_salt);
}

void AesCryptorImpl::DeriveKeysFromPassphrase(
    const CryptoPP::SecByteBlock& passphrase,
    const vector<byte>& encryption_salt,
    const vector<byte>& authentication_salt) {
  const int kPbkdf2Iterations = 100000;
  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

  encryption_key_.reset(new CryptoPP::SecByteBlock(kAesKeyLength));
  pbkdf2.DeriveKey(
      *encryption_key_, encryption_key_->size(),
      0x00,  // not used
      passphrase.data(), passphrase.size(),
      encryption_salt.data(), encryption_salt.size(),
      kPbkdf2Iterations);

  authentication_key_.reset(new CryptoPP::SecByteBlock(kAesKeyLength));
  pbkdf2.DeriveKey(
      *authentication_key_, authentication_key_->size(),
      0x00,  // not used
      passphrase.data(), passphrase.size(),
      authentication_salt.data(), authentication_salt.size(),
      kPbkdf2Iterations);

  encrypted_file_headers_->SetKeyDerivationPbkdf2(
      kPbkdf2Iterations, encryption_salt, authentication_salt);
}

void AesCryptorImpl::GenerateRandomSalts(
    vector<byte>* encryption_salt, vector<byte>* authentication_salt) const {
  vector<byte> random_salt(kKeyDerivationSaltLength);
  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(random_salt.data(), random_salt.size());

  *CHECK_NOTNULL(encryption_salt) = random_salt;
  *CHECK_NOTNULL(authentication_salt) = random_salt;

  for (int i = 0; i < random_salt.size(); ++i) {
    if (i < strlen(EncryptedFileHeaders::kEncryptionTypeIdAes256Cbf)) {
      (*encryption_salt)[i] =
          ((*encryption_salt)[i] ^
           EncryptedFileHeaders::kEncryptionTypeIdAes256Cbf[i]);
    }
    if (i < strlen(EncryptedFileHeaders::kMacTypeIdSha256)) {
      (*authentication_salt)[i] =
          ((*authentication_salt)[i] ^
           EncryptedFileHeaders::kMacTypeIdSha256[i]);
    }
  }
}

void AesCryptorImpl::Sha256Hkdf(const CryptoPP::SecByteBlock& secret_key,
                                const vector<byte>& salt,
                                CryptoPP::SecByteBlock* derived_key) const {
  // TODO: Implement
}

}  // namespace polar_express
