/* 
 * Copyright 2010-2011 PathScale, Inc. All rights reserved.
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

#include <stdlib.h>

namespace std
{
	// Forward declaration of standard library terminate() function used to
	// abort execution.
	void terminate(void);
}

extern "C" void __cxa_rethrow_primary_exception(void* thrown_exception)
{
	if (NULL == thrown_exception) { return; }

  std::terminate();
}

extern "C" void *__cxa_current_primary_exception(void)
{
	return NULL;
}

extern "C" void __cxa_increment_exception_refcount(void* thrown_exception)
{
	if (NULL == thrown_exception) { return; }

  std::terminate();
}

extern "C" void __cxa_decrement_exception_refcount(void* thrown_exception)
{
	if (NULL == thrown_exception) { return; }

  std::terminate();
}

namespace std
{
  /**
	 * Terminates the program.
	 */
	void terminate()
	{
		abort();
	}
	/**
	 * Returns whether there are any exceptions currently being thrown that
	 * have not been caught.  This can occur inside a nested catch statement.
	 */
	bool uncaught_exception() throw()
	{
		return false;
	}
	/**
	 * Returns the number of exceptions currently being thrown that have not
	 * been caught.  This can occur inside a nested catch statement.
	 */
	int uncaught_exceptions() throw()
	{
		return 0;
	}
}
