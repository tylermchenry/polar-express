#ifndef METADATA_DB_IMPL_H
#define METADATA_DB_IMPL_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"
#include "metadata-db.h"

class sqlite3;

namespace polar_express {

class File;
class Snapshot;

class MetadataDbImpl : public MetadataDb {
 public:
  MetadataDbImpl();
  virtual ~MetadataDbImpl();

  virtual void ReadLatestSnapshot(
      const File& file, boost::shared_ptr<Snapshot> snapshot,
      Callback callback);

  virtual void RecordNewSnapshot(
      boost::shared_ptr<Snapshot> snapshot, Callback callback);
  
 private:
  void WriteNewBlocks(boost::shared_ptr<Snapshot> snapshot) const;
  
  static sqlite3* db_;
  
  static sqlite3* db();
  static void InitDb();

  DISALLOW_COPY_AND_ASSIGN(MetadataDbImpl);  
};

}  // namespace polar_express

#endif  // METADATA_DB_IMPL_H
