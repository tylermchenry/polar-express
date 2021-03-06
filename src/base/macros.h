#ifndef MACROS_H
#define MACROS_H

#include <cassert>

#include <boost/shared_ptr.hpp>  // To get boost namespace for 'using' below.

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CHECK_NOTNULL(ptr) \
  (assert((ptr) != nullptr), (ptr))

#define GUARDED_BY(x)
#define LOCKS_EXCLUDED(x)
#define SHARED_LOCKS_REQUIRED(x)
#define EXCLUSIVE_LOCKS_REQUIRED(x)

// TODO: Once boost::thread adds lockable annotations to mutex classes, enable
// these macros if __clang__ is defined.
//
// #define GUARDED_BY(x) __attribute__((guarded_by(x)))
// #define LOCKS_EXCLUDED(x) __attribute__((locks_excluded(x)))
// #define SHARED_LOCKS_REQUIRED(x) __attribute__((shared_locks_required(x)))
// #define EXCLUSIVE_LOCKS_REQUIRED(x) __attribute__((exclusive_locks_required(x)))

#ifndef byte
typedef unsigned char byte;
#endif  // byte

#if defined(DEBUG) && !defined(NDEBUG) && !defined(NDLOG)
#define DLOG(x) do { (x); } while(0)
#else
#define DLOG(x)
#endif  // DEBUG

using namespace std;
using namespace boost;

#endif  // MACROS_H
