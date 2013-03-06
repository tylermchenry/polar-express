#include "candidate-snapshot-generator.h"

#include "boost/bind.hpp"

#include "asio-dispatcher.h"
#include "candidate-snapshot-generator-impl.h"

namespace polar_express {

CandidateSnapshotGenerator::CandidateSnapshotGenerator()
    : impl_(new CandidateSnapshotGeneratorImpl) {
}

CandidateSnapshotGenerator::CandidateSnapshotGenerator(bool create_impl)
    : impl_(create_impl ? new CandidateSnapshotGeneratorImpl : nullptr) {
}

CandidateSnapshotGenerator::~CandidateSnapshotGenerator() {
}

void CandidateSnapshotGenerator::GenerateCandidateSnapshot(
    const string& root,
    const filesystem::path& path,
    CandidateSnapshotCallback callback) const {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&CandidateSnapshotGenerator::GenerateCandidateSnapshot,
           impl_.get(), root, path, callback));
}

void CandidateSnapshotGenerator::GenerateCandidateSnapshots(
    const string& root,
    const vector<filesystem::path>& paths,
    CandidateSnapshotsCallback callback,
    int callback_interval) const {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&CandidateSnapshotGenerator::GenerateCandidateSnapshots,
           impl_.get(), root, paths, callback, callback_interval));
}

}  // namespace polar_express
