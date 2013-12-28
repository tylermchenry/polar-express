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
  int initial_paths = paths_with_size_.size();
  while ((paths_with_size_.size() - initial_paths < max_paths) && itr_ != eod) {
    AddPath(*itr_++);
  }
  callback();
}

bool FilesystemScannerImpl::GetPaths(
    vector<boost::filesystem::path>* paths) const {
  CHECK_NOTNULL(paths)->reserve(paths_with_size_.size());
  for (const auto& path_with_size : paths_with_size_) {
    paths->push_back(path_with_size.first);
  }
  return !paths_with_size_.empty();
}

bool FilesystemScannerImpl::GetPathsWithFilesize(
    vector<pair<boost::filesystem::path, size_t> >* paths_with_size) const {
  CHECK_NOTNULL(paths_with_size)->insert(
      paths_with_size->end(), paths_with_size_.begin(), paths_with_size_.end());
  return !paths_with_size_.empty();
}

void FilesystemScannerImpl::ClearPaths() {
  paths_with_size_.clear();
}

void FilesystemScannerImpl::AddPath(const boost::filesystem::path& path) {
  paths_with_size_.push_back(
      make_pair(path, is_regular(path) ? file_size(path) : 0));
}

}  // namespace polar_express
