#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "proc-monitor/cpu_stats.hpp"
#include "proc-monitor/system_info.hpp"

using namespace proc_monitor;
using Catch::Approx;

// ── compute_cpu_usage unit tests ──

TEST_CASE("compute_cpu_usage: zero delta", "[cpu]") {
  CpuData d;
  d.name = "cpu";
  d.user = 100; d.nice = 0; d.system = 50; d.idle = 800; d.iowait = 30;
  d.irq = 5; d.softirq = 10; d.steal = 5;

  auto result = compute_cpu_usage({d}, {d});
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].usage_pct == Approx(0.0));
}

TEST_CASE("compute_cpu_usage: 100% busy", "[cpu]") {
  CpuData prev;
  prev.name = "cpu";
  prev.user = 0; prev.system = 0; prev.idle = 0; prev.iowait = 0;
  prev.nice = 0; prev.irq = 0; prev.softirq = 0; prev.steal = 0;

  CpuData curr;
  curr.name = "cpu";
  curr.user = 200; curr.system = 100; curr.idle = 0; curr.iowait = 0;
  curr.nice = 0; curr.irq = 0; curr.softirq = 0; curr.steal = 0;

  auto result = compute_cpu_usage({prev}, {curr});
  REQUIRE(result.size() == 1);
  // busy = user+system = 300 out of 300 total
  REQUIRE(result[0].usage_pct == Approx(100.0));
}

TEST_CASE("compute_cpu_usage: 50% busy", "[cpu]") {
  CpuData prev;
  prev.name = "cpu";
  prev.user = 0; prev.system = 0; prev.idle = 0;
  prev.nice = prev.irq = prev.softirq = prev.steal = prev.iowait = 0;

  CpuData curr;
  curr.name = "cpu";
  curr.user = 50; curr.system = 50; curr.idle = 100;
  curr.nice = curr.irq = curr.softirq = curr.steal = curr.iowait = 0;

  auto result = compute_cpu_usage({prev}, {curr});
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].usage_pct == Approx(50.0));
}

TEST_CASE("compute_cpu_usage: multi-core", "[cpu]") {
  CpuData cpu_prev, cpu0_prev, cpu1_prev;
  cpu_prev.name = "cpu";
  cpu0_prev.name = "cpu0";
  cpu1_prev.name = "cpu1";

  CpuData cpu_curr, cpu0_curr, cpu1_curr;
  cpu_curr.name = "cpu";
  cpu0_curr.name = "cpu0";
  cpu1_curr.name = "cpu1";

  cpu0_curr.user = 100; cpu0_curr.idle = 100;
  cpu1_curr.user = 0;   cpu1_curr.idle = 200;

  auto result = compute_cpu_usage(
    {cpu_prev, cpu0_prev, cpu1_prev},
    {cpu_curr, cpu0_curr, cpu1_curr}
  );

  REQUIRE(result.size() == 3);
  REQUIRE(result[1].usage_pct == Approx(50.0));  // cpu0: 50%
  REQUIRE(result[2].usage_pct == Approx(0.0));   // cpu1: 0%
}

// ── proc filesystem tests (requires /proc) ──

TEST_CASE("read_proc_stat returns valid data", "[proc][integration]") {
  auto result = read_proc_stat();
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->empty());
  // First entry should be aggregate "cpu"
  REQUIRE(result->at(0).name == "cpu");
  // At least one core line
  REQUIRE(result->size() >= 2);
}

TEST_CASE("read_loadavg returns valid data", "[proc][integration]") {
  auto result = read_loadavg();
  REQUIRE(result.has_value());
  // Load averages should be non-negative
  REQUIRE(result->load_1m >= 0.0);
  REQUIRE(result->load_5m >= 0.0);
  REQUIRE(result->load_15m >= 0.0);
  // Should have at least one process
  REQUIRE(result->total_threads > 0);
}

TEST_CASE("read_uptime returns valid data", "[proc][integration]") {
  auto result = read_uptime();
  REQUIRE(result.has_value());
  // Uptime should be positive (system has been running)
  REQUIRE(result->uptime_seconds > 0.0);
  REQUIRE(result->idle_seconds >= 0.0);
}

TEST_CASE("read_meminfo returns valid data", "[proc][integration]") {
  auto result = read_meminfo();
  REQUIRE(result.has_value());
  // Memory should be > 0
  REQUIRE(result->total_kb > 0);
  REQUIRE(result->available_kb > 0);
  // Percentages should be valid
  auto used = result->used_pct();
  auto avail = result->available_pct();
  REQUIRE(used >= 0.0);
  REQUIRE(used <= 100.0);
  REQUIRE(avail >= 0.0);
  REQUIRE(avail <= 100.0);
}

TEST_CASE("read_meminfo used + available sanity", "[proc][integration]") {
  auto result = read_meminfo();
  REQUIRE(result.has_value());
  // Used + Available should not exceed total + a small margin
  // (available accounts for reclaimable memory, so it can be > free)
  long long used = result->total_kb - result->available_kb;
  REQUIRE(used >= 0);
  REQUIRE(used <= result->total_kb);
}

// ── compute_cpu_usage edge cases ──

TEST_CASE("compute_cpu_usage: empty input", "[cpu]") {
  auto result = compute_cpu_usage({}, {});
  REQUIRE(result.empty());
}

TEST_CASE("compute_cpu_usage: mismatched sizes", "[cpu]") {
  CpuData d;
  d.name = "cpu";

  auto result = compute_cpu_usage({d}, {});  // prev has 1, curr has 0
  REQUIRE(result.empty());

  result = compute_cpu_usage({}, {d});  // prev has 0, curr has 1
  REQUIRE(result.empty());
}

// ── candidate Standard: fmt_pct / format helpers ──

// We test the struct fields directly rather than formatting
TEST_CASE("CpuData total and busy", "[cpu]") {
  CpuData d;
  d.user = 100; d.nice = 20; d.system = 80; d.idle = 400;
  d.iowait = 40; d.irq = 10; d.softirq = 5; d.steal = 5;

  REQUIRE(d.total() == 660);
  REQUIRE(d.busy() == 220);  // total - idle - iowait
}

// ── JSON output mode test ──

TEST_CASE("CpuData struct initializes to zero", "[cpu]") {
  CpuData d;
  REQUIRE(d.name.empty());
  REQUIRE(d.user == 0);
  REQUIRE(d.system == 0);
  REQUIRE(d.idle == 0);
}
