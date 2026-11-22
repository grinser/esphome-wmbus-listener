// Load-profile chart for page_main (see wmbus.yaml), split across two regions:
//
//   bottom band  (y53-63)  24 h load profile — 96 slot columns spanning the
//                          full width, 2 px gap every 3 h. Bars grow UP from
//                          y61; the bottom lane (y63) holds the write-head tick.
//   right column (x113-126, y17-46)  15-min fine chart, rotated 90°: one
//                          horizontal bar per minute-of-quarter, growing left
//                          from the axis (x124), rows stacked top->bottom
//                          (time flows down), vertically centred between the
//                          status bar and the 24 h band.
//
// Templated on the display type so it pulls in no esphome headers — `it`'s
// methods resolve at the call site.
//
// Shared behaviour:
//  - Fixed sqrt scale (Chart max from HA, fallback 2.5 kW), so a bar length
//    means the same kW in both regions. The 24 h panel scales slot energy vs
//    maxkw/4 (a slot held at max power); the fine panel scales power vs maxkw.
//  - ECG sweep: the current slot/row is the write head. Behind it = today
//    (solid); ahead still holds yesterday / the previous quarter (ghost 50%
//    stipple). The sweep alone splits the two — no midnight wipe, no 2nd array.
//  - Write head = a 1 px tick just past the bar's origin, 1 px-gapped: the
//    24 h tick sits in the bottom lane (y63, gap at y62); the fine tick sits at
//    the right edge (x126, gap at x125). Same marker in both, no caret.
//  - Only the horizontal burn-in shift (ox) is applied; both regions stay clear
//    of y63+ / the screen edges so nothing spills.
#pragma once
#include <math.h>

template <typename DisplayT>
void draw_load_chart(DisplayT &it, int ox, const float *qh, const float *pm,
                     float maxkw, int cur, int m) {
  if (isnan(maxkw) || maxkw <= 0.0f) maxkw = 2.5f;

  // value -> bar length (1..cap px), fixed sqrt scale. 0 use -> 0 px; any
  // nonzero use shows at least 1 px; over-scale clips to the cap.
  auto len = [](float v, float vmax, int cap) -> int {
    if (!(v > 0.0f) || !(vmax > 0.0f)) return 0;   // also catches NaN
    float f = v / vmax; if (f > 1.0f) f = 1.0f;
    int L = (int)(sqrtf(f) * cap + 0.5f);
    return L < 1 ? 1 : L;
  };
  auto q3 = [](int i) {   // 2 px inserted after each 3 h boundary (slots 12..84)
    return 2 * ((i>=12)+(i>=24)+(i>=36)+(i>=48)+(i>=60)+(i>=72)+(i>=84));
  };

  // ---- 24 h panel: full width, bars grow up from y61, tick lane at y63 ----
  const float vmax24 = maxkw / 4.0f;               // kWh in a full slot
  const int BASE = 61;
  for (int i = 0; i < 96; i++) {
    int x = 1 + i + q3(i) + ox;
    int L = len(qh[i], vmax24, 9);
    bool ghost = i > cur;
    for (int k = 0; k < L; k++) {
      int y = BASE - k;
      if (!ghost || ((x + y) & 1) == 0) it.draw_pixel_at(x, y);
    }
  }
  it.draw_pixel_at(1 + cur + q3(cur) + ox, 63);    // write-head tick (gap at y62)

  // ---- 15-min fine panel: rotated, right column, grows left from x124 ----
  const int AXIS = 124, TOP = 17, PITCH = 2, MAXL = 12;
  for (int r = 0; r < 15; r++) {
    int L = len(pm[r], maxkw, MAXL);
    int y = TOP + r * PITCH;
    bool ghost = r > m;
    for (int dx = 0; dx < L; dx++) {
      int x = AXIS - dx + ox;
      for (int yy = y; yy <= y + 1; yy++)
        if (!ghost || ((x + yy) & 1) == 0) it.draw_pixel_at(x, yy);
    }
  }
  { int y = TOP + m * PITCH;                        // write-head tick (gap at x125)
    it.draw_pixel_at(126 + ox, y); it.draw_pixel_at(126 + ox, y + 1); }
}
