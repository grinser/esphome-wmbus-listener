// Status bar icons (the 868 MHz bars with the telegram dot, the Wi-Fi fan) and
// the meter state behind "Meter status" and the setup screen. Pure logic and
// pixel tables, so tests/status_test.cpp can check them on the host.
#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <string>

// --- 868 MHz -----------------------------------------------------------------
// Bars by the RSSI of our meter's last telegram. This board hears its meter at
// about -75 to -90 dBm, so 2 or 3 bars are normal.
inline int rx_bars(float dbm) {
  if (std::isnan(dbm)) return 0;
  return dbm > -70 ? 4 : dbm > -80 ? 3 : dbm > -90 ? 2 : dbm > -100 ? 1 : 0;
}

enum RxDot { RX_DOT_NONE, RX_DOT_FLASH, RX_DOT_FOREIGN, RX_DOT_SILENT };

constexpr uint32_t RX_HEARD_MS = 60000;  // a meter counts as heard this long

// FLASH: our telegram arrived less than 1 s ago (like the LED; with the 1 s
// display refresh it shows for 1-2 s). NONE: our meter was heard in the last
// 60 s, between telegrams. FOREIGN: frames in the last 60 s, but none of ours.
// SILENT: no frame at all in the last 60 s (or none since the start).
// Times are millis(); unsigned subtraction survives the 49-day wrap.
inline RxDot rx_dot(uint32_t now, int frame_count, uint32_t last_frame_ms, int our_count,
                    uint32_t last_our_ms) {
  if (our_count > 0) {
    uint32_t age = now - last_our_ms;
    if (age < 1000) return RX_DOT_FLASH;
    if (age < RX_HEARD_MS) return RX_DOT_NONE;
  }
  return frame_count > 0 && now - last_frame_ms < RX_HEARD_MS ? RX_DOT_FOREIGN : RX_DOT_SILENT;
}

// True while our meter counts as heard: then the bars show its RSSI.
inline bool rx_ours_recent(uint32_t now, int our_count, uint32_t last_our_ms) {
  return our_count > 0 && now - last_our_ms < RX_HEARD_MS;
}

// --- Wi-Fi fan -----------------------------------------------------------------
// iOS-style fan: a wedge with a rounded top and up to 2 arcs. An unlit arc is a
// dotted line; without a connection the whole fan is dotted. Agreed in #5.
enum WifiFan { FAN_STRONG = 0, FAN_MEDIUM = 1, FAN_WEAK = 2, FAN_NONE = 3 };

// Connected without a reading yet (the signal sensor publishes every 10 s) counts
// as weak, not as "no connection".
inline WifiFan wifi_fan(bool connected, float dbm) {
  if (!connected) return FAN_NONE;
  if (std::isnan(dbm)) return FAN_WEAK;
  return dbm > -60 ? FAN_STRONG : dbm > -72 ? FAN_MEDIUM : FAN_WEAK;
}

constexpr int WIFI_FAN_W = 15, WIFI_FAN_H = 12;
static const char *const WIFI_FAN_ROWS[4][WIFI_FAN_H] = {
  {  // strong
    "...............",
    "....#######....",
    "..###########..",
    ".###.......###.",
    "##...#####...##",
    "...#########...",
    "..##.......##..",
    "......###......",
    ".....#####.....",
    "......###......",
    ".......#.......",
    "...............",
  },
  {  // medium
    "...............",
    "....#.#.#.#....",
    "..#.........#..",
    "...............",
    "#....#####....#",
    "...#########...",
    "..##.......##..",
    "......###......",
    ".....#####.....",
    "......###......",
    ".......#.......",
    "...............",
  },
  {  // weak
    "...............",
    "....#.#.#.#....",
    "..#.........#..",
    "...............",
    "#....#.#.#....#",
    "...#.......#...",
    "...............",
    "......###......",
    ".....#####.....",
    "......###......",
    ".......#.......",
    "...............",
  },
  {  // none
    "...............",
    "....#.#.#.#....",
    "..#.........#..",
    "...............",
    "#.............#",
    ".....#.#.#.....",
    "...#.......#...",
    "...............",
    ".......#.......",
    "......#.#......",
    ".......#.......",
    "...............",
  },
};

// Draws the fan with its top-left corner at (x, y). Works with any display
// class that has draw_pixel_at(x, y), such as ESPHome's display::Display.
template<typename D> void draw_wifi_fan(D &it, int x, int y, WifiFan state) {
  for (int r = 0; r < WIFI_FAN_H; r++)
    for (int c = 0; c < WIFI_FAN_W; c++)
      if (WIFI_FAN_ROWS[state][r][c] == '#') it.draw_pixel_at(x + c, y + r);
}

// --- Meter state -----------------------------------------------------------------
// One state for the "Meter status" sensor and the display, so the two never
// disagree. Until the first OK after a start, the display shows a setup screen
// for NOT_SET_UP, RADIO_SILENT, FOREIGN_ONLY and NO_VALUES.
enum MeterState {
  METER_NOT_SET_UP,     // still the example address
  METER_WAITING,        // first 2 min after the start, nothing of ours yet
  METER_RADIO_SILENT,   // no frame at all since the start (checked after 2 min)
  METER_FOREIGN_ONLY,   // frames since the start, none from our meter (after 2 min)
  METER_LOST,           // ours was heard, but not for 2 min
  METER_NO_VALUES,      // our telegrams arrive, nothing decodes: the key
  METER_OK,
};

constexpr uint32_t METER_GRACE_MS = 120000;  // after the start, and for a lost meter
constexpr int METER_KEY_FRAMES = 3;          // ours heard this often without a value

inline bool meter_set_up(const std::string &addr) {
  // 00000000 = web installer, 0A1B2C3D = secrets.yaml.example
  return addr != "00000000" && addr != "0A1B2C3D";
}

// in_grace: the first 2 min after the start (latched by the caller, so the
// 49-day millis() wrap does not bring it back).
inline MeterState meter_state(bool set_up, bool in_grace, uint32_t now, int frame_count,
                              int our_count, uint32_t last_our_ms, bool has_values) {
  if (!set_up) return METER_NOT_SET_UP;
  if (our_count == 0) {
    if (in_grace) return METER_WAITING;
    return frame_count == 0 ? METER_RADIO_SILENT : METER_FOREIGN_ONLY;
  }
  if (now - last_our_ms > METER_GRACE_MS) return METER_LOST;
  if (!has_values) return our_count >= METER_KEY_FRAMES ? METER_NO_VALUES : METER_WAITING;
  return METER_OK;
}

// Text for the "Meter status" sensor. The older texts stay as they were, the docs
// quote them.
inline const char *meter_state_text(MeterState s) {
  switch (s) {
    case METER_NOT_SET_UP: return "Not set up";
    case METER_WAITING: return "Waiting for the meter";
    case METER_RADIO_SILENT: return "No radio frames at all";
    case METER_FOREIGN_ONLY: return "Waiting for the meter: only others heard";
    case METER_LOST: return "No telegram for 2 min";
    case METER_NO_VALUES: return "Telegrams, but no values: check the key";
    case METER_OK: return "OK";
  }
  return "";
}

inline bool meter_setup_screen(MeterState s) {
  return s == METER_NOT_SET_UP || s == METER_RADIO_SILENT || s == METER_FOREIGN_ONLY ||
         s == METER_NO_VALUES;
}

// Which setup screen the display shows: -1 for none. Until the first OK, a
// setup state sets the screen and WAITING or LOST keep the last one, so the
// first telegrams with a wrong key, or a gap of more than 2 min between them,
// do not flip back to the main page. OK ends it for good (ok_seen).
constexpr int METER_SCREEN_NONE = -1;
inline int meter_screen_next(int prev, MeterState s, bool ok_seen) {
  if (ok_seen || s == METER_OK) return METER_SCREEN_NONE;
  return meter_setup_screen(s) ? (int) s : prev;
}

// Strongest entry of "Meters nearby" (address -> RSSI) other than `skip` (our own
// address), and how many others there are. False if there is none.
inline bool strongest_nearby(const std::map<std::string, int> &nearby, const std::string &skip,
                             std::string &addr, int &rssi, int &others) {
  others = 0;
  bool any = false;
  for (auto &e : nearby) {
    if (e.first == skip) continue;
    others++;
    if (!any || e.second > rssi) {
      addr = e.first;
      rssi = e.second;
      any = true;
    }
  }
  return any;
}

// --- Wi-Fi and Home Assistant before the first OK ---------------------------------
// Checked before the meter: without Wi-Fi or Home Assistant the meter cannot be
// set up anyway. Not latched: these follow the live state.
enum LinkState {
  LINK_OK,
  LINK_NO_CREDS,        // no Wi-Fi stored yet (web installer, before Improv)
  LINK_WIFI_NOT_FOUND,  // Wi-Fi stored, not connected for LINK_WIFI_GRACE_MS
  LINK_HA_ADD,          // factory firmware: waiting to be added in Home Assistant
  LINK_HA_CLOSED,       // the provisioning window closed before that
};

constexpr uint32_t LINK_WIFI_GRACE_MS = 30000;  // a normal connect takes a few seconds
// Matches `provisioning: timeout: 30min` in wmbus.factory.yaml (the manager does
// not expose its timeout). Only used for the minutes left on the screen.
constexpr int PROVISIONING_WINDOW_MIN = 30;

inline LinkState link_state(bool has_creds, bool wifi_connected, uint32_t wifi_down_ms,
                            bool provisioning, bool provisioned, bool window_closed) {
  if (!has_creds) return LINK_NO_CREDS;
  if (!wifi_connected) return wifi_down_ms > LINK_WIFI_GRACE_MS ? LINK_WIFI_NOT_FOUND : LINK_OK;
  if (provisioning && !provisioned) return window_closed ? LINK_HA_CLOSED : LINK_HA_ADD;
  return LINK_OK;
}

// Minutes left in the provisioning window, rounded up, 0 when over.
inline int provisioning_minutes_left(uint32_t now) {
  uint32_t total = (uint32_t) PROVISIONING_WINDOW_MIN * 60000;
  return now >= total ? 0 : (int) ((total - now + 59999) / 60000);
}

// The provisioning manager exists only in builds with `provisioning:` (the
// factory firmware); elsewhere this reports "no provisioning".
inline void provisioning_status(bool &provisioning, bool &provisioned, bool &closed) {
#ifdef USE_PROVISIONING
  auto *pm = esphome::provisioning::global_provisioning_manager;
  provisioning = pm != nullptr;
  provisioned = pm == nullptr || pm->is_provisioned();
  closed = pm != nullptr && pm->closed();
#else
  provisioning = false;
  provisioned = true;
  closed = false;
#endif
}

// --- Telegram rate -----------------------------------------------------------------
// Our telegrams per minute over the last 10 minutes. The expected rate is the best
// full minute seen since the start: a minute with nothing lost. Counting per minute
// works for meters that send at a fixed interval and for meters that send bursts,
// and loss cannot drag the expectation down. Minutes count from our first
// telegram, and a silence of METER_SILENT_MS starts the window again, so the empty
// minutes of a silence do not read as "few" afterwards (the "No telegram" hint
// covers the silence itself).
constexpr uint32_t METER_SILENT_MS = 300000;

struct TelegramRate {
  static constexpr int MINUTES = 10;
  static constexpr uint32_t MIN_GAP_MS = 1000;  // shorter: a duplicate frame
  uint16_t per_min[MINUTES] = {};  // completed minutes, ring buffer
  int head = 0, filled = 0;
  uint16_t cur = 0;                // the running minute
  uint16_t best = 0;               // best completed full minute since the start
  bool started_mid_minute = false; // the first minute after a (re)start is partial
  bool few_on = false;
  uint32_t last_ms = 0;
  bool have_last = false;

  void on_telegram(uint32_t now) {
    if (have_last) {
      uint32_t gap = now - last_ms;
      if (gap < MIN_GAP_MS) return;
      if (gap > METER_SILENT_MS) restart();
    } else {
      started_mid_minute = true;
    }
    if (cur < 0xFFFF) cur++;
    last_ms = now;
    have_last = true;
  }

  void end_minute() {
    if (!have_last) return;  // nothing of ours yet
    per_min[head] = cur;
    head = (head + 1) % MINUTES;
    if (filled < MINUTES) filled++;
    if (!started_mid_minute && cur > best) best = cur;
    started_mid_minute = false;
    cur = 0;
    // Hysteresis: on below 50 % of the expected count, off again above 60 %.
    if (filled < MINUTES || best == 0) {
      few_on = false;
    } else {
      int n = count_last(MINUTES), expected = MINUTES * best;
      if (few_on) few_on = n * 10 <= expected * 6;
      else few_on = n * 2 < expected;
    }
  }

  void restart() {
    filled = 0;
    cur = 0;
    few_on = false;
    started_mid_minute = true;
  }

  // Telegrams in the last n completed minutes (n <= filled).
  int count_last(int n) const {
    int sum = 0;
    for (int i = 1; i <= n; i++) sum += per_min[(head - i + MINUTES) % MINUTES];
    return sum;
  }

  // Average per minute over the completed minutes (up to 10). NaN before the first.
  float per_minute() const { return filled == 0 ? NAN : (float) count_last(filled) / filled; }

  bool few() const { return few_on; }
};

// ESPHome declares globals before it includes this header, so these live here.
inline TelegramRate &telegram_rate() {
  static TelegramRate r;
  return r;
}

// --- Hint line -----------------------------------------------------------------------
// While something is wrong in normal operation, the line under the power value
// alternates every 4 s between "kWh today" and a hint. Several hints take turns.
enum Hint { HINT_WIFI = 0, HINT_HA, HINT_NO_TELEGRAM, HINT_FEW, HINT_COUNT };
constexpr uint32_t HINT_DOWN_MS = 120000;       // Wi-Fi or HA down this long
constexpr uint32_t HINT_SILENT_MS = METER_SILENT_MS;  // our meter quiet this long
// ("Meter status" says "No telegram for 2 min" earlier: HA gets the detail,
// the display waits longer before it nags.)
constexpr uint32_t HINT_SLOT_MS = 4000;

inline const char *hint_text(Hint h) {
  switch (h) {
    case HINT_WIFI: return "Wi-Fi offline";
    case HINT_HA: return "HA offline";
    case HINT_NO_TELEGRAM: return "No telegram";
    case HINT_FEW: return "Few telegrams";
    default: return "";
  }
}

// Bit mask of the hints to show, most important first. A hint that follows from
// another one is left out: no HA without Wi-Fi, no "few" without any.
inline int hint_mask(bool wifi_down, bool ha_down, bool no_telegram, bool few) {
  int m = 0;
  if (wifi_down) m |= 1 << HINT_WIFI;
  else if (ha_down) m |= 1 << HINT_HA;
  if (no_telegram) m |= 1 << HINT_NO_TELEGRAM;
  else if (few) m |= 1 << HINT_FEW;
  return m;
}

// The hint for this 4 s slot, or HINT_COUNT for "kWh today": kWh, hint 1, kWh,
// hint 2, and so on.
inline Hint hint_for_slot(uint32_t now, int mask) {
  Hint list[HINT_COUNT];
  int n = 0;
  for (int h = 0; h < HINT_COUNT; h++)
    if (mask & (1 << h)) list[n++] = (Hint) h;
  if (n == 0) return HINT_COUNT;
  uint32_t k = (now / HINT_SLOT_MS) % (2 * n);
  return k % 2 == 0 ? HINT_COUNT : list[k / 2];
}

// Tracks how long a link has been down: down_for() is 0 while it is up.
struct DownTimer {
  bool down = false;
  uint32_t since = 0;
  void update(bool up, uint32_t now) {
    if (up) down = false;
    else if (!down) { down = true; since = now; }
  }
  uint32_t down_for(uint32_t now) const { return down ? now - since : 0; }
};

inline DownTimer &wifi_down_timer() {
  static DownTimer t;
  return t;
}
inline DownTimer &ha_down_timer() {
  static DownTimer t;
  return t;
}
