#include "services/cryptor.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include <crypto++/osrng.h>
#include <crypto++/pwdbased.h>

#include "base/asio-dispatcher.h"
#include "base/options.h"
#include "file/encrypted-file-headers.h"
#include "services/aes-cryptor-impl.h"
#include "services/null-cryptor-impl.h"

DEFINE_OPTION(encrypt_with_master_key, bool, false,
              "If true, the master key will be used directly for encrypting "
              "outgoing data. This means the master key must be in memory at "
              "all times. (Not relevant if using a passphrase.)");

namespace polar_express {
namespace {

const uint8_t kPbkdf2IterationsExponent = 20;

// Not using 'info' bytes currently. This has no effect on the security of the
// key derivation algorithm, see documentation in RFC 5869.
const byte kHkdfInfoSha256[] = {};

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
  // MAC.
  if (options::encrypt_with_master_key) {
    // TODO: Mismatching key size should be a runtime error, not just an assert.
    assert(master_key->size() == kKeyLength);
    keying_data->encryption_key = master_key;
    keying_data->mac_key = master_key;
    keying_data->key_derivation_type_id =
        EncryptedFileHeaders::kKeyDerivationTypeIdNone;
    return;
  }

  keying_data->encryption_key.reset(new CryptoPP::SecByteBlock(kKeyLength));
  keying_data->mac_key.reset(new CryptoPP::SecByteBlock(kKeyLength));

  if (kKeyLength == 0) {
    keying_data->key_derivation_type_id =
        EncryptedFileHeaders::kKeyDerivationTypeIdNone;
    return;
  }

  keying_data->key_derivation_type_id =
      EncryptedFileHeaders::kKeyDerivationTypeIdHkdfSha256;
  keying_data->hkdf_info.reset(new vector<byte>(
      kHkdfInfoSha256, kHkdfInfoSha256 + sizeof(kHkdfInfoSha256)));

  keying_data->encryption_key_salt
      .reset(new vector<byte>(CryptoPP::HMAC<CryptoPP::SHA256>::MAX_KEYLENGTH));
  DeriveKeyHkdfSha256(master_key, keying_data->encryption_key_salt.get(),
                      keying_data->encryption_key.get());

  keying_data->mac_key_salt
      .reset(new vector<byte>(CryptoPP::HMAC<CryptoPP::SHA256>::MAX_KEYLENGTH));
  DeriveKeyHkdfSha256(master_key, keying_data->mac_key_salt.get(),
                      keying_data->mac_key.get());
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

  keying_data->encryption_key_salt.reset(new vector<byte>(kKeyLength));
  DeriveKeyPbkdf2(passphrase, keying_data->encryption_key_salt.get(),
                  keying_data->encryption_key.get());

  keying_data->mac_key_salt.reset(new vector<byte>(kKeyLength));
  DeriveKeyPbkdf2(passphrase, keying_data->mac_key_salt.get(),
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
        *CHECK_NOTNULL(keying_data.encryption_key_salt),
        *CHECK_NOTNULL(keying_data.mac_key_salt));
  } else if (keying_data.key_derivation_type_id ==
             EncryptedFileHeaders::kKeyDerivationTypeIdHkdfSha256) {
    encrypted_file_headers->SetKeyDerivationHkdfSha256(
        *CHECK_NOTNULL(keying_data.hkdf_info),
        *CHECK_NOTNULL(keying_data.encryption_key_salt),
        *CHECK_NOTNULL(keying_data.mac_key_salt));
  } else {
    assert(false);
  }
}

// static
void Cryptor::DeriveKeyPbkdf2(
    const boost::shared_ptr<CryptoPP::SecByteBlock> passphrase,
    vector<byte>* salt, CryptoPP::SecByteBlock* derived_key) {
  CHECK_NOTNULL(passphrase);
  assert(CHECK_NOTNULL(salt)->size() == CHECK_NOTNULL(derived_key)->size());
  const int kPbkdf2Iterations = 1 << kPbkdf2IterationsExponent;

  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(salt->data(), salt->size());

  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
  pbkdf2.DeriveKey(
      *derived_key, derived_key->size(),
      0x00,  // not used
      passphrase->data(), passphrase->size(),
      salt->data(), salt->size(),
      kPbkdf2Iterations);
}

// static
void Cryptor::DeriveKeyHkdfSha256(
    const boost::shared_ptr<CryptoPP::SecByteBlock> master_key,
    vector<byte>* salt, CryptoPP::SecByteBlock* derived_key) {
  CHECK_NOTNULL(master_key);
  assert(CHECK_NOTNULL(salt)->size() >=
         CryptoPP::HMAC<CryptoPP::SHA256>::MIN_KEYLENGTH);
  assert(CHECK_NOTNULL(salt)->size() <=
         CryptoPP::HMAC<CryptoPP::SHA256>::MAX_KEYLENGTH);

  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(salt->data(), salt->size());

  // This implements the HKDF defined in RFC 5869 using SHA256 as the underlying
  // hash. The Crypto++ library currently lacks this algorithm as a built-in,
  // although it has all of the necessary components.
  //
  // This has two steps: Extract, which uses the salt and master key to produce
  // an intermediate key of a size appropriate for the HMAC, and Expand, which
  // uses the intermediate key to produce a derived key of the desired size.
  CryptoPP::SecByteBlock intermediate_key(
      CryptoPP::HMAC<CryptoPP::SHA256>::DIGESTSIZE);

  // "Extract" step. Not strictly necessary since master_key should already be
  // strongly random, but this allows us to use master keys that are not exactly
  // 256 bits if necessary.
  //
  // Note the one counterintuitive subtlety that the salt, not
  // the master key, is used as the key for the HMAC.
  CryptoPP::HMAC<CryptoPP::SHA256> extract_hmac_sha256(salt->data(),
                                                       salt->size());
  extract_hmac_sha256.CalculateDigest(intermediate_key.data(),
                                      master_key->data(), master_key->size());

  // "Expand" step. Short variable names are from RFC 5869.
  //
  // Note that in the common case where the desired derived key length is equal
  // to the HMAC digest size (256 bits) the loop will run exactly once.
  const size_t l = derived_key->size();
  const size_t n = ceil(static_cast<double>(l) /
                        CryptoPP::HMAC<CryptoPP::SHA256>::DIGESTSIZE);
  assert(n <= 255);
  assert(sizeof(byte) == sizeof(uint8_t));

  CryptoPP::HMAC<CryptoPP::SHA256> expand_hmac_sha256(intermediate_key.data(),
                                                      intermediate_key.size());
  auto derived_key_output_itr = derived_key->begin();
  vector<byte> prev_t;
  for (uint8_t i = 1; i <= n ; ++i) {
    expand_hmac_sha256.Restart();
    expand_hmac_sha256.Update(prev_t.data(), prev_t.size());
    expand_hmac_sha256.Update(kHkdfInfoSha256, sizeof(kHkdfInfoSha256));
    expand_hmac_sha256.Update(static_cast<const byte*>(&i), sizeof(uint8_t));

    vector<byte> t(CryptoPP::HMAC<CryptoPP::SHA256>::DIGESTSIZE);
    expand_hmac_sha256.Final(t.data());

    assert(derived_key_output_itr != derived_key->end());
    derived_key_output_itr = copy_n(
        t.begin(),
        min<size_t>(t.size(),
                    distance(derived_key_output_itr, derived_key->end())),
        derived_key_output_itr);
    prev_t.swap(t);
  }
}

}  // namespace polar_express
