cmake_minimum_required(VERSION 3.0.0)
project(anyf VERSION 0.1.0)

include(CTest)
enable_testing()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
add_definitions(-DPACK_DEBUG)
endif(CMAKE_BUILD_TYPE)

if (MINGW AND CMAKE_BUILD_TYPE MATCHES "Release")
set(CMAKE_C_FLAGS "-fexec-charset=GBK ${CMAKE_C_FLAGS}")
endif(MINGW AND CMAKE_BUILD_TYPE MATCHES "Release")

set(main_include ./anyf)
set(osp_include ./ospath)
include_directories(${main_include} ${osp_include})

aux_source_directory(./entry source_code)
aux_source_directory(./anyf source_code)
aux_source_directory(./codecs source_code)
aux_source_directory(./ospath source_code)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../binary)
add_executable(anyf ${source_code})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
