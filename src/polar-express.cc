#include <iostream>
#include <string>

#include "boost/shared_ptr.hpp"

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
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data;

    {
      Cryptor::KeyingData tmp_keying_data;
      tmp_keying_data.passphrase.Assign(
          reinterpret_cast<const byte*>(kPassphrase), sizeof(kPassphrase));
      encryption_keying_data.reset(new Cryptor::KeyingData(tmp_keying_data));
    }

    BackupExecutor backup_executor;
    backup_executor.Start(root, encryption_type, encryption_keying_data);

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
