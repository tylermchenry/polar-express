#ifndef CALLBACK_H
#define CALLBACK_H

#include "boost/function.hpp"

namespace polar_express {

typedef boost::function<void()> Callback;

}  // polar_express

#endif  // CALLBACK_H
