#include "services/null-cryptor-impl.h"

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

void NullCryptorImpl::InitializeEncryption(
    const Cryptor::KeyingData& keying_Data) {
  // No-op.
}

void NullCryptorImpl::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  // No-op.
  callback();
}

void NullCryptorImpl::FinalizeEncryption(
    vector<byte>* encrypted_file_header_block,
    vector<byte>* message_authentication_code) {
  // No-op.
}

}  // namespace polar_express
