#pragma once

#include "kiwi/port/builtin.hh"

namespace kiwi {

[[noreturn]] inline void unreachable() {
  // Uses compiler specific extensions if possible.
  // Even if no extension is used, undefined behavior is still raised by
  // an empty function body and the noreturn attribute.
#if IS_MSC() && !IS_CLANG()  // MSVC
  __assume(false);
#else  // GCC, Clang
  __builtin_unreachable();
#endif
}

}  // namespace kiwi
