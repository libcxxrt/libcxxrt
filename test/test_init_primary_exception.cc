#ifdef TEST_INIT_PRIMARY_EXCEPTION

#include <typeinfo>
#include <new>
#include <stdint.h>
#include <stdlib.h>

namespace __cxxabiv1 {
extern "C"
{
	struct __cxa_exception;

	void __cxa_free_exception(void *thrown_exception) throw();
	void *__cxa_allocate_exception(size_t thrown_size) throw();

	void __cxa_increment_exception_refcount(void* thrown_exception);
	void __cxa_decrement_exception_refcount(void* thrown_exception);

	__cxa_exception *__cxa_init_primary_exception(
		void *object, std::type_info* tinfo, void (*dest)(void *)) throw();

	void __cxa_rethrow_primary_exception(void* thrown_exception);
	void *__cxa_current_primary_exception();
}
}

using namespace __cxxabiv1;

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

void test_init_primary_exception(void)
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
#else
void test_init_primary_exception(void) {}
#endif

