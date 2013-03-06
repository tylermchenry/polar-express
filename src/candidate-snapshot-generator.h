#ifndef CANDIDATE_SNAPSHOT_GENERATOR_H
#define CANDIDATE_SNAPSHOT_GENERATOR_H

#include <vector>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/function.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

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

  typedef boost::function<void(boost::shared_ptr<Snapshot>)>
  CandidateSnapshotCallback;

  // Generates one candidate snapshot for the given path, and invokes the given
  // callback.
  virtual void GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      CandidateSnapshotCallback callback) const;
  
  typedef boost::function<void(const vector<boost::shared_ptr<Snapshot> >&)>
  CandidateSnapshotsCallback;

  // Generates candidate snapshots for each of the given paths. Every time
  // callback_interval candidate snapshots are generated, the callback is
  // invoked with a vector of these snapshots. This method blocks until it has
  // attempted to generate candidate snapshots for all paths.
  virtual void GenerateCandidateSnapshots(
      const string& root,
      const vector<filesystem::path>& paths,
      CandidateSnapshotsCallback callback,
      int callback_interval = 10) const;

 protected:
  explicit CandidateSnapshotGenerator(bool create_impl);
  
 private:
  scoped_ptr<CandidateSnapshotGeneratorImpl> impl_;
  
  DISALLOW_COPY_AND_ASSIGN(CandidateSnapshotGenerator);
};
  
}  // namespace polar_express

#endif  // CANDIDATE_SNAPSHOT_GENERATOR_H
