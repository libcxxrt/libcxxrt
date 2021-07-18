#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "test.h"

static int static_count;
struct static_struct
{
	int i;
	static_struct()
	{
		static_count++;
		i = 12;
	};
};

static static_struct ss;

int init_static(void)
{
	static static_struct s;
	return s.i;
}

thread_local bool mainThread = false;

void *init_static_race(void*);

static int instances = 0; 
struct static_slow_struct
{
	int field;
	static_slow_struct()
	{
		if (mainThread)
		{
			pthread_t thr;
			// See if another thread can make progress while we hold the lock.
			// This will happen if we get the lock and init bytes the wrong way
			// around.
			pthread_create(&thr, nullptr, init_static_race, nullptr);
			sleep(1);
		}
		field = 1;
		instances++;
	};
};

void *init_static_race(void*)
{
	static static_slow_struct s;
	TEST(s.field == 1, "Field correctly initialised");
	return nullptr;
}

void test_guards(void)
{
	init_static();
	int i = init_static();
	TEST(i == 12, "Static initialized");
	TEST(static_count == 2, "Each static only initialized once");
	mainThread = true;
	init_static_race(nullptr);
	TEST(instances == 1, "Two threads both tried to initialise a static");
}
