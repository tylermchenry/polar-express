#include "metadata-db.h"

#include "asio-dispatcher.h"
#include "metadata-db-impl.h"

namespace polar_express {

MetadataDb::MetadataDb()
    : impl_(new MetadataDbImpl) {
}

MetadataDb::MetadataDb(bool create_impl)
    : impl_(create_impl ? new MetadataDbImpl : nullptr) {
}

MetadataDb::~MetadataDb() {
}

void MetadataDb::GetLatestSnapshot(
    const File& file, boost::shared_ptr<Snapshot>* snapshot,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&MetadataDb::GetLatestSnapshot,
           impl_.get(), boost::cref(file), snapshot, callback));
}

void MetadataDb::RecordNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&MetadataDb::RecordNewSnapshot,
           impl_.get(), snapshot, callback));
}

void MetadataDb::RecordNewBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&MetadataDb::RecordNewBundle,
           impl_.get(), bundle, callback));
}

}  // polar_express
