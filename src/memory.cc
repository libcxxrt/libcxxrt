

// This file contains definition of C++ new/delete operators


#include <stddef.h>
#include <malloc.h>
#include "stdexcept.h"

void * operator new(size_t size) {
    void * mem = malloc(size);
    if(mem == NULL) {
        throw std::bad_alloc();
    }

    return mem;
}


void operator delete(void * ptr) {
    free(ptr);
}

