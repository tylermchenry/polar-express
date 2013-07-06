#ifndef CRYPTOR_H
#define CRYPTOR_H

#include <memory>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "crypto++/secblock.h"

#include "callback.h"
#include "macros.h"

namespace polar_express {

// Base class for asynchronous data cryptors (which use various
// algorithms).
class Cryptor {
 public:
  enum class EncryptionType {
    kNone,
    kAES,
  };

  static unique_ptr<Cryptor> CreateCryptor(EncryptionType encryption_type);

  virtual ~Cryptor();

  virtual EncryptionType encryption_type() const;

  virtual size_t key_length() const;

  virtual size_t iv_length() const;

  // Uses password-based key derivation to derive a key of the
  // appropriate length for the cryptor.
  //
  // If salt is shorter than the key length, it is hashed to
  // expand. If the salt is longer than the key length, it is
  // truncated. THIS MAY REDUCE SECURITY. For maximum security,
  // provide a randomly-generated salt equal to the key length.
  //
  // This call is NOT lightweight but IS synchronous. It should not be
  // called frequently.
  boost::shared_ptr<const CryptoPP::SecByteBlock> DeriveKeyFromPassword(
      const CryptoPP::SecByteBlock& password, const vector<byte>& salt) const;

  // This must be called once before the first call to EncryptData
  // and again after FinalizeEncryption before EncryptData may be
  // called again. Calling InitializeEncryption discards any
  // buffered encrypted data that has not yet been finalized.
  //
  // If the encryption mechanism requires an initialization vecotr, it
  // is returned in the iv parameter. The iv parameter pay be null if
  // the caller is not interested in the initialization vector.
  //
  // This call is lightweight and synchronous.
  virtual void InitializeEncryption(
      const CryptoPP::SecByteBlock& key, boost::shared_ptr<vector<byte> > iv);

  // Encrypts data in place.
  virtual void EncryptData(
      boost::shared_ptr<vector<byte> > data, Callback callback);

  // TODO(tylermchenry): Add decryption.

 protected:
  Cryptor();
  explicit Cryptor(unique_ptr<Cryptor>&& impl);

 private:
  template<typename CryptorImplT>
  static unique_ptr<Cryptor> CreateCryptorWithImpl();

  vector<byte> GenerateRealSalt(const vector<byte>& salt) const;

  unique_ptr<Cryptor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Cryptor);
};

}  // namespace polar_express

#endif  // CRYPTOR_H
