
set(CXXTEST_SOURCES
    test.cc
    test_guard.cc
    test_typeinfo.cc
    test_demangle.cc
   )
if (NOT CXXRT_NO_EXCEPTIONS)
set(CXXTEST_SOURCES
    ${CXXTEST_SOURCES}
    test_exception.cc
   )
endif()


option(COMPARE_TEST_OUTPUT_TO_SYSTEM_OUTPUT "Compare tests with system output" ON)
if(COMPARE_TEST_OUTPUT_TO_SYSTEM_OUTPUT)
    add_executable(system-test ${CXXTEST_SOURCES})
    set_property(TARGET system-test PROPERTY CXX_STANDARD 11)
    target_link_libraries(system-test pthread)

    # Generating expected output with system-test
    add_custom_target(test-expected-output ALL
                      COMMAND system-test > ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log 2>&1
                      DEPENDS system-test)
    set(HAVE_EXPECTED_OUTPUT TRUE)
else()
    set(HAVE_EXPECTED_OUTPUT FALSE)
endif()

set(NO_STATIC_TEST_DEFAULT OFF)
option(NO_STATIC_TEST "Don't build the static test" ${NO_STATIC_TEST_DEFAULT})
set(STATIC_LIB_DEPS gcc_eh)
find_library(LIBCOMPILER_RT_STATIC libcompiler_rt.a)
find_library(LIBGCC_STATIC libgcc.a)
if(LIBCOMPILER_RT_STATIC)
    list(APPEND STATIC_LIB_DEPS ${LIBCOMPILER_RT_STATIC})
elseif(LIBGCC_STATIC)
    list(APPEND STATIC_LIB_DEPS ${LIBGCC_STATIC})
else()
    message(STATUS "Could not find libgcc.a or libcompiler_rt.a, not building static tests")
    set(NO_STATIC_TEST TRUE)
endif()
message(STATUS "static lib deps: ${STATIC_LIB_DEPS}")

function(add_cxxrt_test NAME)
    if (HAVE_EXPECTED_OUTPUT)
        add_test(${NAME}
                 ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${NAME}
                 ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log
                 ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-output.log)
    else()
        add_test(${NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${NAME})
    endif()
endfunction()

if(NOT NO_STATIC_TEST)
    add_executable(cxxrt-test-static ${CXXTEST_SOURCES})
    set_property(TARGET cxxrt-test-static PROPERTY LINK_FLAGS "-nodefaultlibs -static")
    set_property(TARGET cxxrt-test-static PROPERTY CXX_STANDARD 11)
    target_link_libraries(cxxrt-test-static cxxrt-static pthread ${CMAKE_DL_LIBS} c ${STATIC_LIB_DEPS})
    add_cxxrt_test("cxxrt-test-static")
endif()

if (APPLE)
else()
    set(SHARED_LIB_DEPS gcc_s)
endif()

add_executable(cxxrt-test-shared ${CXXTEST_SOURCES})
set_property(TARGET cxxrt-test-shared PROPERTY LINK_FLAGS -nodefaultlibs)
set_property(TARGET cxxrt-test-shared PROPERTY CXX_STANDARD 11)
target_link_libraries(cxxrt-test-shared cxxrt-shared pthread ${CMAKE_DL_LIBS} c ${SHARED_LIB_DEPS})
add_cxxrt_test("cxxrt-test-shared")

include_directories(${CMAKE_SOURCE_DIR}/src)
if(NOT APPLE AND NOT CXXRT_NO_EXCEPTIONS)
    # No --wrap option for the macOS ld
    add_executable(cxxrt-test-foreign-exceptions test_foreign_exceptions.cc)
    set_property(TARGET cxxrt-test-foreign-exceptions PROPERTY LINK_FLAGS "-nodefaultlibs -Wl,--wrap,_Unwind_RaiseException")
    target_link_libraries(cxxrt-test-foreign-exceptions cxxrt-static gcc_s pthread ${CMAKE_DL_LIBS} c)
    add_test(cxxrt-test-foreign-exceptions
             ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-foreign-exceptions)
endif()

set(valgrind "valgrind -q")

if(TEST_VALGRIND)
    if (NOT NO_STATIC_TEST)
        add_test(cxxrt-test-static-test-valgrind
                 ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                 "${valgrind} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-static"
                 ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log
                 ${CMAKE_CURRENT_BINARY_DIR}/test-static-output.log)
    endif()
    add_test(cxxrt-test-shared-test-valgrind
             ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
             "${valgrind} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-shared"
             ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log
             ${CMAKE_CURRENT_BINARY_DIR}/test-shared-output.log)
endif()


# Testing with libunwind
option(TEST_LIBUNWIND "Test libcxxrt with libunwind" OFF)

if(TEST_LIBUNWIND AND NOT CXXRT_NO_EXCEPTIONS)
    if(NOT LIBUNWIND_PATH)
        message(FATAL_ERROR "Path to libunwind should be specified. Please set LIBUNWIND_PATH variable")
    endif()

    if (NOT NO_STATIC_TEST)
        add_executable(cxxrt-test-libunwind-static ${CXXTEST_SOURCES})
        set_property(TARGET cxxrt-test-libunwind-static PROPERTY LINK_FLAGS
                     "-L${LIBUNWIND_PATH} -nodefaultlibs")
        target_link_libraries(cxxrt-test-libunwind-static cxxrt-static
                              ${LIBUNWIND_PATH}/libunwind.a pthread ${CMAKE_DL_LIBS} c ${STATIC_LIB_DEPS})
    endif()
    add_cxxrt_test("cxxrt-test-libunwind-static")

    add_executable(cxxrt-test-libunwind-shared ${CXXTEST_SOURCES})
    set_property(TARGET cxxrt-test-libunwind-shared PROPERTY LINK_FLAGS
                 "-L${LIBUNWIND_PATH} -nodefaultlibs")
    target_link_libraries(cxxrt-test-libunwind-shared cxxrt-shared
                         ${LIBUNWIND_PATH}/libunwind.so  unwind pthread ${CMAKE_DL_LIBS} c)

    add_cxxrt_test("cxxrt-test-libunwind-shared")

    if(TEST_VALGRIND)
        if (NOT NO_STATIC_TEST)
            add_test(cxxrt-test-libunwind-static-test-valgrind
                     ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                     "${valgrind} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-libunwind-static"
                     ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log
                     ${CMAKE_CURRENT_BINARY_DIR}/test-libunwind-static-output.log)
         endif()

        add_test(cxxrt-test-libunwind-shared-test-valgrind
                 ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                 "${valgrind} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-libunwind-shared"
                 ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log
                 ${CMAKE_CURRENT_BINARY_DIR}/test-libunwind-shared-output.log)
    endif()
endif()

