// Polar Express
// Copyright (C) 2014  Tyler McHenry
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <ctime>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <crypto++/secblock.h>

#include "backup-executor.h"
#include "base/asio-dispatcher.h"
#include "base/options.h"
#include "services/cryptor.h"
#include "util/io-util.h"
#include "util/key-loading-util.h"

using namespace polar_express;

DEFINE_OPTION(passphrase, string, "", "Passphrase for encrypting backups.");
DEFINE_OPTION(aws_region_name, string, "",
              "Amazon Web Services region (e.g. 'us-west').");
DEFINE_OPTION(aws_glacier_vault_name, string, "",
              "Name of Glacier vault in which to store backups.");
DEFINE_OPTION(backup_root, string, "", "Local path to back up.");

// Initializes the KeyingData structure with derived keys for use in client-side
// encryption. Importantly, once this function returns, the master key is no
// longer in memory (unless the user has elected to encrypt data directly with
// the master key).
bool InitializeEncryptionKeyingData(
    const Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData>* encryption_keying_data) {
  Cryptor::KeyingData tmp_encryption_keying_data;
  boost::shared_ptr<CryptoPP::SecByteBlock> master_key(
      new CryptoPP::SecByteBlock);
  if (key_loading_util::LoadMasterKey(Cryptor::GetKeyLength(encryption_type),
                                      master_key.get())) {
    Cryptor::DeriveKeysFromMasterKey(master_key, encryption_type,
                                     &tmp_encryption_keying_data);
  } else if (!options::passphrase.empty()) {
    boost::shared_ptr<CryptoPP::SecByteBlock> passphrase(
        new CryptoPP::SecByteBlock(
            reinterpret_cast<const byte*>(options::passphrase.c_str()),
            options::passphrase.size()));
    Cryptor::DeriveKeysFromPassphrase(passphrase, Cryptor::EncryptionType::kAES,
                                      &tmp_encryption_keying_data);
  } else {
    std::cerr << "ERROR: Unable to load or master key, and no passphrase "
              << "is specified. Run with --help for usage instructions."
              << std::endl;
    return false;
  }

  CHECK_NOTNULL(encryption_keying_data)
      ->reset(new Cryptor::KeyingData(tmp_encryption_keying_data));
  return true;
}

int main(int argc, char** argv) {
  if (!options::Init(argc, argv)) {
    return 0;
  }

  const time_t start_time = time(nullptr);
  AsioDispatcher::GetInstance()->Start();

  // TODO: Encryption type should be configurable.
  const Cryptor::EncryptionType encryption_type = Cryptor::EncryptionType::kAES;
  boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data;
  if (!InitializeEncryptionKeyingData(encryption_type,
                                      &encryption_keying_data)) {
    std::cerr << "FATAL: Failed to initialize encryption keying data."
              << std::endl;
    return -1;
  }

  string aws_access_key;
  CryptoPP::SecByteBlock aws_secret_key;
  if (!key_loading_util::LoadAwsKeys(&aws_access_key, &aws_secret_key)) {
    std::cerr << "FATAL: Failed to load AWS keys." << std::endl;
    return -1;
  }

  BackupExecutor backup_executor;
  backup_executor.Start(options::backup_root, encryption_type,
                        encryption_keying_data, options::aws_region_name,
                        aws_access_key, aws_secret_key,
                        options::aws_glacier_vault_name);

  AsioDispatcher::GetInstance()->WaitForFinish();
  const time_t end_time = time(nullptr);

  std::cout << "Processed " << backup_executor.GetNumFilesProcessed()
            << " files ("
            << io_util::HumanReadableSize(
                backup_executor.GetSizeOfFilesProcessed())
            << ")." << std::endl;
  std::cout << "Generated " << backup_executor.GetNumSnapshotsGenerated()
            << " new snapshots ("
            << io_util::HumanReadableSize(
                backup_executor.GetSizeOfSnapshotsGenerated()) << ")."
            << std::endl;
  std::cout << "Generated " << backup_executor.GetNumBundlesGenerated()
            << " new bundles ("
            << io_util::HumanReadableSize(
                backup_executor.GetSizeOfBundlesGenerated()) << ")."
            << std::endl;
  std::cout << "Uploaded " << backup_executor.GetNumBundlesUploaded()
            << " new bundles ("
            << io_util::HumanReadableSize(
                backup_executor.GetSizeOfBundlesUploaded()) << ")."
            << std::endl;
  std::cout << "Took "
            << io_util::HumanReadableDuration(end_time - start_time) << "."
            << std::endl;

  return 0;
}
