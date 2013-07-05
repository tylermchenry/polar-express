#ifndef AES_CRYPTOR_IMPL_H
#define AES_CRYPTOR_IMPL_H

#include <memory>
#include <string>
#include <vector>

#include "callback.h"
#include "cryptor.h"
#include "macros.h"

#include "crypto++/aes.h"
#include "crypto++/modes.h"
#include "crypto++/secblock.h"

namespace polar_express {

class AesCryptorImpl : public Cryptor {
 public:
  AesCryptorImpl();
  virtual ~AesCryptorImpl();

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
  vector<byte> iv_;
  unique_ptr<CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption> aes_cfb_encryption_;

  DISALLOW_COPY_AND_ASSIGN(AesCryptorImpl);
};

}  // namespace polar_express

#endif  // AES_CRYPTOR_IMPL_H
