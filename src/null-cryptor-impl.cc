#include "null-cryptor-impl.h"

#include "make-unique.h"

#include <algorithm>
#include <iterator>

namespace polar_express {

NullCryptorImpl::NullCryptorImpl() {
}

NullCryptorImpl::~NullCryptorImpl() {
}

Cryptor::EncryptionType NullCryptorImpl::encryption_type() const {
  return Cryptor::EncryptionType::kNone;
}

size_t NullCryptorImpl::key_length() const {
  return 0;
}

size_t NullCryptorImpl::iv_length() const {
  return 0;
}

void NullCryptorImpl::InitializeEncryption(
    const CryptoPP::SecByteBlock& key, boost::shared_ptr<vector<byte> > iv) {
  // No-op.
}

void NullCryptorImpl::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  // No-op.
  callback();
}

}  // namespace polar_express
