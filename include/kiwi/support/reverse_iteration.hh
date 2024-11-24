#pragma once

#include "kiwi/support/pointer_like_type_traits.hh"

namespace kiwi {

template <typename T = void*>
bool should_reverse_iterate() {
  // TODO(gc): Needs to figure out why reversing iteration?
#if LLVM_ENABLE_REVERSE_ITERATION
  return detail::IsPointerLike<T>::value;
#else
  return false;
#endif
}

}  // namespace kiwi