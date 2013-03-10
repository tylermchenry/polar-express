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

void FilesystemScanner::Scan(const string& root, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&FilesystemScanner::Scan, impl_.get(), root, callback));
}

bool FilesystemScanner::GetNextPath(boost::filesystem::path* path) {
  return impl_->GetNextPath(path);
}
  
}  // namespace polar_express
