#include "cryptor.h"

#include "asio-dispatcher.h"
#include "make-unique.h"
#include "null-cryptor-impl.h"
#include "aes-cryptor-impl.h"

#include "crypto++/aes.h"
#include "crypto++/osrng.h"
#include "crypto++/pwdbased.h"
#include "crypto++/sha.h"

namespace polar_express {
namespace {

// Arbitrary, but the problem is that it needs to be the same when
// decrypting, and I have no way to note this in the file.
//
// TODO(tylermchenry): Some kind of file format that would allow me to
// specify this? Or is this just something the user needs to know when
// recovering?
const int kPbkdf2Iterations = 100000;

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

size_t Cryptor::iv_length() const {
  return impl_->iv_length();
}

boost::shared_ptr<const CryptoPP::SecByteBlock> Cryptor::DeriveKeyFromPassword(
    const CryptoPP::SecByteBlock& password, const vector<byte>& salt) const {
  if (key_length() <= 0) {
    return boost::shared_ptr<CryptoPP::SecByteBlock>();
  }

  boost::shared_ptr<CryptoPP::SecByteBlock> key(
      new CryptoPP::SecByteBlock(key_length()));
  vector<byte> real_salt(GenerateRealSalt(salt));

  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
  pbkdf2.DeriveKey(
      *key, key->size(),
      0x00,  // not used
      password.data(), password.size(),
      real_salt.data(), real_salt.size(),
      kPbkdf2Iterations);

  return std::move(key);
}

void Cryptor::InitializeEncryption(
    const CryptoPP::SecByteBlock& key, boost::shared_ptr<vector<byte> > iv) {
  impl_->InitializeEncryption(key, iv);
}

void Cryptor::EncryptData(
    boost::shared_ptr<vector<byte> > data, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Cryptor::EncryptData, impl_.get(), data, callback));
}

vector<byte> Cryptor::GenerateRealSalt(const vector<byte>& salt) const {
  vector<byte> real_salt(key_length());

  if (salt.size() >= real_salt.size()) {
    std::copy(salt.begin(), salt.begin() + key_length(), real_salt.begin());
  } else {
    // If the user provided a salt that was too short, take its hash
    // to get it to the appropriate length.
    CryptoPP::SHA256 sha256_engine;
    sha256_engine.CalculateDigest(real_salt.data(), salt.data(), salt.size());
  }

  return real_salt;
}

}  // namespace polar_express
