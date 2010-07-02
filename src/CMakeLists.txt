
set(CXXRT_SOURCES
    dynamic_cast.cc
    exception.cc
    guard.cc
    terminate.cc
    typeinfo.cc
   )


add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})

set_target_properties(cxxrt-static cxxrt-shared PROPERTIES
                      OUTPUT_NAME "cxxrt"
                      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
                      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")



