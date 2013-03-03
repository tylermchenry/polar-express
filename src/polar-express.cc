#include <iostream>
#include <string>
#include <vector>

#include "filesystem-scanner.h"
#include "macros.h"

using namespace polar_express;

namespace {

void PrintPathsAndClear(FilesystemScanner* scanner) {
  vector<string> paths;
  CHECK_NOTNULL(scanner)->GetFilePathsAndClear(&paths);
  for (const auto& path : paths) {
    cout << path << endl;
  }
}

}  // namespace

int main(int argc, char** argv) {
  FilesystemScanner scanner;
  if (argc > 1) {
    if (scanner.Scan(argv[1])) {
      while (scanner.is_scanning()) {
        PrintPathsAndClear(&scanner);
        sleep(1);
      }
      PrintPathsAndClear(&scanner);
    }
  }
  
  return 0;
}
