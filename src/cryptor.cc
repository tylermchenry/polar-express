#include "cryptor.h"

#include "crypto++/osrng.h"
#include "crypto++/pwdbased.h"

#include "asio-dispatcher.h"
#include "encrypted-file-headers.h"
#include "make-unique.h"
#include "null-cryptor-impl.h"
#include "aes-cryptor-impl.h"

namespace polar_express {
namespace {

const uint8_t kPbkdf2IterationsExponent = 20;

}  // namespace

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

// static
void Cryptor::DeriveKeysFromMasterKey(
    const boost::shared_ptr<CryptoPP::SecByteBlock> master_key,
    const EncryptionType encryption_type,
    KeyingData* keying_data) {
  assert(keying_data != nullptr);
  const size_t kKeyLength = CreateCryptor(encryption_type)->key_length();

  // Using the same key for encryption and MAC is not safe, but no
  // currently-supported authentication modes in this program actually use a
  // MAC. However even without the need for a MAC, it would still be nice to use
  // a derived key for encryption so that the master key does not need to be
  // kept in memory the entire time that the program is running.
  //
  // TODO: Introduce a key-derivation function for producing two keys from a
  // master.
  keying_data->encryption_key = master_key;
  keying_data->mac_key = master_key;
  keying_data->key_derivation_type_id =
      EncryptedFileHeaders::kKeyDerivationTypeIdNone;
}

// static
void Cryptor::DeriveKeysFromPassphrase(
    const boost::shared_ptr<CryptoPP::SecByteBlock> passphrase,
    const EncryptionType encryption_type,
    KeyingData* keying_data) {
  assert(keying_data != nullptr);
  const size_t kKeyLength = CreateCryptor(encryption_type)->key_length();

  keying_data->encryption_key.reset(new CryptoPP::SecByteBlock(kKeyLength));
  keying_data->mac_key.reset(new CryptoPP::SecByteBlock(kKeyLength));

  if (kKeyLength == 0) {
    keying_data->key_derivation_type_id =
        EncryptedFileHeaders::kKeyDerivationTypeIdNone;
    return;
  }

  keying_data->key_derivation_type_id =
      EncryptedFileHeaders::kKeyDerivationTypeIdPbkdf2;
  keying_data->pbkdf2_iterations_exponent = kPbkdf2IterationsExponent;

  keying_data->pbkdf2_encryption_key_salt.reset(new vector<byte>(kKeyLength));
  DeriveKeyPbkdf2(passphrase, keying_data->pbkdf2_encryption_key_salt.get(),
                  keying_data->encryption_key.get());

  keying_data->pbkdf2_mac_key_salt.reset(new vector<byte>(kKeyLength));
  DeriveKeyPbkdf2(passphrase, keying_data->pbkdf2_mac_key_salt.get(),
                  keying_data->mac_key.get());
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

size_t Cryptor::key_length() const {
  return impl_->key_length();
}

void Cryptor::InitializeEncryption(const KeyingData& keying_data) {
  impl_->InitializeEncryption(keying_data);
}

void Cryptor::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Cryptor::EncryptData, impl_.get(), data, callback));
}

void Cryptor::FinalizeEncryption(vector<byte>* encrypted_file_header_block,
                                 vector<byte>* message_authentication_code) {
  impl_->FinalizeEncryption(encrypted_file_header_block,
                            message_authentication_code);
}

void Cryptor::SetKeyDerivationHeaders(
    const KeyingData& keying_data,
    EncryptedFileHeaders* encrypted_file_headers) const {
  assert(encrypted_file_headers != nullptr);

  if (keying_data.key_derivation_type_id ==
      EncryptedFileHeaders::kKeyDerivationTypeIdNone) {
    encrypted_file_headers->SetKeyDerivationNone();
  } else if (keying_data.key_derivation_type_id ==
             EncryptedFileHeaders::kKeyDerivationTypeIdPbkdf2) {
    encrypted_file_headers->SetKeyDerivationPbkdf2(
        keying_data.pbkdf2_iterations_exponent,
        *CHECK_NOTNULL(keying_data.pbkdf2_encryption_key_salt),
        *CHECK_NOTNULL(keying_data.pbkdf2_mac_key_salt));
  } else {
    assert(false);
  }
}

// static
void Cryptor::DeriveKeyPbkdf2(
    const boost::shared_ptr<CryptoPP::SecByteBlock> passphrase,
    vector<byte>* salt, CryptoPP::SecByteBlock* derived_key) {
  assert(CHECK_NOTNULL(salt)->size() == CHECK_NOTNULL(derived_key)->size());
  const int kPbkdf2Iterations = 1 << kPbkdf2IterationsExponent;

  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(salt->data(), salt->size());

  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
  pbkdf2.DeriveKey(
      *derived_key, derived_key->size(),
      0x00,  // not used
      CHECK_NOTNULL(passphrase)->data(), passphrase->size(),
      salt->data(), salt->size(),
      kPbkdf2Iterations);
}

}  // namespace polar_express
