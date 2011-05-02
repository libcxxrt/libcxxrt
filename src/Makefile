OBJECTS = \
		aux.o \
		dynamic_cast.o \
		exception.o \
		guard.o \
		libelftc_vstr.o \
		libelftc_dem_gnu3.o \
		memory.o \
		stdexcept.o \
		typeinfo.o

# Needed for building the shared library
CXXFLAGS = -fPIC
# Needed for GCC atomic ops to work on x86.
CXXFLAGS += -march=native
# This library implements exception handling, so make sure that the compiler
# emits the correct code
CXXFLAGS += -fexceptions 

# Useful flags for debugging
CXXFLAGS += -Wall -pedantic -g 

# silence warnings about LL suffix (only works with clang / recent GCC):
#CXXFLAGS += -std=c++0x

# Find the unwind.h header installed from ports
CPPFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib -L. -lpthread -fexceptions

PRODUCTS = libcxxabi.so.1

all: libcxxabi.so.1 libcxxabi.a

libcxxabi.so.1: $(OBJECTS)
	@echo Linking $@...
	@gcc -fexception -shared $(OBJECTS) $(LDFLAGS) -o libcxxabi.so.1 #-lunwind

libcxxabi.a: $(OBJECTS)
	@ar cr libcxxabi.a $(OBJECTS)


.cc.o:
	@echo Compiling $<...
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< 

.c.o:
	@echo Compiling $<...
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $< 

clean:
	@echo Cleaning...
	@rm -f $(OBJECTS) $(PRODUCTS) $(TEST_OBJECTS) vgcore* *.core

# Make seems to think that typeinfo should be created by compiling typeinfo.cc
# into an executable.  This is bad because it introduces a circular dependency,
# and if you're unlucky tries to do some crazy things and deletes typeinfo.  We
# add this rule as a little fudge to tell Make to behave like a good little
# program.
typeinfo: typeinfo.h
	@touch typeinfo

dynamic_cast.o: dynamic_cast.cc typeinfo.h abi_namespace.h typeinfo
exception.o: exception.cc typeinfo.h abi_namespace.h typeinfo dwarf_eh.h
guard.o: guard.cc
typeinfo.o: typeinfo.cc typeinfo.h abi_namespace.h typeinfo
memory.o: memory.cc 
aux.o: aux.cc 
