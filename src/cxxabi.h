#ifndef __CXX_ABI_H_INCLUDED__
#define __CXX_ABI_H_INCLUDED__
#ifdef __cplusplus
extern "C" {
#endif


/**
 * Demangles a C++ symbol or type name.  The buffer, if non-NULL, must be
 * allocated with malloc() and must be *n bytes or more long.  This function
 * may call realloc() on the value pointed to by buf, and will return the
 * length of the string via *n.
 *
 * The value pointed to by status is set to one of the following:
 *
 * 0: success
 * -1: memory allocation failure
 * -2: invalid mangled name
 * -3: invalid arguments
 */
char* __cxa_demangle(const char* mangled_name,
                     char* buf,
                     size_t* n,
                     int* status);
#ifdef __cplusplus
}
#endif

#endif // __CXX_ABI_H_INCLUDED__
