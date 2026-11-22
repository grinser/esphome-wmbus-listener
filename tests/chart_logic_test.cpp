// Host tests for the load-chart lambdas in packages/load_chart.yaml (restore
// on time sync, energy and power on_value). extract.py copies the lambda bodies
// into lambdas.inc; id(x) maps to the globals below. Run tests/run.sh.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
using std::isnan;  // the firmware lambdas call it unqualified

struct Now {
  bool valid; int hour, minute, day_of_year; long timestamp;
  bool is_valid() const { return valid; }
};
static long g_utc = 0;          // simulated unix time (s)
static int g_off = 3600;        // UTC offset (s), CET; DST tests change it
static bool g_valid = true;
struct Clock {
  Now now() const {
    long l = g_utc + g_off;
    Now n{g_valid, (int)((l / 3600) % 24), (int)((l / 60) % 60), (int)((l / 86400) % 365) + 1, g_utc};
    return n;
  }
};

// --- globals mirroring packages/load_chart.yaml ---
Clock ha_time;
bool hist_restored = false;
float energy_today_base = NAN; int energy_today_day = 0; bool energy_today_valid = false;
float qh_kwh[96]; float qh_base = NAN; int qh_index = -1; int qh_qnum = 0;
float qh_hist[96]; int last_qnum = 0;
float pm_min[15]; float pm_sum = 0; int pm_count = 0; int pm_epoch_min = -1;
#define id(x) x

#include "lambdas.inc"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float energy = 1000.0f;  // kWh register
// Run telegrams every 5 s at constant power kw for `secs`, unless silent.
static void run(double kw, long secs, bool silent = false) {
  for (long t = 0; t < secs; t += 5) {
    g_utc += 5;
    energy += kw * 5.0 / 3600.0;
    if (!silent) { energy_fn(energy); power_fn((float)kw); }
  }
}
static void reboot() {  // RAM state lost, restore_value globals kept
  hist_restored = false;
  for (int i = 0; i < 96; i++) qh_kwh[i] = 0;
  qh_base = NAN; qh_index = -1; qh_qnum = 0;
  for (int i = 0; i < 15; i++) pm_min[i] = 0;
  pm_sum = 0; pm_count = 0; pm_epoch_min = -1;
  restore_fn();  // first time sync
}
static int slot_now() { auto n = ha_time.now(); return n.hour * 4 + n.minute / 15; }
static float sum(const float *a, int n) { float s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }

int main() {
  // Start at a local (CET) midnight in November; only relative times matter.
  g_utc = 1795129200L - 3600;
  g_utc -= (g_utc + g_off) % 86400;

  // 1) Normal: full day at 1 kW -> every finalized slot ~0.25 kWh.
  run(1.0, 86400);
  for (int i = 1; i < 96; i++) CHECK(fabsf(qh_kwh[i] - 0.25f) < 0.01f, "normal slot %d = %.3f", i, qh_kwh[i]);
  CHECK(fabsf(qh_hist[10] - 0.25f) < 0.01f, "hist mirrors finalized slots");

  // 2) Day 2: 2 kW until 10:00, then radio silent 10:05-10:50, then 2 kW again.
  run(2.0, 10 * 3600 + 300);
  run(2.0, 45 * 60, true);
  run(2.0, 3600);
  // Slots 40-42 share the energy of 10:00-10:50 evenly (2 kW * 50 min / 3) and
  // hold nothing from yesterday.
  for (int s = 40; s <= 42; s++) CHECK(fabsf(qh_kwh[s] - 2.0f * 50 / 60 / 3) < 0.01f, "gap slot %d = %.3f", s, qh_kwh[s]);
  CHECK(fabsf(qh_kwh[39] - 0.5f) < 0.01f, "slot before gap = %.3f", qh_kwh[39]);
  // Fine panel: 2 kW everywhere after an hour of telegrams.
  for (int r = 0; r < 15; r++) CHECK(fabsf(pm_min[r] - 2.0f) < 0.01f, "pm row %d = %.3f", r, pm_min[r]);

  // 3) Reboot with an outage of 1 h (reboot at ~11:50, back at ~12:50).
  int before = slot_now();
  g_utc += 3600;
  reboot();
  int s_now = slot_now();
  for (int k = 0; k <= 4; k++) {
    int s = ((s_now - k) % 96 + 96) % 96;
    CHECK(qh_kwh[s] == 0.0f && qh_hist[s] == 0.0f, "outage slot %d not blanked (kwh %.3f hist %.3f)", s, qh_kwh[s], qh_hist[s]);
  }
  CHECK(fabsf(qh_kwh[(s_now + 10) % 96] - 0.25f) < 0.01f, "yesterday ahead of write head kept");
  CHECK(fabsf(qh_kwh[before - 10] - 0.5f) < 0.01f, "today before the outage restored = %.3f", qh_kwh[before - 10]);
  // 4) Second, short reboot 1 min later: blanked slots must stay blank (review blocker).
  run(2.0, 60);
  g_utc += 30; reboot();
  for (int k = 1; k <= 4; k++) {
    int s = ((s_now - k) % 96 + 96) % 96;
    CHECK(qh_kwh[s] == 0.0f, "second reboot brought slot %d back: %.3f", s, qh_kwh[s]);
  }

  // 5) Fine panel gap of 5 minutes: those rows go to 0, others keep 2 kW.
  run(2.0, 20 * 60);
  run(2.0, 5 * 60, true);
  run(3.0, 30);
  auto n = ha_time.now();
  int m = n.minute % 15;
  // Silence 13:11:30-13:16:30: minutes 12-15 had no telegram, 16 and 17 did.
  for (int k = 2; k <= 5; k++) CHECK(pm_min[((m - k) % 15 + 15) % 15] == 0.0f, "silent minute row %d not blank", ((m - k) % 15 + 15) % 15);
  CHECK(fabsf(pm_min[((m - 1) % 15 + 15) % 15] - 3.0f) < 0.01f, "minute before current = %.3f", pm_min[((m - 1) % 15 + 15) % 15]);
  CHECK(fabsf(pm_min[m] - 3.0f) < 0.01f, "current minute avg = %.3f", pm_min[m]);
  CHECK(fabsf(pm_min[(m + 3) % 15] - 2.0f) < 0.01f, "older row kept = %.3f", pm_min[(m + 3) % 15]);

  // 6) Exactly 60 silent minutes must not merge the old minute's sum.
  run(1.0, 30);
  run(1.0, 3600, true);
  run(4.0, 5);
  n = ha_time.now();
  CHECK(fabsf(pm_min[n.minute % 15] - 4.0f) < 0.01f, "60-min gap merged sums: %.3f", pm_min[n.minute % 15]);

  // 7) Silent for more than a day: everything blank except the live slot.
  run(1.0, 26 * 3600, true);
  run(1.0, 10);
  CHECK(sum(qh_kwh, 96) < 0.01f, "stale day survived a 26 h silence: %.3f", sum(qh_kwh, 96));
  CHECK(sum(qh_hist, 96) < 0.01f, "stale hist survived a 26 h silence");

  // 8) DST spring forward at 02:00 local -> 03:00: slot 7 finalized, 8-11 blank.
  run(1.0, 86400 * 2);
  while (slot_now() != 7) run(1.0, 5);
  run(1.0, 15 * 60 - 5);   // to 01:59:55; the clock jumps at 02:00
  g_off += 3600;           // clocks jump forward
  run(1.0, 60);
  CHECK(fabsf(qh_kwh[7] - 0.25f) < 0.02f, "DST: slot 7 = %.3f", qh_kwh[7]);
  for (int s = 8; s <= 11; s++) CHECK(qh_kwh[s] == 0.0f, "DST: skipped slot %d = %.3f", s, qh_kwh[s]);

  // 9) DST fall back 03:00 -> 02:00: no crash, no mass blanking.
  run(1.0, 86400 * 2);
  while (slot_now() != 11) run(1.0, 5);
  run(1.0, 14 * 60);
  g_off -= 3600;
  run(1.0, 30 * 60);
  CHECK(sum(qh_kwh, 96) > 20.0f, "DST back: chart wiped (%.2f)", sum(qh_kwh, 96));

  // 10) Restore after a fresh flash (last_qnum == 0) is blank and safe.
  last_qnum = 0; for (int i = 0; i < 96; i++) qh_hist[i] = 5.0f;
  reboot();
  CHECK(sum(qh_kwh, 96) == 0.0f && sum(qh_hist, 96) == 0.0f, "fresh flash restore not blank");

  printf(fails ? "%d FAILED\n" : "all checks passed\n", fails);
  return fails ? 1 : 0;
}
