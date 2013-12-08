#include "file-writer.h"

#include "asio-dispatcher.h"
#include "file-writer-impl.h"

namespace polar_express {

FileWriter::FileWriter()
    : impl_(new FileWriterImpl) {
}

FileWriter::FileWriter(bool create_impl)
    : impl_(create_impl ? new FileWriterImpl : nullptr) {
}

FileWriter::~FileWriter() {
}

void FileWriter::WriteDataToPath(
    const vector<byte>* data, const boost::filesystem::path& path,
    Callback callback) {
  WriteSequentialDataToPath({ data }, path, callback);
}

void FileWriter::WriteSequentialDataToPath(
    const vector<const vector<byte>*>& sequential_data,
    const boost::filesystem::path& path,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FileWriter::WriteSequentialDataToPath,
           impl_.get(), sequential_data, path, callback));
}

void FileWriter::WriteDataToTemporaryFile(
    const vector<byte>* data, const string& filename_prefix,
    string* path_str, Callback callback) {
  WriteSequentialDataToTemporaryFile(
      { data }, filename_prefix, path_str, callback);
}

void FileWriter::WriteSequentialDataToTemporaryFile(
    const vector<const vector<byte>*>& sequential_data,
    const string& filename_prefix,
    string* path_str, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FileWriter::WriteSequentialDataToTemporaryFile,
           impl_.get(), sequential_data, filename_prefix, path_str, callback));
}

}  // namespace polar_express

