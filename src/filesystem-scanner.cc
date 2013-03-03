#include "filesystem-scanner.h"

#include <cassert>
#include <utility>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>

namespace polar_express {
namespace {

string PathWithoutPrefix(const filesystem::path& path, const string& prefix) {
  string full_path = path.string();
  if (full_path.find(prefix) == 0) {
    return full_path.substr(prefix.length(),
                            full_path.length() - prefix.length());
  }
  return full_path;
}

}  // namespace

FilesystemScanner::FilesystemScanner()
    : is_scanning_(false) {
}

FilesystemScanner::~FilesystemScanner() {
  StopScan();
}
  
bool FilesystemScanner::Scan(const string& root) {
  unique_lock<shared_mutex> write_lock(mu_);
  if (is_scanning_) {
    return false;
  }
  if (scan_thread_.get() != nullptr) {
    scan_thread_->join();
    scan_thread_.reset();
  }
  is_scanning_ = true;
  scan_thread_.reset(new thread(ScannerThread(this, root)));
  return true;
}

void FilesystemScanner::StopScan() {
  unique_lock<shared_mutex> write_lock(mu_);
  if (!is_scanning_) {
    return;
  }
  CHECK_NOTNULL(scan_thread_.get());
  scan_thread_->interrupt();
  scan_thread_->join();
  scan_thread_.reset();
  is_scanning_ = false;
}
  
bool FilesystemScanner::is_scanning() const {
  shared_lock<shared_mutex> read_lock(mu_);
  return is_scanning_;
}

void FilesystemScanner::GetFilePaths(vector<string>* paths) const {
  shared_lock<shared_mutex> read_lock(mu_);
  *CHECK_NOTNULL(paths) = paths_;
}

void FilesystemScanner::GetFilePathsAndClear(vector<string>* paths) {
  CHECK_NOTNULL(paths)->clear();
  
  unique_lock<shared_mutex> write_lock(mu_);
  paths->swap(paths_);
}
  
void FilesystemScanner::AddFilePaths(const vector<string>& paths) {
  unique_lock<shared_mutex> write_lock(mu_);
  paths_.insert(paths_.end(), paths.begin(), paths.end()); 
}

void FilesystemScanner::ScanFinished() {
  unique_lock<shared_mutex> write_lock(mu_);
  assert(is_scanning_);
  is_scanning_ = false;
}

FilesystemScanner::ScannerThread::ScannerThread(
    FilesystemScanner* fs_scanner, const string& root)
    : fs_scanner_(CHECK_NOTNULL(fs_scanner)),
      root_(root) {
}

void FilesystemScanner::ScannerThread::operator()() {
  vector<string> tmp_paths;
  filesystem::recursive_directory_iterator itr(root_);
  filesystem::recursive_directory_iterator eod;
  BOOST_FOREACH(const filesystem::path& path, make_pair(itr, eod)) {
    if (exists(path) && is_regular_file(path)) {
      tmp_paths.push_back(PathWithoutPrefix(path, root_));
    }
    if (tmp_paths.size() >= 100) {
      this_thread::interruption_point();
      fs_scanner_->AddFilePaths(tmp_paths);
      tmp_paths.clear();
    }
  }

  fs_scanner_->AddFilePaths(tmp_paths);
  fs_scanner_->ScanFinished();
}
  
}  // namespace polar_express
