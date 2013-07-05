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
  // If salt is empty, a random salt is used. The salt may be
  // determinstically transformed (i.e. to pad out to a certain size)
  // before being used.
  //
  // This call is NOT lightweight but IS synchronous. It should not be
  // called frequently.
  boost::shared_ptr<const CryptoPP::SecByteBlock> DeriveKeyFromPassword(
      const CryptoPP::SecByteBlock& password, const string& salt = "") const;

  // This must be called once before the first call to EncryptData
  // and again after FinalizeEncryption before EncryptData may be
  // called again. Calling InitializeEncryption discards any
  // buffered encrypted data that has not yet been finalized.
  //
  // If the encryption mechanism requires an initialization vector, a
  // random initialization vector will be generated and prepended to
  // the encrypted output. If iv is empty, a random iv will be
  // generated.
  //
  // TODO(tylermchenry): Change interface to RETURN the iv instead of
  // accepting it. There is no reasonable case where we'd want an
  // explicit iv.
  //
  // This call is lightweight and synchronous.
  virtual void InitializeEncryption(
      const CryptoPP::SecByteBlock& key, const string& iv = "");

  // Encryptes (more) data. Some or all of the encrypted data may be
  // appeneded to encrypted_data. It is possible that nothing will be
  // appended to encrypted_data if it all remains buffered.
  virtual void EncryptData(
      const vector<char>& data, string* encrypted_data,
      Callback callback);

  // Outputs any remaining buffered encrypted data to encrypted_data
  // and clears all state. InitializeEncryption must have been called
  // at least once before each call to FinalizeEncryption.
  //
  // This call is lightweight and synchronous.
  virtual void FinalizeEncryption(string* encrypted_data);

  // TODO(tylermchenry): Add decryption.

 protected:
  Cryptor();
  explicit Cryptor(unique_ptr<Cryptor>&& impl);

 private:
  template<typename CryptorImplT>
  static unique_ptr<Cryptor> CreateCryptorWithImpl();

  unique_ptr<CryptoPP::SecByteBlock> GenerateRealSalt(
      const CryptoPP::SecByteBlock& password, const string& salt) const;

  void GenerateSaltByHashing(
      const CryptoPP::SecByteBlock& data,
      CryptoPP::SecByteBlock* real_salt) const;

  unique_ptr<Cryptor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Cryptor);
};

}  // namespace polar_express

#endif  // CRYPTOR_H
