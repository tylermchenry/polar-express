#ifndef FILE_WRITER_IMPL_H
#define FILE_WRITER_IMPL_H

#include <vector>

#include "boost/filesystem.hpp"

#include "callback.h"
#include "file-writer.h"
#include "macros.h"

namespace polar_express {

class FileWriterImpl : public FileWriter {
 public:
  FileWriterImpl();
  virtual ~FileWriterImpl();

  virtual void WriteSequentialDataToPath(
      const vector<const vector<byte>*>& sequential_data,
      const boost::filesystem::path& path,
      Callback callback);

  virtual void WriteSequentialDataToTemporaryFile(
      const vector<const vector<byte>*>& sequential_data,
      const string& filename_prefix,
      string* path_str, Callback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileWriterImpl);
};

}   // namespace file_writer

#endif  // FILE_WRITER_IMPL_H
