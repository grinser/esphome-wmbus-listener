// Helpers for the "power off" deep-sleep mode (see deep_sleep + script in wmbus.yaml).
//
// The one thing plain deep sleep gets wrong on this board is the radio: the SX1262
// would otherwise leave reset and sit in RX (~5 mA) for the whole sleep. So before
// sleeping we drive its NRESET (GPIO12) low and latch it; on boot the latch must be
// released again or the radio component can't re-initialise the pin.
//
// Only GPIO12 is held. We deliberately do not hold GPIO36 (Vext) or GPIO37
// (ADC_Ctrl): on the ESP32-S3 those sit in the flash power domain, and a deep-sleep
// pad hold there survives a plain reset (only a power-on reset clears it), which can
// stall the bootloader's flash read after a PRG/RST wake. Their power cost when
// unheld is negligible — the OLED is already put to sleep (~uA) before we sleep, and
// the VBAT divider is ~9 uA — so dropping those holds is the right trade.
//
// GPIO12 is an RTC-capable pad, so we use the RTC IO hold (which is what actually
// persists through deep sleep on the S3); a plain digital gpio_hold does not reliably
// cover RTC pads.
#pragma once
#include <driver/gpio.h>
#include <driver/rtc_io.h>

inline void wmbus_hold_pins_for_off() {
  rtc_gpio_init(GPIO_NUM_12);
  rtc_gpio_set_direction(GPIO_NUM_12, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(GPIO_NUM_12, 0);   // radio held in reset = sub-uA instead of RX
  rtc_gpio_hold_en(GPIO_NUM_12);
}

inline void wmbus_release_held_pins() {
  rtc_gpio_hold_dis(GPIO_NUM_12);
  rtc_gpio_deinit(GPIO_NUM_12);          // hand GPIO12 back to the digital matrix / radio
}
