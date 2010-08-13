
set(CXXRT_SOURCES
    dynamic_cast.cc
    exception.cc
    guard.cc
    terminate.cc
    typeinfo.cc
    stdexcept.cc
    memory.cc
    aux.cc
   )


add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})

set_target_properties(cxxrt-static cxxrt-shared PROPERTIES
                      OUTPUT_NAME "cxxrt"
                     )



