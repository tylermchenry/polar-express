#include "services/file-writer-impl.h"

#include <iostream>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>

namespace polar_express {

FileWriterImpl::FileWriterImpl()
    : FileWriter(false) {
}

FileWriterImpl::~FileWriterImpl() {
}

void FileWriterImpl::WriteSequentialDataToPath(
    const vector<const vector<byte>*>& sequential_data,
    const boost::filesystem::path& path,
    Callback callback) {
  {
    boost::iostreams::stream_buffer<boost::iostreams::file_sink> sink(
        path.string());
    ostream sink_stream(&sink);
    for (const auto* data : sequential_data) {
      copy(data->begin(), data->end(), ostreambuf_iterator<char>(sink_stream));
    }
  }

  callback();
}

void FileWriterImpl::WriteSequentialDataToTemporaryFile(
    const vector<const vector<byte>*>& sequential_data,
    const string& filename_prefix,
    string* path_str, Callback callback) {
  boost::filesystem::path path =
      boost::filesystem::temp_directory_path() /
      boost::filesystem::unique_path(filename_prefix +
                                     "%%%%-%%%%-%%%%-%%%%.tmp");
  *CHECK_NOTNULL(path_str) = path.string();
  WriteSequentialDataToPath(sequential_data, path, callback);
}

}  // namespace polar_express
