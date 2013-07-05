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
    const CryptoPP::SecByteBlock& key, const string& iv) {
  // No-op.
}

void NullCryptorImpl::EncryptData(
    const vector<char>& data, string* encrypted_data,
    Callback callback) {
  std::copy(data.begin(), data.end(),
            std::back_inserter(*CHECK_NOTNULL(encrypted_data)));
  callback();
}

void NullCryptorImpl::FinalizeEncryption(string* encrypted_data) {
  // No-op.
}

}  // namespace polar_express
