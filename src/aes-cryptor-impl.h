#ifndef AES_CRYPTOR_IMPL_H
#define AES_CRYPTOR_IMPL_H

#include <memory>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "crypto++/aes.h"
#include "crypto++/modes.h"
#include "crypto++/secblock.h"

#include "callback.h"
#include "cryptor.h"
#include "encrypted-file-headers.h"
#include "macros.h"

namespace polar_express {

// This class implements encryption based on AES256 CBF mode encryption, plus a
// SHA256 HMAC. It generates a new pair of encryption/authentication keys each
// time it is initialized, and can use either PBKDF2 or HKDF with SHA256
// depending on whether a passphrase or a secret key is available.
//
// TODO: If other forms of AES encryption are supported (e.g. an authenticated
// mode that does not require a separate HMAC, this can become a base class).
class AesCryptorImpl : public Cryptor {
 public:
  AesCryptorImpl();
  virtual ~AesCryptorImpl();

  virtual Cryptor::EncryptionType encryption_type() const;

  virtual size_t key_length() const;

  virtual void InitializeEncryption(const Cryptor::KeyingData& keying_data);

  virtual void EncryptData(
      boost::shared_ptr<vector<byte> >, Callback callback);

  virtual void FinalizeEncryption(vector<byte>* encrypted_file_header_block,
                                  vector<byte>* message_authentication_code);

 private:
  void DeriveKeysFromSecretKey(const CryptoPP::SecByteBlock& secret_key,
                               const vector<byte>& encryption_salt,
                               const vector<byte>& authentication_salt);

  void DeriveKeysFromPassphrase(const CryptoPP::SecByteBlock& passphrase,
                                const vector<byte>& encryption_salt,
                                const vector<byte>& authentication_salt);

  void GenerateRandomSalts(vector<byte>* encryption_salt,
                           vector<byte>* authentication_salt) const;

  void Sha256Hkdf(const CryptoPP::SecByteBlock& secret_key,
                  const vector<byte>& salt,
                  CryptoPP::SecByteBlock* derived_key) const;

  unique_ptr<CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption> aes_cfb_encryption_;
  unique_ptr<EncryptedFileHeaders> encrypted_file_headers_;
  unique_ptr<CryptoPP::SecByteBlock> encryption_key_;
  unique_ptr<CryptoPP::SecByteBlock> authentication_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCryptorImpl);
};

}  // namespace polar_express

#endif  // AES_CRYPTOR_IMPL_H
