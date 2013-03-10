#include "filesystem-scanner-impl.h"

namespace polar_express {

FilesystemScannerImpl::FilesystemScannerImpl()
  : FilesystemScanner(false) {
}

FilesystemScannerImpl::~FilesystemScannerImpl() {
}

void FilesystemScannerImpl::StartScan(
    const string& root, int max_paths, Callback callback) {
  ClearPaths();
  itr_ = filesystem::recursive_directory_iterator(root);
  ContinueScan(max_paths, callback);
}

void FilesystemScannerImpl::ContinueScan(int max_paths, Callback callback) {
  const filesystem::recursive_directory_iterator eod;
  int initial_paths = paths_.size();
  while ((paths_.size() - initial_paths < max_paths) && itr_ != eod) {
    paths_.push_back(*itr_++);
  }
  callback();
}

bool FilesystemScannerImpl::GetPaths(
    vector<boost::filesystem::path>* paths) const {
  CHECK_NOTNULL(paths)->insert(paths->end(), paths_.begin(), paths_.end());
  return !paths_.empty();
}

void FilesystemScannerImpl::ClearPaths() {
  paths_.clear();
}
  
}  // namespace polar_express
