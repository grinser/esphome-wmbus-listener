// Host test for is_night_hour() and the meter input helpers in packages/settings.h.
#include <cstdio>
#include <string>
#include "../packages/settings.h"

static int failures = 0;
static void check(int hour, float start, float end, bool want) {
  bool got = is_night_hour(hour, start, end);
  if (got != want) {
    std::printf("FAIL is_night_hour(%d, %g, %g) = %d, want %d\n", hour, start, end, got, want);
    failures++;
  }
}

static void check_clean(const char *in, const char *want, size_t len, bool valid) {
  std::string got = clean_hex_input(in);
  if (got != want || is_hex_len(got, len) != valid) {
    std::printf("FAIL clean_hex_input(\"%s\") = \"%s\" (valid %d), want \"%s\" (valid %d)\n", in, got.c_str(),
                is_hex_len(got, len), want, valid);
    failures++;
  }
}

static void check_str(const char *what, const std::string &got, const char *want) {
  if (got != want) {
    std::printf("FAIL %s = \"%s\", want \"%s\"\n", what, got.c_str(), want);
    failures++;
  }
}

int main() {
  // wM-Bus header: manufacturer and device type
  check_str("wmbus_manufacturer(27 4C)", wmbus_manufacturer(0x27, 0x4C), "SAG");
  check_str("wmbus_manufacturer(2D 2C)", wmbus_manufacturer(0x2D, 0x2C), "KAM");
  check_str("wmbus_manufacturer(00 00)", wmbus_manufacturer(0x00, 0x00), "???");
  check_str("wmbus_medium(02)", wmbus_medium(0x02), "electricity");
  check_str("wmbus_medium(07)", wmbus_medium(0x07), "water");
  check_str("wmbus_medium(16)", wmbus_medium(0x16), "cold water");
  check_str("wmbus_medium(99)", wmbus_medium(0x99), "type 99");

  // Meter address and key as pasted
  check_clean("0a1b2c3d", "0A1B2C3D", 8, true);
  check_clean("0x0A1B2C3D", "0A1B2C3D", 8, true);
  check_clean(" 0A 1B 2C 3D ", "0A1B2C3D", 8, true);
  check_clean("0A1B2C3", "0A1B2C3", 8, false);
  check_clean("0A1B2C3G", "0A1B2C3G", 8, false);
  check_clean("", "", 8, false);
  check_clean("00112233-44556677-8899AABB-CCDDEEFF", "00112233445566778899AABBCCDDEEFF", 32, true);
  check_clean("00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff", "00112233445566778899AABBCCDDEEFF", 32, true);
  check_clean("0011 2233 4455 6677\n8899 AABB CCDD EEFF", "00112233445566778899AABBCCDDEEFF", 32, true);
  check_clean("0011223344556677", "0011223344556677", 32, false);

  // Wraps past midnight: 21 -> 7
  check(20, 21, 7, false);
  check(21, 21, 7, true);
  check(23, 21, 7, true);
  check(0, 21, 7, true);
  check(6, 21, 7, true);
  check(7, 21, 7, false);
  check(12, 21, 7, false);
  // Within one day: 1 -> 6
  check(0, 1, 6, false);
  check(1, 1, 6, true);
  check(5, 1, 6, true);
  check(6, 1, 6, false);
  // Equal: never night
  for (int h = 0; h < 24; h++) check(h, 7, 7, false);
  if (failures) return 1;
  std::printf("settings.h: all checks passed\n");
  return 0;
}
