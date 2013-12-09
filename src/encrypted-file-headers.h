#ifndef ENCRYPTED_FILE_HEADERS_H
#define ENCRYPTED_FILE_HEADERS_H

#include <cinttypes>
#include <memory>

#include "macros.h"

namespace polar_express {

// The format of a Polar Express encrypted file is:
//
// 1. Magic bytes
// 2. Generic Header
// 3. Key Derivation parameters
// 4. Encryption parameters
// 5. MAC parameters
// 6. Encrypted Data
// 7. MAC (optional)
//
// The MAC may be omitted if, for example, a self-authenticating form of
// encryption is being used. If a MAC is used, it should be computed over all
// data in the file, including magic bytes and headers.
//
// This class defines and produces sections 1-5 above.
//
// TODO: This class should also be able to parse the headers that it generates.
class EncryptedFileHeaders {
 public:
  EncryptedFileHeaders();
  ~EncryptedFileHeaders();

  // Salts must be exactly 32 bytes.
  void SetKeyDerivationPbkdf2(uint32_t iteration_count,
                              const vector<byte>& encryption_key_salt,
                              const vector<byte>& mac_key_salt);

  // Salts must be exactly 32 bytes.
  void SetKeyDerivationPexSha256Hkdf(const vector<byte>& encryption_key_salt,
                                     const vector<byte>& mac_key_salt);

  // Initialization vector must be exactly 32 bytes.
  void SetEncryptionAes256Cbf(const vector<byte>& initialization_vector);

  void SetMacSha256();

  void SetMacNone();

  void GetHeaderBlock(vector<byte>* header_block) const;

  static const char* const kKeyDerivationTypeIdPbkdf2;
  static const char* const kKeyDerivationTypeIdPexSha256Hkdf;
  static const char* const kEncryptionTypeIdAes256Cbf;
  static const char* const kMacTypeIdSha256;
  static const char* const kMacTypeIdNone;

 private:
  struct GenericHeaderFields;
  struct KeyDerivationParametersPbkdf2;
  struct KeyDerivationParametersPexSha256Hkdf;
  struct EncryptionParametersAes256Cbf;

  std::unique_ptr<GenericHeaderFields> generic_header_fields_;
  std::unique_ptr<KeyDerivationParametersPbkdf2>
      key_derivation_parameters_pbkdf2_;
  std::unique_ptr<KeyDerivationParametersPexSha256Hkdf>
      key_derivation_parameters_pex_sha256_hkdf_;
  std::unique_ptr<EncryptionParametersAes256Cbf>
      encryption_parameters_aes256_cbf_;

  DISALLOW_COPY_AND_ASSIGN(EncryptedFileHeaders);
};

}  // namespace polar_express

#endif  // ENCRYPTED_FILE_HEADERS_H
