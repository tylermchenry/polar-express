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

  void WriteDataToPath(
      const vector<byte>& data, const boost::filesystem::path& path,
      Callback callback);

  virtual void WriteSequentialDataToPath(
      const vector<const vector<byte>*>& sequential_data,
      const boost::filesystem::path& path,
      Callback callback);

  void WriteDataToTemporaryFile(
      const vector<byte>& data, const string& filename_prefix,
      string* path_str, Callback callback);

  virtual void WriteSequentialDataToTemporaryFile(
      const vector<const vector<byte>*>& sequential_data,
      const string& filename_prefix,
      string* path_str, Callback callback);

 protected:
  FileWriter(bool create_impl);

 private:
  unique_ptr<FileWriterImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

}   // namespace file_writer

#endif  // FILE_WRITER_H
