function(kiwi_cc_library)
    cmake_parse_arguments(LIB "" "NAME;ALIAS" "HDRS;SRCS;COPTS;DEPS" ${ARGN})
        
    add_library(${LIB_NAME} ${LIB_SRCS})

    if(LIB_ALIAS)
        add_library(${LIB_ALIAS} ALIAS ${LIB_NAME})
    endif()

    if(LIB_HDRS)
        target_include_directories(${LIB_NAME} ${LIB_HDRS})
    endif()

    if(LIB_COPTS)
        target_compile_options(${LIB_NAME} PRIVATE ${LIB_COPTS})
    endif()

    if(LIB_DEPS)
        target_link_libraries(${LIB_NAME} PRIVATE ${LIB_DEPS})
    endif()
endfunction()