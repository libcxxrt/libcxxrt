/* 
 * Copyright 2010-2012 PathScale, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * guard.cc: Functions for thread-safe static initialisation.
 *
 * Static values in C++ can be initialised lazily their first use.  This file
 * contains functions that are used to ensure that two threads attempting to
 * initialize the same static do not call the constructor twice.  This is
 * important because constructors can have side effects, so calling the
 * constructor twice may be very bad.
 *
 * Statics that require initialisation are protected by a 64-bit value.  Any
 * platform that can do 32-bit atomic test and set operations can use this
 * value as a low-overhead lock.  Because statics (in most sane code) are
 * accessed far more times than they are initialised, this lock implementation
 * is heavily optimised towards the case where the static has already been
 * initialised.  
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "atomic.h"

#ifdef __arm__
// ARM ABI - 32-bit guards.

/*
 * The least significant bit of the guard variable indicates that the object
 * has been initialised, the most significant bit is used for a spinlock.
 */
static const uint32_t LOCKED = ((uint32_t)1) << 31;
static const uint32_t INITIALISED = 1;

/**
 * Acquires a lock on a guard, returning 0 if the object has already been
 * initialised, and 1 if it has not.  If the object is already constructed then
 * this function just needs to read a byte from memory and return.
 */
extern "C" int __cxa_guard_acquire(volatile uint32_t *guard_object)
{
	// Not an atomic read, doesn't establish a happens-before relationship, but
	// if one is already established and we end up seeing an initialised state
	// then it's a fast path, otherwise we'll do something more expensive than
	// this test anyway...
	if ((INITIALISED == *guard_object)) { return 0; }
	// Spin trying to do the initialisation
	while (1)
	{
		// Loop trying to move the value of the guard from 0 (not
		// locked, not initialised) to the locked-uninitialised
		// position.
		switch (__sync_val_compare_and_swap(guard_object, 0, LOCKED))
		{
			// If the old value was 0, we succeeded, so continue
			// initialising
			case 0:
				return 1;
			// If this was already initialised, return and let the caller skip
			// initialising it again.
			case INITIALISED:
				return 0;
			// If it is locked by another thread, relinquish the CPU and try
			// again later.
			case LOCKED:
				sched_yield();
				break;
			// If it is some other value, then something has gone badly wrong.
			// Give up.
			default:
				fprintf(stderr, "Invalid state detected attempting to lock static initialiser.\n");
				abort();
		}
	}
	//__builtin_unreachable();
	return 0;
}

/**
 * Releases the lock without marking the object as initialised.  This function
 * is called if initialising a static causes an exception to be thrown.
 */
extern "C" void __cxa_guard_abort(uint32_t *guard_object)
{
	__attribute__((unused))
	bool reset = __sync_bool_compare_and_swap(guard_object, LOCKED, 0);
	assert(reset);
}
/**
 * Releases the guard and marks the object as initialised.  This function is
 * called after successful initialisation of a static.
 */
extern "C" void __cxa_guard_release(uint32_t *guard_object)
{
	__attribute__((unused))
	bool reset = __sync_bool_compare_and_swap(guard_object, LOCKED, INITIALISED);
	assert(reset);
}


#else
// Itanium ABI: 64-bit guards

/**
 * Returns a pointer to the low 32 bits in a 64-bit value, respecting the
 * platform's byte order.
 */
static int32_t *low_32_bits(volatile int64_t *ptr)
{
	int32_t *low= (int32_t*)ptr;
	// Test if the machine is big endian - constant propagation at compile
	// time should eliminate this completely.
	int one = 1;
	if (*(char*)&one != 1)
	{
		low++;
	}
	return low;
}

/**
 * Acquires a lock on a guard, returning 0 if the object has already been
 * initialised, and 1 if it has not.  If the object is already constructed then
 * this function just needs to read a byte from memory and return.
 */
extern "C" int __cxa_guard_acquire(volatile int64_t *guard_object)
{
	char first_byte = (*guard_object) >> 56;
	if (1 == first_byte) { return 0; }
	int32_t *lock = low_32_bits(guard_object);
	// Simple spin lock using the low 32 bits.  We assume that concurrent
	// attempts to initialize statics are very rare, so we don't need to
	// optimise for the case where we have lots of threads trying to acquire
	// the lock at the same time.
	while (!__sync_bool_compare_and_swap_4(lock, 0, 1))
	{
		if (1 == ((*guard_object) >> 56))
		{
			break;
		}
		sched_yield();
	}
	// We have to test the guard again, in case another thread has performed
	// the initialisation while we were trying to acquire the lock.
	first_byte = (*guard_object) >> 56;
	return (1 != first_byte);
}

/**
 * Releases the lock without marking the object as initialised.  This function
 * is called if initialising a static causes an exception to be thrown.
 */
extern "C" void __cxa_guard_abort(int64_t *guard_object)
{
	int32_t *lock = low_32_bits(guard_object);
	ATOMIC_STORE(lock, 0);
}
/**
 * Releases the guard and marks the object as initialised.  This function is
 * called after successful initialisation of a static.
 */
extern "C" void __cxa_guard_release(int64_t *guard_object)
{
	// Set the first byte to 1
	// Note: We don't do an atomic load here because getting a stale value
	// is not a problem in the current implementation.
	ATOMIC_STORE(guard_object, (*guard_object | ((int64_t)1) << 56));
	__cxa_guard_abort(guard_object);
}

#endif
