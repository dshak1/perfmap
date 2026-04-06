#include <cstddef>

#include <gtest/gtest.h>

#include "perfmap/hash_map.h"
#include "perfmap/indirect_hash_map.h"
#include "perfmap/map_adapters.h"
#include "perfmap/memory_metrics.h"
#include "perfmap/scratch_hash_map.h"
#include "perfmap/scratch_indirect_hash_map.h"

namespace perfmap {
namespace {

TEST(MemoryMetricsTest, HashMapReportsExactTombstonesAndBytes) {
  HashMap<int, int> map;
  map.Reserve(32);
  ASSERT_TRUE(map.Insert(1, 10).ok());
  ASSERT_TRUE(map.Insert(2, 20).ok());
  ASSERT_TRUE(map.Erase(1).ok());

  const MemoryMetrics metrics = map.memory_metrics();
  EXPECT_EQ(metrics.live_entries, 1u);
  EXPECT_EQ(metrics.tombstone_count, 1u);
  EXPECT_EQ(metrics.reserved_capacity, map.capacity());
  EXPECT_EQ(metrics.hot_table_bytes, map.capacity() * sizeof(Slot<int, int>));
  EXPECT_EQ(metrics.payload_storage_bytes, 0u);
  EXPECT_EQ(metrics.bytes_precision, MetricPrecision::kExact);
}

TEST(MemoryMetricsTest, IndirectHashMapSplitsHotAndColdStorage) {
  IndirectHashMap<int, uint64_t> map;
  map.Reserve(16);
  ASSERT_TRUE(map.Insert(1, 11).ok());
  ASSERT_TRUE(map.Insert(2, 22).ok());

  const MemoryMetrics metrics = map.memory_metrics();
  EXPECT_EQ(metrics.live_entries, 2u);
  EXPECT_EQ(metrics.reserved_capacity, map.capacity());
  EXPECT_GT(metrics.hot_table_bytes, 0u);
  EXPECT_GT(metrics.payload_storage_bytes, 0u);
  EXPECT_EQ(metrics.bytes_precision, MetricPrecision::kExact);
}

TEST(MemoryMetricsTest, ScratchMapsExposeExactCapacity) {
  ScratchHashMap<int, int> scratch;
  scratch.Reserve(64);
  ASSERT_TRUE(scratch.Insert(1, 1).ok());

  const MemoryMetrics scratch_metrics = scratch.memory_metrics();
  EXPECT_EQ(scratch_metrics.live_entries, 1u);
  EXPECT_EQ(scratch_metrics.tombstone_count, 0u);
  EXPECT_EQ(scratch_metrics.reserved_capacity, scratch.capacity());

  ScratchIndirectHashMap<int, uint64_t> indirect;
  indirect.Reserve(32);
  ASSERT_TRUE(indirect.Insert(7, 77).ok());
  const MemoryMetrics indirect_metrics = indirect.memory_metrics();
  EXPECT_EQ(indirect_metrics.live_entries, 1u);
  EXPECT_EQ(indirect_metrics.reserved_capacity, indirect.capacity());
  EXPECT_GT(indirect_metrics.payload_storage_bytes, 0u);
}

TEST(MemoryMetricsTest, BaselineAdaptersMarkEstimatedBytes) {
  adapters::StdUnorderedMapAdapter<int, int> std_map;
  std_map.Reserve(32);
  std_map.InsertOrAssign(1, 1);
  std_map.InsertOrAssign(2, 2);
  const MemoryMetrics std_metrics = std_map.Metrics();
  EXPECT_EQ(std_metrics.live_entries, 2u);
  EXPECT_EQ(std_metrics.bytes_precision, MetricPrecision::kEstimated);

  adapters::AbslFlatHashMapAdapter<int, int> flat_map;
  flat_map.Reserve(32);
  flat_map.InsertOrAssign(1, 1);
  flat_map.InsertOrAssign(2, 2);
  const MemoryMetrics flat_metrics = flat_map.Metrics();
  EXPECT_EQ(flat_metrics.live_entries, 2u);
  EXPECT_EQ(flat_metrics.bytes_precision, MetricPrecision::kEstimated);
}

}  // namespace
}  // namespace perfmap
