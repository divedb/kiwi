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
    kiwi::io
    kiwi::strings)
endmacro()

kiwi_files_add_test(file_path_test)
