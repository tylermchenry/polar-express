#include <iostream>
#include <string>
#include <vector>

#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"

#include "filesystem-scanner.h"
#include "macros.h"

using namespace polar_express;

void PrintPaths(const vector<string>& paths) {
  for (const auto& path : paths) {
    cout << path << endl;
  }
}

void ScanFilesystemCallback(
    boost::shared_ptr<asio::io_service> io_service,
    const vector<string>& paths) {
  io_service->post(bind(&PrintPaths, paths));
}

void ScanFilesystem(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root) {
  FilesystemScanner fs_scanner;
  FilesystemScanner::FilePathsCallback callback =
      boost::bind(&ScanFilesystemCallback, io_service, _1);
  fs_scanner.Scan(root, callback);
}

void DoWork(boost::shared_ptr<asio::io_service> io_service) {
  io_service->run();
}

int main(int argc, char** argv) {
  boost::shared_ptr<asio::io_service> io_service(new asio::io_service);
  boost::shared_ptr<asio::io_service::work> work(
      new asio::io_service::work(*io_service));

  const int kNumWorkers = 4;
  thread_group worker_threads;
  for (int i = 0; i < kNumWorkers; ++i) {
    worker_threads.create_thread(bind(&DoWork, io_service));
  }

  if (argc > 1) {
    io_service->post(bind(&ScanFilesystem, io_service, argv[1]));
  }

  work.reset();
  worker_threads.join_all();
  
  return 0;
}
