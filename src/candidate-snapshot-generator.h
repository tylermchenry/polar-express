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

// A class that asynchronously reads filesystem metadata and generates candidate
// snapshot protobufs for a given file path.
//
// This class is the asynchronous stub. Calls to its asynchronous methods post a
// task which will invoke the equivalent method on its implementation
// class. Calls to other methods are forwarded directly to the implementation.
class CandidateSnapshotGenerator {
 public:
  CandidateSnapshotGenerator();
  virtual ~CandidateSnapshotGenerator();

  // Asynchronously generates the candidate snapshot for the given root and
  // path, and invokes the given callback when done.
  virtual void GenerateCandidateSnapshot(
      const string& root,
      const filesystem::path& path,
      boost::shared_ptr<Snapshot>* snapshot_ptr,
      Callback callback) const;

 protected:
  explicit CandidateSnapshotGenerator(bool create_impl);

 private:
  scoped_ptr<CandidateSnapshotGeneratorImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(CandidateSnapshotGenerator);
};

}  // namespace polar_express

#endif  // CANDIDATE_SNAPSHOT_GENERATOR_H
