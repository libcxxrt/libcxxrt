cmake_minimum_required(VERSION 3.0)
project(libcxxrt)

set(CMAKE_CXX_STANDARD 11)

enable_testing()

option(LIBCXXRT_NO_EMERGENCY_BUFFERS
       "Disable emergency buffers when allocation fails throwing an exception (see section 3.3.1 of the Itanium ABI specification)"
       OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

if (CXXRT_NO_EXCEPTIONS)
    add_definitions(-D_CXXRT_NO_EXCEPTIONS)
    add_compile_options(-fno-exceptions)
endif()

add_compile_definitions($<$<BOOL:${LIBCXXRT_NO_EMERGENCY_BUFFERS}>:LIBCXXRT_NO_EMERGENCY_MALLOC>)

add_subdirectory(src)
IF(BUILD_TESTS)
add_subdirectory(test)
ENDIF()

