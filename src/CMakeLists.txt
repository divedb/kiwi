# ================================================================================
# CHRONO
# ================================================================================
kiwi_cc_library(
  NAME
    chrono
  HDRS
    ${glog_SOURCE_DIR}/src
  SRCS
    "chrono/time_conversion_posix.cc"
  COPTS
    ${KIWI_DEFAULT_COPTS}
  DEPS
    glog::glog
)

# ================================================================================
# COMMON
# ================================================================================
kiwi_cc_library(
  NAME
    common
  HDRS
    ${glog_SOURCE_DIR}/src
  SRCS
    "common/environment.cc"
  COPTS
    ${KIWI_DEFAULT_COPTS}
  DEPS
    glog::glog
    kiwi::strings
)

# ================================================================================
# IO
# ================================================================================
kiwi_cc_library(
  NAME
    io
  HDRS
    ${glog_SOURCE_DIR}/src
  SRCS
    "io/file_enumerator_posix.cc"
    # "io/file_enumerator_win.cc"
    "io/file_enumerator.cc"
    "io/file_path.cc"
    "io/file_posix.cc"
    # "io/file_util_apple.mm"
    "io/file_util_posix.cc"
    "io/file_util.cc"
    "io/file_win.cc"
    "io/file.cc"
    "io/iobuf.cc"
    "io/safe_base_name.cc"
    "io/scoped_file.cc"
    "io/scoped_temp_file.cc"
    "io/uio.cc"
  COPTS
    ${KIWI_DEFAULT_COPTS}
  DEPS
    glog::glog
    kiwi::chrono
    kiwi::strings
)

if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation)
  target_link_libraries(kiwi_io PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

# ================================================================================
# STRINGS
# ================================================================================
kiwi_cc_library(
  NAME
    strings
  HDRS
    ${glog_SOURCE_DIR}/src
  SRCS
    "strings/strcat.cc"
    "strings/string_number_conversions.cc"
    "strings/string_split.cc"
    "strings/string_util_constants.cc"
    "strings/string_util.cc"
    "strings/sys_string_conversions_posix.cc"
    "strings/utf_ostream_operators.cc"
    "strings/utf_string_conversion_utils.cc"
    "strings/utf_string_conversions.cc"
  COPTS
    ${KIWI_DEFAULT_COPTS}
  DEPS
    glog::glog
    double-conversion
)
