#include <iostream>
#include <string>
#include <vector>

#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"

#include "candidate-snapshot-generator.h"
#include "filesystem-scanner.h"
#include "macros.h"
#include "proto/snapshot.pb.h"
#include "snapshot-state-machine.h"

using namespace polar_express;

void DeleteSnapshotStateMachine(
    SnapshotStateMachine::BackEnd* snapshot_state_machine) {
  delete snapshot_state_machine;
}

void StartSnapshotStateMachine(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root, filesystem::path filepath) {
  SnapshotStateMachine::BackEnd* snapshot_state_machine = 
      new SnapshotStateMachine::BackEnd(io_service);
  snapshot_state_machine->Initialize(
      snapshot_state_machine, &DeleteSnapshotStateMachine);

  SnapshotStateMachine::NewFilePathReady event;
  event.root_ = root;
  event.filepath_ = filepath;
  snapshot_state_machine->enqueue_event(event);

  io_service->post(
      bind(&SnapshotStateMachine::ExecuteEventsCallback,
           snapshot_state_machine));
}

void ScanFilesystemCallback(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root,
    const vector<filesystem::path>& paths) {
  for (const auto& path : paths) {
    StartSnapshotStateMachine(io_service, root, path);
  }
}

void ScanFilesystem(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root) {
  FilesystemScanner fs_scanner;
  FilesystemScanner::FilePathsCallback callback =
      bind(&ScanFilesystemCallback, io_service, root, _1);
  fs_scanner.Scan(root, callback);
}

void DoWork(boost::shared_ptr<asio::io_service> io_service) {
  io_service->run();
}

int main(int argc, char** argv) {
  boost::shared_ptr<asio::io_service> io_service(new asio::io_service);
  boost::shared_ptr<asio::io_service::work> work(
      new asio::io_service::work(*io_service));

  const int kNumWorkers = 1;
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
