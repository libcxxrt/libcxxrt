#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "typeinfo.h"
#include "dwarf_eh.h"


namespace std {
    void unexpected();
}
extern "C" std::type_info *__cxa_current_exception_type();
extern "C" char* __cxa_demangle(const char* mangled_name,
                                char* buf,
                                size_t* n,
                                int* status);


/**
 * Class of exceptions to distinguish between this and other exception types.
 *
 * The first four characters are the vendor ID.  Currently, we use GNUC,
 * because we aim for ABI-compatibility with the GNU implementation, and
 * various checks may test for equality of the class, which is incorrect.
 */
static const uint64_t exception_class = *(int64_t*)"GNUCC++\0";
/**
 * The low four bytes of the exception class, indicating that we conform to the
 * Itanium C++ ABI.  
 */
static const uint32_t abi_exception_class = *(int32_t*)"C++\0";

namespace std
{
	// Forward declaration of standard library terminate() function used to
	// abort execution.
	void terminate(void);
}

using namespace ABI_NAMESPACE;

/**
 * Function type to call when an unexpected exception is encountered.
 */
typedef void (*unexpected_handler)();
/**
 * Function type to call when an unrecoverable condition is encountered.
 */
typedef void (*terminate_handler)();

/**
 * Structure used as a header on thrown exceptions.  This is the same layout as
 * defined by the Itanium ABI spec, so should be interoperable with any other
 * implementation of this spec, such as GNU libsupc++.
 *
 * Note: Several fields of this structure have not-very-informative names.
 * These are taken from the ABI spec and have not been changed to make it
 * easier for people referring to to the spec while reading this code.
 */
struct __cxa_exception
{
	/** Type info for the thrown object. */
	std::type_info *exceptionType;
	/** Destructor for the object, if one exists. */
	void (*exceptionDestructor) (void *); 
	/** Handler called when an exception specification is violated. */
	unexpected_handler unexpectedHandler;
	/** Hander called to terminate. */
	terminate_handler terminateHandler;
	/** Next exception in the list.  If an exception is thrown inside a catch
	 * block and caught in a nested catch, this points to the exception that
	 * will be handled after the inner catch block completes. */
	__cxa_exception *nextException;

	/** The number of handlers that currently have references to this
	 * exception.  The top (non-sign) bit of this is used as a flag to indicate
	 * that the exception is being rethrown, so should not be deleted when its
	 * handler count reaches 0 (which it doesn't with the top bit set).*/
	int handlerCount;
	/** The selector value to be returned when installing the catch handler.
	 * Used at the call site to determine which catch() block should execute.
	 * This is found in phase 1 of unwinding then installed in phase 2.*/
	int handlerSwitchValue;
	const char *actionRecord;
	/** Pointer to the language-specific data area (LSDA) for the handler
	 * frame.  This is unused in this implementation, but set for ABI
	 * compatibility in case we want to mix code in very weird ways. */
	const char *languageSpecificData;
	/** The cached landing pad for the catch handler.*/
	void *catchTemp;
	/** The pointer that will be returned as the pointer to the object.  When
	 * throwing a class and catching a virtual superclass (for example), we
	 * need to adjust the thrown pointer to make it all work correctly. */
	void *adjustedPtr;

	/** The language-agnostic part of the exception header. */
	_Unwind_Exception unwindHeader;
};

/**
 * ABI-specified globals structure.  Returned by the __cxa_get_globals()
 * function and its fast variant.
 */
struct __cxa_eh_globals
{
	__cxa_exception *caughtExceptions;
	unsigned int uncaughtExceptions;
};

/**
 * Per-thread info required by the runtime.  We store a single structure
 * pointer in thread-local storage, because this tends to be a scarce resource
 * and it's impolite to steal all of it and not leave any for the rest of the
 * program.
 *
 * Instances of this structure are allocated lazily - at most one per thread -
 * and are destroyed on thread termination.
 */
struct __cxa_thread_info
{
	/** The termination handler for this thread. */
	terminate_handler terminateHandler;
	/** The unexpected exception handler for this thread. */
	unexpected_handler unexpectedHandler;
	/** The number of emergency buffers held by this thread.  This is 0 in
	 * normal operation - the emergency buffers are only used when malloc()
	 * fails to return memory for allocating an exception.  Threads are not
	 * permitted to hold more than 4 emergency buffers (as per recommendation
	 * in ABI spec).*/
	int emergencyBuffersHeld;
	/**
	 * The public part of this structure, accessible from outside of this
	 * module.
	 */
	__cxa_eh_globals globals;
};

/** The global termination handler. */
static terminate_handler terminateHandler = abort;
/** The global unexpected exception handler. */
static unexpected_handler unexpectedHandler = abort;

/** Key used for thread-local data. */
static pthread_key_t eh_key;

typedef enum
{
	handler_none,
	handler_cleanup,
	handler_catch
} handler_type;


// FIXME: Put all of the ABI functions in a header.
extern "C" void __cxa_free_exception(void *thrown_exception);

/**
 * Cleanup function, allowing foreign exception handlers to correctly destroy
 * this exception if they catch it.
 */
static void exception_cleanup(_Unwind_Reason_Code reason, 
                              struct _Unwind_Exception *ex)
{
	__cxa_free_exception((void*)ex);
}

/**
 * Recursively walk a list of exceptions and delete them all in post-order.
 */
static void free_exception_list(__cxa_exception *ex)
{
	if (0 != ex->nextException)
	{
		free_exception_list(ex->nextException);
	}
	// __cxa_free_exception() expects to be passed the thrown object, which
	// immediately follows the exception, not the exception itself
	__cxa_free_exception(ex+1);
}

/**
 * Cleanup function called when a thread exists to make certain that all of the
 * per-thread data is deleted.
 */
static void thread_cleanup(void* thread_info)
{
	__cxa_thread_info *info = (__cxa_thread_info*)thread_info;
	if (info->globals.caughtExceptions)
	{
		free_exception_list(info->globals.caughtExceptions);
	}
	free(thread_info);
}


/**
 * Once control used to protect the key creation.
 */
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

/**
 * Initialise eh_key.
 */
static void init_key(void)
{
	pthread_key_create(&eh_key, thread_cleanup);
}

/**
 * Returns the thread info structure, creating it if it is not already created.
 */
static __cxa_thread_info *thread_info()
{
	pthread_once(&once_control, init_key);
	__cxa_thread_info *info = (__cxa_thread_info*)pthread_getspecific(eh_key);
	if (0 == info)
	{
		info = (__cxa_thread_info*)calloc(1, sizeof(__cxa_thread_info));
		pthread_setspecific(eh_key, info);
	}
	return info;
}
/**
 * Fast version of thread_info().  May fail if thread_info() is not called on
 * this thread at least once already.
 */
static __cxa_thread_info *thread_info_fast()
{
	return (__cxa_thread_info*)pthread_getspecific(eh_key);
}
/**
 * ABI function returning the __cxa_eh_globals structure.
 */
extern "C" __cxa_eh_globals *__cxa_get_globals(void)
{
	return &(thread_info()->globals);
}
/**
 * Version of __cxa_get_globals() assuming that __cxa_get_globals() has already
 * been called at least once by this thread.
 */
extern "C" __cxa_eh_globals *__cxa_get_globals_fast(void)
{
	return &(thread_info_fast()->globals);
}

/**
 * An emergency allocation reserved for when malloc fails.  This is treated as
 * 16 buffers of 1KB each.
 */
static char emergency_buffer[16384];
/**
 * Flag indicating whether each buffer is allocated.
 */
static bool buffer_allocated[16];
/**
 * Lock used to protect emergency allocation.
 */
static pthread_mutex_t emergency_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
/**
 * Condition variable used to wait when two threads are both trying to use the
 * emergency malloc() buffer at once.
 */
static pthread_cond_t emergency_malloc_wait = PTHREAD_COND_INITIALIZER;

/**
 * Allocates size bytes from the emergency allocation mechanism, if possible.
 * This function will fail if size is over 1KB or if this thread already has 4
 * emergency buffers.  If all emergency buffers are allocated, it will sleep
 * until one becomes available.
 */
static char *emergency_malloc(size_t size)
{
	if (size > 1024) { return 0; }

	__cxa_thread_info *info = thread_info();
	// Only 4 emergency buffers allowed per thread!
	if (info->emergencyBuffersHeld > 3) { return 0; }

	pthread_mutex_lock(&emergency_malloc_lock);
	int buffer = -1;
	while (buffer < 0)
	{
		// While we were sleeping on the lock, another thread might have free'd
		// enough memory for us to use, so try the allocation again - no point
		// using the emergency buffer if there is some real memory that we can
		// use...
		void *m = calloc(1, size);
		if (0 != m)
		{
			pthread_mutex_unlock(&emergency_malloc_lock);
			return (char*)m;
		}
		for (int i=0 ; i<16 ; i++)
		{
			if (!buffer_allocated[i])
			{
				buffer = i;
				buffer_allocated[i] = true;
				break;
			}
		}
		if (buffer < 0)
		{
			pthread_cond_wait(&emergency_malloc_wait, &emergency_malloc_lock);
		}
	}
	pthread_mutex_unlock(&emergency_malloc_lock);
	info->emergencyBuffersHeld++;
	return emergency_buffer + (1024 * buffer);
}

/**
 * Frees a buffer returned by emergency_malloc().
 *
 * Note: Neither this nor emergency_malloc() is particularly efficient.  This
 * should not matter, because neither will be called in normal operation - they
 * are only used when the program runs out of memory, which should not happen
 * often.
 */
static void emergency_malloc_free(char *ptr)
{
	int buffer = -1;
	// Find the buffer corresponding to this pointer.
	for (int i=0 ; i<16 ; i++)
	{
		if (ptr == (void*)(emergency_buffer + (1024 * i)))
		{
			buffer = i;
			break;
		}
	}
	assert(buffer > 0 && "Trying to free something that is not an emergency buffer!");
	// emergency_malloc() is expected to return 0-initialized data.  We don't
	// zero the buffer when allocating it, because the static buffers will
	// begin life containing 0 values.
	memset((void*)ptr, 0, 1024);
	// Signal the condition variable to wake up any threads that are blocking
	// waiting for some space in the emergency buffer
	pthread_mutex_lock(&emergency_malloc_lock);
	buffer_allocated[buffer] = true;
	pthread_cond_signal(&emergency_malloc_wait);
	pthread_mutex_unlock(&emergency_malloc_lock);
}

/**
 * Allocates an exception structure.  Returns a pointer to the space that can
 * be used to store an object of thrown_size bytes.
 */
extern "C" void *__cxa_allocate_exception(size_t thrown_size)
{
	size_t size = thrown_size + sizeof(__cxa_exception);
	char *buffer = (char*)calloc(1, size);

	// If calloc() doesn't want to give us any memory, try using an emergency
	// buffer.
	if (0 == buffer)
	{
		buffer = emergency_malloc(size);
		if (0 == buffer)
		{
			fprintf(stderr, "Out of memory attempting to allocate exception\n");
			std::terminate();
		}
	}

	return buffer+sizeof(__cxa_exception);
}
/**
 * __cxa_free_exception() is called when an exception was thrown in between
 * calling __cxa_allocate_exception() and actually throwing the exception.
 * This happens when the object's copy constructor throws an exception.
 *
 * In this implementation, it is also called by __cxa_end_catch() and during
 * thread cleanup.
 */
extern "C" void __cxa_free_exception(void *thrown_exception)
{
	__cxa_exception *ex = ((__cxa_exception*)thrown_exception) - 1;

	// Free the 
	if (0 != ex->exceptionDestructor)
	{
		try
		{
			ex->exceptionDestructor(thrown_exception);
		}
		catch(...)
		{
			// FIXME: Check that this is really what the spec says to do.
			std::terminate();
		}
	}

	char *e = (char*)ex;
	// If this allocation is within the address range of the emergency buffer,
	// don't call free() because it was not allocated with malloc()
	if ((e > emergency_buffer) && (e < (emergency_buffer + sizeof(emergency_buffer))))
	{
		emergency_malloc_free(e);
	}
	else
	{
		free(e);
	}
}

/**
 * Callback function used with _Unwind_Backtrace().
 *
 * Prints a stack trace.  Used only for debugging help.
 *
 * Note: As of FreeBSD 8.1, dladd() still doesn't work properly, so this only
 * correctly prints function names from public, relocatable, symbols.
 */
static _Unwind_Reason_Code trace(struct _Unwind_Context *context, void *c)
{
	Dl_info myinfo;
	int mylookup = dladdr((void*)(uintptr_t)__cxa_current_exception_type, &myinfo);
	void *ip = (void*)_Unwind_GetIP(context);
	Dl_info info;
	if (dladdr(ip, &info) != 0)
	{
		if (mylookup == 0 || strcmp(info.dli_fname, myinfo.dli_fname) != 0)
		{
			printf("%p:%s() in %s\n", ip, info.dli_sname, info.dli_fname);
		}
	}
	return _URC_CONTINUE_UNWIND;
}

/**
 * Report a failure that occurred when attempting to throw an exception.
 *
 * If the failure happened by falling off the end of the stack without finding
 * a handler, prints a back trace before aborting.
 */
static void report_failure(_Unwind_Reason_Code err, void *thrown_exception)
{
	switch (err)
	{
		default: break;
		case _URC_FATAL_PHASE1_ERROR:
			fprintf(stderr, "Fatal error during phase 1 unwinding\n");
			break;
		case _URC_FATAL_PHASE2_ERROR:
			fprintf(stderr, "Fatal error during phase 2 unwinding\n");
			break;
		case _URC_END_OF_STACK:
			fprintf(stderr, "Terminating due to uncaught exception %p", 
					thrown_exception);
	
			size_t bufferSize = 128;
			char *demangled = (char*)malloc(bufferSize);
			const char *mangled = __cxa_current_exception_type()->name();
			int status;
			__cxa_demangle(mangled, demangled, &bufferSize, &status);
			fprintf(stderr, " of type %s\n", 
				status == 0 ? (const char*)demangled : mangled);
			if (status == 0) { free(demangled); }
			// Print a back trace if no handler is found.
			// TODO: Make this optional
			_Unwind_Backtrace(trace, 0);
			break;
	}
	std::terminate();
}

/**
 * ABI function for throwing an exception.  Takes the object to be thrown (the
 * pointer returned by __cxa_allocate_exception()), the type info for the
 * pointee, and the destructor (if there is one) as arguments.
 */
extern "C" void __cxa_throw(void *thrown_exception,
                            std::type_info *tinfo,
                            void(*dest)(void*))
{
	__cxa_exception *ex = ((__cxa_exception*)thrown_exception) - 1;

	__cxa_thread_info *info = thread_info();

	ex->unexpectedHandler = info->unexpectedHandler;
	ex->terminateHandler  = info->terminateHandler;

	ex->exceptionType = tinfo;
	
	ex->exceptionDestructor = dest;
	
	ex->unwindHeader.exception_class = exception_class;
	ex->unwindHeader.exception_cleanup = exception_cleanup;

	info->globals.uncaughtExceptions++;

	_Unwind_Reason_Code err = _Unwind_RaiseException(&ex->unwindHeader);
	report_failure(err, thrown_exception);
}

/**
 * Sets the flag used to prevent an exception being deallocated while
 * rethrowing.
 */
static void set_rethrow_flag(__cxa_exception *ex)
{
	ex->handlerCount |= 1 << ((sizeof(int) * 8) - 2);
}
/**
 * Unconditionally clears the flag used to prevent an exception being
 * deallocated while rethrowing.  This function is safe to call even when the
 * flag is not set.
 */
static void clear_rethrow_flag(__cxa_exception *ex)
{
	ex->handlerCount &= ~(1 << ((sizeof(int) * 8) - 2));
}

/**
 * ABI function.  Rethrows the current exception.  Does not remove the
 * exception from the stack or decrement its handler count - the compiler is
 * expected to set the landing pad for this function to the end of the catch
 * block, and then call _Unwind_Resume() to continue unwinding once
 * __cxa_end_catch() has been called and any cleanup code has been run.
 */
extern "C" void __cxa_rethrow()
{
	__cxa_eh_globals *globals = __cxa_get_globals_fast();
	// Note: We don't remove this from the caught list here, because
	// __cxa_end_catch will be called when we unwind out of the try block.  We
	// could probably make this faster by providing an alternative rethrow
	// function and ensuring that all cleanup code is run before calling it, so
	// we can skip the top stack frame when unwinding.
	__cxa_exception *ex = globals->caughtExceptions;

	if (0 == ex)
	{
		fprintf(stderr, "Attempting to rethrow an exception that doesn't exist!\n");
		std::terminate();
	}


	assert(ex->handlerCount > 0 && "Rethrowing uncaught exception!");
	ex->handlerCount--;

	// Set the most significant bit in the exception's handler count as a flag
	// indicating that the exception is being rethrown.  This flag is not
	// explicitly tested - we just set a high bit to ensure that the value of
	// the handlerCount field does not drop to 0.  This bit is then cleared
	// when the exception is caught again.  As long as we don't have more than
	// 2^30 exception handlers on the stack, and the compiler correctly
	// balances calls to begin / end catch, this should work correctly.
	set_rethrow_flag(ex);

	// Continue unwinding the stack with this exception.  This should unwind to
	// the place in the caller where __cxa_end_catch() is called.  The caller
	// will then run cleanup code and bounce the exception back with
	// _Unwind_Resume().
	_Unwind_Reason_Code err = _Unwind_Resume_or_Rethrow(&ex->unwindHeader);
	report_failure(err, ex + 1);
}

/**
 * Returns the type_info object corresponding to the filter.
 */
static std::type_info *get_type_info_entry(_Unwind_Context *context,
                                           dwarf_eh_lsda *lsda,
                                           int filter)
{
	// Get the address of the record in the table.
	dw_eh_ptr_t record = lsda->type_table - 
		dwarf_size_of_fixed_size_field(lsda->type_table_encoding)*filter;
	dw_eh_ptr_t start = record;
	// Read the value, but it's probably an indirect reference...
	int64_t offset = read_value(lsda->type_table_encoding, &record);

	// If the entry is 0, don't try to dereference it.  That would be bad.
	if (offset == 0) { return 0; }

	// ...so we need to resolve it
	return (std::type_info*)resolve_indirect_value(context,
			lsda->type_table_encoding, offset, start);
}


/**
 * Checks the type signature found in a handler against the type of the thrown
 * object.  If ex is 0 then it is assumed to be a foreign exception and only
 * matches cleanups.
 */
static bool check_type_signature(__cxa_exception *ex, const std::type_info *type)
{
	void *exception_ptr = (void*)(ex+1);
    const std::type_info *ex_type = ex->exceptionType;

	const __pointer_type_info *ptr_type =
        dynamic_cast<const __pointer_type_info*>(ex_type);
	if (0 != ptr_type)
	{
		exception_ptr = *(void**)exception_ptr;
	}
	// Always match a catchall, even with a foreign exception
	//
	// Note: A 0 here is a catchall, not a cleanup, so we return true to
	// indicate that we found a catch.
	//
	// TODO: Provide a class for matching against foreign exceptions.
	if (0 == type)
	{
		if (ex)
		{
			ex->adjustedPtr = exception_ptr;
		}
		return true;
	}

	if (0 == ex) { return false; }

    const __pointer_type_info *target_ptr_type =
        dynamic_cast<const __pointer_type_info*>(type);

    if (0 != ptr_type && 0 != target_ptr_type)
    {
        if (ptr_type->__flags & ~target_ptr_type->__flags) {
            // handler pointer is less qualified
            return false;
        }

        // special case for void* handler
        if(*target_ptr_type->__pointee == typeid(void)) {
            ex->adjustedPtr = exception_ptr;
            return true;
        }

        ex_type = ptr_type->__pointee;
        type = target_ptr_type->__pointee;
    }

	// If the types are the same, no casting is needed.
	if (*type == *ex_type)
	{
		ex->adjustedPtr = exception_ptr;
		return true;
	}

	const __class_type_info *cls_type =
        dynamic_cast<const __class_type_info*>(ex_type);
	const __class_type_info *target_cls_type =
        dynamic_cast<const __class_type_info*>(type);

	if (0 != cls_type &&
        0 != target_cls_type &&
        cls_type->can_cast_to(target_cls_type))
	{
		ex->adjustedPtr = cls_type->cast_to(exception_ptr, target_cls_type);
        return true;
	}
	return false;
}
/**
 * Checks whether the exception matches the type specifiers in this action
 * record.  If the exception only matches cleanups, then this returns false.
 * If it matches a catch (including a catchall) then it returns true.
 *
 * The selector argument is used to return the selector that is passed in the
 * second exception register when installing the context.
 */
static handler_type check_action_record(_Unwind_Context *context,
                                        dwarf_eh_lsda *lsda,
                                        dw_eh_ptr_t action_record,
                                        __cxa_exception *ex,
                                        unsigned long *selector)
{
	if (!action_record) { return handler_cleanup; }
	handler_type found = handler_none;
	while (action_record)
	{
		int filter = read_sleb128(&action_record);
		dw_eh_ptr_t action_record_offset_base = action_record;
		int displacement = read_sleb128(&action_record);
		action_record = displacement ? 
			action_record_offset_base + displacement : 0;
		// We only check handler types for C++ exceptions - foreign exceptions
		// are only allowed for cleanup.
		if (filter > 0 && 0 != ex)
		{
			std::type_info *handler_type = get_type_info_entry(context, lsda, filter);
			if (check_type_signature(ex, handler_type))
			{
				*selector = filter;
				return handler_catch;
			}
		}
		else if (filter < 0 && 0 != ex)
		{
			unsigned char *type_index = ((unsigned char*)lsda->type_table - filter - 1);
			bool matched = false;
			*selector = filter;
			while (*type_index)
			{
				std::type_info *handler_type = get_type_info_entry(context, lsda, *(type_index++));
				// If the exception spec matches a permitted throw type for
				// this function, don't report a handler - we are allowed to
				// propagate this exception out.
				if (check_type_signature(ex, handler_type))
				{
					matched = true;
					break;
				}
			}
			if (matched) { continue; }
			// If we don't find an allowed exception spec, we need to install
			// the context for this action.  The landing pad will then call the
			// unexpected exception function.  Treat this as a catch
			return handler_catch;
		}
		else if (filter == 0)
		{
			*selector = filter;
			found = handler_cleanup;
		}
	}
	return found;
}

/**
 * The exception personality function.  This is referenced in the unwinding
 * DWARF metadata and is called by the unwind library for each C++ stack frame
 * containing catch or cleanup code.
 */
extern "C" _Unwind_Reason_Code  __gxx_personality_v0(int version,
                                                     _Unwind_Action actions,
                                                     uint64_t exceptionClass,
                                                     struct _Unwind_Exception *exceptionObject,
                                                     struct _Unwind_Context *context)
{
	// This personality function is for version 1 of the ABI.  If you use it
	// with a future version of the ABI, it won't know what to do, so it
	// reports a fatal error and give up before it breaks anything.
	if (1 != version)
	{
		return _URC_FATAL_PHASE1_ERROR;
	}
	__cxa_exception *ex = 0;

	// If this exception is throw by something else then we can't make any
	// assumptions about its layout beyond the fields declared in
	// _Unwind_Exception.
	bool foreignException = exceptionClass != exception_class;

	if (!foreignException)
	{
		ex = (__cxa_exception*) ((char*)exceptionObject - offsetof(struct
					__cxa_exception, unwindHeader));
	}

	unsigned char *lsda_addr = (unsigned char*)_Unwind_GetLanguageSpecificData(context);

	// No LSDA implies no landing pads - try the next frame
	if (0 == lsda_addr) { return _URC_CONTINUE_UNWIND; }

	// These two variables define how the exception will be handled.
	dwarf_eh_action action = {0};
	unsigned long selector = 0;
	
	// During the search phase, we do a complete lookup.  If we return
	// _URC_HANDLER_FOUND, then the phase 2 unwind will call this function with
	// a _UA_HANDLER_FRAME action, telling us to install the handler frame.  If
	// we return _URC_CONTINUE_UNWIND, we may be called again later with a
	// _UA_CLEANUP_PHASE action for this frame.
	//
	// The point of the two-stage unwind allows us to entirely avoid any stack
	// unwinding if there is no handler.  If there are just cleanups found,
	// then we can just panic call an abort function.
	//
	// Matching a handler is much more expensive than matching a cleanup,
	// because we don't need to bother doing type comparisons (or looking at
	// the type table at all) for a cleanup.  This means that there is no need
	// to cache the result of finding a cleanup, because it's (quite) quick to
	// look it up again from the action table.
	if (actions & _UA_SEARCH_PHASE)
	{
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		if (!dwarf_eh_find_callsite(context, &lsda, &action))
        {
            // EH range not found. This happens if exception is thrown
            // and not catched inside cleanup (destructor).
            // We should call terminate() in this case
            // catchTemp (landing pad) field of exception object will
            // contain null when personality function will be called
            // with _UA_HANDLER_FRAME action
            return _URC_HANDLER_FOUND;
        }

		handler_type found_handler = check_action_record(context, &lsda,
				action.action_record, ex, &selector);
		// If there's no action record, we've only found a cleanup, so keep
		// searching for something real
		if (found_handler == handler_catch)
		{
			// Cache the results for the phase 2 unwind, if we found a handler
			// and this is not a foreign exception.
			if (ex)
			{
				ex->handlerSwitchValue = selector;
				ex->actionRecord = (const char*)action.action_record;
				ex->languageSpecificData = (const char*)lsda_addr;
				ex->catchTemp = action.landing_pad;
				// ex->adjustedPtr is set when finding the action record.
			}
			return _URC_HANDLER_FOUND;
		}
		return _URC_CONTINUE_UNWIND;
	}


	// If this is a foreign exception, we didn't have anywhere to cache the
	// lookup stuff, so we need to do it again.  If this is either a forced
	// unwind, a foreign exception, or a cleanup, then we just install the
	// context for a cleanup.
	if (!(actions & _UA_HANDLER_FRAME))
	{
        // cleanup

		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		dwarf_eh_find_callsite(context, &lsda, &action);
		if (0 == action.landing_pad) { return _URC_CONTINUE_UNWIND; }
		handler_type found_handler = check_action_record(context, &lsda,
				action.action_record, ex, &selector);
		// Ignore handlers this time.
		if (found_handler != handler_cleanup) { return _URC_CONTINUE_UNWIND; }
	}
    else if (ex->catchTemp == 0)
    {
        // uncaught exception in claenup, calling terminate
        std::terminate();
    }
	else if (foreignException)
	{
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
        dwarf_eh_find_callsite(context, &lsda, &action);
		check_action_record(context, &lsda, action.action_record, ex,
				&selector);
	}
	else
	{
		// Restore the saved info if we saved some last time.
		action.landing_pad = (dw_eh_ptr_t)ex->catchTemp;
		ex->catchTemp = 0;
		selector = (unsigned long)ex->handlerSwitchValue;
		ex->handlerSwitchValue = 0;
	}


	_Unwind_SetIP(context, (unsigned long)action.landing_pad);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(0), (unsigned long)exceptionObject);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(1), selector);

	return _URC_INSTALL_CONTEXT;


	exit(0);
}

/**
 * ABI function called when entering a catch statement.  The argument is the
 * pointer passed out of the personality function.  This is always the start of
 * the _Unwind_Exception object.  The return value for this function is the
 * pointer to the caught exception, which is either the adjusted pointer (for
 * C++ exceptions) of the unadjusted pointer (for foreign exceptions).
 */
#if __GNUC__ > 3 && __GNUC_MINOR__ > 2
extern "C" void *__cxa_begin_catch(void *e) throw()
#else
extern "C" void *__cxa_begin_catch(void *e)
#endif
{ 
	// Decrement the uncaught exceptions count
	__cxa_eh_globals *globals = __cxa_get_globals();
	globals->uncaughtExceptions--;
	_Unwind_Exception *exceptionObject = (_Unwind_Exception*)e;

	if (exceptionObject->exception_class == exception_class)
	{
		__cxa_exception *ex = (__cxa_exception*)
			((char*)exceptionObject - offsetof(struct __cxa_exception, unwindHeader));

		// Add this to the front of the list of exceptions being handled and
		// increment its handler count so that it won't be deleted prematurely.
		ex->nextException = globals->caughtExceptions;
		globals->caughtExceptions = ex;
		ex->handlerCount++;
		
		// Clear the rethrow flag if it's set - we are allowed to delete this
		// exception at the end of the catch block, as long as it isn't thrown
		// again later.
		clear_rethrow_flag(ex);
		return ex->adjustedPtr;
	}
	// exceptionObject is the pointer to the _Unwind_Exception within the
	// __cxa_exception.  The throw object is after this
	return ((char*)exceptionObject + sizeof(_Unwind_Exception));
}

/**
 * ABI function called when exiting a catch block.  This will free the current
 * exception if it is no longer referenced in other catch blocks.
 */
extern "C" void __cxa_end_catch()
{
	// We can call the fast version here because the slow version is called in
	// __cxa_throw(), which must have been called before we end a catch block
	__cxa_eh_globals *globals = __cxa_get_globals_fast();
	__cxa_exception *ex = globals->caughtExceptions;

	ex->handlerCount--;

	assert(0 != ex && "Ending catch when no exception is on the stack!");

	if (ex->handlerCount == 0)
	{
		globals->caughtExceptions = ex->nextException;
		// __cxa_free_exception() expects to be passed the thrown object, which
		// immediately follows the exception, not the exception itself
		__cxa_free_exception(ex+1);
	}
}

/**
 * ABI function.  Returns the type of the current exception.
 */
extern "C" std::type_info *__cxa_current_exception_type()
{
	__cxa_eh_globals *globals = __cxa_get_globals();
	__cxa_exception *ex = globals->caughtExceptions;
	return ex ? ex->exceptionType : 0;
}

/**
 * ABI function, called when an exception specification is violated.
 *
 * This function does not return.
 */
extern "C" void __cxa_call_unexpected(void*exception) 
{
	_Unwind_Exception *exceptionObject = (_Unwind_Exception*)exception;
	if (exceptionObject->exception_class == exception_class)
	{
		__cxa_exception *ex = (__cxa_exception*)
			((char*)exceptionObject - offsetof(struct __cxa_exception, unwindHeader));
		if (ex->unexpectedHandler)
		{
			ex->unexpectedHandler();
			// Should not be reached.  
			abort();
		}
	}
	std::unexpected();
	// Should not be reached.  
	abort();
}

/**
 * ABI function, returns the adjusted pointer to the exception object.
 */
extern "C" void *__cxa_get_exception_ptr(void *exceptionObject)
{
	return ((__cxa_exception*)((char*)exceptionObject - 
		offsetof(struct __cxa_exception, unwindHeader)))->adjustedPtr;
}


static bool thread_local_handlers = false;

namespace pathscale
{
	void set_use_thread_local_handlers(bool flag)
	{
		thread_local_handlers = flag;
	}
	unexpected_handler set_unexpected(unexpected_handler f) throw()
	{
		static __cxa_thread_info *info = thread_info();
		unexpected_handler old = info->unexpectedHandler;
		info->unexpectedHandler = f;
		return old;
	}
	terminate_handler set_terminate(terminate_handler f) throw()
	{
		static __cxa_thread_info *info = thread_info();
		terminate_handler old = info->terminateHandler;
		info->terminateHandler = f;
		return old;
	}
}

namespace std
{
	unexpected_handler set_unexpected(unexpected_handler f) throw()
	{
		if (thread_local_handlers) { return pathscale::set_unexpected(f); }

		return __sync_lock_test_and_set(&unexpectedHandler, f);
	}
	terminate_handler set_terminate(terminate_handler f) throw()
	{
		if (thread_local_handlers) { return pathscale::set_terminate(f); }
		return __sync_lock_test_and_set(&terminateHandler, f);
	}
	void terminate()
	{
		static __cxa_thread_info *info = thread_info_fast();
		if (0 != info && 0 != info->terminateHandler)
		{
			info->terminateHandler();
			// Should not be reached - a terminate handler is not expected to
			// return.
			abort();
		}
		terminateHandler();
	}
	void unexpected()
	{
		static __cxa_thread_info *info = thread_info_fast();
		if (0 != info && 0 != info->unexpectedHandler)
		{
			info->unexpectedHandler();
			// Should not be reached - a terminate handler is not expected to
			// return.
			abort();
		}
		unexpectedHandler();
	}
    bool uncaught_exception() throw() {
        __cxa_thread_info *info = thread_info();
        return info->globals.uncaughtExceptions != 0;
    }

}
