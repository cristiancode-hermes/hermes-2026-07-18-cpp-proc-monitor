#include "proc-monitor/system_info.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <spdlog/spdlog.h>

namespace proc_monitor {

Result<LoadAvg> read_loadavg() {
  std::ifstream file("/proc/loadavg");
  if (!file.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/loadavg"});
  }

  LoadAvg la;
  std::string thread_info;
  char slash;

  file >> la.load_1m >> la.load_5m >> la.load_15m
       >> la.running >> slash >> la.total_threads
       >> la.last_pid;

  if (file.fail()) {
    return std::unexpected(Error{2, "Failed to parse /proc/loadavg"});
  }

  return la;
}

Result<UptimeInfo> read_uptime() {
  std::ifstream file("/proc/uptime");
  if (!file.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/uptime"});
  }

  UptimeInfo up;
  file >> up.uptime_seconds >> up.idle_seconds;

  if (file.fail()) {
    return std::unexpected(Error{2, "Failed to parse /proc/uptime"});
  }

  return up;
}

Result<MemInfo> read_meminfo() {
  std::ifstream file("/proc/meminfo");
  if (!file.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/meminfo"});
  }

  MemInfo mem{};
  std::string line;

  while (std::getline(file, line)) {
    if (line.starts_with("MemTotal:"))       std::sscanf(line.c_str(), "MemTotal: %lld", &mem.total_kb);
    else if (line.starts_with("MemFree:"))   std::sscanf(line.c_str(), "MemFree: %lld", &mem.free_kb);
    else if (line.starts_with("MemAvailable:")) std::sscanf(line.c_str(), "MemAvailable: %lld", &mem.available_kb);
    else if (line.starts_with("Buffers:"))   std::sscanf(line.c_str(), "Buffers: %lld", &mem.buffers_kb);
    else if (line.starts_with("Cached:"))    std::sscanf(line.c_str(), "Cached: %lld", &mem.cached_kb);
    else if (line.starts_with("SwapTotal:")) std::sscanf(line.c_str(), "SwapTotal: %lld", &mem.swap_total_kb);
    else if (line.starts_with("SwapFree:"))  std::sscanf(line.c_str(), "SwapFree: %lld", &mem.swap_free_kb);
  }

  if (mem.total_kb == 0) {
    return std::unexpected(Error{2, "Failed to parse /proc/meminfo - MemTotal not found"});
  }

  return mem;
}

double MemInfo::used_pct() const {
  if (total_kb == 0) return 0.0;
  return 100.0 * (total_kb - free_kb - buffers_kb - cached_kb) / total_kb;
}

double MemInfo::available_pct() const {
  if (total_kb == 0) return 0.0;
  return 100.0 * available_kb / total_kb;
}

} // namespace proc_monitor
