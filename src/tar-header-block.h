#ifndef TAR_HEADER_BLOCK_H
#define TAR_HEADER_BLOCK_H

#include <ctime>

#include <unistd.h>

#include "macros.h"
#include "container-offset-ptr.h"

namespace polar_express {

struct TarFileHeader;

class TarHeaderBlock {
 public:
  static const size_t kTarHeaderBlockLength = 512;

  explicit TarHeaderBlock(ContainerOffsetPtr<char> data);

  string filename() const;
  void set_filename(const string& filename);

  int mode() const;
  void set_mode(int mode);

  uid_t owner_uid() const;
  void set_owner_uid(uid_t owner_uid);

  gid_t owner_gid() const;
  void set_owner_gid(gid_t owner_gid);

  size_t size() const;
  void set_size(size_t size);

  time_t modification_time() const;
  void set_modification_time(time_t time);

  int checksum() const;
  void set_checksum(int checksum);
  void ComputeAndSetChecksum();

  // Link indicator and link filename not supported.

 private:
  TarFileHeader* header();
  const TarFileHeader* header() const;

  ContainerOffsetPtr<char> data_;

  DISALLOW_COPY_AND_ASSIGN(TarHeaderBlock);
};

}  // namespace polar_express

#endif  // TAR_HEADER_BLOCK_H
