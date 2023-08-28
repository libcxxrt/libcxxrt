#include "test.h"
#include "unwind.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <exception>

#define fprintf(...)

void log_cleanup(void* ignored)
{
	//printf("Cleanup called on %s\n", *(char**)ignored);
}
#define CLEANUP\
	__attribute__((cleanup(log_cleanup))) __attribute__((unused))\
		const char *f = __func__;

/**
 * Simple struct to test throwing.
 */
struct foo
{
	int i;
};

struct bar : foo
{
	float bar;
};


/**
 * Non-pod type to test throwing
 */
class non_pod {
public:
    non_pod(int i): x(i) {}
    int x;
};


static int cleanup_count;
/**
 * Simple structure declared with a destructor.  Destroying this object will
 * increment cleanup count.  The destructor should be called automatically if
 * an instance of cl is allocated with automatic storage.
 */
struct cl
{
	int i;
	~cl() { fprintf(stderr, "cl destroyed: %d\n", i); cleanup_count++; }
};
/**
 * Test that one cl was destroyed when running the argument.
 */
#define TEST_CLEANUP(x) do {\
		int cleanups = cleanup_count;\
		{ x; }\
		TEST(cleanup_count == cleanups+1, "Cleanup ran correctly");\
	} while(0)

int inner(int i)
{
	CLEANUP
	switch (i)
	{
		case 0: throw (int)1.0;
		case 1: throw (float)1.0;
		case 2: fprintf(stderr, "Throwing int64_t\n");throw (int64_t)1;
		case 3: { foo f = {2} ; throw f; }
		case 4: { bar f; f.i = 2 ; f.bar=1 ; throw f; }
        case 5: throw non_pod(3);
	}
	return -1;
}

int outer(int i) throw(float, int, foo, non_pod)
{
	//CLEANUP
	inner(i);
	return 1;
}

static void test_const(void)
{
	int a = 1;
	try
	{
		throw a;
	}
	catch (const int b)
	{
		TEST(a == b, "Caught int as const int");
	}
	catch(...)
	{
		TEST(0, "Failed to catch int as const int");
	}
	try
	{
		throw &a;
	}
	catch (const int *b)
	{
		TEST(&a == b, "Caught int* as const int*");
	}
	catch(...)
	{
		TEST(0, "Failed to catch int* as const int*");
	}
}

static void test_catch(int s) 
{
	cl c;
	c.i = 12;
	fprintf(stderr, "Entering try\n");
	try
	{
		outer(s);
	}
	catch(int i)
	{
		fprintf(stderr, "Caught int %d in test %d\n", i, s);
		TEST((s == 0 && i == 1) || (s == 2 && i == 0), "Caught int");
		return;
	}
	catch (float f)
	{
		fprintf(stderr, "Caught float %f!\n", f);
		TEST(s == 1 && f == 1, "Caught float");
		return;
	}
	catch (foo f)
	{
		fprintf(stderr, "Caught struct {%d}!\n", f.i);
		TEST((s == 3 || s == 4) && f.i == 2, "Caught struct");
		return;
	}
    catch (non_pod np) {
        fprintf(stderr, "Caught non_pod {%d}!\n", np.x);
        TEST(s == 5 && np.x == 3, "Caught non_pod");
        return;
    }
	//abort();
	TEST(0, "Unreachable line reached");
}

void test_nested1(void)
{
	CLEANUP;
	cl c;
	c.i = 123;
	try 
	{
		outer(0);
	}
	catch (int a)
	{
		try
		{
			TEST(a == 1, "Caught int");
			outer(1);
		}
		catch (float f)
		{
			TEST(f == 1, "Caught float inside outer catch block");
			throw;
		}
	}
}

void test_nested()
{
	try
	{
		test_nested1();
	}
	catch (float f)
	{
		fprintf(stderr, "Caught re-thrown float\n");
		TEST(f == 1, "Caught re-thrown float");
	}
}

static int violations = 0;
static void throw_zero()
{
	violations++;
	fprintf(stderr, "Throwing 0\n");
	throw 0;
}

struct uncaught_exception_checker
{
	uncaught_exception_checker(bool uncaught) : m_uncaught(uncaught) {}
	~uncaught_exception_checker() {
		if (std::uncaught_exception())
			TEST(m_uncaught, "At least one uncaught exception is in flight");
		else
			TEST(!m_uncaught, "No uncaught exceptions are in flight");
	}
	bool m_uncaught;
};

void test_rethrown_uncaught_exception()
{
	uncaught_exception_checker outer(false);
	try
	{
		try
		{
			throw 42;
		}
		catch (int)
		{
			uncaught_exception_checker inner(true);
			throw;
		}
	}
	catch (...) {}
}

static void exception_cleanup(_Unwind_Reason_Code, struct _Unwind_Exception *ex)
{
	delete ex;
}

void test_rethrown_uncaught_foreign_exception()
{
	uncaught_exception_checker outer(false);
	try
	{
		try
		{
			// Throw a foreign exception.
			_Unwind_Exception *ex = new _Unwind_Exception;
			ex->exception_class = 1234;
			ex->exception_cleanup = exception_cleanup;
			_Unwind_RaiseException(ex);
		}
		catch (...)
		{
			// Note: Uncaught exceptions doesn't report foreign exceptions,
			// because we have no way of receiving a report that the other
			// language has caught it.
			uncaught_exception_checker inner(false);
			throw;
		}
	}
	catch (...) {}
}


void test_uncaught_exception()
{
	uncaught_exception_checker outer(false);
	try {
		uncaught_exception_checker inner(true);
		throw 42;
	}
	catch (...) {}
}

struct uncaught_exceptions_checker
{
	uncaught_exceptions_checker(int uncaught) : m_uncaught(uncaught) {}
	~uncaught_exceptions_checker() {
		char msg[128];
		int uncaught = std::uncaught_exceptions();
		snprintf(msg, sizeof msg, "%d uncaught exception%s in flight",
		    uncaught, uncaught == 1 ? " is" : "s are");
		TEST(uncaught == m_uncaught, msg);
	}
	int m_uncaught;
};

class top {
public:
	~top() {
		try {
			uncaught_exceptions_checker uec(4);
			throw "top";
		}
		catch (...) {}
	}
};

class middle {
public:
	~middle() {
		try {
			top f;
			uncaught_exceptions_checker uec(3);
			throw "middle";
		}
		catch (...) {}
	}
};

class bottom {
public:
	~bottom() {
		try {
			middle f;
			uncaught_exceptions_checker uec(2);
			throw "bottom";
		}
		catch (...) {}
	}
};

void test_uncaught_exceptions()
{
	uncaught_exceptions_checker outer(0);
	try {
		bottom b;
		uncaught_exceptions_checker inner(1);
		throw "test";
	}
	catch (...) {}
}

extern "C"
{
	struct __cxa_exception;

	void __cxa_increment_exception_refcount(void* thrown_exception);
	void __cxa_decrement_exception_refcount(void* thrown_exception);

	__cxa_exception *__cxa_init_primary_exception(
		void *object, std::type_info* tinfo, void (*dest)(void *));

	void __cxa_rethrow_primary_exception(void* thrown_exception);
	void *__cxa_current_primary_exception();
}

class ExceptionPtr final {
public:
	ExceptionPtr(void *ex) : ex_(ex) {
		__cxa_increment_exception_refcount(ex_);
	}
	ExceptionPtr(const ExceptionPtr& other) : ex_(other.ex_) {
		__cxa_increment_exception_refcount(ex_);
	}
	~ExceptionPtr() {
		__cxa_decrement_exception_refcount(ex_);
	}

private:
	template <typename Ex>
	friend ExceptionPtr MakeExceptionPtr(Ex);

	friend void RethrowException(ExceptionPtr);

	template <typename Ex>
	static void Dest(void *e) {
		static_cast<Ex*>(e)->~Ex();
	}

	void *ex_ = nullptr;
};

template <typename Ex>
ExceptionPtr MakeExceptionPtr(Ex e) {
	void *ex = __cxa_allocate_exception(sizeof(Ex));
	(void)__cxa_init_primary_exception(
		ex, const_cast<std::type_info*>(&typeid(Ex)), ExceptionPtr::Dest<Ex>);
	try {
		new (ex) Ex(e);
		return ExceptionPtr(ex);
	} catch (...) {
		__cxa_free_exception(ex);
		return ExceptionPtr(__cxa_current_primary_exception());
	}
}

void RethrowException(ExceptionPtr eptr) {
	__cxa_rethrow_primary_exception(eptr.ex_);
}

struct Base
{
	virtual ~Base() {}
};

static int derived_dtor_count;
struct Derived final : Base
{
	~Derived() override {
		++derived_dtor_count;
	}
};

void test_exception_ptr_direct_init()
{
	{
		ExceptionPtr eptr = MakeExceptionPtr(Derived());

		// Argument to MakeExceptionPtr is destroyed
		if (--derived_dtor_count != 0) {
			abort();
		}

		bool exception_caught = false;
		try {
			RethrowException(eptr);
		} catch (const Derived&) {
			exception_caught = true;
		} catch (...) {
		}

		if (!exception_caught) {
			abort();
		}
	}

	// ExceptionPtr went out of scope,
	// its destructor should've called the exception destructor.
	if (--derived_dtor_count != 0) {
		abort();
	}
}

extern "C" void __cxa_bad_cast();

void test_exceptions(void)
{
	std::set_unexpected(throw_zero);
	TEST_CLEANUP(test_catch(0));
	TEST_CLEANUP(test_catch(1));
	TEST_CLEANUP(test_catch(3));
	TEST_CLEANUP(test_catch(4));
	TEST_CLEANUP(test_catch(5));
	TEST_CLEANUP(test_nested());
	try{
		test_catch(2);
		TEST(violations == 1, "Exactly one exception spec violation");
	}
	catch (int64_t i) {
		TEST(0, "Caught int64_t, but that violates an exception spec");
	}
	int a;
	try {
		throw &a;
	}
	catch (const int *b)
	{
		TEST(&a==b, "Caught const int from thrown int");
	}
	try {
		throw &a;
	}
	catch (int *b)
	{
		TEST(&a==b, "Caught int from thrown int");
	}
	try
	{
		__cxa_bad_cast();
	}
	catch (std::exception b)
	{
		TEST(1, "Caught bad cast");
	}
	catch (...)
	{
		TEST(0, "Bad cast was not caught correctly");
	}
	test_const();
	test_uncaught_exception();
	test_rethrown_uncaught_exception();
	test_rethrown_uncaught_foreign_exception();
	test_uncaught_exceptions();
	test_exception_ptr_direct_init();


	//printf("Test: %s\n",
}
