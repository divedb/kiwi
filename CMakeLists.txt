cmake_minimum_required(VERSION 3.20)
project(kiwi LANGUAGES CXX)
enable_testing()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  message(STATUS "setting C++ standard to C++${CMAKE_CXX_STANDARD}")
endif()

list(APPEND KIWI_COMMON_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
list(APPEND KIWI_COMMON_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/src")
set(KIWI_TESTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/include")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(KiwiHelpers)
include(KiwiConfigureCopts)
include(KiwiConfigChecks)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/kiwi-config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/src/kiwi-config.h)

if(MSVC)
  include(KiwiCompilerMSVC)
else()
  include(KiwiCompilerUnix)
endif()

find_package(GTest REQUIRED)

option(USE_ZLIB "Enable ZLIB compression support" OFF)

if(USE_ZLIB)
  find_package(ZLIB REQUIRED)
  add_definitions(-DENABLE_ZLIB)
endif()

add_subdirectory(third_party/glog)
add_subdirectory(third_party/double-conversion)
add_subdirectory(src)
# add_subdirectory(tests)
