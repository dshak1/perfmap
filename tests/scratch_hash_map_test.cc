#include "perfmap/scratch_hash_map.h"

#include "gtest/gtest.h"

namespace perfmap {
namespace {

TEST(ScratchHashMapTest, ClearRemovesAllEntries) {
  ScratchHashMap<int, int> map;
  ASSERT_TRUE(map.Insert(1, 10).ok());
  ASSERT_TRUE(map.Insert(2, 20).ok());

  map.Clear();

  EXPECT_EQ(map.FindPtr(1), nullptr);
  EXPECT_EQ(map.FindPtr(2), nullptr);
  EXPECT_TRUE(map.empty());
}

TEST(ScratchHashMapTest, ReuseAfterClearWorks) {
  ScratchHashMap<int, int> map;
  ASSERT_TRUE(map.Insert(1, 10).ok());

  map.Clear();
  ASSERT_TRUE(map.Insert(1, 99).ok());

  ASSERT_NE(map.FindPtr(1), nullptr);
  EXPECT_EQ(*map.FindPtr(1), 99);
  EXPECT_EQ(map.size(), 1u);
}

TEST(ScratchHashMapTest, ReserveAndRoundTrip) {
  ScratchHashMap<int, int> map;
  map.Reserve(4096);

  for (int i = 0; i < 4096; ++i) {
    ASSERT_TRUE(map.Insert(i, i * 2).ok());
  }

  for (int i = 0; i < 4096; ++i) {
    const int* value = map.FindPtr(i);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, i * 2);
  }
}

}  // namespace
}  // namespace perfmap
