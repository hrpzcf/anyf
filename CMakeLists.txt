cmake_minimum_required(VERSION 3.27.0)
project(anyf VERSION 0.1.0)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DANYF_DEBUG)
endif(CMAKE_BUILD_TYPE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

set(include_dirs
    "anyf"
    "entry"
    "ospath"
)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    list(APPEND include_dirs "msch")
endif()

include_directories(${include_dirs})

if (MINGW AND CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_C_FLAGS "-fexec-charset=GBK ${CMAKE_C_FLAGS}")
endif(MINGW AND CMAKE_BUILD_TYPE MATCHES "Release")

add_executable(anyf
    "anyf/anyf.c"
    "codecs/m2mcvt.c"
    "entry/main.c"
    "ospath/ospath.c"
)
