#ifndef NULL_CRYPTOR_IMPL_H
#define NULL_CRYPTOR_IMPL_H

#include <vector>

#include "callback.h"
#include "cryptor.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

// Cryptor implementation that doesn't actually do any encryption
// (just copies input to output).
class NullCryptorImpl : public Cryptor {
 public:
  NullCryptorImpl();
  virtual ~NullCryptorImpl();

  virtual Cryptor::EncryptionType encryption_type() const;

  virtual void InitializeEncryption(const string& key);

  virtual void EncryptData(
      const vector<char>& data, string* encrypted_data,
      Callback callback);

  virtual void FinalizeEncryption(string* encrypted_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCryptorImpl);
};

}  // namespace polar_express

#endif  // NULL_CRYPTOR_IMPL_H
