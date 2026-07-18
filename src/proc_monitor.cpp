#include "proc-monitor/proc_monitor.hpp"
#include "proc-monitor/cpu_stats.hpp"
#include "proc-monitor/system_info.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;
using namespace std::chrono_literals;

namespace proc_monitor {

// ── Format helpers (must be defined before cpu_line) ──
std::string fmt_pct(double pct) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%5.1f%%", pct);
  return buf;
}

std::string fmt_kb(long long kb) {
  if (kb >= 1'000'000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f GB", kb / 1'000'000.0);
    return buf;
  }
  if (kb >= 1'000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB", kb / 1'000.0);
    return buf;
  }
  return std::to_string(kb) + " KB";
}

std::string fmt_duration(double seconds) {
  int d = static_cast<int>(seconds) / 86400;
  int h = (static_cast<int>(seconds) % 86400) / 3600;
  int m = (static_cast<int>(seconds) % 3600) / 60;
  int s = static_cast<int>(seconds) % 60;

  if (d > 0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%dd %02d:%02d:%02d", d, h, m, s);
    return buf;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  return buf;
}

// ── Color helpers ──
Decorator pct_color(double pct) {
  if (pct > 90.0) return color(Color::RedLight);
  if (pct > 70.0) return color(Color::Red);
  if (pct > 50.0) return color(Color::YellowLight);
  if (pct > 30.0) return color(Color::Yellow);
  return color(Color::Green);
}

Decorator mem_color(double pct) {
  if (pct > 90.0) return color(Color::RedLight);
  if (pct > 75.0) return color(Color::Red);
  if (pct > 60.0) return color(Color::YellowLight);
  return color(Color::Green);
}

// ── Progress bar Element ──
Element make_percent_bar(double pct, int width = 20) {
  int filled = std::clamp(static_cast<int>(std::round(pct / 100.0 * width)), 0, width);
  auto fill  = text(std::string(filled, '|')) | pct_color(pct);
  auto empty = text(std::string(width - filled, '.')) | color(Color::GrayDark);
  return hbox(Elements{std::move(fill), std::move(empty)});
}

// ── Helper: line with label, bar, and percentage ──
// Forward declarations needed before cpu_line
std::string fmt_pct(double pct);

Element cpu_line(const std::string& label, double pct, bool bold_label) {
  auto lbl = text(label);
  if (bold_label) lbl = lbl | bold;

  auto bar_pct = fmt_pct(pct);

  return hbox(Elements{
    lbl | size(WIDTH, GREATER_THAN, 8),
    separator(),
    make_percent_bar(pct) | size(WIDTH, GREATER_THAN, 22),
    text(" " + bar_pct),
  }) | pct_color(pct);
}

// ── TUI runner ──
void run_tui() {
  auto screen = ScreenInteractive::Fullscreen();

  // Shared state
  std::vector<CpuData> prev_cpu;
  std::vector<CpuUsage> cpu_usages;
  LoadAvg load_avg;
  UptimeInfo uptime;
  MemInfo meminfo;
  bool has_error = false;
  std::string error_msg;

  struct Snapshot {
    std::vector<CpuUsage> cpu;
    LoadAvg load;
    UptimeInfo up;
    MemInfo mem;
    bool ok;
    std::string err;
  };
  std::atomic<Snapshot*> g_snapshot{nullptr};

  // Background refresh thread
  std::atomic<bool> running{true};
  std::thread refresh_thread([&] {
    // First snapshot to get baseline
    auto first_cpu = read_proc_stat();
    if (first_cpu) prev_cpu = *first_cpu;
    std::this_thread::sleep_for(1s);

    // Minimum idle between polls to avoid busy-loop
    constexpr auto poll_interval = 1000ms;

    while (running) {
      auto snap = std::make_unique<Snapshot>();

      auto curr_cpu = read_proc_stat();
      if (curr_cpu && !prev_cpu.empty()) {
        snap->cpu = compute_cpu_usage(prev_cpu, *curr_cpu);
        prev_cpu = std::move(*curr_cpu);
      }

      auto la = read_loadavg();
      if (la) snap->load = *la;

      auto up = read_uptime();
      if (up) snap->up = *up;

      auto mem = read_meminfo();
      if (mem) snap->mem = *mem;

      snap->ok = curr_cpu.has_value();
      if (!curr_cpu) snap->err = curr_cpu.error().message;

      // Publish snapshot atomically
      Snapshot* old = g_snapshot.exchange(snap.release());
      delete old;

      screen.PostEvent(Event::Custom);
      std::this_thread::sleep_for(poll_interval);
    }
  });

  // Renderer
  auto renderer = Renderer([&] {
    // Consume latest snapshot
    Snapshot* snap = g_snapshot.exchange(nullptr);
    if (snap) {
      cpu_usages = std::move(snap->cpu);
      load_avg = snap->load;
      uptime = snap->up;
      meminfo = snap->mem;
      has_error = !snap->ok;
      error_msg = snap->err;
      delete snap;
    }

    // ── Error state ──
    if (has_error) {
      return window(
        text(" WARNING ") | bold | color(Color::RedLight),
        vbox(Elements{
          text("Error reading /proc: " + error_msg) | color(Color::Red),
          filler(),
          text("Ctrl+C to exit") | color(Color::GrayDark),
        })
      ) | border;
    }

    // ── CPU section ──
    Elements cpu_elems;
    cpu_elems.push_back(text(" Usage by Core") | bold | color(Color::Cyan));

    for (const auto& cpu : cpu_usages) {
      std::string label = (cpu.name == "cpu")
        ? "  Total "
        : "  " + cpu.name + " ";
      cpu_elems.push_back(cpu_line(label, cpu.usage_pct, cpu.name == "cpu"));
    }

    auto cpu_box = window(
      text(" CPU ") | bold | color(Color::Cyan),
      vbox(std::move(cpu_elems))
    ) | border;

    // ── Memory section ──
    double mem_used_pct = meminfo.used_pct();
    double mem_avail_pct = meminfo.available_pct();
    double swap_pct = (meminfo.swap_total_kb > 0)
      ? 100.0 * (meminfo.swap_total_kb - meminfo.swap_free_kb) / meminfo.swap_total_kb
      : 0.0;

    Elements mem_elems;
    mem_elems.push_back(text("") | size(HEIGHT, GREATER_THAN, 0));
    mem_elems.push_back(hbox(Elements{
      text("  Total:     ") | size(WIDTH, GREATER_THAN, 16),
      text(fmt_kb(meminfo.total_kb)),
    }));
    mem_elems.push_back(hbox(Elements{
      text("  Available: ") | size(WIDTH, GREATER_THAN, 16),
      text(fmt_kb(meminfo.available_kb)),
    }) | mem_color(100.0 - mem_avail_pct));
    mem_elems.push_back(hbox(Elements{
      text("  Used:      ") | size(WIDTH, GREATER_THAN, 16),
      text(fmt_kb(meminfo.total_kb - meminfo.available_kb)),
    }) | mem_color(mem_used_pct));
    mem_elems.push_back(separator());
    mem_elems.push_back(hbox(Elements{
      text("  ") | size(WIDTH, GREATER_THAN, 1),
      make_percent_bar(mem_used_pct),
      text(" " + fmt_pct(mem_used_pct)),
    }) | mem_color(mem_used_pct));

    if (meminfo.swap_total_kb > 0) {
      long long swap_used = meminfo.swap_total_kb - meminfo.swap_free_kb;
      mem_elems.push_back(hbox(Elements{
        text("  Swap:      ") | size(WIDTH, GREATER_THAN, 16),
        text(fmt_kb(swap_used) + " / " + fmt_kb(meminfo.swap_total_kb)),
      }) | mem_color(swap_pct));
    }
    mem_elems.push_back(text(""));

    auto mem_box = window(
      text(" MEMORY ") | bold | color(Color::YellowLight),
      vbox(std::move(mem_elems))
    ) | border;

    // ── System info section ──
    char load_buf[64];
    std::snprintf(load_buf, sizeof(load_buf), "  Load:     %.2f  %.2f  %.2f",
      load_avg.load_1m, load_avg.load_5m, load_avg.load_15m);

    char proc_buf[64];
    std::snprintf(proc_buf, sizeof(proc_buf), "  Procs:    %d running / %d total",
      load_avg.running, load_avg.total_threads);

    int num_cores = static_cast<int>(cpu_usages.size());
    char cores_buf[64];
    std::snprintf(cores_buf, sizeof(cores_buf), "  Cores:    %d",
      std::max(1, num_cores - 1));

    auto sys_box = window(
      text(" SYSTEM ") | bold | color(Color::Green),
      vbox(Elements{
        text("") | size(HEIGHT, GREATER_THAN, 0),
        text(load_buf),
        text(proc_buf),
        text("  Uptime:   " + fmt_duration(uptime.uptime_seconds)),
        text(cores_buf),
        text("") | size(HEIGHT, GREATER_THAN, 0),
      })
    ) | border;

    // ── Main layout ──
    return vbox(Elements{
      text(" Proc Monitor — Linux System Dashboard ") | bold | color(Color::CyanLight) | center,
      separator(),
      hbox(Elements{
        vbox(Elements{cpu_box, filler()}) | flex,
        separator(),
        vbox(Elements{mem_box, sys_box, filler()}) | flex,
      }) | flex,
      separator(),
      text(" Ctrl+C or 'q' to exit  |  Updates every 1s") | color(Color::GrayDark) | center,
    }) | border | flex;
  });

  screen.Loop(renderer);
  running = false;
  if (refresh_thread.joinable()) refresh_thread.join();
}

} // namespace proc_monitor
