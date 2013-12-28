#include "filesystem-scanner.h"

#include <utility>

#include "boost/bind.hpp"

#include "asio-dispatcher.h"
#include "filesystem-scanner-impl.h"

namespace polar_express {

FilesystemScanner::FilesystemScanner()
    : impl_(new FilesystemScannerImpl) {
}

FilesystemScanner::FilesystemScanner(bool create_impl)
    : impl_(create_impl ? new FilesystemScannerImpl : nullptr) {
}

FilesystemScanner::~FilesystemScanner() {
}

void FilesystemScanner::StartScan(
    const string& root, int max_paths, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FilesystemScanner::StartScan, impl_.get(),
           root, max_paths, callback));
}

void FilesystemScanner::ContinueScan(int max_paths, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FilesystemScanner::ContinueScan, impl_.get(),
           max_paths, callback));
}

bool FilesystemScanner::GetPaths(
    vector<boost::filesystem::path>* paths) const {
  return impl_->GetPaths(paths);
}

bool FilesystemScanner::GetPathsWithFilesize(
    vector<pair<boost::filesystem::path, size_t> >* paths_with_size) const {
  return impl_->GetPathsWithFilesize(paths_with_size);
}

void FilesystemScanner::ClearPaths() {
  impl_->ClearPaths();
}

}  // namespace polar_express
