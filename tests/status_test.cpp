// Host test for packages/status.h: status bar, Wi-Fi fan, meter state, hint line.
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../packages/status.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL line %d: %s\n", __LINE__, #cond);        \
      failures++;                                                \
    }                                                            \
  } while (0)

// Collects pixels like display::Display::draw_pixel_at.
struct FakeDisplay {
  bool px[64][128] = {};
  int outside = 0;
  void draw_pixel_at(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) { outside++; return; }
    px[y][x] = true;
  }
};

int main() {
  // 868 MHz bars: -70 / -80 / -90 / -100 dBm
  CHECK(rx_bars(-65) == 4);
  CHECK(rx_bars(-75) == 3);
  CHECK(rx_bars(-85) == 2);
  CHECK(rx_bars(-95) == 1);
  CHECK(rx_bars(-105) == 0);
  CHECK(rx_bars(-70) == 3);  // thresholds are exclusive
  CHECK(rx_bars(NAN) == 0);

  // Telegram dot: rx_dot(now, frames, last frame, ours, last ours)
  CHECK(rx_dot(5000, 0, 0, 0, 0) == RX_DOT_SILENT);
  CHECK(rx_dot(5000, 3, 4000, 0, 0) == RX_DOT_FOREIGN);
  CHECK(rx_dot(5000, 3, 4500, 1, 4500) == RX_DOT_FLASH);
  CHECK(rx_dot(5000, 3, 3000, 1, 3000) == RX_DOT_NONE);
  CHECK(rx_dot(70000, 3, 69000, 1, 5000) == RX_DOT_FOREIGN);  // ours silent 65 s, others heard
  CHECK(rx_dot(70000, 3, 5000, 1, 5000) == RX_DOT_SILENT);    // nothing at all for 65 s
  CHECK(rx_dot(200, 3, 0xFFFFFF00u, 1, 0xFFFFFF00u) == RX_DOT_FLASH);  // across the millis() wrap
  CHECK(rx_ours_recent(5000, 1, 3000));
  CHECK(!rx_ours_recent(70000, 1, 5000));
  CHECK(!rx_ours_recent(5000, 0, 0));

  // Wi-Fi fan state: -60 / -72 dBm, none without a connection
  CHECK(wifi_fan(true, -50) == FAN_STRONG);
  CHECK(wifi_fan(true, -65) == FAN_MEDIUM);
  CHECK(wifi_fan(true, -80) == FAN_WEAK);
  CHECK(wifi_fan(false, -50) == FAN_NONE);
  CHECK(wifi_fan(true, NAN) == FAN_WEAK);  // connected, no reading yet

  // Pixel tables: right size, only '.' and '#', mirror-symmetric.
  for (int s = 0; s < 4; s++) {
    for (int r = 0; r < WIFI_FAN_H; r++) {
      const char *row = WIFI_FAN_ROWS[s][r];
      CHECK((int) std::strlen(row) == WIFI_FAN_W);
      for (int c = 0; c < WIFI_FAN_W; c++) {
        CHECK(row[c] == '.' || row[c] == '#');
        CHECK(row[c] == row[WIFI_FAN_W - 1 - c]);
      }
    }
  }

  // draw_wifi_fan places the table at (x, y), stays inside the status bar
  // (y 0-11 at x 25) and ends before the clock.
  for (int s = 0; s < 4; s++) {
    FakeDisplay d;
    draw_wifi_fan(d, 25, 0, (WifiFan) s);
    CHECK(d.outside == 0);
    int lit = 0;
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 128; x++)
        if (d.px[y][x]) {
          lit++;
          CHECK(y <= 11 && x >= 25 && x <= 39);
          CHECK(WIFI_FAN_ROWS[s][y][x - 25] == '#');
        }
    CHECK(lit > 0);
  }

  // Meter state: meter_state(set up, now < METER_GRACE_MS, now, frames, ours, last ours, has values)
  CHECK(!meter_set_up("00000000"));
  CHECK(!meter_set_up("0A1B2C3D"));
  CHECK(meter_set_up("1234ABCD"));
  CHECK(meter_state(false, 5000 < METER_GRACE_MS, 5000, 9, 9, 4000, true) == METER_NOT_SET_UP);
  CHECK(meter_state(true, 5000 < METER_GRACE_MS, 5000, 0, 0, 0, false) == METER_WAITING);            // grace period
  CHECK(meter_state(true, 130000 < METER_GRACE_MS, 130000, 0, 0, 0, false) == METER_RADIO_SILENT);
  CHECK(meter_state(true, 130000 < METER_GRACE_MS, 130000, 7, 0, 0, false) == METER_FOREIGN_ONLY);
  CHECK(meter_state(true, 130000 < METER_GRACE_MS, 130000, 7, 1, 129000, false) == METER_WAITING);     // first telegram, decoding
  CHECK(meter_state(true, 130000 < METER_GRACE_MS, 130000, 7, 3, 129000, false) == METER_NO_VALUES);
  CHECK(meter_state(true, 130000 < METER_GRACE_MS, 130000, 7, 3, 129000, true) == METER_OK);
  CHECK(meter_state(true, 300000 < METER_GRACE_MS, 300000, 7, 3, 129000, true) == METER_LOST);
  CHECK(meter_state(true, false, 100, 7, 3, 0xFFFFFF00u, true) == METER_OK);  // across the millis() wrap
  CHECK(meter_state(true, false, 100, 0, 0, 0, false) == METER_RADIO_SILENT); // grace latched over
  CHECK(meter_setup_screen(METER_NOT_SET_UP) && meter_setup_screen(METER_RADIO_SILENT) &&
        meter_setup_screen(METER_FOREIGN_ONLY) && meter_setup_screen(METER_NO_VALUES));
  CHECK(!meter_setup_screen(METER_WAITING) && !meter_setup_screen(METER_LOST) &&
        !meter_setup_screen(METER_OK));
  CHECK(std::strcmp(meter_state_text(METER_OK), "OK") == 0);
  CHECK(std::strcmp(meter_state_text(METER_NOT_SET_UP), "Not set up") == 0);
  for (int st = METER_NOT_SET_UP; st <= METER_OK; st++)
    CHECK(std::strlen(meter_state_text((MeterState) st)) > 0);

  // Setup screen: latched until the first OK.
  CHECK(meter_screen_next(METER_SCREEN_NONE, METER_WAITING, false) == METER_SCREEN_NONE);
  CHECK(meter_screen_next(METER_SCREEN_NONE, METER_FOREIGN_ONLY, false) == METER_FOREIGN_ONLY);
  CHECK(meter_screen_next(METER_FOREIGN_ONLY, METER_WAITING, false) == METER_FOREIGN_ONLY);  // first wrong-key telegram
  CHECK(meter_screen_next(METER_FOREIGN_ONLY, METER_NO_VALUES, false) == METER_NO_VALUES);
  CHECK(meter_screen_next(METER_NO_VALUES, METER_LOST, false) == METER_NO_VALUES);           // long gap, still no key
  CHECK(meter_screen_next(METER_NO_VALUES, METER_OK, false) == METER_SCREEN_NONE);
  CHECK(meter_screen_next(METER_SCREEN_NONE, METER_RADIO_SILENT, true) == METER_SCREEN_NONE);  // after an OK

  std::map<std::string, int> nearby;
  std::string addr;
  int rssi = 0;
  int others = 0;
  CHECK(!strongest_nearby(nearby, "", addr, rssi, others) && others == 0);
  nearby["11111111"] = -85;
  nearby["22222222"] = -70;
  nearby["33333333"] = -90;
  CHECK(strongest_nearby(nearby, "", addr, rssi, others) && addr == "22222222" && rssi == -70 && others == 3);
  CHECK(strongest_nearby(nearby, "22222222", addr, rssi, others) && addr == "11111111" && others == 2);  // not ours
  std::map<std::string, int> only_ours = {{"22222222", -60}};
  CHECK(!strongest_nearby(only_ours, "22222222", addr, rssi, others) && others == 0);

  // Wi-Fi / HA setup state: link_state(creds, connected, down ms, provisioning, provisioned, closed)
  CHECK(link_state(false, false, 0, true, false, false) == LINK_NO_CREDS);
  CHECK(link_state(true, false, 5000, true, false, false) == LINK_OK);            // still connecting
  CHECK(link_state(true, false, 31000, true, false, false) == LINK_WIFI_NOT_FOUND);
  CHECK(link_state(true, true, 0, true, false, false) == LINK_HA_ADD);
  CHECK(link_state(true, true, 0, true, false, true) == LINK_HA_CLOSED);
  CHECK(link_state(true, true, 0, true, true, true) == LINK_OK);                  // added in HA
  CHECK(link_state(true, true, 0, false, true, false) == LINK_OK);                // build without provisioning
  CHECK(provisioning_minutes_left(0) == 30);
  CHECK(provisioning_minutes_left(60000) == 29);
  CHECK(provisioning_minutes_left(61000) == 29);  // rounded up
  CHECK(provisioning_minutes_left(29 * 60000 + 1) == 1);
  CHECK(provisioning_minutes_left(30 * 60000) == 0);
  {
    bool p = true, pd = false, c = true;
    provisioning_status(p, pd, c);  // host build: no provisioning
    CHECK(!p && pd && !c);
  }

  // Telegram rate. Helper: one minute of a meter that sends every 5 s (12 slots),
  // each slot received with probability `p` (fixed-seed LCG, reproducible).
  {
    uint32_t seed = 12345;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 16) % 1000; };
    auto minute = [&](TelegramRate &r, uint32_t &t, int p_per_mille) {
      for (int i = 0; i < 12; i++)
        if ((int) rnd() < p_per_mille) r.on_telegram(t + i * 5000 + 100);
      t += 60000;
      r.end_minute();
    };

    TelegramRate r;
    CHECK(std::isnan(r.per_minute()) && !r.few());
    for (int m = 0; m < 5; m++) r.end_minute();  // nothing of ours yet: not counted
    CHECK(r.filled == 0);
    uint32_t t = 300000;
    for (int m = 0; m < 12; m++) minute(r, t, 1000);  // all received
    CHECK(r.best == 12);
    CHECK(std::fabs(r.per_minute() - 12.0f) < 0.01f && !r.few());

    // Sustained loss: the hint must hold steadily, not come and go.
    for (int p : {450, 300, 100}) {
      TelegramRate q = r;
      int on = 0, total = 0;
      for (int m = 0; m < 200; m++) {
        minute(q, t, p);
        if (m >= 10) { total++; on += q.few(); }
      }
      CHECK(on * 100 >= total * 95);
      CHECK(q.best == 12);  // loss does not lower the expectation
    }
    // Light loss never triggers it.
    {
      TelegramRate q = r;
      int on = 0;
      for (int m = 0; m < 200; m++) { minute(q, t, 900); on += q.few(); }
      CHECK(on == 0);
    }
  }
  {
    TelegramRate r;  // a burst meter: 2 telegrams 1.5 s apart every 15 s = 8 per minute
    uint32_t t = 0;
    for (int m = 0; m < 30; m++) {
      for (int i = 0; i < 4; i++) { r.on_telegram(t + i * 15000); r.on_telegram(t + i * 15000 + 1500); }
      t += 60000;
      r.end_minute();
    }
    CHECK(r.best == 8 && !r.few());
  }
  {
    TelegramRate r;  // duplicates under 1 s are ignored
    uint32_t t = 0;
    for (int m = 0; m < 12; m++) {
      for (int i = 0; i < 12; i++) { r.on_telegram(t + i * 5000); r.on_telegram(t + i * 5000 + 50); }
      t += 60000;
      r.end_minute();
    }
    CHECK(r.best == 12 && !r.few());
    for (int m = 0; m < 6; m++) r.end_minute();  // 6 min silence
    t += 360000;
    r.on_telegram(t);                            // back: the window starts again
    CHECK(r.filled == 0 && !r.few());
    for (int m = 0; m < 10; m++) {
      r.end_minute();
      for (int i = 1; i < 12; i++) r.on_telegram(t + i * 5000);
      t += 60000;
    }
    CHECK(!r.few());  // no stale "few" from the silence
  }

  // Hints: priority order and suppression.
  CHECK(hint_mask(false, false, false, false) == 0);
  CHECK(hint_mask(true, true, false, false) == (1 << HINT_WIFI));             // no HA without Wi-Fi
  CHECK(hint_mask(false, true, false, false) == (1 << HINT_HA));
  CHECK(hint_mask(false, false, true, true) == (1 << HINT_NO_TELEGRAM));      // no "few" without any
  CHECK(hint_mask(false, false, false, true) == (1 << HINT_FEW));
  CHECK(hint_for_slot(0, 0) == HINT_COUNT);
  int two = (1 << HINT_HA) | (1 << HINT_FEW);
  CHECK(hint_for_slot(0, two) == HINT_COUNT);       // kWh
  CHECK(hint_for_slot(4000, two) == HINT_HA);
  CHECK(hint_for_slot(8000, two) == HINT_COUNT);    // kWh
  CHECK(hint_for_slot(12000, two) == HINT_FEW);
  CHECK(hint_for_slot(16000, two) == HINT_COUNT);   // and around again
  CHECK(hint_for_slot(4000, 1 << HINT_WIFI) == HINT_WIFI);
  CHECK(hint_for_slot(8000, 1 << HINT_WIFI) == HINT_COUNT);
  for (int h = 0; h < HINT_COUNT; h++) CHECK(std::strlen(hint_text((Hint) h)) > 0);

  DownTimer dt;
  CHECK(dt.down_for(1000) == 0);
  dt.update(false, 1000);
  dt.update(false, 5000);
  CHECK(dt.down_for(9000) == 8000);
  dt.update(true, 9000);
  CHECK(dt.down_for(9500) == 0);

  if (failures) return 1;
  std::printf("status.h: all checks passed\n");
  return 0;
}
