

// This file contains definition of C++ new/delete operators


#include <stddef.h>
#include <malloc.h>
#include "stdexcept.h"

__attribute__((weak))
void * operator new(size_t size) {
    void * mem = malloc(size);
    if(mem == NULL) {
        throw std::bad_alloc();
    }

    return mem;
}


__attribute__((weak))
void operator delete(void * ptr) {
    free(ptr);
}


__attribute__((weak))
void * operator new[](size_t size) {
    return ::operator new(size);
}


__attribute__((weak))
void operator delete[](void * ptr) {
    ::operator delete(ptr);
}


