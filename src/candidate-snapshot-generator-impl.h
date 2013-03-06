#ifndef CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H
#define CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H

#include <vector>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/function.hpp"
#include "boost/shared_ptr.hpp"

#include "candidate-snapshot-generator.h"
#include "macros.h"

namespace polar_express {

class Snapshot;

class CandidateSnapshotGeneratorImpl : public CandidateSnapshotGenerator {
 public:
  CandidateSnapshotGeneratorImpl();
  virtual ~CandidateSnapshotGeneratorImpl();

  using CandidateSnapshotGenerator::CandidateSnapshotCallback;

  virtual void GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      CandidateSnapshotCallback callback) const;
  
  using CandidateSnapshotGenerator::CandidateSnapshotsCallback;

  virtual void GenerateCandidateSnapshots(
      const string& root,
      const vector<filesystem::path>& paths,
      CandidateSnapshotsCallback callback,
      int callback_interval = 10) const;
  
 private:
  bool GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      Snapshot* candidate_snapshot) const;

  string RemoveRootFromPath(
      const string& root,
      const string& path_str) const;

  string GetUserNameFromUid(int uid) const;

  string GetGroupNameFromGid(int gid) const;
  
  DISALLOW_COPY_AND_ASSIGN(CandidateSnapshotGeneratorImpl);
};
  
}  // namespace polar_express

#endif  // CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H
