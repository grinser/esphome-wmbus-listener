// Helpers for the settings that Home Assistant can change at runtime.
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>

// Night between start and end (full hours). start > end wraps past midnight
// (21 -> 7), start < end stays within one day (1 -> 6), start == end: never night.
inline bool is_night_hour(int hour, float start, float end) {
  int s = (int) start, e = (int) end;
  if (s == e) return false;
  if (s > e) return hour >= s || hour < e;
  return hour >= s && hour < e;
}

// Meter address or key as typed or pasted from a portal: drop spaces, tabs, line
// breaks, '-', ':' and a leading "0x", upper case. "" means the field was blank.
inline std::string clean_hex_input(const std::string &in) {
  std::string s;
  for (char c : in) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-' || c == ':') continue;
    s += (char) toupper((unsigned char) c);
  }
  if (s.size() > 2 && s[0] == '0' && s[1] == 'X') s.erase(0, 2);
  return s;
}

// Exactly len hex digits (after clean_hex_input).
inline bool is_hex_len(const std::string &s, size_t len) {
  if (s.size() != len) return false;
  for (char c : s)
    if (!isxdigit((unsigned char) c)) return false;
  return true;
}

// Manufacturer code from the cleartext wM-Bus header (bytes 2..3, little-endian):
// three letters, 5 bits each (EN 13757-3), e.g. 0x4C27 -> "SAG". "???" if not A-Z.
inline std::string wmbus_manufacturer(uint8_t lo, uint8_t hi) {
  uint16_t m = (uint16_t) (lo | (hi << 8));
  std::string s;
  for (int shift : {10, 5, 0}) {
    int c = ((m >> shift) & 0x1F) + 64;
    if (c < 'A' || c > 'Z') return "???";
    s += (char) c;
  }
  return s;
}

// Device type from the cleartext header (byte 9), the common ones by name.
inline std::string wmbus_medium(uint8_t t) {
  switch (t) {
    case 0x02: return "electricity";
    case 0x03: return "gas";
    case 0x04: case 0x0C: return "heat";
    case 0x06: return "warm water";
    case 0x07: return "water";
    case 0x08: return "heat cost";
    case 0x0A: case 0x0B: return "cooling";
    case 0x0D: return "heat/cooling";
    case 0x15: return "hot water";
    case 0x16: return "cold water";
    case 0x1A: return "smoke detector";
    case 0x1B: return "room sensor";
    case 0x32: case 0x33: return "repeater";
    case 0x36: case 0x37: return "radio converter";
    default: {
      char buf[10];
      snprintf(buf, sizeof(buf), "type %02X", t);
      return buf;
    }
  }
}
