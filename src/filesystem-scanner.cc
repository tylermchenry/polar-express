#include "filesystem-scanner.h"

#include <utility>

#include "boost/foreach.hpp"
#include "boost/thread.hpp"

namespace polar_express {

FilesystemScanner::FilesystemScanner() {
}

FilesystemScanner::~FilesystemScanner() {
}

void FilesystemScanner::Scan(
    const string& root,
    FilePathsCallback callback,
    int callback_interval) const {
  vector<filesystem::path> paths;
  filesystem::recursive_directory_iterator itr(root);
  filesystem::recursive_directory_iterator eod;
  BOOST_FOREACH(const filesystem::path& path, make_pair(itr, eod)) {
    paths.push_back(path);
    if (paths.size() >= callback_interval) {
      this_thread::interruption_point();
      callback(paths);
      paths.clear();
    }
  }

  if (!paths.empty()) {
    callback(paths);
  }
}
  
}  // namespace polar_express
