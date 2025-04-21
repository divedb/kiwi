macro(KIWI_ADD_TEST test_name)
  kiwi_cc_test(
    NAME 
        ${test_name}
    HDRS 
        ${GTEST_INCLUDE_DIR}
    SRCS 
        "${test_name}.cc"
    COPTS 
        ${KIWI_TEST_COPTS}
    DEPS 
        ${GTEST_LIBRARIES} kiwi::strings
  )
endmacro()

KIWI_ADD_TEST(ascii_test)
KIWI_ADD_TEST(cstrijng_view_test)