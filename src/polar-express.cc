#include <iostream>
#include <string>
#include <vector>

#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"

#include "candidate-snapshot-generator.h"
#include "filesystem-scanner.h"
#include "macros.h"
#include "proto/snapshot.pb.h"

using namespace polar_express;

boost::mutex output_mutex;

void PrintCandidateSnapshots(
    const vector<boost::shared_ptr<Snapshot> >& candidate_snapshots) {
  unique_lock<mutex> output_lock(output_mutex);
  for (const auto& snapshot_ptr : candidate_snapshots) {
    cout << snapshot_ptr->DebugString();
  }
}

void GenerateCandidateSnapshotsCallback(
    boost::shared_ptr<asio::io_service> io_service,
    const vector<boost::shared_ptr<Snapshot> >& candidate_snapshots) {
  io_service->post(bind(&PrintCandidateSnapshots, candidate_snapshots));
}

void GenerateCandidateSnapshots(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root,
    const vector<filesystem::path>& paths) {
  CandidateSnapshotGenerator cs_generator;
  CandidateSnapshotGenerator::CandidateSnapshotsCallback callback =
      bind(&GenerateCandidateSnapshotsCallback, io_service, _1);
  cs_generator.GenerateCandidateSnapshots(root, paths, callback);
}

void ScanFilesystemCallback(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root,
    const vector<filesystem::path>& paths) {
  io_service->post(
      bind(&GenerateCandidateSnapshots, io_service, root, paths));
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
