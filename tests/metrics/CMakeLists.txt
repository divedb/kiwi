macro(KIWI_METRICS_ADD_TEST test_name)
  kiwi_cc_test(
    NAME 
        ${test_name}
    HDRS 
        ${GTEST_INCLUDE_DIR}
        ${KIWI_INCLUDE_DIR}
    SRCS 
        "${test_name}.cc"
    COPTS 
        ${KIWI_TEST_COPTS}
    DEPS 
        gtest
        gtest_main
        kiwi::metrics
  )
endmacro()

KIWI_METRICS_ADD_TEST(crc32_test)