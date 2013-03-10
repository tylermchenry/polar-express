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
    Callback callback) const {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&CandidateSnapshotGenerator::GenerateCandidateSnapshot,
           impl_.get(), root, path, callback));
}

boost::shared_ptr<Snapshot>
CandidateSnapshotGenerator::GetGeneratedCandidateSnapshot() const {
  return impl_->GetGeneratedCandidateSnapshot();
}

}  // namespace polar_express
