#pragma once

#include "cpu_stats.hpp"
#include "system_info.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace proc_monitor {

struct MonitorState {
  std::vector<CpuUsage> cpu_usages;
  LoadAvg load_avg{};
  UptimeInfo uptime{};
  MemInfo meminfo{};
  int num_cores = 1;
};

/// Runs an FTXUI fullscreen TUI that updates every second.
/// Blocks until the user exits (Ctrl+C / 'q').
void run_tui();

} // namespace proc_monitor
