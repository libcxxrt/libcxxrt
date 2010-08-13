

// This header defines standard exception classes which are needed
// for implementing new/delete operators


#ifndef __LIBCXXSUP_STDEXCEPT_H__
#define __LIBCXXSUP_STDEXCEPT_H__

namespace std {

class exception {
public:
    exception() throw();
    exception(const exception&) throw();
    exception& operator=(const exception&) throw();
    virtual ~exception();
    virtual const char* what() const throw();
};


class bad_alloc: public exception {
public:
    bad_alloc() throw();
    bad_alloc(const bad_alloc&) throw();
    bad_alloc& operator=(const bad_alloc&) throw();
    ~bad_alloc();
    virtual const char* what() const throw();
};


class bad_cast: public exception {
public:
    bad_cast() throw();
    bad_cast(const bad_cast&) throw();
    bad_cast& operator=(const bad_cast&) throw();
    virtual ~bad_cast();
    virtual const char* what() const throw();
};


class bad_typeid: public exception {
public:
    bad_typeid() throw();
    bad_typeid(const bad_typeid &__rhs) throw();
    virtual ~bad_typeid();
    bad_typeid& operator=(const bad_typeid &__rhs) throw();
    virtual const char* what() const throw();
};



} // namespace std

#endif // __LIBCXXSUP_STDEXCEPT_H__

