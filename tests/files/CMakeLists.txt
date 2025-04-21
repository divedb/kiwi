macro(KIWI_FILES_ADD_TEST test_name)
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
        kiwi::files
        kiwi::strings
  )
endmacro()

KIWI_FILES_ADD_TEST(file_path_test)