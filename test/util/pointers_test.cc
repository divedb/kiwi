#include "kiwi/util/pointers.hh"

#include <gtest/gtest.h>

using namespace kiwi;
using namespace std;

namespace {

// #define CONFIRM_COMPILATION_ERRORS

bool helper(NotNull<int*> p) { return *p == 12; }
bool helper_const(NotNull<const int*> p) { return *p == 12; }
bool strict_helper(StrictNotNull<int*> p) { return *p == 12; }
bool strict_helper_const(StrictNotNull<const int*> p) { return *p == 12; }

#ifdef CONFIRM_COMPILATION_ERRORS
int* return_pointer() { return nullptr; }
const int* return_pointer_const() { return nullptr; }
#endif

}  // namespace

TEST(TestStrictNotNull, Constructor) {
  {
    int x = 42;

#ifdef CONFIRM_COMPILATION_ERRORS
    StrictNotNull<int*> snn = &x;
    strict_helper(&x);
    strict_helper_const(&x);
    strict_helper(return_pointer());
    strict_helper_const(return_pointer_const());
#endif

    const StrictNotNull<int*> snn1{&x};

    helper(snn1);
    helper_const(snn1);

    EXPECT_TRUE(*snn1 == 42);
  }

  {
    // raw ptr <-> StrictNotNull
    const int x = 42;

#ifdef CONFIRM_COMPILATION_ERRORS
    StrictNotNull<int*> snn = &x;
    strict_helper(&x);
    strict_helper_const(&x);
    strict_helper(return_pointer());
    strict_helper_const(return_pointer_const());
#endif

    const StrictNotNull<const int*> snn1{&x};

#ifdef CONFIRM_COMPILATION_ERRORS
    helper(snn1);
#endif
    helper_const(snn1);

    EXPECT_TRUE(*snn1 == 42);
  }

  {
    // StrictNotNull -> StrictNotNull
    int x = 42;

    StrictNotNull<int*> snn1{&x};
    const StrictNotNull<int*> snn2{&x};

    strict_helper(snn1);
    strict_helper_const(snn1);
    strict_helper_const(snn2);

    EXPECT_TRUE(snn1 == snn2);
  }

  {
    // StrictNotNull -> StrictNotNull
    const int x = 42;

    StrictNotNull<const int*> snn1{&x};
    const StrictNotNull<const int*> snn2{&x};

#ifdef CONFIRM_COMPILATION_ERRORS
    strict_helper(snn1);
#endif
    strict_helper_const(snn1);
    strict_helper_const(snn2);

    EXPECT_TRUE(snn1 == snn2);
  }

  {
    // StrictNotNull -> NotNull
    int x = 42;

    StrictNotNull<int*> snn{&x};

    const NotNull<int*> nn1 = snn;
    const NotNull<int*> nn2{snn};

    helper(snn);
    helper_const(snn);

    EXPECT_TRUE(snn == nn1);
    EXPECT_TRUE(snn == nn2);
  }

  {
    // StrictNotNull -> NotNull
    const int x = 42;

    StrictNotNull<const int*> snn{&x};

    const NotNull<const int*> nn1 = snn;
    const NotNull<const int*> nn2{snn};

#ifdef CONFIRM_COMPILATION_ERRORS
    helper(snn);
#endif
    helper_const(snn);

    EXPECT_TRUE(snn == nn1);
    EXPECT_TRUE(snn == nn2);
  }
}

TEST(TestStrictNotNull, HashFunction) {
  {
    // NotNull -> StrictNotNull
    const int x = 42;

    NotNull<const int*> nn{&x};

    const StrictNotNull<const int*> snn1{nn};
    const StrictNotNull<const int*> snn2{nn};

#ifdef CONFIRM_COMPILATION_ERRORS
    strict_helper(nn);
#endif
    strict_helper_const(nn);

    EXPECT_TRUE(snn1 == nn);
    EXPECT_TRUE(snn2 == nn);

    std::hash<StrictNotNull<const int*>> hash_snn;
    std::hash<NotNull<const int*>> hash_nn;

    EXPECT_TRUE(hash_nn(snn1) == hash_nn(nn));
    EXPECT_TRUE(hash_snn(snn1) == hash_nn(nn));
    EXPECT_TRUE(hash_nn(snn1) == hash_nn(snn2));
    EXPECT_TRUE(hash_snn(snn1) == hash_snn(nn));
  }
}

TEST(TestStrictNotNull, TypeDeduction) {
  {
    int i = 42;

    StrictNotNull x{&i};
    helper(StrictNotNull{&i});
    helper_const(StrictNotNull{&i});

    EXPECT_TRUE(*x == 42);
  }

  {
    const int i = 42;

    StrictNotNull x{&i};
#ifdef CONFIRM_COMPILATION_ERRORS
    helper(StrictNotNull{&i});
#endif
    helper_const(StrictNotNull{&i});

    EXPECT_TRUE(*x == 42);
  }

  {
    int i = 42;
    int* p = &i;

    StrictNotNull x{p};
    helper(StrictNotNull{p});
    helper_const(StrictNotNull{p});

    EXPECT_TRUE(*x == 42);
  }

  {
    const int i = 42;
    const int* p = &i;

    StrictNotNull x{p};
#ifdef CONFIRM_COMPILATION_ERRORS
    helper(StrictNotNull{p});
#endif
    helper_const(StrictNotNull{p});

    EXPECT_TRUE(*x == 42);
  }
}

namespace {

constexpr char kDeathString[] = "Expected Death";
constexpr char kFailedSetTerminateDeathString[] = ".*";

// This prevents a failed call to set_terminate from failing the test suite.
constexpr const char* GetExpectedDeathString(std::terminate_handler handle) {
  return handle ? kDeathString : kFailedSetTerminateDeathString;
}

}  // namespace

TEST(TestStrictNotNull, DerefNullPointer) {
  const auto terminate_handler = std::set_terminate([] {
    std::cerr << "Expected Death. TestStrictNotNullConstructorTypeDeduction";
    std::abort();
  });
  const auto expected = GetExpectedDeathString(terminate_handler);

  {
    auto workaround_macro = []() {
      int* p1 = nullptr;
      const StrictNotNull x{p1};
    };

    EXPECT_DEATH(workaround_macro(), expected);
  }

  {
    auto workaround_macro = []() {
      const int* p1 = nullptr;
      const StrictNotNull x{p1};
    };
    EXPECT_DEATH(workaround_macro(), expected);
  }

  {
    int* p = nullptr;

    EXPECT_DEATH(helper(StrictNotNull{p}), expected);
    EXPECT_DEATH(helper_const(StrictNotNull{p}), expected);
  }

#ifdef CONFIRM_COMPILATION_ERRORS
  {
    StrictNotNull x{nullptr};
    helper(StrictNotNull{nullptr});
    helper_const(StrictNotNull{nullptr});
  }
#endif
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}