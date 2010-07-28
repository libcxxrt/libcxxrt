
set(CXXTEST_SOURCES
    test.cc
    test_exception.cc
    test_guard.cc
    test_typeinfo.cc
   )


add_executable(cxxrt-test-static ${CXXTEST_SOURCES})
target_link_libraries(cxxrt-test-static cxxrt-static pthread dl)

add_executable(cxxrt-test-shared ${CXXTEST_SOURCES})
target_link_libraries(cxxrt-test-shared cxxrt-shared pthread dl)

add_executable(system-test ${CXXTEST_SOURCES})

# Generating excpected output with system-test
add_custom_target(test-expected-output ALL
                  COMMAND system-test > ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log 2>&1
                  DEPENDS system-test)


add_test(cxxrt-test-static-test
         ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-static
         ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log)

add_test(cxxrt-test-shared-test
         ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-shared
         ${CMAKE_CURRENT_BINARY_DIR}/expected_output.log)


