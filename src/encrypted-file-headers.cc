#include "encrypted-file-headers.h"

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
// All format version numbers are 8-bit, and all type ID strings are 15 bytes,
// nul-terminated and nul-padded. All numbers are stored in NETWORK BYTE ORDER.
//
// This Generic Header also has a format version. This struct is version 0.
//
// Currently supported type IDs:
//   Key Derivation: "", "pbkdf2"
//   Encryption: "aes-256-gcm"
//   MAC: ""
struct EncryptedFileHeaders::GenericHeaderFields {
  const uint8_t generic_header_format_version = 0;
  char key_derivation_type_id[15];
  uint8_t key_derivation_parameters_format_version;
  char encryption_type_id[15];
  uint8_t encryption_parameters_format_version;
  char mac_type_id[15];
  uint8_t mac_parameters_format_version;
} __attribute__((packed));

// Parameters for PBKDF2 key derivation ("pbkdf2"). This struct is version 0.
// Iteration count is stored by exponent instead of directly to avoid needing
// to worry about byte ordering issues.
const char* const EncryptedFileHeaders::kKeyDerivationTypeIdPbkdf2 = "pbkdf2";
struct EncryptedFileHeaders::KeyDerivationParametersPbkdf2 {
  uint8_t iteration_count_exponent;  // 2^N iterations, this is N.
  byte encryption_key_salt[32];
  byte mac_key_salt[32];
} __attribute__((packed));

// Parameters for AES256 GCM-mode encryption ("aes-256-gcm"). This struct is
// version 0.
const char* const EncryptedFileHeaders::kEncryptionTypeIdAes256Gcm =
    "aes-256-gcm";
struct EncryptedFileHeaders::EncryptionParametersAes256Gcm {
  byte initialization_vector[32];
} __attribute__((packed));

// There are no parameters for the 'none' modes of key derivation and HMAC.
const char* const EncryptedFileHeaders::kKeyDerivationTypeIdNone = "";
const char* const EncryptedFileHeaders::kMacTypeIdNone = "";

EncryptedFileHeaders::EncryptedFileHeaders()
    : generic_header_fields_(new GenericHeaderFields({})),
      key_derivation_parameters_pbkdf2_(new KeyDerivationParametersPbkdf2({})),
      encryption_parameters_aes256_gcm_(new EncryptionParametersAes256Gcm({})) {
}

EncryptedFileHeaders::~EncryptedFileHeaders() {
}

void EncryptedFileHeaders::SetKeyDerivationNone() {
  SetTypeId(generic_header_fields_->key_derivation_type_id,
            kKeyDerivationTypeIdNone);
  generic_header_fields_->key_derivation_parameters_format_version = 0;
}

void EncryptedFileHeaders::SetKeyDerivationPbkdf2(
    uint8_t iteration_count_exponent, const vector<byte>& encryption_key_salt,
    const vector<byte>& mac_key_salt) {
  assert(encryption_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPbkdf2::
                    encryption_key_salt));
  assert(mac_key_salt.size() ==
         sizeof(EncryptedFileHeaders::KeyDerivationParametersPbkdf2::
                mac_key_salt));

  SetTypeId(generic_header_fields_->key_derivation_type_id,
            kKeyDerivationTypeIdPbkdf2);
  generic_header_fields_->key_derivation_parameters_format_version = 0;

  *key_derivation_parameters_pbkdf2_ = {};
  key_derivation_parameters_pbkdf2_->iteration_count_exponent =
      iteration_count_exponent;
  std::copy(encryption_key_salt.begin(), encryption_key_salt.end(),
            &key_derivation_parameters_pbkdf2_->encryption_key_salt[0]);
  std::copy(mac_key_salt.begin(), mac_key_salt.end(),
            &key_derivation_parameters_pbkdf2_->mac_key_salt[0]);
}

void EncryptedFileHeaders::SetEncryptionAes256Gcm(
    const vector<byte>& initialization_vector) {
  assert(initialization_vector.size() ==
         sizeof(EncryptedFileHeaders::EncryptionParametersAes256Gcm::
                initialization_vector));

  SetTypeId(generic_header_fields_->encryption_type_id,
            kEncryptionTypeIdAes256Gcm);
  generic_header_fields_->encryption_parameters_format_version = 0;

  *encryption_parameters_aes256_gcm_ = {};
  std::copy(initialization_vector.begin(), initialization_vector.end(),
            &encryption_parameters_aes256_gcm_->initialization_vector[0]);
}

void EncryptedFileHeaders::SetMacNone() {
  SetTypeId(generic_header_fields_->mac_type_id, kMacTypeIdNone);
  generic_header_fields_->mac_parameters_format_version = 0;
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
  }

  if (generic_header_fields_->encryption_type_id ==
      kEncryptionTypeIdAes256Gcm) {
    header_block->insert(
        header_block->end(),
        reinterpret_cast<const byte*>(encryption_parameters_aes256_gcm_.get()),
        reinterpret_cast<const byte*>(encryption_parameters_aes256_gcm_.get()) +
            sizeof(*encryption_parameters_aes256_gcm_));
  }

  // None of the currently supported HMAC types require a parameters block.
}

}  // namespace polar_express
