# ================================================================================
# APPLE
# ================================================================================
# if(APPLE) find_library(COREFOUNDATION_LIB CoreFoundation) if(NOT
# COREFOUNDATION_LIB) message(FATAL_ERROR "CoreFoundation framework not found")
# endif()

# set(APPLE_TEST_TARGETS scoped_cftyperef_test)

# foreach(target ${APPLE_TEST_TARGETS}) kiwi_cc_test( NAME ${target} HDRS
# ${GTEST_INCLUDE_DIR} ${KIWI_INCLUDE_DIR} SRCS "apple/${target}.cc" COPTS
# ${KIWI_TEST_COPTS} DEPS gtest gtest_main)

# target_link_libraries(${target} PRIVATE ${COREFOUNDATION_LIB})

# endforeach() endif()

# ================================================================================
# COMMON
# ================================================================================
set(COMMON_TEST_TARGETS 
  bits_test bit_cast_test
  environment_test
  traits_test
  indestructible_test
  function_test)

foreach(target ${COMMON_TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${target}
    HDRS
      ${KIWI_TESTS_INCLUDE_DIR}
    SRCS
      "common/${target}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS 
      kiwi::common
  )
endforeach()

# ================================================================================
# CONTAINERS
# ================================================================================
set(CONTAINERS_TEST_TARGETS
    adapters_test
    checked_iterators_test
    circular_deque_test
    contains_test
    heap_array_test
    flat_set_test
    flat_tree_test
    span_test
    to_vector_test # vector_buffer_test
)

foreach(target ${CONTAINERS_TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${target}
    SRCS
      "containers/${target}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS
      glog::glog)
endforeach()

# # ================================================================================
# # # IO #
# # ================================================================================
# set(IO_TEST_TARGETS scoped_temp_file_test)

# foreach(target ${IO_TEST_TARGETS})
#   kiwi_cc_test(
#     NAME
#     ${target}
#     HDRS
#     ${GTEST_INCLUDE_DIR}
#     ${KIWI_INCLUDE_DIR}
#     SRCS
#     "io/${target}.cc"
#     COPTS
#     ${KIWI_TEST_COPTS}
#     DEPS
#     GTest::gtest
#     GTest::gtest_main
#     glog::glog
#     kiwi::io)
# endforeach()

# set(TYPES_TEST_TARGETS pass_key_test supports_ostream_operator_test)

# foreach(target ${TYPES_TEST_TARGETS})
#   kiwi_cc_test(
#     NAME
#     ${target}
#     HDRS
#     ${GTEST_INCLUDE_DIR}
#     ${KIWI_INCLUDE_DIR}
#     SRCS
#     "types/${target}.cc"
#     COPTS
#     ${KIWI_TEST_COPTS}
#     DEPS
#     GTest::gtest
#     GTest::gtest_main
#     glog::glog)
# endforeach()

# ================================================================================
# SYNCHRONIZATION
# ================================================================================
set(SYNCHRONIZATION_TEST_TARGETS
    lock_test
)

foreach(target ${SYNCHRONIZATION_TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${target}
    SRCS
      "synchronization/${target}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS
      glog::glog)
endforeach()
