cmake_minimum_required(VERSION 3.10)
project(kiwi)
enable_testing()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(KIWI_TEST_COPTS -std=c++20 -Wall -Wextra -Werror -g)
set(KIWI_LIB_COPTS  -std=c++20 -Wall -Wextra -Werror)

include_directories("${CMAKE_SOURCE_DIR}/include")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FindGTest)

add_subdirectory(src)
add_subdirectory(test)
