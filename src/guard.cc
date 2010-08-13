#include <stdint.h>
#include <pthread.h>

static int32_t *low_32_bits(int64_t *ptr)
{
	int32_t *low= (int32_t*)ptr;
	// Test if the machine is big endian - constant propagation at compile time
	// should eliminate this completely.
	int one = 1;
	if (*(char*)&one != 1)
	{
		low++;
	}
	return low;
}

extern "C" int __cxa_guard_acquire(int64_t *guard_object)
{
	char first_byte = (*guard_object) >> 56;
	if (1 == first_byte) { return 0; }
	int32_t *lock = low_32_bits(guard_object);
	while (!__sync_bool_compare_and_swap_4(lock, 0, 1))
	{
		// TODO: Use some of the remaining 24 bits to define a mutex to sleep
		// on.  Whether this is actually worth bothering with depends on
		// whether there is likely to be any contention.
		sched_yield();
	}
	return 1;
}

extern "C" void __cxa_guard_abort(int64_t *guard_object)
{
	int32_t *lock = low_32_bits(guard_object);
	*lock = 0;
}
extern "C" void __cxa_guard_release(int64_t *guard_object)
{
	// Set the first byte to 1
	*guard_object |= ((int64_t)1) << 57;
	__cxa_guard_abort(guard_object);
}


