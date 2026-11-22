"""Pixel-exact render of the main display page (SSD1306 128x64) for the docs.

Ports page_main (packages/display.yaml) and draw_load_chart (packages/loadchart.h)
line by line, with ESPHome's own draw primitives. A hand port: change it together
with those two files (and rx_bars in packages/status.h). The setup screen that
replaces page_main before the meter is OK is not ported. Glyphs are not re-rasterised:
they are parsed from the main.cpp that `esphome compile wmbus.yaml` generates, so
every letter is the one the board draws. The Wi-Fi fan comes from packages/status.h.
Needs Pillow.

Usage (after a compile):
  python3 tools/render_display.py .esphome/build/wmbus/src/main.cpp \
      tools/display-state.json docs/display.png
Writes the 8x image to the given path and the 1:1 image next to it (*-1x.png is
not committed; it is handy for comparing with a photo).
"""
import json
import math
import os
import re
import sys

from PIL import Image

W, H = 128, 64
HERE = os.path.dirname(os.path.abspath(__file__))


def load_fonts(main_cpp):
    src = open(main_cpp).read()
    data = {m.group(1): [int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2))]
            for m in re.finditer(r"uint8_t (uint8_t_id(?:_\d+)?)\[\] PROGMEM = \{([^}]*)\}", src)}
    fonts = {}
    for m in re.finditer(r"new\((font_\w+)\) font::Font\((font_glyph_id(?:_\d+)?), \d+, (\d+), (\d+)", src):
        name, gid = m.group(1), m.group(2)
        table = re.search(re.escape(gid) + r"\[\](?: PROGMEM)? = \{(.*?)\};", src, re.S).group(1)
        glyphs = {}
        for g in re.finditer(r"\{(\d+), \((uint8_t_id(?:_\d+)?) \+ (\d+)\), (-?\d+), (-?\d+), (-?\d+), (\d+), (\d+)\}", table):
            cp, arr, off, adv, ox, oy, w, h = g.groups()
            glyphs[int(cp)] = dict(data=data[arr], off=int(off), adv=int(adv), ox=int(ox),
                                   oy=int(oy), w=int(w), h=int(h))
        fonts[name] = glyphs
    return fonts


def load_fan(status_h):
    """WIFI_FAN_ROWS from status.h: 4 states x 12 rows of '.'/'#'."""
    src = open(status_h).read()
    body = re.search(r"WIFI_FAN_ROWS\[4\]\[WIFI_FAN_H\] = \{(.*?)\n\};", src, re.S).group(1)
    rows = re.findall(r'"([.#]+)"', body)
    return [rows[i * 12:(i + 1) * 12] for i in range(4)]


class Disp:
    """The subset of esphome::display::Display that page_main uses."""

    def __init__(self, fonts):
        self.fonts = fonts
        self.px = [[0] * W for _ in range(H)]

    def pixel(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = 1

    def hline(self, x, y, w):
        for i in range(x, x + w):
            self.pixel(i, y)

    def vline(self, x, y, h):
        for i in range(y, y + h):
            self.pixel(x, i)

    def rectangle(self, x, y, w, h):
        self.hline(x, y, w); self.hline(x, y + h - 1, w)
        self.vline(x, y, h); self.vline(x + w - 1, y, h)

    def filled_rectangle(self, x, y, w, h):
        for i in range(y, y + h):
            self.hline(x, i, w)

    def line(self, x1, y1, x2, y2):
        dx, sx = abs(x2 - x1), (1 if x1 < x2 else -1)
        dy, sy = -abs(y2 - y1), (1 if y1 < y2 else -1)
        err = dx + dy
        while True:
            self.pixel(x1, y1)
            if x1 == x2 and y1 == y2:
                break
            e2 = 2 * err
            if e2 >= dy: err += dy; x1 += sx
            if e2 <= dx: err += dx; y1 += sy

    def _circle(self, cx, cy, r, fill):
        dx, dy, err = -r, 0, 2 - 2 * r
        while True:
            for (a, b) in ((cx - dx, cy + dy), (cx + dx, cy + dy), (cx + dx, cy - dy), (cx - dx, cy - dy)):
                self.pixel(a, b)
            if fill:
                self.hline(cx + dx, cy + dy, 2 * (-dx) + 1)
                self.hline(cx + dx, cy - dy, 2 * (-dx) + 1)
            e2 = err
            if e2 < dy:
                dy += 1; err += dy * 2 + 1
                if -dx == dy and e2 <= dx: e2 = 0
            if e2 > dx:
                dx += 1; err += dx * 2 + 1
            if dx > 0:
                break

    def circle(self, cx, cy, r):
        self._circle(cx, cy, r, False)

    def filled_circle(self, cx, cy, r):
        self._circle(cx, cy, r, True)

    def measure(self, font, s):
        x, min_x, has = 0, 0, False
        for ch in s:
            g = self.fonts[font][ord(ch)]
            min_x = g["ox"] if not has else min(min_x, x + g["ox"])
            x += g["adv"]; has = True
        return x - min_x, min_x

    def print(self, x, y, font, align, s):
        w, xo = self.measure(font, s)
        if align == "RIGHT": x1 = x - w - xo
        elif align == "CENTER": x1 = x - (w + xo) // 2
        else: x1 = x
        for ch in s:
            g = self.fonts[font][ord(ch)]
            data, i, mask, byte = g["data"], g["off"], 0, 0
            for gy in range(y + g["oy"], y + g["oy"] + g["h"]):
                for gx in range(x1 + g["ox"], x1 + g["ox"] + g["w"]):
                    if mask == 0:
                        byte = data[i]; i += 1; mask = 0x80
                    if byte & mask:
                        self.pixel(gx, gy)
                    mask >>= 1
            x1 += g["adv"]


def draw_load_chart(d, ox, qh, pm, maxkw, cur, m):
    if maxkw is None or maxkw <= 0:
        maxkw = 2.5

    def length(v, vmax, cap):
        if v <= 0 or vmax <= 0:
            return 0
        return max(int(math.sqrt(min(v / vmax, 1.0)) * cap + 0.5), 1)

    def q3(i):
        return 2 * sum(i >= b for b in (12, 24, 36, 48, 60, 72, 84))

    for i in range(96):
        x = 1 + i + q3(i) + ox
        for k in range(length(qh[i], maxkw / 4.0, 9)):
            y = 61 - k
            if i <= cur or ((x + y) & 1) == 0:
                d.pixel(x, y)
    d.pixel(1 + cur + q3(cur) + ox, 63)
    AXIS, TOP, PITCH, MAXL = 124, 17, 2, 12
    for r in range(15):
        y = TOP + r * PITCH
        for dx in range(length(pm[r], maxkw, MAXL)):
            x = AXIS - dx + ox
            for yy in (y, y + 1):
                if r <= m or ((x + yy) & 1) == 0:
                    d.pixel(x, yy)
    y = TOP + m * PITCH
    d.pixel(126 + ox, y); d.pixel(126 + ox, y + 1)


def rx_bars(dbm):  # rx_bars in packages/status.h
    if dbm is None or dbm != dbm:
        return 0
    return 4 if dbm > -70 else 3 if dbm > -80 else 2 if dbm > -90 else 1 if dbm > -100 else 0


def page_main(d, fan, st):
    ox, oy = st["ox"], st["oy"]
    # 868 MHz bars + telegram dot ("flash", "none", "foreign", "silent")
    bars = rx_bars(st["rx_dbm"]) if st["rx_dot"] in ("flash", "none") else 0
    for i in range(4):
        h = 3 + i * 2
        (d.filled_rectangle if i < bars else d.rectangle)(3 + i * 4 + ox, 12 - h + oy, 3, h)
    dx, dy = 3 + ox, 4 + oy
    if st["rx_dot"] == "flash":
        d.filled_circle(dx, dy, 2)
    elif st["rx_dot"] == "foreign":
        d.circle(dx, dy, 2)
    elif st["rx_dot"] == "silent":
        d.line(dx - 2, dy - 2, dx + 2, dy + 2); d.line(dx - 2, dy + 2, dx + 2, dy - 2)
    # Wi-Fi fan ("strong", "medium", "weak", "none")
    rows = fan[["strong", "medium", "weak", "none"].index(st["wifi"])]
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch == "#":
                d.pixel(22 + ox + c, oy + 1 + r)
    # Battery
    pct = st["battery_pct"]
    d.rectangle(107 + ox, 1 + oy, 18, 10)
    d.filled_rectangle(125 + ox, 4 + oy, 2, 4)
    fw = int(pct * 0.14 + 0.5)
    if fw > 0:
        d.filled_rectangle(109 + ox, 3 + oy, fw, 6)
    d.print(103 + ox, 0 + oy, "font_small", "RIGHT", "%.0f%%" % pct)
    d.print(60 + ox, 0 + oy, "font_small", "CENTER", st["clock"])

    def fit(s, big, sm, right):  # as fit() in display.yaml
        w, _ = d.measure(big, s)
        return big if 64 + w // 2 <= right else sm

    p = st["power_kw"]
    buf = ("%.0f W" % (p * 1000)) if p < 1 else ("%.2f kW" % p)
    d.print(64 + ox, 12 + oy, fit(buf, "font_big", "font_big_sm", 108), "CENTER", buf)
    if st.get("hint"):  # inverted hint instead of "kWh today", e.g. "HA offline"
        w, _ = d.measure("font_body", st["hint"])
        if 64 - w // 2 - 3 + w + 6 > 112:
            sys.exit("hint %r reaches the chart column" % st["hint"])
        d.filled_rectangle(64 - w // 2 - 3 + ox, 37 + oy, w + 6, 13)
        t = Disp(d.fonts)
        t.print(64 + ox, 36 + oy, "font_body", "CENTER", st["hint"])
        for y in range(H):
            for x in range(W):
                if t.px[y][x]:
                    d.px[y][x] = 0
    else:
        buf = "%.2f kWh today" % st["today_kwh"]
        d.print(64 + ox, 36 + oy, fit(buf, "font_body", "font_small", 111), "CENTER", buf)
    if st["clock"] == "--:--":
        return  # no valid time: the board draws no chart either
    hh, mm = map(int, st["clock"].split(":"))
    draw_load_chart(d, ox, st["qh_kwh"], st["pm_min"], st["chart_max_kw"], hh * 4 + mm // 15, mm % 15)


def main():
    main_cpp, state, out = sys.argv[1:4]
    d = Disp(load_fonts(main_cpp))
    page_main(d, load_fan(os.path.join(HERE, "..", "packages", "status.h")), json.load(open(state)))
    im = Image.new("RGB", (W, H))
    for y in range(H):
        for x in range(W):
            if d.px[y][x]:
                im.putpixel((x, y), (255, 255, 255))
    im.resize((W * 8, H * 8), Image.NEAREST).save(out)
    im.save(os.path.splitext(out)[0] + "-1x.png")


if __name__ == "__main__":
    main()
