#include "filesystem-scanner-impl.h"

#include <utility>

#include "boost/foreach.hpp"

namespace polar_express {

FilesystemScannerImpl::FilesystemScannerImpl()
  : FilesystemScanner(false) {
}

FilesystemScannerImpl::~FilesystemScannerImpl() {
}

void FilesystemScannerImpl::Scan(const string& root, Callback callback) {
  filesystem::recursive_directory_iterator itr(root);
  filesystem::recursive_directory_iterator eod;
  BOOST_FOREACH(const filesystem::path& path, make_pair(itr, eod)) {
    AddPath(path);
    callback();
  }
}

bool FilesystemScannerImpl::GetNextPath(boost::filesystem::path* path) {
  boost::mutex::scoped_lock lock(mu_);
  if (paths_.empty()) {
    return false;
  }
  *CHECK_NOTNULL(path) = paths_.front();
  paths_.pop();
  return true;
}

void FilesystemScannerImpl::AddPath(const boost::filesystem::path& path) {
  boost::mutex::scoped_lock lock(mu_);
  paths_.push(path);
}
  
}  // namespace polar_express
