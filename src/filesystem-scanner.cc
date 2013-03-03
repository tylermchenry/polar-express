#include "filesystem-scanner.h"

#include <cassert>
#include <utility>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

namespace polar_express {
namespace {

// TODO: Might be useful to have this in a util file somewhere.
template <typename T>
void AppendToVec(vector<T>* dst, const vector<T>& src) {
  assert(dst != nullptr);
  dst->insert(dst->end(), src.begin(), src.end());
}

string PathWithoutPrefix(const filesystem::path& path, const string& prefix) {
  string full_path = path.string();
  if (full_path.find(prefix) == 0) {
    return full_path.substr(prefix.length(),
                            full_path.length() - prefix.length());
  }
  return full_path;
}

} // namespace

FilesystemScanner::FilesystemScanner()
    : is_scanning_(false) {
}

FilesystemScanner::~FilesystemScanner() {
}
  
bool FilesystemScanner::Scan(const string& root) {
  if (!StartScan()) {
    return false;
  }

  vector<string> tmp_paths;
  filesystem::recursive_directory_iterator itr(root);
  filesystem::recursive_directory_iterator eod;
  BOOST_FOREACH(const filesystem::path& path, make_pair(itr, eod)) {
    if (is_regular_file(path)) {
      tmp_paths.push_back(PathWithoutPrefix(path, root));
    }
    if (tmp_paths.size() >= 100) {
      unique_lock<shared_mutex> write_lock(mu_);
      AppendToVec(&paths_, tmp_paths);
      tmp_paths.clear();
    }
  }

  unique_lock<shared_mutex> write_lock(mu_);
  AppendToVec(&paths_, tmp_paths);
  is_scanning_ = false;
  return true;
}

bool FilesystemScanner::is_scanning() const {
  shared_lock<shared_mutex> read_lock(mu_);
  return is_scanning_;
}

void FilesystemScanner::GetFilePaths(vector<string>* paths) const {
  assert(paths != nullptr);
  shared_lock<shared_mutex> read_lock(mu_);
  *paths = paths_;
}

void FilesystemScanner::GetFilePathsAndClear(vector<string>* paths) {
  assert(paths != nullptr);
  paths->clear();
  
  unique_lock<shared_mutex> write_lock(mu_);
  paths->swap(paths_);
}

bool FilesystemScanner::StartScan() {
  unique_lock<shared_mutex> write_lock(mu_);
  if (is_scanning_) {
    return false;
  } 
  is_scanning_ = true;
  return true;
}
  
}  // namespace polar_express
