#ifndef NULL_CRYPTOR_IMPL_H
#define NULL_CRYPTOR_IMPL_H

#include <memory>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <crypto++/secblock.h>

#include "base/callback.h"
#include "base/macros.h"
#include "services/cryptor.h"

namespace polar_express {

// Cryptor implementation that doesn't actually do any encryption.
class NullCryptorImpl : public Cryptor {
 public:
  NullCryptorImpl();
  virtual ~NullCryptorImpl();

  virtual Cryptor::EncryptionType encryption_type() const;

  virtual size_t key_length() const;

  virtual void InitializeEncryption(const Cryptor::KeyingData& keying_data);

  virtual void EncryptData(
      boost::shared_ptr<vector<byte> > data, Callback callback);

  virtual void FinalizeEncryption(vector<byte>* encrypted_file_header_block,
                                  vector<byte>* message_authentication_code);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCryptorImpl);
};

}  // namespace polar_express

#endif  // NULL_CRYPTOR_IMPL_H
