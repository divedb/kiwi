#include "kiwi/containers/span.hh"

#include <gtest/gtest.h>

TEST(Span, SplitAt) {
  constexpr std::string_view sv("abc");
  constexpr int size = sv.size();

  auto sp = kiwi::span(sv);

  auto [first, remain] = sp.split_at(static_cast<size_t>(1));

  std::cout << first.size() << std::endl;
  std::cout << remain.size() << std::endl;
}