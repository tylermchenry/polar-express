#include "file-writer-impl.h"

#include "boost/iostreams/device/mapped_file.hpp"

namespace polar_express {

FileWriterImpl::FileWriterImpl()
    : FileWriter(false) {
}

FileWriterImpl::~FileWriterImpl() {
}

void FileWriterImpl::WriteDataToPath(
    const string& data, const boost::filesystem::path& path,
    Callback callback) {
  boost::filesystem::resize_file(path, data.size());
  {
    boost::iostreams::mapped_file mapped_file(path.string(), ios_base::out);
    if (!mapped_file.is_open() && mapped_file.size() == data.size()) {
      copy(data.begin(), data.end(), mapped_file.data());
    }
    // TODO: Error reporting?
  }

  callback();
}

void FileWriterImpl::WriteDataToTemporaryFile(
    const string& data, string* path_str, Callback callback) {
  boost::filesystem::path path = boost::filesystem::unique_path();
  *CHECK_NOTNULL(path_str) = path.string();
  WriteDataToPath(data, path, callback);
}

}  // namespace polar_express
