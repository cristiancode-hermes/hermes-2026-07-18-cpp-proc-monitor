#pragma once

#include "common.hpp"

#include <string>
#include <vector>

namespace proc_monitor {

struct CpuData {
  std::string name;     // "cpu" for aggregate, "cpu0", "cpu1", ...
  long long user = 0;
  long long nice = 0;
  long long system = 0;
  long long idle = 0;
  long long iowait = 0;
  long long irq = 0;
  long long softirq = 0;
  long long steal = 0;

  long long total() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }

  long long busy() const {
    return total() - idle - iowait;
  }
};

struct CpuUsage {
  std::string name;
  double usage_pct = 0.0;     // 0.0-100.0
  double user_pct = 0.0;
  double system_pct = 0.0;
  double iowait_pct = 0.0;
};

/// Reads all CPU lines from /proc/stat (aggregate + per-core)
Result<std::vector<CpuData>> read_proc_stat();

/// Computes delta between two snapshots → per-core usage percentages
std::vector<CpuUsage> compute_cpu_usage(
    const std::vector<CpuData>& prev,
    const std::vector<CpuData>& curr);

} // namespace proc_monitor
