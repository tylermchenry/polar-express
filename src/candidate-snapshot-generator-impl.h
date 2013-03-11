#ifndef CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H
#define CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H

#include <string>

#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/mutex.hpp"

#include "callback.h"
#include "candidate-snapshot-generator.h"
#include "macros.h"

namespace polar_express {

class Snapshot;

// This class is the synchronous implementation of the asynchronous stub class
// CandidateSnapshotGenerator. Do not use it directly; use the stub instead. See
// the stub class header for documentation on the behavior of the public
// methods.
class CandidateSnapshotGeneratorImpl : public CandidateSnapshotGenerator {
 public:
  CandidateSnapshotGeneratorImpl();
  virtual ~CandidateSnapshotGeneratorImpl();

  virtual void GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      Callback callback) const;

  virtual boost::shared_ptr<Snapshot> GetGeneratedCandidateSnapshot() const;
  
 private:
  // Fills the candidate snapshot information into a pre-existing snapshot
  // protocol buffer.
  bool GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      Snapshot* candidate_snapshot) const;

  // If path_str starts with root, returns path_str with the root prefix
  // removed. Otherwise return path_str as-is.
  string RemoveRootFromPath(
      const string& root,
      const string& path_str) const;

  string GetUserNameFromUid(int uid) const;

  string GetGroupNameFromGid(int gid) const;

  boost::shared_ptr<Snapshot> candidate_snapshot_;
  
  DISALLOW_COPY_AND_ASSIGN(CandidateSnapshotGeneratorImpl);
};
  
}  // namespace polar_express

#endif  // CANDIDATE_SNAPSHOT_GENERATOR_IMPL_H
