#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static int succeeded;
static int failed;
static bool verbose;

void log_test(bool predicate, const char *file, int line, const char *message)
{
	if (predicate)
	{
		if (verbose)
		{
			printf("Test passed: %s:%d: %s\n", file, line, message);
		}
		succeeded++;
		return;
	}
	failed++;
	printf("Test failed: %s:%d: %s\n", file, line, message);
}

static void log_totals(void)
{
	printf("\n%d tests, %d passed, %d failed\n", succeeded+failed, succeeded, failed);
}

static void __attribute__((constructor)) init(void)
{
	atexit(log_totals);
}

void test_type_info(void);
void test_exceptions();
void test_init_primary_exception(void);
void test_guards(void);
void test_demangle(void);
int main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "v")) != -1)
	{
		switch (ch)
		{
			case 'v':
				verbose = true;
			default: break;
		}
	}

	test_type_info();
	test_guards();
#if !defined(_CXXRT_NO_EXCEPTIONS)
	test_exceptions();
	test_init_primary_exception();
#endif
	test_demangle();
	return 0;
}
