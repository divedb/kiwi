# cmake_parse_arguments(<prefix> <options> <one_value_keywords> 
#                       <multi_value_keywords> <args>...)
#
# The <options> argument contains all options for the respective function or macro. 
# These are keywords that have no value following them, like the OPTIONAL keyword of the install() command.
#
# Arguments:
#   NAME: The name of the test executable and CTest test (REQUIRED).
#   SRCS: List of source files for the test executable (REQUIRED).
#   HDRS: List of header files associated with the test (OPTIONAL).
#   COPTS: List of compile options to apply to the test executable (OPTIONAL).
#   DEPS: List of libraries to link against (OPTIONAL).
function(kiwi_cc_test)
    # kiwi_cc_test(NAME <name> SRCS <srcs> [HDRS <hdrs>] [COPTS <opts>] [DEPS <deps>])
    cmake_parse_arguments(KIWI_TARGET "" "NAME" "HDRS;SRCS;COPTS;DEPS" ${ARGN})

    if(NOT KIWI_TARGET_NAME)
        message(FATAL_ERROR "kiwi_cc_test requires NAME to be set.")
    endif()

    if(NOT KIWI_TARGET_SRCS)
        message(FATAL_ERROR "kiwi_cc_test requires SRCS to be set.")
    endif()

    add_executable(${KIWI_TARGET_NAME})
    target_sources(${KIWI_TARGET_NAME} PRIVATE ${KIWI_TARGET_SRCS})
    target_compile_features(${KIWI_TARGET_NAME} PRIVATE cxx_std_20)

    if(KIWI_TARGET_HDRS)
        target_include_directories(${KIWI_TARGET_NAME} PRIVATE ${KIWI_TARGET_HDRS})
    endif()

    if(KIWI_TARGET_COPTS)
        target_compile_options(${KIWI_TARGET_NAME} PRIVATE ${KIWI_TARGET_COPTS})
    endif()

    if(KIWI_TARGET_DEPS)
        target_link_libraries(${KIWI_TARGET_NAME} PRIVATE ${KIWI_TARGET_DEPS})
    endif()

    add_test(NAME ${KIWI_TARGET_NAME} COMMAND ${KIWI_TARGET_NAME})
endfunction()

# add_subdirectory(util)
set(COMMON_TEST_TARGETS
  traits_test
  function_test
)

foreach(target ${COMMON_TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${target}
    HDRS
      ${GTEST_INCLUDE_DIR}
      ${KIWI_INCLUDE_DIR}
    SRCS
      "common/${target}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS
      gtest
      gtest_main
  )
endforeach()

set(CONTAINERS_TEST_TARGETS
  checked_iterators_test  
  circular_deque_test
  contains_test
  flat_set_test
  flat_tree_test
  heap_array_test
)

foreach(target ${CONTAINERS_TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${target}
    HDRS
      ${GTEST_INCLUDE_DIR}
      ${KIWI_INCLUDE_DIR}
      ${KIWI_TESTS_INCLUDE_DIR}
    SRCS
      "containers/${target}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS
      gtest
      gtest_main
      glog::glog
  )
endforeach()

add_subdirectory(metrics)
add_subdirectory(strings)
add_subdirectory(types)
add_subdirectory(files)