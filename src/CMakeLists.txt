function(kiwi_cc_library)
  cmake_parse_arguments(LIB "" "NAME;ALIAS" "HDRS;SRCS;COPTS;DEPS" ${ARGN})

  add_library(${LIB_NAME} ${LIB_SRCS})

  if(LIB_ALIAS)
    add_library(${LIB_ALIAS} ALIAS ${LIB_NAME})
  endif()

  if(LIB_HDRS)
    target_include_directories(${LIB_NAME} PRIVATE ${LIB_HDRS})
  endif()

  if(LIB_COPTS)
    target_compile_options(${LIB_NAME} PRIVATE ${LIB_COPTS})
  endif()

  if(LIB_DEPS)
    target_link_libraries(${LIB_NAME} PRIVATE ${LIB_DEPS})
  endif()
endfunction()

# ================================================================================
# CHRONO
# ================================================================================
kiwi_cc_library(
  NAME
  chrono
  HDRS
  ${KIWI_INCLUDE_DIR}
  ${glog_SOURCE_DIR}/src
  SRCS
  "chrono/time_conversion_posix.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  glog::glog
  ALIAS
  kiwi::chrono)

# ================================================================================
# COMMON
# ================================================================================
kiwi_cc_library(
  NAME
  chommon
  HDRS
  ${KIWI_INCLUDE_DIR}
  ${glog_SOURCE_DIR}/src
  SRCS
  "common/environment.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  glog::glog
  kiwi::strings
  ALIAS
  kiwi::common)

# ================================================================================
# IO
# ================================================================================
# set_source_files_properties("io/file_util_apple.mm" PROPERTIES LANGUAGE
# OBJCXX)

kiwi_cc_library(
  NAME
  io
  HDRS
  ${KIWI_INCLUDE_DIR}
  ${glog_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  SRCS
  "io/file.cc"
  "io/file_enumerator.cc"
  "io/file_enumerator_posix.cc"
  "io/file_path.cc"
  "io/file_posix.cc"
  "io/file_util.cc"
  # "io/file_util_apple.mm"
  "io/file_util_posix.cc"
  "io/iobuf.cc"
  "io/safe_base_name.cc"
  "io/scoped_file.cc"
  "io/scoped_temp_file.cc"
  "io/uio.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  glog::glog
  kiwi::chrono
  kiwi::common
  kiwi::strings
  ALIAS
  kiwi::io)

if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation)
  target_link_libraries(io PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

# ================================================================================
# METRICS
# ================================================================================
kiwi_cc_library(
  NAME
  metrics
  HDRS
  ${KIWI_INCLUDE_DIR}
  SRCS
  "metrics/crc32.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  ALIAS
  kiwi::metrics)

# ================================================================================
# STRINGS
# ================================================================================
kiwi_cc_library(
  NAME
  strings
  HDRS
  ${KIWI_INCLUDE_DIR}
  ${glog_SOURCE_DIR}/src
  SRCS
  "strings/strcat.cc"
  "strings/string_number_conversions.cc"
  "strings/string_split.cc"
  "strings/string_util.cc"
  "strings/string_util_constants.cc"
  "strings/sys_string_conversions_posix.cc"
  "strings/utf_ostream_operators.cc"
  "strings/utf_string_conversion_utils.cc"
  "strings/utf_string_conversions.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  glog::glog
  double-conversion
  ALIAS
  kiwi::strings)

# ================================================================================
# SYS
# ================================================================================
kiwi_cc_library(
  NAME
  sys
  HDRS
  ${KIWI_INCLUDE_DIR}
  SRCS
  "sys/unistd.cc"
  COPTS
  ${KIWI_LIB_COPTS}
  DEPS
  ALIAS
  kiwi::sys)
