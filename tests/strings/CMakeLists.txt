macro(KIWI_ADD_TEST test_name)
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
        kiwi::strings
  )
endmacro()

# KIWI_ADD_TEST(ascii_test)
KIWI_ADD_TEST(cstring_view_test)
KIWI_ADD_TEST(utf_string_conversion_utils_test)