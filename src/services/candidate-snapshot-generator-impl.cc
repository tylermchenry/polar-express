#include "services/candidate-snapshot-generator-impl.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "proto/snapshot.pb.h"

namespace polar_express {

CandidateSnapshotGeneratorImpl::CandidateSnapshotGeneratorImpl()
  : CandidateSnapshotGenerator(false) {
}

CandidateSnapshotGeneratorImpl::~CandidateSnapshotGeneratorImpl() {
}

void CandidateSnapshotGeneratorImpl::GenerateCandidateSnapshot(
    const string& root,
    const filesystem::path& path,
    boost::shared_ptr<Snapshot>* snapshot_ptr,
    Callback callback) const {
  CHECK_NOTNULL(snapshot_ptr)->reset(new Snapshot);
  GenerateCandidateSnapshot(root, path, snapshot_ptr->get());
  callback();
}

bool CandidateSnapshotGeneratorImpl::GenerateCandidateSnapshot(
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

string CandidateSnapshotGeneratorImpl::RemoveRootFromPath(
    const string& root,
    const string& path_str) const {
  if (path_str.find(root) == 0) {
    return path_str.substr(root.length(), path_str.length() - root.length());
  }
  return path_str;
}

string CandidateSnapshotGeneratorImpl::GetUserNameFromUid(int uid) const {
  passwd pwd;
  passwd* tmp_pwdptr;
  char pwdbuf[256];

  if (getpwuid_r(uid, &pwd, pwdbuf, sizeof(pwdbuf), &tmp_pwdptr) == 0) {
    return pwd.pw_name;
  }
  return "";
}

string CandidateSnapshotGeneratorImpl::GetGroupNameFromGid(int gid) const {
  group grp;
  group* tmp_grpptr;
  char grpbuf[256];

  if (getgrgid_r(gid, &grp, grpbuf, sizeof(grpbuf), &tmp_grpptr) == 0) {
    return grp.gr_name;
  }
  return "";
}

}  // namespace polar_express
