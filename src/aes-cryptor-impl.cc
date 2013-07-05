#include "aes-cryptor-impl.h"

#include <algorithm>
#include <iterator>

namespace polar_express {

AesCryptorImpl::AesCryptorImpl() {
}

AesCryptorImpl::~AesCryptorImpl() {
}

Cryptor::EncryptionType AesCryptorImpl::encryption_type() const {
  return Cryptor::EncryptionType::kAES;
}

void AesCryptorImpl::InitializeEncryption(const string& key) {
  // TODO(tylermchenry): Implement.
}

void AesCryptorImpl::EncryptData(
    const vector<char>& data, string* encrypted_data,
    Callback callback) {
  // TODO(tylermchenry): Implement.
  callback();
}

void AesCryptorImpl::FinalizeEncryption(string* encrypted_data) {
  // TODO(tylermchenry): Implement.
}

}  // namespace polar_express
