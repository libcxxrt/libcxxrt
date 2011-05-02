TEST_OBJECTS = test_typeinfo.o test.o test_exception.o test_guard.o

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

PRODUCTS = test libcxxabi.so.1 system_test

test: $(TEST_OBJECTS) libcxxabi.so library
	@gcc $(CPPFLAGS) $(LDFLAGS) -o test $(TEST_OBJECTS) -lcxxabi -lstdc++

library:
	@cd ../src && $(MAKE)

libcxxabi.so: 
	@ln -sf ../src/libcxxabi.so.1 libcxxabi.so

# Fudge the dynamic library search path to include the current directory so
# that we can run the tests without having to install the .so
runtest: test
	@LD_LIBRARY_PATH=.:$(LD_LIBRARY_PATH) ./test

# Run the test program in the debugger
debug: test
	@LD_LIBRARY_PATH=.:$(LD_LIBRARY_PATH) gdb ./test

# Run the test program with valgrind.  Make sure that the output from this has
# no memory leaks
valgrind: test
	@LD_LIBRARY_PATH=.:$(LD_LIBRARY_PATH) valgrind --leak-check=full ./test

# Compile another version of the test program linked against libstdc++, run it,
# and ensure that both versions pass the same number of tests.  Bugs in the
# unwinding can cause some test not to be executed - this is a quick way of
# testing that the correct number pass.
compare: test
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_OBJECTS) -lstdc++ -o system_test
	@./system_test 2>&1 | tail -1 > system_test.out
	@echo Comparing libcxxabi and libstdc++ versions...
	@LD_LIBRARY_PATH=.:$(LD_LIBRARY_PATH) ./test 2>&1 | tail -1 | diff system_test.out -


.cc.o:
	@echo Compiling $<...
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< 

clean:
	@echo Cleaning...
	@rm -f $(OBJECTS) $(PRODUCTS) $(TEST_OBJECTS) vgcore* *.core
	@cd ../src && $(MAKE) clean
