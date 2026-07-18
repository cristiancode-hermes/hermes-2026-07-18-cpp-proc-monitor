#include "proc-monitor/proc_monitor.hpp"
#include "proc-monitor/cpu_stats.hpp"
#include "proc-monitor/system_info.hpp"

#include <cstdio>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

void print_json_snapshot() {
  auto la = proc_monitor::read_loadavg();
  auto up = proc_monitor::read_uptime();
  auto mem = proc_monitor::read_meminfo();

  json j = {
    {"load_avg", la ? json{
      {"1m", la->load_1m},
      {"5m", la->load_5m},
      {"15m", la->load_15m},
      {"running", la->running},
      {"total_threads", la->total_threads},
    } : json(nullptr)},
    {"uptime", up ? json{
      {"seconds", up->uptime_seconds},
      {"idle", up->idle_seconds},
    } : json(nullptr)},
    {"memory", mem ? json{
      {"total_kb", mem->total_kb},
      {"available_kb", mem->available_kb},
      {"free_kb", mem->free_kb},
      {"used_pct", mem->used_pct()},
      {"available_pct", mem->available_pct()},
      {"swap_total_kb", mem->swap_total_kb},
      {"swap_free_kb", mem->swap_free_kb},
    } : json(nullptr)},
  };

  std::cout << j.dump(2) << std::endl;
}

int main(int argc, char** argv) {
  spdlog::set_pattern("[%T] %^%v%$");

  bool json_mode = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--json" || arg == "-j") {
      json_mode = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Proc Monitor — Linux System Dashboard" << std::endl;
      std::cout << std::endl;
      std::cout << "  {{}} --help         Show this help" << std::endl;
      std::cout << "  {{}} --json, -j     Print one JSON snapshot and exit" << std::endl;
      std::cout << "  (no flags)          Run interactive TUI dashboard" << std::endl;
      return 0;
    }
  }

  if (json_mode) {
    print_json_snapshot();
    return 0;
  }

  spdlog::info("Starting Proc Monitor — press Ctrl+C to exit");
  proc_monitor::run_tui();
  spdlog::info("Proc Monitor stopped");

  return 0;
}
