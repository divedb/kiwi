if("${GOOGLETEST_ROOT}" STREQUAL "")
    set(GOOGLETEST_ROOT "/usr/local")
endif()

if("${GOOGMOCK_ROOT}" STREQUAL "")
    set(GOOGMOCK_ROOT "/usr/local")
endif()

find_path(GTEST_INCLUDE_DIR
    NAMES gtest/gtest.h
    PATHS ${GOOGLETEST_ROOT}/include)

find_path(GMOCK_INCLUDE_DIR
    NAMES gmock/gmock.h
    PATHS ${GOOGMOCK_ROOT}/include)

find_library(GTEST_LIBRARIES
    NAME gtest
    PATHS ${GOOGLETEST_ROOT}/lib)

find_library(GMOCK_LIBRARIES
    NAME gmock
    PATHS ${GOOGMOCK_ROOT}/lib)

if(NOT GTEST_LIBRARIES OR NOT GMOCK_LIBRARIES)
  message(STATUS "fetch GTest from https://github.com/google/googletest.git")
  include(FetchContent)
  FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG release-1.11.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(BUILD_GTEST ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
else()
  message(STATUS "use system GTest")
endif()