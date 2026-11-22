# Hardware and how it works

## How it works

The meter sends an encrypted wM-Bus message about every 5 seconds. The SX1262 radio on the board receives all messages
on 868.95 MHz. The board keeps the messages from your meter's radio address and decrypts them with your AES key. The
`amiplus` driver from wmbusmeters reads the values. ESPHome sends them to Home Assistant over its native API.

The decoding runs on the board. The configuration uses
[SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), pinned to a reviewed commit. It
contains no custom decoder code.

## Heltec V3 pins

| Function                             | GPIO |
| ------------------------------------ | ---- |
| SPI SCK                              | 9    |
| SPI MOSI                             | 10   |
| SPI MISO                             | 11   |
| Radio CS (NSS)                       | 8    |
| Radio DIO1 (IRQ)                     | 14   |
| Radio RESET                          | 12   |
| Radio BUSY                           | 13   |
| OLED SDA                             | 17   |
| OLED SCL                             | 18   |
| OLED RST                             | 21   |
| Vext (OLED power, active low)        | 36   |
| LED                                  | 35   |
| VBAT sense (ADC1_CH0)                | 1    |
| ADC_Ctrl (VBAT divider, active high) | 37   |
| PRG button                           | 0    |

Source: [ESPHome devices: Heltec WiFi LoRa 32 V3](https://devices.esphome.io/devices/heltec-wifi-lora-32-v3/).
