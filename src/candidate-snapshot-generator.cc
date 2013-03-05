#include "candidate-snapshot-generator.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

#include "boost/thread.hpp"

#include "proto/snapshot.pb.h"

namespace polar_express {

CandidateSnapshotGenerator::CandidateSnapshotGenerator() {
}

CandidateSnapshotGenerator::~CandidateSnapshotGenerator() {
}

void CandidateSnapshotGenerator::GenerateCandidateSnapshot(
    const string& root,
    const filesystem::path& path,
    CandidateSnapshotCallback callback) const {
  boost::shared_ptr<Snapshot> candidate_snapshot(new Snapshot);
  // TODO: Error reporting?
  GenerateCandidateSnapshot(root, path, candidate_snapshot.get());
  callback(candidate_snapshot);
}

void CandidateSnapshotGenerator::GenerateCandidateSnapshots(
    const string& root,
    const vector<filesystem::path>& paths,
    CandidateSnapshotsCallback callback,
    int callback_interval) const {
  vector<boost::shared_ptr<Snapshot> > candidate_snapshots;
  for (const auto& path : paths) {
    boost::shared_ptr<Snapshot> candidate_snapshot(new Snapshot);
    if (GenerateCandidateSnapshot(root, path, candidate_snapshot.get())) {
      candidate_snapshots.push_back(candidate_snapshot);
      if (candidate_snapshots.size() >= callback_interval) {
        this_thread::interruption_point();
        callback(candidate_snapshots);
        candidate_snapshots.clear();
      }
    }
  }

  if (!candidate_snapshots.empty()) {
    callback(candidate_snapshots);
  }
}

bool CandidateSnapshotGenerator::GenerateCandidateSnapshot(
    const string& root,
    const filesystem::path& path,
    Snapshot* candidate_snapshot) const {
  assert(candidate_snapshot != nullptr);
  
  system::error_code ec;
  filesystem::path canonical_path = canonical(path, root, ec);
  if (ec) {
    return false;
  }
  
  string canonical_path_str = canonical_path.string();
  if (canonical_path_str.empty()) {
    return false;
  }

  // Generic platform-independent stuff.
  filesystem::file_status file_stat = status(path);

  // Unix-specific stuff. TODO: Deal with Windows FS as well.
  struct stat unix_stat;
  stat(canonical_path_str.c_str(), &unix_stat);

  File* file = candidate_snapshot->mutable_file();
  file->set_path(RemoveRootFromPath(root, canonical_path_str));
  
  Attributes* attribs = candidate_snapshot->mutable_attributes();
  attribs->set_owner_user(GetUserNameFromUid(unix_stat.st_uid));
  attribs->set_owner_group(GetGroupNameFromGid(unix_stat.st_gid));
  attribs->set_uid(unix_stat.st_uid);
  attribs->set_gid(unix_stat.st_gid);
  attribs->set_mode(file_stat.permissions());
  
  candidate_snapshot->set_modification_time(last_write_time(canonical_path));
  candidate_snapshot->set_is_regular(is_regular_file(canonical_path));
  candidate_snapshot->set_is_deleted(!exists(canonical_path));
  if (is_regular_file(canonical_path)) {
    candidate_snapshot->set_length(file_size(canonical_path));
  }
  candidate_snapshot->set_observation_time(time(NULL));
  return true;
}

string CandidateSnapshotGenerator::RemoveRootFromPath(
    const string& root,
    const string& path_str) const {
  if (path_str.find(root) == 0) {
    return path_str.substr(root.length(), path_str.length() - root.length());
  }
  return path_str;
}

string CandidateSnapshotGenerator::GetUserNameFromUid(int uid) const {
  passwd pwd;
  passwd* tmp_pwdptr;
  char pwdbuf[256];

  if (getpwuid_r(uid, &pwd, pwdbuf, sizeof(pwdbuf), &tmp_pwdptr) == 0) {
    return pwd.pw_name;
  }
  return "";
}

string CandidateSnapshotGenerator::GetGroupNameFromGid(int gid) const {
  group grp;
  group* tmp_grpptr;
  char grpbuf[256];

  if (getgrgid_r(gid, &grp, grpbuf, sizeof(grpbuf), &tmp_grpptr) == 0) {
    return grp.gr_name;
  }
  return "";
}

}  // namespace polar_express
