include(CMakeParseArguments)

function(kiwi_internal_dll_contains)
  cmake_parse_arguments(KIWI_INTERNAL_DLL "" "OUTPUT;TARGET" "" ${ARGN})

  string(REGEX REPLACE "^kiwi::" "" _target ${KIWI_INTERNAL_DLL_TARGET})

  if(_target IN_LIST KIWI_INTERNAL_DLL_TARGETS)
    set(${KIWI_INTERNAL_DLL_OUTPUT}
        1
        PARENT_SCOPE)
  else()
    set(${KIWI_INTERNAL_DLL_OUTPUT}
        0
        PARENT_SCOPE)
  endif()
endfunction()

function(kiwi_internal_test_dll_contains)
  cmake_parse_arguments(KIWI_INTERNAL_TEST_DLL "" "OUTPUT;TARGET" "" ${ARGN})

  string(REGEX REPLACE "^kiwi::" "" _target ${KIWI_INTERNAL_TEST_DLL_TARGET})

  if(_target IN_LIST KIWI_INTERNAL_TEST_DLL_TARGETS)
    set(${KIWI_INTERNAL_TEST_DLL_OUTPUT}
        1
        PARENT_SCOPE)
  else()
    set(${KIWI_INTERNAL_TEST_DLL_OUTPUT}
        0
        PARENT_SCOPE)
  endif()
endfunction()
