#ifndef ENCRYPTED_FILE_HEADERS_H
#define ENCRYPTED_FILE_HEADERS_H

#include <cinttypes>
#include <memory>

#include "base/macros.h"

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
// This class defines and produces sections 1-5 above. Methods will be added as
// needed as different forms of encryption are added to the program. Note that
// these headers are NOT used if encryption is entirely omitted, so there is no
// "none" encryption type.
//
// TODO: This class should also be able to parse the headers that it generates.
class EncryptedFileHeaders {
 public:
  EncryptedFileHeaders();
  ~EncryptedFileHeaders();

  void SetKeyDerivationNone();

  // Salts must be exactly 32 bytes.
  void SetKeyDerivationPbkdf2(uint8_t iteration_count_exponent,
                              const vector<byte>& encryption_key_salt,
                              const vector<byte>& mac_key_salt);

  // Salts must be exactly 32 bytes. Info must be at most 32 bytes (and may be
  // empty).
  void SetKeyDerivationHkdfSha256(
      const vector<byte>& info,
      const vector<byte>& encryption_key_salt,
      const vector<byte>& mac_key_salt);

  // Initialization vector must be exactly 32 bytes.
  void SetEncryptionAes256Gcm(const vector<byte>& initialization_vector);

  void SetMacNone();

  void GetHeaderBlock(vector<byte>* header_block) const;

  static const char* const kKeyDerivationTypeIdNone;
  static const char* const kKeyDerivationTypeIdPbkdf2;
  static const char* const kKeyDerivationTypeIdHkdfSha256;
  static const char* const kEncryptionTypeIdAes256Gcm;
  static const char* const kMacTypeIdNone;

 private:
  struct GenericHeaderFields;
  struct KeyDerivationParametersPbkdf2;
  struct KeyDerivationParametersHkdfSha256;
  struct EncryptionParametersAes256Gcm;

  std::unique_ptr<GenericHeaderFields> generic_header_fields_;
  std::unique_ptr<KeyDerivationParametersPbkdf2>
      key_derivation_parameters_pbkdf2_;
  std::unique_ptr<KeyDerivationParametersHkdfSha256>
      key_derivation_parameters_hkdf_sha256_;
  std::unique_ptr<EncryptionParametersAes256Gcm>
      encryption_parameters_aes256_gcm_;

  DISALLOW_COPY_AND_ASSIGN(EncryptedFileHeaders);
};

}  // namespace polar_express

#endif  // ENCRYPTED_FILE_HEADERS_H
