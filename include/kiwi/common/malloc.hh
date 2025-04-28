#pragma once

#include <stdlib.h>

#include <stdexcept>

namespace kiwi {

/// TODO(gc): Adapt mimalloc.
inline size_t good_malloc_size(size_t min_size) noexcept { return min_size; }

/// Trivial wrapper around malloc that check for allocation failure and throw
/// std::bad_alloc in that case.
///
/// \param size The number of bytes to allocate.
///
/// \return The pointer to allocated buffer.
/// \methodset Allocation Wrappers
inline void* checked_malloc(size_t size) {
  void* p = malloc(size);

  if (!p) {
    throw_exception<std::bad_alloc>();
  }

  return p;
}

}  // namespace kiwi