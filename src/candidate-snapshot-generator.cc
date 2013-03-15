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
    boost::shared_ptr<Snapshot>* snapshot_ptr,   
    Callback callback) const {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&CandidateSnapshotGenerator::GenerateCandidateSnapshot,
           impl_.get(), root, path, snapshot_ptr, callback));
}

}  // namespace polar_express
