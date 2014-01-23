#ifndef CRYPTOR_H
#define CRYPTOR_H

#include <cinttypes>
#include <memory>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <crypto++/secblock.h>

#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

class EncryptedFileHeaders;

// Base class for asynchronous data cryptors (which use various
// algorithms).
class Cryptor {
 public:
  enum class EncryptionType {
    kNone,
    kAES,
  };

  // The keying data derived from the credentials provided by the user (either a
  // master key or a passphrase).
  struct KeyingData {
    boost::shared_ptr<CryptoPP::SecByteBlock> encryption_key;
    boost::shared_ptr<CryptoPP::SecByteBlock> mac_key;

    const char* key_derivation_type_id;  // From encrypted-file-headers.h

    // The below are relevant only when encryption_key and mac_key were
    // derived using PBKDF2.
    uint8_t pbkdf2_iterations_exponent;
    boost::shared_ptr<vector<byte> > pbkdf2_encryption_key_salt;
    boost::shared_ptr<vector<byte> > pbkdf2_mac_key_salt;
  };

  static unique_ptr<Cryptor> CreateCryptor(EncryptionType encryption_type);

  static void DeriveKeysFromMasterKey(
      const boost::shared_ptr<CryptoPP::SecByteBlock> master_key,
      const EncryptionType encryption_type,
      KeyingData* keying_data);

  static void DeriveKeysFromPassphrase(
      const boost::shared_ptr<CryptoPP::SecByteBlock> passphrase,
      const EncryptionType encryption_type,
      KeyingData* keying_data);

  virtual ~Cryptor();

  virtual EncryptionType encryption_type() const;

  virtual size_t key_length() const;

  // This method must be called once before the first call to EncryptData and
  // again after FinalizeEncryption before EncryptData may be called again.
  // Calling InitializeEncryption discards any buffered encrypted data that has
  // not yet been finalized.
  //
  // This call is lightweight and synchronous (faster if secret key is
  // populated).
  virtual void InitializeEncryption(const KeyingData& keying_data);

  // Encrypts data in place.
  virtual void EncryptData(
      boost::shared_ptr<vector<byte> > data, Callback callback);

  // Returns a header block containing vital information about the encryption
  // process that should be prepended to the file and a MAC code that
  // authenticates the contents of the file, which should be appended to the
  // file. Either or both of these may be empty.
  virtual void FinalizeEncryption(vector<byte>* encrypted_file_header_block,
                                  vector<byte>* message_authentication_code);

  // TODO(tylermchenry): Add decryption.

 protected:
  Cryptor();
  explicit Cryptor(unique_ptr<Cryptor>&& impl);

  void SetKeyDerivationHeaders(
      const KeyingData& keying_data,
      EncryptedFileHeaders* encrypted_file_headers) const;

 private:
  template<typename CryptorImplT>
  static unique_ptr<Cryptor> CreateCryptorWithImpl();

  static void DeriveKeyPbkdf2(
      const boost::shared_ptr<CryptoPP::SecByteBlock> passphrase,
      vector<byte>* salt, CryptoPP::SecByteBlock* derived_key);

  std::unique_ptr<Cryptor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Cryptor);
};

}  // namespace polar_express

#endif  // CRYPTOR_H
