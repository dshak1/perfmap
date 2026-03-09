// Copyright 2026 SFU GDSC — PerfMap Workshop
//
// Correctness tests for PerfMap's HashMap.
//
// Rule of thumb at Google: correctness before performance.
// If the data structure is wrong, benchmark numbers are meaningless.
//
// Run:  ./perfmap_tests

#include "perfmap/hash_map.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

// Helper: call Insert/Erase and discard status when we don't need it.
// Silences [[nodiscard]] warnings in test setup code.
#define PERFMAP_IGNORE_STATUS(expr) (void)(expr)

namespace perfmap {
namespace {

// =========================================================================
// Basic operations
// =========================================================================

TEST(HashMapTest, DefaultConstructedIsEmpty) {
  HashMap<int, int> map;
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0);
}

TEST(HashMapTest, InsertAndFind) {
  HashMap<int, std::string> map;
  ASSERT_TRUE(map.Insert(1, "one").ok());
  ASSERT_TRUE(map.Insert(2, "two").ok());

  auto r1 = map.Find(1);
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(*r1, "one");

  auto r2 = map.Find(2);
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(*r2, "two");
}

TEST(HashMapTest, FindMissingKeyReturnsNotFound) {
  HashMap<int, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, 10));
  auto result = map.Find(999);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsNotFound(result.status()));
}

TEST(HashMapTest, InsertUpdatesExistingKey) {
  HashMap<int, std::string> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, "one"));
  PERFMAP_IGNORE_STATUS(map.Insert(1, "ONE"));  // upsert

  auto result = map.Find(1);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, "ONE");
  EXPECT_EQ(map.size(), 1);  // size should NOT increase
}

// =========================================================================
// Erase + tombstone correctness
// =========================================================================

TEST(HashMapTest, EraseReducesSize) {
  HashMap<int, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, 10));
  PERFMAP_IGNORE_STATUS(map.Insert(2, 20));
  EXPECT_EQ(map.size(), 2);

  ASSERT_TRUE(map.Erase(1).ok());
  EXPECT_EQ(map.size(), 1);
}

TEST(HashMapTest, EraseKeyNotFoundReturnsError) {
  HashMap<int, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, 10));
  auto status = map.Erase(999);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsNotFound(status));
}

TEST(HashMapTest, ErasedKeyIsNotFoundAfterwards) {
  HashMap<int, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, 10));
  PERFMAP_IGNORE_STATUS(map.Erase(1));

  auto result = map.Find(1);
  EXPECT_FALSE(result.ok());
}

// This is THE critical test for tombstone correctness.
// If deletion incorrectly marks a slot as kEmpty instead of kDeleted,
// the probe chain breaks and subsequent lookups fail.
TEST(HashMapTest, DeleteDoesNotBreakProbeChain) {
  // Use a tiny table to guarantee collisions.
  HashMap<int, int> map(4);
  // With capacity 4, keys 1 and 5 will hash to the same bucket
  // (assuming hash(k) % 4 yields the same index for both).
  // Even if they don't collide directly, the small table ensures
  // probe-chain interactions.
  PERFMAP_IGNORE_STATUS(map.Insert(0, 100));
  PERFMAP_IGNORE_STATUS(map.Insert(4, 400));  // collides with 0 (both map to index 0 in a 4-slot table)
  PERFMAP_IGNORE_STATUS(map.Insert(8, 800));  // also collides — probe chain: 0 → 1 → 2

  // Erase the middle element of the chain
  ASSERT_TRUE(map.Erase(4).ok());

  // Key 8 must still be findable despite the tombstone
  auto result = map.Find(8);
  ASSERT_TRUE(result.ok()) << "Tombstone broke the probe chain!";
  EXPECT_EQ(*result, 800);

  // Key 0 should still be there
  auto result0 = map.Find(0);
  ASSERT_TRUE(result0.ok());
  EXPECT_EQ(*result0, 100);
}

// =========================================================================
// Rehash
// =========================================================================

TEST(HashMapTest, RehashPreservesAllEntries) {
  HashMap<int, int> map(8);
  constexpr int kCount = 100;
  for (int i = 0; i < kCount; ++i) {
    ASSERT_TRUE(map.Insert(i, i * 10).ok());
  }

  // Verify every entry survived rehashes
  for (int i = 0; i < kCount; ++i) {
    auto result = map.Find(i);
    ASSERT_TRUE(result.ok()) << "Missing key " << i << " after rehash";
    EXPECT_EQ(*result, i * 10);
  }
  EXPECT_EQ(map.size(), kCount);
}

TEST(HashMapTest, RehashCleansUpTombstones) {
  HashMap<int, int> map(16);
  // Insert, then delete, creating tombstones
  for (int i = 0; i < 10; ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(i, i));
  }
  for (int i = 0; i < 5; ++i) {
    PERFMAP_IGNORE_STATUS(map.Erase(i));
  }
  EXPECT_EQ(map.size(), 5);

  // Force a rehash
  map.Rehash(32);

  // Remaining entries should be intact
  for (int i = 5; i < 10; ++i) {
    auto result = map.Find(i);
    ASSERT_TRUE(result.ok()) << "Missing key " << i << " after rehash";
    EXPECT_EQ(*result, i);
  }

  // Deleted entries should still be gone
  for (int i = 0; i < 5; ++i) {
    auto result = map.Find(i);
    EXPECT_FALSE(result.ok()) << "Deleted key " << i << " reappeared after rehash!";
  }
}

// =========================================================================
// Load factor
// =========================================================================

TEST(HashMapTest, LoadFactorIsCorrect) {
  HashMap<int, int> map(100);
  for (int i = 0; i < 50; ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(i, i));
  }
  EXPECT_NEAR(map.load_factor(), 0.5, 0.01);
}

TEST(HashMapTest, AutoRehashKeepsLoadFactorBelowThreshold) {
  HashMap<int, int> map(16);
  for (int i = 0; i < 1000; ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(i, i));
  }
  // After many inserts, load factor should still be managed
  EXPECT_LE(map.load_factor(), 0.75);  // small margin above 0.7
  EXPECT_EQ(map.size(), 1000);
}

// =========================================================================
// String keys (non-trivial hash + equality)
// =========================================================================

TEST(HashMapTest, StringKeys) {
  HashMap<std::string, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert("alice", 1));
  PERFMAP_IGNORE_STATUS(map.Insert("bob", 2));
  PERFMAP_IGNORE_STATUS(map.Insert("charlie", 3));

  EXPECT_EQ(*map.Find("alice"), 1);
  EXPECT_EQ(*map.Find("bob"), 2);
  EXPECT_EQ(*map.Find("charlie"), 3);
  EXPECT_FALSE(map.Find("dave").ok());
}

// =========================================================================
// Edge cases
// =========================================================================

TEST(HashMapTest, InsertAndEraseRepeatedly) {
  HashMap<int, int> map;
  for (int round = 0; round < 10; ++round) {
    for (int i = 0; i < 100; ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(i, round * 100 + i));
    }
    for (int i = 0; i < 100; ++i) {
      ASSERT_TRUE(map.Erase(i).ok());
    }
    EXPECT_EQ(map.size(), 0);
  }
}

TEST(HashMapTest, LargeNumberOfEntries) {
  HashMap<int, int> map;
  constexpr int kN = 50000;
  for (int i = 0; i < kN; ++i) {
    ASSERT_TRUE(map.Insert(i, i).ok());
  }
  EXPECT_EQ(map.size(), kN);
  for (int i = 0; i < kN; ++i) {
    auto r = map.Find(i);
    ASSERT_TRUE(r.ok()) << "Missing key " << i;
    EXPECT_EQ(*r, i);
  }
}

TEST(HashMapTest, ReinsertAfterErase) {
  HashMap<int, int> map;
  PERFMAP_IGNORE_STATUS(map.Insert(1, 10));
  PERFMAP_IGNORE_STATUS(map.Erase(1));
  PERFMAP_IGNORE_STATUS(map.Insert(1, 20));  // reinsert into tombstone slot

  auto result = map.Find(1);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, 20);
  EXPECT_EQ(map.size(), 1);
}

}  // namespace
}  // namespace perfmap
