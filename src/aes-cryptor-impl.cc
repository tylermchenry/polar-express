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
    const CryptoPP::SecByteBlock& key, boost::shared_ptr<vector<byte> > iv) {
  vector<byte> tmp_iv(iv_length());
  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(tmp_iv.data(), tmp_iv.size());

  assert(key.size() == key_length());
  aes_cfb_encryption_.reset(
      new CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption(
          key, key.size(), tmp_iv.data()));

  if (iv != nullptr) {
    *iv = tmp_iv;
  }
}

void AesCryptorImpl::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  assert(aes_cfb_encryption_ != nullptr);
  assert(data != nullptr);

  aes_cfb_encryption_->ProcessData(data->data(), data->data(), data->size());
  callback();
}

}  // namespace polar_express
