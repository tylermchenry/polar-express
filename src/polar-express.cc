#include <ctime>
#include <iostream>
#include <string>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/shared_ptr.hpp>

#include "backup-executor.h"
#include "base/asio-dispatcher.h"
#include "services/cryptor.h"
#include "util/io-util.h"

using namespace polar_express;

bool InitOptions(int argc, char** argv, program_options::variables_map* vm) {
  CHECK_NOTNULL(vm);

  program_options::options_description desc("Options");
  desc.add_options()
      ("help", "Produce this message.")
      ("passphrase", program_options::value<string>(),
       "Passphrase for encrypting backups.")
      ("aws_region_name", program_options::value<string>(),
       "Amazon Web Services region (e.g. 'us-west').")
      ("aws_access_key", program_options::value<string>(),
       "Amazon Web Services access key.")
      ("aws_secret_key", program_options::value<string>(),
       "Amazon Web Services secret key.")
      ("aws_glacier_vault_name", program_options::value<string>(),
       "Name of Glacier vault in which to store backups.")
      ("backup_root", program_options::value<string>(),
       "Local path to back up.");

  program_options::positional_options_description pd;
  pd.add("backup_root", 1);

  program_options::store(
      program_options::parse_command_line(argc, argv, desc), *vm);
  program_options::notify(*vm);

  if (vm->count("help")) {
    std::cout << desc << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  program_options::variables_map vm;
  if (!InitOptions(argc, argv, &vm)) {
    return 0;
  }

  const string kPassphrase(vm["passphrase"].as<string>());
  const string kAwsRegionName(vm["aws_region_name"].as<string>());
  const string kAwsAccessKey(vm["aws_access_key"].as<string>());
  const string kAwsSecretKey(vm["aws_secret_key"].as<string>());
  const string kAwsGlacierVaultName(vm["aws_glacier_vault_name"].as<string>());
  const string kBackupRoot(vm["backup_root"].as<string>());

  const time_t start_time = time(nullptr);
  AsioDispatcher::GetInstance()->Start();

  Cryptor::EncryptionType encryption_type = Cryptor::EncryptionType::kAES;
  boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data;

  {
    boost::shared_ptr<CryptoPP::SecByteBlock> passphrase(
        new CryptoPP::SecByteBlock);
    passphrase->Assign(reinterpret_cast<const byte*>(kPassphrase.c_str()),
                       kPassphrase.size());

    Cryptor::KeyingData tmp_keying_data;
    Cryptor::DeriveKeysFromPassphrase(
        passphrase, Cryptor::EncryptionType::kAES, &tmp_keying_data);
    encryption_keying_data.reset(new Cryptor::KeyingData(tmp_keying_data));
  }

  const CryptoPP::SecByteBlock aws_secret_key(
      reinterpret_cast<const byte*>(kAwsSecretKey.c_str()),
      kAwsSecretKey.size());

  BackupExecutor backup_executor;
  backup_executor.Start(kBackupRoot, encryption_type, encryption_keying_data,
                        kAwsRegionName, kAwsAccessKey, aws_secret_key,
                        kAwsGlacierVaultName);

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
