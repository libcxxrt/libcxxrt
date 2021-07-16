#include "test.h"
#include <cxxabi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <typeinfo>

void test(const char* mangled, const char* expected, int expstat, int line) {
	int status = 0;
	using abi::__cxa_demangle;
	char* demangled = __cxa_demangle(mangled, NULL, NULL, &status);
	printf("mangled='%s' demangled='%s', status=%d\n", mangled, demangled,
	    status);
	if (expstat == 0) {
		TEST_LOC(status == 0, "should be able to demangle", __FILE__,
		    line);
	} else {
		TEST_LOC(status == expstat, "should fail with expected status",
		    __FILE__, line);
	}
	if (expected == NULL) {
		TEST_LOC(demangled == NULL, "should fail to demangle", __FILE__,
		    line);
	} else {
		TEST_LOC(demangled != NULL, "should be able to demangle",
		    __FILE__, line);
		TEST_LOC(strcmp(expected, demangled) == 0,
		    "should be able to demangle", __FILE__, line);
		TEST_LOC(strcmp(mangled, demangled) != 0,
		    "should be able to demangle", __FILE__, line);
	}
	free(demangled);
}

template <typename T> void test(const char* expected, int line) {
	test(typeid(T).name(), expected, 0, line);
}


namespace N {
template<typename T, int U>
class Templated {
	virtual ~Templated() {};
};
}

void test_demangle(void)
{
	using namespace N;

	// Positive tests (have to demangle successfully)
	test<int>("int", __LINE__);
	test<char[4]>("char [4]", __LINE__);
	test<char[]>("char []", __LINE__);
	test<Templated<Templated<long, 7>, 8> >(
	    "N::Templated<N::Templated<long, 7>, 8>", __LINE__);
	test<Templated<void(long), -1> >(
	    "N::Templated<void (long), -1>", __LINE__);

	// Negative tests (expected to fail with the indicated status)
	test("NSt3__15tupleIJibEEE", NULL, -2, __LINE__);
}
