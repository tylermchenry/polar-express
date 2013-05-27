#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class FileWriterImpl;

class FileWriter {
 public:
  FileWriter();
  virtual ~FileWriter();

  virtual void WriteDataToPath(
      const string& data, const boost::filesystem::path& path,
      Callback callback);

  virtual void WriteDataToTemporaryFile(
      const string& data, string* path_str, Callback callback);

 protected:
  FileWriter(bool create_impl);

 private:
  unique_ptr<FileWriterImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

}   // namespace file_writer

#endif  // FILE_WRITER_H
