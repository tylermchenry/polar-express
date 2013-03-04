#include <grp.h>
#include <iostream>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <vector>

#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"

#include "filesystem-scanner.h"
#include "macros.h"
#include "proto/snapshot.pb.h"

using namespace polar_express;

boost::mutex output_mutex;

string GetUserNameFromUid(int uid) {
  passwd pwd;
  passwd* tmp_pwdptr;
  char pwdbuf[256];

  if (getpwuid_r(uid, &pwd, pwdbuf, sizeof(pwdbuf), &tmp_pwdptr) == 0) {
    return pwd.pw_name;
  }
  return "";
}

string GetGroupNameFromGid(int gid) {
  group grp;
  group* tmp_grpptr;
  char grpbuf[256];

  if (getgrgid_r(gid, &grp, grpbuf, sizeof(grpbuf), &tmp_grpptr) == 0) {
    return grp.gr_name;
  }
  return "";
}

void AnalyzePath(const string& root, const filesystem::path& path) {
  system::error_code ec;
  filesystem::path canonical_path = canonical(path, root, ec);
  if (ec) {
    return;
  }
  
  string canonical_path_str = canonical_path.string();
  if (canonical_path_str.empty()) {
    return;
  }
 
  filesystem::file_status file_stat = status(path);

  // Unix-specific stuff. TODO: Deal with Windows FS as well
  struct stat unix_stat;
  stat(canonical_path_str.c_str(), &unix_stat);

  Snapshot snapshot;
  File* file = snapshot.mutable_file();
  Attributes* attribs = snapshot.mutable_attributes();
  file->set_path(canonical_path_str);
  attribs->set_owner_user(GetUserNameFromUid(unix_stat.st_uid));
  attribs->set_owner_group(GetGroupNameFromGid(unix_stat.st_gid));
  attribs->set_uid(unix_stat.st_uid);
  attribs->set_gid(unix_stat.st_gid);
  attribs->set_mode(file_stat.permissions());
  snapshot.set_modification_time(last_write_time(canonical_path));
  snapshot.set_is_regular(is_regular_file(canonical_path));
  snapshot.set_is_deleted(!exists(canonical_path));
  if (is_regular_file(canonical_path)) {
    snapshot.set_length(file_size(canonical_path));
  }
  snapshot.set_observation_time(time(NULL));
  
  unique_lock<mutex> output_lock(output_mutex);
  cout << snapshot.DebugString();
}

void AnalyzePaths(
    const string& root,
    const vector<filesystem::path>& paths) {
  for (const auto& path : paths) {
    AnalyzePath(root, path);
  }
}

void ScanFilesystemCallback(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root,
    const vector<filesystem::path>& paths) {
  io_service->post(bind(&AnalyzePaths, root, paths));
}

void ScanFilesystem(
    boost::shared_ptr<asio::io_service> io_service,
    const string& root) {
  FilesystemScanner fs_scanner;
  FilesystemScanner::FilePathsCallback callback =
      boost::bind(&ScanFilesystemCallback, io_service, root, _1);
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
