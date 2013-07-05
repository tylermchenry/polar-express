#ifndef NULL_CRYPTOR_IMPL_H
#define NULL_CRYPTOR_IMPL_H

#include <memory>
#include <string>
#include <vector>

#include "crypto++/secblock.h"

#include "callback.h"
#include "cryptor.h"
#include "macros.h"

namespace polar_express {

// Cryptor implementation that doesn't actually do any encryption
// (just copies input to output).
class NullCryptorImpl : public Cryptor {
 public:
  NullCryptorImpl();
  virtual ~NullCryptorImpl();

  virtual Cryptor::EncryptionType encryption_type() const;

  virtual size_t key_length() const;

  virtual size_t iv_length() const;

  virtual void InitializeEncryption(
      const CryptoPP::SecByteBlock& key, const string& iv);

  virtual void EncryptData(
      const vector<char>& data, string* encrypted_data,
      Callback callback);

  virtual void FinalizeEncryption(string* encrypted_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCryptorImpl);
};

}  // namespace polar_express

#endif  // NULL_CRYPTOR_IMPL_H
