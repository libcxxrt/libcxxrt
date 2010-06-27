#include <stdlib.h>

// Stub (incomplete) implementations to allow testing the library without an
// STL implementation
//
// Note: gcc complains on x86 if this takes a size_t, and on x86-64 if it takes
// an unsigned int.  This is just a stub, so it doesn't matter too much.
void *operator new(unsigned int s)
{
	return malloc(s);
}

void operator delete(void *o)
{
	free(o);
}
