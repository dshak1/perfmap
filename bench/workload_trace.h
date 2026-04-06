#ifndef PERFMAP_WORKLOAD_TRACE_H_
#define PERFMAP_WORKLOAD_TRACE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace perfmap::benchmarks {

enum class TraceOperation {
  kClear,
  kInsert,
  kFind,
};

enum class TraceValueType {
  kInt,
  kLargeValue,
};

struct TraceEvent {
  TraceOperation operation = TraceOperation::kFind;
  int key = 0;
  uint64_t value_seed = 0;
  bool expected_hit = false;
};

struct WorkloadTrace {
  std::string name;
  std::string description;
  std::string story;
  TraceValueType value_type = TraceValueType::kInt;
  size_t peak_live_entries = 0;
  size_t clear_ops = 0;
  size_t insert_ops = 0;
  size_t find_ops = 0;
  std::vector<TraceEvent> events;
};

inline std::string Trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

inline std::filesystem::path PerfMapSourceDir() {
#ifdef PERFMAP_SOURCE_DIR
  return std::filesystem::path(PERFMAP_SOURCE_DIR);
#else
  throw std::runtime_error("PERFMAP_SOURCE_DIR is not defined");
#endif
}

inline TraceOperation ParseOperation(const std::string& token) {
  if (token == "clear") return TraceOperation::kClear;
  if (token == "insert") return TraceOperation::kInsert;
  if (token == "find") return TraceOperation::kFind;
  throw std::runtime_error("Unsupported trace op: " + token);
}

inline TraceValueType ParseValueType(const std::string& token) {
  if (token == "int") return TraceValueType::kInt;
  if (token == "large_value") return TraceValueType::kLargeValue;
  throw std::runtime_error("Unsupported trace value type: " + token);
}

inline WorkloadTrace LoadTraceFixture(const std::string& fixture_name) {
  const std::filesystem::path path =
      PerfMapSourceDir() / "bench" / "workloads" / fixture_name;
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Unable to open trace fixture: " + path.string());
  }

  WorkloadTrace trace;
  std::string line;
  bool saw_header = false;
  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty()) continue;

    if (line[0] == '#') {
      const std::string comment = Trim(line.substr(1));
      const size_t colon = comment.find(':');
      if (colon == std::string::npos) continue;
      const std::string key = Trim(comment.substr(0, colon));
      const std::string value = Trim(comment.substr(colon + 1));
      if (key == "name") {
        trace.name = value;
      } else if (key == "description") {
        trace.description = value;
      } else if (key == "story") {
        trace.story = value;
      } else if (key == "value_type") {
        trace.value_type = ParseValueType(value);
      } else if (key == "peak_live_entries") {
        trace.peak_live_entries = static_cast<size_t>(std::stoull(value));
      }
      continue;
    }

    if (!saw_header) {
      saw_header = true;
      continue;
    }

    std::stringstream row(line);
    std::string op_token;
    std::string key_token;
    std::string seed_token;
    std::string hit_token;
    if (!std::getline(row, op_token, ',')) continue;
    if (!std::getline(row, key_token, ',')) continue;
    if (!std::getline(row, seed_token, ',')) continue;
    if (!std::getline(row, hit_token, ',')) continue;

    TraceEvent event;
    event.operation = ParseOperation(Trim(op_token));
    event.key = std::stoi(Trim(key_token));
    event.value_seed = static_cast<uint64_t>(std::stoull(Trim(seed_token)));
    event.expected_hit = Trim(hit_token) == "1";
    trace.events.push_back(event);

    switch (event.operation) {
      case TraceOperation::kClear:
        ++trace.clear_ops;
        break;
      case TraceOperation::kInsert:
        ++trace.insert_ops;
        break;
      case TraceOperation::kFind:
        ++trace.find_ops;
        break;
    }
  }

  if (trace.name.empty()) {
    trace.name = path.stem().string();
  }
  return trace;
}

}  // namespace perfmap::benchmarks

#endif  // PERFMAP_WORKLOAD_TRACE_H_
