#include "util/key-loading-util.h"

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>

#include "base/options.h"

DECLARE_OPTION(aws_access_key, string);
DECLARE_OPTION(aws_secret_key_file, string);

namespace polar_express {
namespace key_loading_util {
namespace {

const size_t kAwsAccessKeyLength = 20;
const size_t kAwsSecretKeyLength = 40;

bool LoadAwsSecretKeyFromFile(
    const boost::filesystem::path& aws_secret_key_file_path,
    CryptoPP::SecByteBlock* aws_secret_key) {
  filesystem::file_status file_stat;
  size_t key_size;
  try {
    file_stat = status(aws_secret_key_file_path);
    key_size = filesystem::file_size(aws_secret_key_file_path);
  } catch (const filesystem::filesystem_error& ex) {
    std::cerr << "ERROR: Error accessing AWS secret key file '"
              << aws_secret_key_file_path.string()
              << "'. Details: " << ex.what() << std::endl;
    return false;
  }

  if (file_stat.permissions() & filesystem::group_read ||
      file_stat.permissions() & filesystem::others_read) {
    std::cerr
        << "ERROR: Permissions on AWS secret key file '"
        << aws_secret_key_file_path.string()
        << "' are set poorly! This file should be readable only by its owner."
        << " Refusing to use this key file." << std::endl;
    return false;
  }

  if (key_size != kAwsSecretKeyLength) {
    std::cerr << "ERROR: AWS access key is the wrong length. Expected "
              << kAwsSecretKeyLength << " bytes, but provided key file '"
              << aws_secret_key_file_path.string() << "' is " << key_size
              << " bytes." << std::endl;
    if (key_size > kAwsSecretKeyLength) {
      std::cerr << "Check that there are no stray characters, including "
          "whitespace, in the key file. For instance, check that "
          "there is newline after the key." << std::endl;
    }
    return false;
  }

  try {
    iostreams::stream_buffer<iostreams::file_source> source(
        aws_secret_key_file_path.string());
    istream source_stream(&source);
    if (!source.is_open() || !source_stream) {
      std::cerr << "ERROR: Unable to open AWS secret key file '"
                << aws_secret_key_file_path.string() << "'." << std::endl;
      return false;
    }

    CHECK_NOTNULL(aws_secret_key)->CleanNew(key_size);
    source_stream.read(reinterpret_cast<char*>(aws_secret_key->data()),
                       aws_secret_key->size());
  } catch (const filesystem::filesystem_error& ex) {
    std::cerr << "ERROR: Error reading AWS secret key file '"
              << aws_secret_key_file_path.string()
              << "'. Details: " << ex.what() << std::endl;
    return false;
  } catch (...) {
    std::cerr << "ERROR: Error reading AWS secret key file '"
              << aws_secret_key_file_path.string()
              << "'. (No details)" << std::endl;
    return false;
  }
  return true;
}

}  // namespace

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
