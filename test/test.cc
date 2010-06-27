#include <stdio.h>
#include <stdlib.h>

static int succeeded;
static int failed;

void log_test(bool predicate, const char *file, int line, const char *message)
{
	if (predicate)
	{
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
