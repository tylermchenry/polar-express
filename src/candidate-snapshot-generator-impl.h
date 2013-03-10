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
  bool GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      Snapshot* candidate_snapshot) const;

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
