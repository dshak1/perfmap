#include <string>

#include "gtest/gtest.h"
#include "perfmap/map_adapters.h"

namespace perfmap {
namespace {

struct AdapterNameGenerator {
  template <typename Adapter>
  static std::string GetName(int) {
    return std::string(Adapter::kTestName);
  }
};

template <typename Adapter>
class IntHashMapContractTest : public ::testing::Test {};

using IntHashMapImplementations =
    ::testing::Types<adapters::StdUnorderedMapAdapter<int, int>,
                     adapters::AbslFlatHashMapAdapter<int, int>,
                     adapters::AbslNodeHashMapAdapter<int, int>,
                     adapters::PerfMapIndirectAdapter<int, int>,
                     adapters::PerfMapAdapter<int, int>,
                     adapters::PerfMapAdapter<int, int, ReadHeavyPolicy>,
                     adapters::PerfMapAdapter<int, int, ChurnHeavyPolicy>,
                     adapters::PerfMapAdapter<int, int, SpaceEfficientPolicy>>;

TYPED_TEST_SUITE(IntHashMapContractTest, IntHashMapImplementations,
                 AdapterNameGenerator);

TYPED_TEST(IntHashMapContractTest, DefaultConstructedIsEmpty) {
  TypeParam map;
  EXPECT_TRUE(map.Empty());
  EXPECT_EQ(map.Size(), 0u);
}

TYPED_TEST(IntHashMapContractTest, InsertFindAndContainsWork) {
  TypeParam map;
  map.InsertOrAssign(1, 10);
  map.InsertOrAssign(2, 20);

  ASSERT_NE(map.FindPtr(1), nullptr);
  ASSERT_NE(map.FindPtr(2), nullptr);
  EXPECT_EQ(*map.FindPtr(1), 10);
  EXPECT_EQ(*map.FindPtr(2), 20);
  EXPECT_TRUE(map.Contains(1));
  EXPECT_TRUE(map.Contains(2));
  EXPECT_FALSE(map.Contains(99));
  EXPECT_EQ(map.Size(), 2u);
}

TYPED_TEST(IntHashMapContractTest, InsertUpdatesExistingKey) {
  TypeParam map;
  map.InsertOrAssign(7, 70);
  map.InsertOrAssign(7, 700);

  ASSERT_NE(map.FindPtr(7), nullptr);
  EXPECT_EQ(*map.FindPtr(7), 700);
  EXPECT_EQ(map.Size(), 1u);
}

TYPED_TEST(IntHashMapContractTest, MissingKeyReturnsNullptr) {
  TypeParam map;
  map.InsertOrAssign(1, 10);

  EXPECT_EQ(map.FindPtr(404), nullptr);
  EXPECT_FALSE(map.Contains(404));
}

TYPED_TEST(IntHashMapContractTest, EraseRemovesKey) {
  TypeParam map;
  map.InsertOrAssign(1, 10);
  map.InsertOrAssign(2, 20);

  EXPECT_TRUE(map.Erase(1));
  EXPECT_EQ(map.FindPtr(1), nullptr);
  EXPECT_FALSE(map.Contains(1));
  EXPECT_EQ(map.Size(), 1u);
}

TYPED_TEST(IntHashMapContractTest, ErasingMissingKeyReturnsFalse) {
  TypeParam map;
  map.InsertOrAssign(1, 10);

  EXPECT_FALSE(map.Erase(999));
  EXPECT_EQ(map.Size(), 1u);
}

TYPED_TEST(IntHashMapContractTest, ReinsertAfterEraseWorks) {
  TypeParam map;
  map.InsertOrAssign(5, 50);
  ASSERT_TRUE(map.Erase(5));

  map.InsertOrAssign(5, 500);
  ASSERT_NE(map.FindPtr(5), nullptr);
  EXPECT_EQ(*map.FindPtr(5), 500);
  EXPECT_EQ(map.Size(), 1u);
}

TYPED_TEST(IntHashMapContractTest, ManyEntriesRoundTripCorrectly) {
  TypeParam map;
  constexpr int kCount = 5000;
  map.Reserve(kCount);

  for (int i = 0; i < kCount; ++i) {
    map.InsertOrAssign(i, i * 2);
  }

  for (int i = 0; i < kCount; ++i) {
    const int* value = map.FindPtr(i);
    ASSERT_NE(value, nullptr) << "Missing key " << i;
    EXPECT_EQ(*value, i * 2);
  }
  EXPECT_EQ(map.Size(), static_cast<size_t>(kCount));
}

template <typename Adapter>
class StringHashMapContractTest : public ::testing::Test {};

using StringHashMapImplementations =
    ::testing::Types<adapters::StdUnorderedMapAdapter<std::string, int>,
                     adapters::AbslFlatHashMapAdapter<std::string, int>,
                     adapters::AbslNodeHashMapAdapter<std::string, int>,
                     adapters::PerfMapIndirectAdapter<std::string, int>,
                     adapters::PerfMapAdapter<std::string, int>,
                     adapters::PerfMapAdapter<std::string, int,
                                              ReadHeavyPolicy>,
                     adapters::PerfMapAdapter<std::string, int,
                                              ChurnHeavyPolicy>,
                     adapters::PerfMapAdapter<std::string, int,
                                              SpaceEfficientPolicy>>;

TYPED_TEST_SUITE(StringHashMapContractTest, StringHashMapImplementations,
                 AdapterNameGenerator);

TYPED_TEST(StringHashMapContractTest, SupportsStringKeys) {
  TypeParam map;
  map.InsertOrAssign("alice", 1);
  map.InsertOrAssign("bob", 2);
  map.InsertOrAssign("charlie", 3);

  ASSERT_NE(map.FindPtr("alice"), nullptr);
  ASSERT_NE(map.FindPtr("bob"), nullptr);
  ASSERT_NE(map.FindPtr("charlie"), nullptr);
  EXPECT_EQ(*map.FindPtr("alice"), 1);
  EXPECT_EQ(*map.FindPtr("bob"), 2);
  EXPECT_EQ(*map.FindPtr("charlie"), 3);
  EXPECT_EQ(map.FindPtr("dave"), nullptr);
}

}  // namespace
}  // namespace perfmap
