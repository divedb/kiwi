# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# -g:      Generate debug information.
# -Wall:   Enable most common warnings.
# -Wextra: Enable additional useful warnings.
set(CMAKE_CXX_FLAGS_COMMON "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS_COMMON}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_COMMON} -O3")

list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")

# _REENTRANT:  Enables thread-safe code (for multithreading).
# _GNU_SOURCE: Enables GNU extensions (e.g., pthread, glibc features).

function(apply_kiwi_compile_options_to_target THETARGET)
  target_compile_definitions(${THETARGET}
    PRIVATE
      _REENTRANT
      _GNU_SOURCE
  )

  target_compile_options(${THETARGET}
    PRIVATE
      -g                                # Debug symbols
      -finput-charset=UTF-8             # Source files are UTF-8 encoded
      -fsigned-char                     # Treat `char` as signed (-128 to 127)
      -Wall                             # Standard warnings
      -Wno-deprecated                   # Suppress deprecated declarations warnings
      -Wno-deprecated-declarations      # Suppress deprecated API warnings
      -Wno-sign-compare                 # Ignore signed/unsigned comparison warnings
      -Wno-unused                       # Ignore unused variable warnings
      -Wuninitialized                   # Warn about uninitialized variables
      -Wunused-label                    # Warn about unused labels (e.g., `label:`)
      -Wunused-result                   # Warn if function return value is ignored
      ${KIWI_CXX_FLAGS}                 # Additional kiwi-specific flags
  )
endfunction()
