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
    const string& data, const boost::filesystem::path& path,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FileWriter::WriteDataToPath,
           impl_.get(), data, path, callback));
}

void FileWriter::WriteDataToTemporaryFile(
    const string& data, string* path_str, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FileWriter::WriteDataToTemporaryFile,
           impl_.get(), data, path_str, callback));
}

}  // namespace polar_express
