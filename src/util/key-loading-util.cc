#include "util/key-loading-util.h"

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>

#include <crypto++/osrng.h>

#include "base/options.h"

DEFINE_OPTION(master_key_file, string, "",
              "Path to file where master encryption key is stored (must be "
              "owner-readable only). Passphrase is ignored when this is set.");
DEFINE_OPTION(generate_new_master_key, bool, false,
              "Generates and uses a new master key in the location referenced "
              "by the 'master_key_file' option. That option must be set and "
              "the file must not already exist.")

DEFINE_OPTION(aws_access_key, string, "", "Amazon Web Services access key.");
DEFINE_OPTION(aws_secret_key_file, string, "",
              "Path to file where Amazon Web Services secret key is stored "
              "(must be owner-readable only).");

namespace polar_express {
namespace key_loading_util {
namespace {

const size_t kAwsAccessKeyLength = 20;
const size_t kAwsSecretKeyLength = 40;

bool LoadKeyFromFile(const char* const key_name,
                     const boost::filesystem::path& key_file_path,
                     const size_t expected_key_length,
                     CryptoPP::SecByteBlock* key) {
  filesystem::file_status file_stat;
  size_t key_length;
  try {
    file_stat = status(key_file_path);
    key_length = filesystem::file_size(key_file_path);
  } catch (const filesystem::filesystem_error& ex) {
    std::cerr << "ERROR: Error accessing " << key_name << " file '"
              << key_file_path.string() << "'. Details: " << ex.what()
              << std::endl;
    return false;
  }

  if (file_stat.permissions() & filesystem::group_read ||
      file_stat.permissions() & filesystem::others_read) {
    std::cerr
        << "ERROR: Permissions on " << key_name << " file '"
        << key_file_path.string()
        << "' are set poorly! This file should be readable only by its owner."
        << " Refusing to use this key file." << std::endl;
    return false;
  }

  if (key_length != expected_key_length) {
    std::cerr << "ERROR: " << key_name << " is the wrong length. Expected "
              << expected_key_length << " bytes, but provided key file '"
              << key_file_path.string() << "' is " << key_length << " bytes."
              << std::endl;
    if (key_length > expected_key_length) {
      std::cerr << "Check that there are no stray characters, including "
                   "whitespace, in the key file. For instance, check that "
                   "there is no newline after the key." << std::endl;
    }
    return false;
  }

  try {
    iostreams::stream_buffer<iostreams::file_source> source(
        key_file_path.string(), std::ios_base::in | std::ios_base::binary);
    istream source_stream(&source);
    if (!source.is_open() || !source_stream) {
      std::cerr << "ERROR: Unable to open " << key_name << " file '"
                << key_file_path.string() << "'." << std::endl;
      return false;
    }

    CHECK_NOTNULL(key)->CleanNew(key_length);
    source_stream.read(reinterpret_cast<char*>(key->data()), key->size());
  } catch (const filesystem::filesystem_error& ex) {
    std::cerr << "ERROR: Error reading " << key_name << " file '"
              << key_file_path.string() << "'. Details: " << ex.what()
              << std::endl;
    return false;
  } catch (...) {
    std::cerr << "ERROR: Error reading " << key_name << " file '"
              << key_file_path.string() << "'. (No details)" << std::endl;
    return false;
  }
  return true;
}

bool GenerateNewMasterKeyInFile(
    const boost::filesystem::path& master_key_file_path,
    const size_t key_length, CryptoPP::SecByteBlock* master_key) {
  if (filesystem::exists(master_key_file_path)) {
    std::cerr << "ERROR: Refusing to generate a new master key to path '"
              << master_key_file_path.string() << "'. This file already exists."
              << std::endl;
    return false;
  }

  CHECK_NOTNULL(master_key)->CleanNew(key_length);
  CryptoPP::AutoSeededX917RNG<CryptoPP::AES> rng;
  rng.GenerateBlock(master_key->data(), master_key->size());

  try {
    iostreams::stream_buffer<iostreams::file_sink> sink(
        master_key_file_path.string(),
        std::ios_base::out | std::ios_base::binary);
    ostream sink_stream(&sink);
    if (!sink.is_open() || !sink_stream) {
      std::cerr << "ERROR: Unable to open master key file '"
                << master_key_file_path.string() << "'." << std::endl;
      return false;
    }

    filesystem::permissions(master_key_file_path,
                            filesystem::owner_read | filesystem::owner_write);
    sink_stream.write(reinterpret_cast<char*>(master_key->data()),
                      master_key->size());
    filesystem::permissions(master_key_file_path, filesystem::owner_read);

  } catch (const filesystem::filesystem_error& ex) {
    std::cerr << "ERROR: Error writing new master key file '"
              << master_key_file_path.string() << "'. Details: " << ex.what()
              << std::endl;
    return false;
  } catch (...) {
    std::cerr << "ERROR: Error writing new master key file '"
              << master_key_file_path.string() << "'. (No details)"
              << std::endl;
    return false;
  }
  return true;
}

bool LoadMasterKeyFromFile(const boost::filesystem::path& master_key_file_path,
                           const size_t expected_key_length,
                           CryptoPP::SecByteBlock* master_key) {
  return LoadKeyFromFile("master key", master_key_file_path,
                         expected_key_length, master_key);
}

bool LoadAwsSecretKeyFromFile(
    const boost::filesystem::path& aws_secret_key_file_path,
    CryptoPP::SecByteBlock* aws_secret_key) {
  return LoadKeyFromFile("AWS secret key", aws_secret_key_file_path,
                         kAwsSecretKeyLength, aws_secret_key);
}

}  // namespace

bool LoadMasterKey(const size_t key_length,
                   CryptoPP::SecByteBlock* master_key) {
  if (options::master_key_file.empty()) {
    if (options::generate_new_master_key) {
      std::cerr << "ERROR: Asked to generate new master key, but no master key "
                   "file specified. Run with --help for usage instructions."
                << std::endl;
    }
    return false;
  }

  if (options::generate_new_master_key) {
    return GenerateNewMasterKeyInFile(options::master_key_file, key_length,
                                      master_key);
  } else {
    return LoadMasterKeyFromFile(options::master_key_file, key_length,
                                 master_key);
  }
}

bool LoadAwsKeys(string* aws_access_key,
                 CryptoPP::SecByteBlock* aws_secret_key) {
  if (options::aws_access_key.empty()) {
    std::cerr << "ERROR: Must specify AWS access key. Run with --help "
                 "for usage instructions." << std::endl;
    return false;
  }
  *aws_access_key = options::aws_access_key;

  if (aws_access_key->size() != kAwsAccessKeyLength) {
    std::cerr << "ERROR: AWS access key is the wrong length. Expected "
              << kAwsAccessKeyLength << " bytes, but provided key '"
              << *aws_access_key << "' is " << aws_access_key->size()
              << " bytes." << std::endl;
    return false;
  }

  if (options::aws_secret_key_file.empty()) {
    std::cerr << "ERROR: Must specify AWS secret key file. Run with --help "
                 "for usage instructions." << std::endl;
    return false;
  }
  return LoadAwsSecretKeyFromFile(options::aws_secret_key_file, aws_secret_key);
}

}  // namespace key_loading_util
}  // namespace polar_express
