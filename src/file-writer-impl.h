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

  virtual void WriteDataToPath(
      const vector<char>& data, const boost::filesystem::path& path,
      Callback callback);

  virtual void WriteDataToTemporaryFile(
      const vector<char>& data, boost::filesystem::path* path,
      Callback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileWriterImpl);
};

}   // namespace file_writer

#endif  // FILE_WRITER_IMPL_H
