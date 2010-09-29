
set(CXXRT_SOURCES
    dynamic_cast.cc
    exception.cc
    guard.cc
    terminate.cc
    typeinfo.cc
    stdexcept.cc
    memory.cc
    aux.cc
    libelftc_dem_gnu3.c
    libelftc_vstr.c
   )


add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})

set_target_properties(cxxrt-static cxxrt-shared PROPERTIES
                      OUTPUT_NAME "cxxrt"
                     )



