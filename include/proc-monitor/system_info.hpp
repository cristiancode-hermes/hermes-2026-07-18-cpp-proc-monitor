#pragma once

#include "common.hpp"

#include <string>

namespace proc_monitor {

struct LoadAvg {
  double load_1m = 0.0;
  double load_5m = 0.0;
  double load_15m = 0.0;
  int running = 0;
  int total_threads = 0;
  int last_pid = 0;
};

struct UptimeInfo {
  double uptime_seconds = 0.0;
  double idle_seconds = 0.0;
};

struct MemInfo {
  long long total_kb = 0;
  long long free_kb = 0;
  long long available_kb = 0;
  long long buffers_kb = 0;
  long long cached_kb = 0;
  long long swap_total_kb = 0;
  long long swap_free_kb = 0;

  double used_pct() const;
  double available_pct() const;
};

Result<LoadAvg> read_loadavg();
Result<UptimeInfo> read_uptime();
Result<MemInfo> read_meminfo();

} // namespace proc_monitor
