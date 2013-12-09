// Disable the clang deprecated warning for this file only. Uses of htonl
// trigger it due to use of 'register' keyword. This is considered a bug in
// clang and is being fixed.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"

#include "encrypted-file-headers.h"

#include <arpa/inet.h>

#include <cassert>
#include <cstring>

#include <algorithm>

namespace polar_express {
namespace {

// These four magic bytes at the beginning of a file help identify it as a Polar
// Express encrypted bundle.
const byte kMagic[4] = {'P', 'E', 'X', '\0'};

template <int N>
void SetTypeId(char (&type_id_field)[N], const char* type_id) {
  assert(type_id != nullptr);
  assert(std::strlen(type_id) < N);
  std::fill(&type_id_field[0], &type_id_field[0] + N, '\0');
  std::strcpy(&type_id_field[0], type_id);
}

}  // namespace

// The generic header that describes the format of the remaining sections of
// this file.
//
// Each part of the file (key derivation, encryption, MAC) has a type id string
// and a format version number for its parameters header. When applicable, the
// type IDs correspond to the ID used for the the same algorithm in the OpenSSL
// command line utility. The parameters header version numbers allow future
// versions of Polar Express to change the parameters header format for a type
// in a backwards-compatible way.
//
// All format version numbers are 32-bit, and all type ID strings are 16 bytes,
// nul-terminated and nul-padded. All numbers are stored in NETWORK BYTE ORDER.
//
// This Generic Header also has a format version. This struct is version 0.
//
// Currently supported type IDs:
//   Key Derivation: "pbkdf2", "pex-sha256-hkdf"
//   Encryption: "aes-256-cfb"
//   MAC: "", "sha256"
struct EncryptedFileHeaders::GenericHeaderFields {
  const uint32_t generic_header_format_version = 0;
  char key_derivation_type_id[16];
  uint32_t key_derivation_parameters_format_version;
  char encryption_type_id[16];
  uint32_t encryption_parameters_format_version;
  char mac_type_id[16];
  uint32_t mac_parameters_format_version;
} __attribute__((packed));

// Parameters for PBKDF2 key derivation ("pbkdf2"). This struct is version 0.
const char* const EncryptedFileHeaders::kKeyDerivationTypeIdPbkdf2 = "pbkdf2";
struct EncryptedFileHeaders::KeyDerivationParametersPbkdf2 {
  uint32_t iteration_count;
  byte encryption_key_salt[32];
  byte mac_key_salt[32];
} __attribute__((packed));

// Parameters for HKDF key derivation using SHA256 HMAC ("pex-sha256-hkdf").
// This name is prefixed with "pex-" to indicate that this used the HKDF
// algorithm implemented inside Polar Express (on top of Crypto++ SHA256 HMAC)
// since as of this writing Crypto++ does not have a native HKDF API. This
// struct is version 0.
const char* const EncryptedFileHeaders::kKeyDerivationTypeIdPexSha256Hkdf =
    "pex-sha256-hkdf";
struct EncryptedFileHeaders::KeyDerivationParametersPexSha256Hkdf {
  byte encryption_key_salt[32];
  byte mac_key_salt[32];
} __attribute__((packed));

// Parameters used for the AES 256 CBF-mode cipher. This struct is version 0.
const char* const EncryptedFileHeaders::kEncryptionTypeIdAes256Cbf =
    "aes-256-cbf";
struct EncryptedFileHeaders::EncryptionParametersAes256Cbf {
  byte initialization_vector[32];
} __attribute__((packed));

// There are no parameters for the null or SHA256 MAC types
const char* const EncryptedFileHeaders::kMacTypeIdSha256 = "sha256";
const char* const EncryptedFileHeaders::kMacTypeIdNone = "";

EncryptedFileHeaders::EncryptedFileHeaders()
    : generic_header_fields_(new GenericHeaderFields({})),
      key_derivation_parameters_pbkdf2_(new KeyDerivationParametersPbkdf2({})),
      key_derivation_parameters_pex_sha256_hkdf_(
          new KeyDerivationParametersPexSha256Hkdf({})),
      encryption_parameters_aes256_cbf_(new EncryptionParametersAes256Cbf({})) {
}

EncryptedFileHeaders::~EncryptedFileHeaders() {
}

void EncryptedFileHeaders::SetKeyDerivationPbkdf2(
    uint32_t iteration_count, const vector<byte>& encryption_key_salt,
    const vector<byte>& mac_key_salt) {
  assert(encryption_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPbkdf2::
                    encryption_key_salt));
  assert(mac_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPbkdf2::
                mac_key_salt));

  SetTypeId(generic_header_fields_->key_derivation_type_id,
            kKeyDerivationTypeIdPbkdf2);
  generic_header_fields_->key_derivation_parameters_format_version = htonl(0);

  *key_derivation_parameters_pbkdf2_ = {};
  key_derivation_parameters_pbkdf2_->iteration_count = htonl(iteration_count);
  std::copy(encryption_key_salt.begin(), encryption_key_salt.end(),
            &key_derivation_parameters_pbkdf2_->encryption_key_salt[0]);
  std::copy(mac_key_salt.begin(), mac_key_salt.end(),
            &key_derivation_parameters_pbkdf2_->mac_key_salt[0]);
}

void EncryptedFileHeaders::SetKeyDerivationPexSha256Hkdf(
    const vector<byte>& encryption_key_salt, const vector<byte>& mac_key_salt) {
  assert(encryption_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPexSha256Hkdf::
                encryption_key_salt));
  assert(mac_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPexSha256Hkdf::
                mac_key_salt));

  SetTypeId(generic_header_fields_->key_derivation_type_id,
            kKeyDerivationTypeIdPexSha256Hkdf);
  generic_header_fields_->key_derivation_parameters_format_version = htonl(0);

  *key_derivation_parameters_pex_sha256_hkdf_ = {};
  std::copy(
      encryption_key_salt.begin(), encryption_key_salt.end(),
      &key_derivation_parameters_pex_sha256_hkdf_->encryption_key_salt[0]);
  std::copy(mac_key_salt.begin(), mac_key_salt.end(),
            &key_derivation_parameters_pex_sha256_hkdf_->mac_key_salt[0]);
}

void EncryptedFileHeaders::SetEncryptionAes256Cbf(
    const vector<byte>& initialization_vector) {
  assert(initialization_vector.size() ==
         sizeof(EncryptedFileHeaders::EncryptionParametersAes256Cbf::
                initialization_vector));

  SetTypeId(generic_header_fields_->encryption_type_id,
            kEncryptionTypeIdAes256Cbf);
  generic_header_fields_->encryption_parameters_format_version = htonl(0);

  *encryption_parameters_aes256_cbf_ = {};
  std::copy(initialization_vector.begin(), initialization_vector.end(),
            &encryption_parameters_aes256_cbf_->initialization_vector[0]);
}

void EncryptedFileHeaders::SetMacSha256() {
  SetTypeId(generic_header_fields_->mac_type_id, kMacTypeIdSha256);
  generic_header_fields_->mac_parameters_format_version = htonl(0);
}

void EncryptedFileHeaders::SetMacNone() {
  SetTypeId(generic_header_fields_->mac_type_id, kMacTypeIdNone);
  generic_header_fields_->mac_parameters_format_version = htonl(0);
}

void EncryptedFileHeaders::GetHeaderBlock(vector<byte>* header_block) const {
  assert(header_block != nullptr);

  header_block->insert(header_block->end(), kMagic, kMagic + sizeof(kMagic));

  header_block->insert(
      header_block->end(),
      reinterpret_cast<const byte*>(generic_header_fields_.get()),
      reinterpret_cast<const byte*>(generic_header_fields_.get()) +
          sizeof(*generic_header_fields_));

  if (generic_header_fields_->key_derivation_type_id ==
      kKeyDerivationTypeIdPbkdf2) {
    header_block->insert(
        header_block->end(),
        reinterpret_cast<const byte*>(key_derivation_parameters_pbkdf2_.get()),
        reinterpret_cast<const byte*>(key_derivation_parameters_pbkdf2_.get()) +
            sizeof(*key_derivation_parameters_pbkdf2_));
  } else if (generic_header_fields_->key_derivation_type_id ==
             kKeyDerivationTypeIdPexSha256Hkdf) {
    header_block->insert(
        header_block->end(),
        reinterpret_cast<const byte*>(
            key_derivation_parameters_pex_sha256_hkdf_.get()),
        reinterpret_cast<const byte*>(
            key_derivation_parameters_pex_sha256_hkdf_.get()) +
            sizeof(*key_derivation_parameters_pex_sha256_hkdf_));
  }

  if (generic_header_fields_->encryption_type_id ==
      kEncryptionTypeIdAes256Cbf) {
    header_block->insert(
        header_block->end(),
        reinterpret_cast<const byte*>(encryption_parameters_aes256_cbf_.get()),
        reinterpret_cast<const byte*>(encryption_parameters_aes256_cbf_.get()) +
            sizeof(*encryption_parameters_aes256_cbf_));
  }

  // None of the currently supported HMAC types require a parameters block.
}

}  // namespace polar_express

#pragma clang diagnostic pop
