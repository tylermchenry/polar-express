#include <iostream>
#include <string>

#include "asio-dispatcher.h"
#include "backup-executor.h"

using namespace polar_express;

int main(int argc, char** argv) {
  if (argc > 1) {
    string root = argv[1];
    BackupExecutor backup_executor;

    AsioDispatcher::GetInstance()->Start();
    backup_executor.Start(root);
    AsioDispatcher::GetInstance()->WaitForFinish();

    std::cout << "Processed " << backup_executor.GetNumFilesProcessed()
              << " files." << std::endl;
  }
  return 0;
}
