#include <stdio.h>

static int static_count;
struct static_struct
{
	int i;
	static_struct()
	{
		fprintf(stderr, "Initialized static\n");
		static_count++;
		i = 12;
	};
};

int init_static(void)
{
	static static_struct s;
	return s.i;
}

void test_guards(void)
{
	init_static();
}
