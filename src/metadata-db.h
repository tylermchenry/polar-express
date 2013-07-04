#ifndef METADATA_DB_H
#define METADATA_DB_H

#include <memory>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class AnnotatedBundleData;
class File;
class MetadataDbImpl;
class Snapshot;

class MetadataDb {
 public:
  MetadataDb();
  virtual ~MetadataDb();

  virtual void GetLatestSnapshot(
      const File& file, boost::shared_ptr<Snapshot>* snapshot,
      Callback callback);

  // This also modifies the snapshot to add IDs for the snapshot itself as well
  // as any blocks that do not already have IDs.
  virtual void RecordNewSnapshot(
      boost::shared_ptr<Snapshot> snapshot, Callback callback);

  // This also modifies the bundle to add an ID for the bundle itself.
  virtual void RecordNewBundle(
      boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback);

 protected:
  explicit MetadataDb(bool create_impl);

 private:
  unique_ptr<MetadataDbImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDb);
};

}  // namespace polar_express

#endif  // METADATA_DB_H
