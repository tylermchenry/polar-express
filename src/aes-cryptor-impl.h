#ifndef AES_CRYPTOR_IMPL_H
#define AES_CRYPTOR_IMPL_H

#include <vector>

#include "callback.h"
#include "cryptor.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

class AesCryptorImpl : public Cryptor {
 public:
  AesCryptorImpl();
  virtual ~AesCryptorImpl();

  virtual Cryptor::EncryptionType encryption_type() const;

  virtual void InitializeEncryption(const string& key);

  virtual void EncryptData(
      const vector<char>& data, string* encrypted_data,
      Callback callback);

  virtual void FinalizeEncryption(string* encrypted_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(AesCryptorImpl);
};

}  // namespace polar_express

#endif  // AES_CRYPTOR_IMPL_H
