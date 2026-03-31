#include <array>

#include "gtest/gtest.h"
#include "perfmap/scratch_indirect_hash_map.h"

namespace perfmap {
namespace {

struct LargeTestValue {
  std::array<uint64_t, 64> words{};
};

TEST(ScratchIndirectHashMapTest, ClearRemovesAllEntries) {
  ScratchIndirectHashMap<int, LargeTestValue> map;
  LargeTestValue value;
  value.words[0] = 7;
  ASSERT_TRUE(map.Insert(1, value).ok());

  map.Clear();

  EXPECT_EQ(map.FindPtr(1), nullptr);
  EXPECT_TRUE(map.empty());
}

TEST(ScratchIndirectHashMapTest, ReuseAfterClearWorks) {
  ScratchIndirectHashMap<int, LargeTestValue> map;
  LargeTestValue first;
  first.words[0] = 1;
  LargeTestValue second;
  second.words[0] = 99;

  ASSERT_TRUE(map.Insert(1, first).ok());
  map.Clear();
  ASSERT_TRUE(map.Insert(1, second).ok());

  const LargeTestValue* result = map.FindPtr(1);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->words[0], 99u);
}

TEST(ScratchIndirectHashMapTest, ReserveAndLookupLargeValues) {
  ScratchIndirectHashMap<int, LargeTestValue> map;
  map.Reserve(2048);

  for (int i = 0; i < 2048; ++i) {
    LargeTestValue value;
    value.words[0] = static_cast<uint64_t>(i);
    value.words[63] = static_cast<uint64_t>(i * 3);
    ASSERT_TRUE(map.Insert(i, value).ok());
  }

  for (int i = 0; i < 2048; ++i) {
    const LargeTestValue* result = map.FindPtr(i);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->words[0], static_cast<uint64_t>(i));
    EXPECT_EQ(result->words[63], static_cast<uint64_t>(i * 3));
  }
}

}  // namespace
}  // namespace perfmap
