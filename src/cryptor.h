#ifndef CRYPTOR_H
#define CRYPTOR_H

#include <vector>

#include "callback.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

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

  // This must be called once before the first call to EncryptData
  // and again after FinalizeEncryption before EncryptData may be
  // called again. Calling InitializeEncryption discards any
  // buffered encrypted data that has not yet been finalized.
  //
  // This call is lightweight and synchronous.
  //
  // TODO(tylermchenry): Investigate a secure way to store keys in
  // memory; passing around std::strings is probably not the best
  // thing to do.
  virtual void InitializeEncryption(const string& key);

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

  unique_ptr<Cryptor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Cryptor);
};

}  // namespace polar_express

#endif  // CRYPTOR_H
