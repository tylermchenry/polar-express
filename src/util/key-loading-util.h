#ifndef KEY_LOADING_UTIL_H
#define KEY_LOADING_UTIL_H

#include <string>

#include <crypto++/secblock.h>

#include "base/macros.h"

namespace polar_express {
namespace key_loading_util {

// Loads and sanity-checks master key for client-side encryption based on
// options, with informative error messages. Returns true only if key loading &
// checking succeeded. Returns false (and prints no error message) if master key
// flags are not being used.
bool LoadMasterKey(const size_t expected_key_length,
                   CryptoPP::SecByteBlock* master_key);

// Loads and sanity-checks keys for Amazon Web Services based on options, with
// informative error messages. Returns true only if key loading & checking
// succeeded.
bool LoadAwsKeys(string* aws_access_key,
                 CryptoPP::SecByteBlock* aws_secret_key);

}  // namespace key_loading_util
}  // namespace polar_express

#endif  // KEY_LOADING_UTIL_H
