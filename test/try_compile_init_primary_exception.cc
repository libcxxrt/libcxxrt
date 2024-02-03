#include "../src/cxxabi.h"
#include <stdio.h>

int main()
{
	printf("%p", &__cxxabiv1::__cxa_init_primary_exception);
}
