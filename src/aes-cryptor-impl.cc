#include "aes-cryptor-impl.h"

#include "crypto++/osrng.h"

namespace polar_express {
namespace {

// TODO(tylermchenry): Should be configurable.
const size_t kAesKeyLength = CryptoPP::AES::MAX_KEYLENGTH;

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

size_t AesCryptorImpl::iv_length() const {
  return CryptoPP::AES::BLOCKSIZE;
}

void AesCryptorImpl::InitializeEncryption(
    const CryptoPP::SecByteBlock& key, const string& iv) {
  iv_.resize(iv_length());
  if (iv.empty()) {
    CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
    rng.GenerateBlock(iv_.data(), iv_.size());
  } else {
    assert(iv.size() == iv.length());
    std::copy(iv.begin(), iv.end(), iv_.begin());
  }

  assert(key.size() == key_length());
  aes_cfb_encryption_.reset(
      new CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption(
          key, key.size(), iv_.data()));
}

void AesCryptorImpl::EncryptData(
    const vector<char>& data, string* encrypted_data,
    Callback callback) {
  assert(aes_cfb_encryption_ != nullptr);
  assert(encrypted_data != nullptr);

  if (!iv_.empty()) {
    encrypted_data->append(iv_.begin(), iv_.end());
    iv_.clear();
  }

  // TODO(tylermchenry): This is horribly inefficient. Get rid of
  // strings holding bytes and use vector<byte> everywhere.
  vector<byte> tmp_encrypted_data(data.begin(), data.end());
  aes_cfb_encryption_->ProcessData(
      tmp_encrypted_data.data(),
      tmp_encrypted_data.data(), tmp_encrypted_data.size());
  encrypted_data->append(tmp_encrypted_data.begin(),
                         tmp_encrypted_data.end());
  callback();
}

void AesCryptorImpl::FinalizeEncryption(string* encrypted_data) {
  if (!iv_.empty()) {
    CHECK_NOTNULL(encrypted_data)->append(iv_.begin(), iv_.end());
    iv_.clear();
  }
}

}  // namespace polar_express
