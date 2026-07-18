#include "proc-monitor/cpu_stats.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <string>

namespace proc_monitor {

Result<std::vector<CpuData>> read_proc_stat() {
  std::ifstream file("/proc/stat");
  if (!file.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/stat"});
  }

  std::vector<CpuData> result;
  std::string line;

  while (std::getline(file, line)) {
    if (!line.starts_with("cpu")) break;

    CpuData data;
    std::istringstream ss(line);
    ss >> data.name
       >> data.user
       >> data.nice
       >> data.system
       >> data.idle
       >> data.iowait
       >> data.irq
       >> data.softirq
       >> data.steal;

    // Handle lines with fewer fields (some kernels omit steal)
    // by zero-initializing missing values (already zero from struct init)

    result.push_back(std::move(data));
  }

  if (result.empty()) {
    return std::unexpected(Error{2, "No CPU data found in /proc/stat"});
  }

  return result;
}

std::vector<CpuUsage> compute_cpu_usage(
    const std::vector<CpuData>& prev,
    const std::vector<CpuData>& curr)
{
  size_t n = std::min(prev.size(), curr.size());
  std::vector<CpuUsage> result;
  result.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    long long delta_total = curr[i].total() - prev[i].total();
    long long delta_busy  = curr[i].busy()  - prev[i].busy();
    long long delta_user  = curr[i].user    - prev[i].user;
    long long delta_sys   = curr[i].system  - prev[i].system;
    long long delta_io    = curr[i].iowait  - prev[i].iowait;

    CpuUsage usage;
    usage.name = curr[i].name;

    if (delta_total > 0) {
      usage.usage_pct   = 100.0 * static_cast<double>(delta_busy)  / delta_total;
      usage.user_pct    = 100.0 * static_cast<double>(delta_user)  / delta_total;
      usage.system_pct  = 100.0 * static_cast<double>(delta_sys)   / delta_total;
      usage.iowait_pct  = 100.0 * static_cast<double>(delta_io)    / delta_total;
    }

    result.push_back(usage);
  }

  return result;
}

} // namespace proc_monitor
