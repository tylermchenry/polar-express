#ifndef CANDIDATE_SNAPSHOT_GENERATOR_H
#define CANDIDATE_SNAPSHOT_GENERATOR_H

#include <string>

#include "boost/filesystem.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class CandidateSnapshotGeneratorImpl;
class Snapshot;

// A class that reads filesystem metadata and generates candidate snapshot
// protobufs for a given collection of file paths. A callback is repeatedly
// invoked with groups of candidate snapshots.
class CandidateSnapshotGenerator {
 public:
  CandidateSnapshotGenerator();
  virtual ~CandidateSnapshotGenerator();

  // Generates one candidate snapshot for the given path, and invokes the given
  // callback.
  virtual void GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      Callback callback) const;

  virtual boost::shared_ptr<Snapshot> GetGeneratedCandidateSnapshot() const;
  
 protected:
  explicit CandidateSnapshotGenerator(bool create_impl);
  
 private:
  scoped_ptr<CandidateSnapshotGeneratorImpl> impl_;
  
  DISALLOW_COPY_AND_ASSIGN(CandidateSnapshotGenerator);
};
  
}  // namespace polar_express

#endif  // CANDIDATE_SNAPSHOT_GENERATOR_H
