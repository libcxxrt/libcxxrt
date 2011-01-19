

// This file contains definition of C++ new/delete operators


#include <stddef.h>
#include <malloc.h>
#include "stdexcept.h"

namespace std {
    void terminate();
}

typedef void (*new_handler)();
static new_handler new_handl = &std::terminate;


namespace std {
    new_handler set_new_handler(new_handler handl) {
        new_handler old = new_handl;
        new_handl = handl;
        return old;
    }
}


__attribute__((weak))
void * operator new(size_t size) {

    void * mem = malloc(size);
    while(mem == NULL) {
        if(new_handl != NULL) {
            new_handl();
        } else {
            throw std::bad_alloc();
        }
        mem = malloc(size);
    }

    return mem;
}


namespace std {
    struct nothrow_t {};
}

__attribute__((weak))
void * operator new(size_t size, const std::nothrow_t &) throw() {

    void * mem = malloc(size);
    while(mem == NULL) {
        if(new_handl != NULL) {
            try {
                new_handl();
            }
            catch(std::bad_alloc &) {
                // nothrow operator new should return NULL in case of
                // std::bad_alloc exception in new handler
                return NULL;
            }
        } else {
            return NULL;
        }
        mem = malloc(size);
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


