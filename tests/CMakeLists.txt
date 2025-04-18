function(kiwi_cc_test)
    # cmake_parse_arguments(<prefix> <options> <one_value_keywords> <multi_value_keywords> <args>...)
    cmake_parse_arguments(TEST "" "NAME" "HDRS;SRCS;COPTS;DEPS" ${ARGN})
    
    add_executable(${TEST_NAME} ${TEST_SRCS})

    if(TEST_HDRS)
        target_include_directories(${TEST_NAME} PRIVATE ${TEST_HDRS})
    endif()

    target_include_directories(${TEST_NAME} PRIVATE ${GTEST_INCLUDE_DIR})
    
    if(TEST_COPTS)
        target_compile_options(${TEST_NAME} PRIVATE ${TEST_COPTS})
    endif()

    if(TEST_DEPS)
        target_link_libraries(${TEST_NAME} PRIVATE ${TEST_DEPS})
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_subdirectory(util)
add_subdirectory(strings)
