
#include "stdexcept.h"
#include <stdlib.h>

extern "C" void __cxa_bad_cast()
{
    throw std::bad_cast();
}

extern "C" void __cxa_bad_typeid()
{
    throw std::bad_typeid();
}

extern "C" void __cxa_pure_virtual() {
    abort();
}

