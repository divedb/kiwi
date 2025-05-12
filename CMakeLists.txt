cmake_minimum_required(VERSION 3.20)
project(kiwi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(KIWI_TEST_COPTS -std=c++20 -Wall -Wextra)
set(KIWI_LIB_COPTS  -std=c++20 -Wall -Wextra)
set(KIWI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(third_party/glog)

enable_testing()
find_package(GTest REQUIRED)
link_directories(/usr/local/lib)

option(USE_ZLIB "Enable ZLIB compression support" OFF)

if(USE_ZLIB)
    find_package(ZLIB REQUIRED)    
    message(STATUS "ZLIB found: ${ZLIB_INCLUDE_DIRS}")
    add_definitions(-DENABLE_ZLIB)
endif()

include(KiwiConfigChecks)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/kiwi-config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/src/kiwi-config.h
)

if(MSVC)
  include(KiwiCompilerMSVC)
else()
  include(KiwiCompilerUnix)
endif()

add_subdirectory(src)
add_subdirectory(tests)
