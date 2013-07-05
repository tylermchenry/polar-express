#include "null-cryptor-impl.h"

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

void NullCryptorImpl::InitializeEncryption(const string& key) {
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
