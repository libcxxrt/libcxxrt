
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

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/expected_output.log" expected_output)

add_test(system-test-test
         ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/system-test
         ${CMAKE_CURRENT_SOURCE_DIR}/expected_output.log)

add_test(cxxrt-test-static-test
         ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-static
         ${CMAKE_CURRENT_SOURCE_DIR}/expected_output.log)

add_test(cxxrt-test-shared-test
         ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cxxrt-test-shared
         ${CMAKE_CURRENT_SOURCE_DIR}/expected_output.log)


