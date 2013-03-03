#ifndef MACROS_H
#define MACROS_H

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CHECK_NOTNULL(ptr) \
  (assert((ptr) != nullptr), (ptr))

using namespace std;
using namespace boost;

#endif  // MACROS_H
