#include <iostream>
#include <string>

#include "boost/shared_ptr.hpp"
#include "crypto++/secblock.h"

#include "asio-dispatcher.h"
#include "backup-executor.h"
#include "cryptor.h"

using namespace polar_express;

// TODO(tylermchenry): User configurable, obviously.
const char kPassphrase[] = "correct horse battery staple";

int main(int argc, char** argv) {
  if (argc > 1) {
    string root = argv[1];
    AsioDispatcher::GetInstance()->Start();

    Cryptor::EncryptionType encryption_type = Cryptor::EncryptionType::kAES;
    boost::shared_ptr<const CryptoPP::SecByteBlock> encryption_key;

    {
      CryptoPP::SecByteBlock passphrase(
          reinterpret_cast<const byte*>(kPassphrase), sizeof(kPassphrase));
      encryption_key =
          Cryptor::CreateCryptor(encryption_type)->DeriveKeyFromPassword(
              passphrase, {});
    }

    BackupExecutor backup_executor;
    backup_executor.Start(root, encryption_type, encryption_key);

    AsioDispatcher::GetInstance()->WaitForFinish();
    std::cout << "Processed " << backup_executor.GetNumFilesProcessed()
              << " files." << std::endl;
    std::cout << "Generated " << backup_executor.GetNumSnapshotsGenerated()
              << " new snapshots." << std::endl;
    std::cout << "Generated " << backup_executor.GetNumBundlesGenerated()
              << " new bundles." << std::endl;

  }
  return 0;
}
