#include "cryptor.h"

#include "asio-dispatcher.h"
#include "null-cryptor-impl.h"
#include "aes-cryptor-impl.h"

namespace polar_express {

// static
template<typename CryptorImplT>
unique_ptr<Cryptor> Cryptor::CreateCryptorWithImpl() {
  return unique_ptr<Cryptor>(new Cryptor(
      unique_ptr<Cryptor>(new CryptorImplT)));
}

// static
unique_ptr<Cryptor> Cryptor::CreateCryptor(EncryptionType encryption_type) {
  // TODO(tylermchenry): If in there future there are a lot of
  // different encryption types, it may make sense to auto-register
  // cryptor implementation classes into a map based on the result
  // of their encryption_type() accessor.
  switch (encryption_type) {
    case EncryptionType::kNone:
      return CreateCryptorWithImpl<NullCryptorImpl>();
    case EncryptionType::kAES:
      return CreateCryptorWithImpl<AesCryptorImpl>();
    default:
      assert(false);
      return nullptr;
  }
}

Cryptor::Cryptor() {
}

Cryptor::Cryptor(unique_ptr<Cryptor>&& impl)
    : impl_(std::move(CHECK_NOTNULL(impl))) {
}

Cryptor::~Cryptor() {
}

Cryptor::EncryptionType Cryptor::encryption_type() const {
  return impl_->encryption_type();
}

void Cryptor::InitializeEncryption(const string& key) {
  impl_->InitializeEncryption(key);
}

void Cryptor::EncryptData(
    const vector<char>& data, string* encrypted_data,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Cryptor::EncryptData,
           impl_.get(), data, encrypted_data, callback));
}

void Cryptor::FinalizeEncryption(string* encrypted_data) {
  impl_->FinalizeEncryption(encrypted_data);
}

}  // namespace polar_express
