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

using namespace polar_express;

DEFINE_OPTION(passphrase, string, "", "Passphrase for encrypting backups.");
DEFINE_OPTION(aws_region_name, string, "",
              "Amazon Web Services region (e.g. 'us-west').");
DEFINE_OPTION(aws_access_key, string, "", "Amazon Web Services access key.");
DEFINE_OPTION(aws_secret_key, string, "", "Amazon Web Services secret key.");
DEFINE_OPTION(aws_glacier_vault_name, string, "",
              "Name of Glacier vault in which to store backups.");
DEFINE_OPTION(backup_root, string, "", "Local path to back up.");

int main(int argc, char** argv) {
  if (!options::Init(argc, argv)) {
    return 0;
  }

  const time_t start_time = time(nullptr);
  AsioDispatcher::GetInstance()->Start();

  Cryptor::EncryptionType encryption_type = Cryptor::EncryptionType::kAES;
  boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data;

  {
    boost::shared_ptr<CryptoPP::SecByteBlock> passphrase(
        new CryptoPP::SecByteBlock);
    passphrase->Assign(
        reinterpret_cast<const byte*>(options::passphrase.c_str()),
        options::passphrase.size());

    Cryptor::KeyingData tmp_keying_data;
    Cryptor::DeriveKeysFromPassphrase(
        passphrase, Cryptor::EncryptionType::kAES, &tmp_keying_data);
    encryption_keying_data.reset(new Cryptor::KeyingData(tmp_keying_data));
  }

  const CryptoPP::SecByteBlock aws_secret_key(
      reinterpret_cast<const byte*>(options::aws_secret_key.c_str()),
      options::aws_secret_key.size());

  BackupExecutor backup_executor;
  backup_executor.Start(options::backup_root, encryption_type,
                        encryption_keying_data, options::aws_region_name,
                        options::aws_access_key, aws_secret_key,
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
