set(TEST_TARGETS
  traits_test
  function_test
)

foreach(test_tgt ${TEST_TARGETS})
  kiwi_cc_test(
    NAME
      ${test_tgt}
    HDRS
      ${GTEST_INCLUDE_DIR}
      ${KIWI_INCLUDE_DIR}
    SRCS
      "${test_tgt}.cc"
    COPTS
      ${KIWI_TEST_COPTS}
    DEPS
      gtest
      gtest_main
  )
endforeach()