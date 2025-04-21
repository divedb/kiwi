cmake_minimum_required(VERSION 3.20)
project(kiwi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(KIWI_TEST_COPTS -std=c++20 -Wall -Wextra)
set(KIWI_LIB_COPTS  -std=c++20 -Wall -Wextra)
set(KIWI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
# include(FindGTest)
enable_testing()
find_package(GTest REQUIRED)
link_directories(/usr/local/lib)

add_subdirectory(src)
add_subdirectory(tests)
