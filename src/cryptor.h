#ifndef CRYPTOR_H
#define CRYPTOR_H

#include <memory>
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

  // The keying data provided by the user. Generally only one of these should be
  // non-empty, but the secret key will be preferred to the password if both are
  // available.
  struct KeyingData {
    CryptoPP::SecByteBlock secret_key;
    CryptoPP::SecByteBlock passphrase;
  };

  static unique_ptr<Cryptor> CreateCryptor(EncryptionType encryption_type);

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

 private:
  template<typename CryptorImplT>
  static unique_ptr<Cryptor> CreateCryptorWithImpl();

  std::unique_ptr<Cryptor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Cryptor);
};

}  // namespace polar_express

#endif  // CRYPTOR_H
