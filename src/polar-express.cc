#include <ctime>
#include <iostream>
#include <string>

#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "backup-executor.h"
#include "cryptor.h"
#include "io-util.h"

using namespace polar_express;

// TODO(tylermchenry): User configurable, obviously.
const char kPassphrase[] = "correct horse battery staple";
const char kAwsRegionName[] = "";
const char kAwsAccessKey[] = "";
const byte kAwsSecretKey[] = "";
const char kAwsGlacierVaultName[] = "";

int main(int argc, char** argv) {
  if (argc > 1) {
    string root = argv[1];
    const time_t start_time = time(nullptr);
    AsioDispatcher::GetInstance()->Start();

    Cryptor::EncryptionType encryption_type = Cryptor::EncryptionType::kAES;
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data;

    {
      boost::shared_ptr<CryptoPP::SecByteBlock> passphrase(
          new CryptoPP::SecByteBlock);
      passphrase->Assign(reinterpret_cast<const byte*>(kPassphrase),
                         sizeof(kPassphrase));

      Cryptor::KeyingData tmp_keying_data;
      Cryptor::DeriveKeysFromPassphrase(
          passphrase, Cryptor::EncryptionType::kAES, &tmp_keying_data);
      encryption_keying_data.reset(new Cryptor::KeyingData(tmp_keying_data));
    }

    // Be sure not to include the trailing NUL in the key block.
    const CryptoPP::SecByteBlock aws_secret_key(kAwsSecretKey,
                                                sizeof(kAwsSecretKey) - 1);

    BackupExecutor backup_executor;
    backup_executor.Start(root, encryption_type, encryption_keying_data,
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
  }
  return 0;
}
