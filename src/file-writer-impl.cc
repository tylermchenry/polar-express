#include "file-writer-impl.h"

#include <iostream>

#include "boost/iostreams/device/file.hpp"
#include "boost/iostreams/stream.hpp"

namespace polar_express {

FileWriterImpl::FileWriterImpl()
    : FileWriter(false) {
}

FileWriterImpl::~FileWriterImpl() {
}

void FileWriterImpl::WriteDataToPath(
    const string& data, const boost::filesystem::path& path,
    Callback callback) {
  {
    boost::iostreams::stream_buffer<boost::iostreams::file_sink> sink(
        path.string());
    ostream sink_stream(&sink);
    copy(data.begin(), data.end(), ostreambuf_iterator<char>(sink_stream));
  }

  callback();
}

void FileWriterImpl::WriteDataToTemporaryFile(
    const string& data, string* path_str, Callback callback) {
  boost::filesystem::path path =
      boost::filesystem::temp_directory_path() /
      boost::filesystem::unique_path();
  *CHECK_NOTNULL(path_str) = path.string();
  WriteDataToPath(data, path, callback);
}

}  // namespace polar_express
