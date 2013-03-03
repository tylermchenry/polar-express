#include <iostream>
#include <string>
#include <vector>

#include "filesystem-scanner.h"
#include "macros.h"

using namespace polar_express;

int main(int argc, char** argv) { 
  vector<string> paths;
  FilesystemScanner scanner;
  if (argc > 1) {
    if (scanner.Scan(argv[1])) {
      scanner.GetFilePaths(&paths);
    }
  }

  for (const auto& path : paths) {
    cout << path << endl;
  }
  
  return 0;
}
