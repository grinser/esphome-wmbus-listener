# Usage

## Settings

`wmbus.yaml` (command line) or `device-builder.yaml` (Device Builder) holds everything you are likely to change, in its
`substitutions:` block: the device name, the log level, Wi-Fi power saving, display and LED brightness, the night hours
and the default chart scale. Keys and passwords stay in your secrets. Install the firmware again
after a change.

Most of these settings are also on the device page in Home Assistant, under **Configuration**. A change there applies
at once and the board keeps it, also after an update. The substitutions are only the start values: once you change a
setting in Home Assistant, the board ignores its substitution, also after a new install. Change it in Home Assistant
from then on.

| Setting in Home Assistant                                    | Start value from                         | Shown            |
| ------------------------------------------------------------ | ---------------------------------------- | ---------------- |
| Night start, Night end                                       | `night_start`, `night_end`               | yes              |
| Display off at night                                         | on                                       | yes              |
| LED day, LED night (0 % = off)                               | `led_day`, `led_night`                   | yes              |
| Chart max                                                    | `chart_max_default`                      | yes              |
| Restart, Scan for meters                                     | (buttons)                                | yes              |
| Display dim, Display bright day, Display bright night        | `display_dim`, `display_bright_*`        | enable it first  |
| Display bright time                                          | `display_bright_time`                    | enable it first  |
| WiFi power save (applies after a restart)                    | `wifi_power_save`                        | enable it first  |
| Log level (back to `log_level` after restart; DEBUG shows the raw telegrams, never the key or readings) | `log_level`                              | enable it first  |
| Factory reset (clears everything the board stored)           | (button)                                 | enable it first  |

"Enable it first": open the entity on the device page, then **Settings → Enabled**. If night start and end are the
same, there is no night. Wi-Fi transmit power stays a substitution. The battery
cutoff (3.4 V) is fixed in `packages/power.yaml`: it protects the cell and matches 0 % on the battery level curve.

The firmware itself is split by topic. You should not need to edit these files to set up a board.

| File                       | Contents                                                 |
| -------------------------- | -------------------------------------------------------- |
| `packages/device.yaml`     | Board, Wi-Fi, Home Assistant API, OTA updates, clock     |
| `packages/meter.yaml`      | Radio, meter decoder, meter sensors, telegram LED        |
| `packages/display.yaml`    | Display pages, brightness, PRG button                    |
| `packages/load_chart.yaml` | Daily kWh and the two load charts                        |
| `packages/power.yaml`      | Battery level, charge detection, low-battery cutoff, off |
| `packages/bluetooth.yaml`  | Optional Bluetooth proxy                                 |
| `packages/loadchart.h`     | Draws the load charts                                    |
| `packages/power_off.h`     | Holds the radio in reset while the board is off          |

## Sensors in Home Assistant

| Sensor                         | Unit   | Note                                   |
| ------------------------------ | ------ | -------------------------------------- |
| Power Import, Power Export     | kW     | Updates about every 5 seconds          |
| Energy Import, Energy Export   | kWh    | Meter totals, for the Energy dashboard |
| Battery Voltage, Battery Level | V, %   |                                        |
| Charging                       | on/off | Best guess, see Battery                |
| wM-Bus RSSI, WiFi Signal       | dBm    | Signal strength of the meter and Wi-Fi |
| IP Address, SSID               | text   |                                        |
| Bluetooth Proxy                | switch | Off by default                         |
| Meter status                   | text   | OK, or what is missing                 |
| Meter address                  | text   | Radio address in use                   |
| Meters nearby                  | text   | Meters heard: address, maker, type     |
| Telegrams per minute           | number | From your meter, average over 10 min   |
| Scan for meters                | button | Empties the list, it fills again       |
| New meter address, key         | config | Set the meter, see below               |

The **Bluetooth Proxy** switch lets Home Assistant use the board as a Bluetooth proxy. It costs battery runtime. The
meter reception does not change.

## Set the meter in Home Assistant

You can set the meter's radio address and AES key in Home Assistant instead of in your secrets. Use this to set up a
board from the web installer, or to fix a wrong address without a new install.

1. Open the board's device page in Home Assistant (**Settings → Devices & services → ESPHome**).
2. Under **Configuration**, enter **New meter address** (8 hex characters, for example `0A1B2C3D`; **Meters nearby**
   lists the candidates) and **New meter key** (32 hex characters from your grid operator).
3. Wait. 60 seconds after your last input, the board saves both values and restarts. **Meter status** counts down in 10 s steps:
   "Saving in 60 s. Enter the key if it is new." After the restart it reads "OK" about a minute later.

Spaces, `-`, `:` and a leading `0x` do not matter, so you can paste the key as your grid operator's portal shows it. A
wrong format changes nothing: **Meter status** says what is wrong at once, and nothing is saved. A field you leave empty
keeps the value the board uses now, so you can change only the address or only the key. Every new input starts the 60
seconds again; empty both fields to cancel. The key field shows only "entered", never the key, and the board never sends the key back to Home
Assistant.

The values from Home Assistant come first, also after an update.

### With the set_meter action

The action does the same, for scripts and the command line. Use it from **Developer tools → Actions**: **ESPHome:
wmbus_set_meter** (`wmbus` is your `device_name`; a board from the web installer has a suffix, for example
`wmbus_a1b2c3_set_meter`), with `address` and `key`. Unlike the fields, an empty field here means "the value from your
secrets", and both empty go back to your secrets entirely. The same values again change nothing and do not restart.

Do not call `set_meter` from an automation or a script: it stores the key in its YAML, its traces and every backup.

**Meter status** tells you what is missing: "Not set up" (still the example address), "Waiting for the meter",
"No radio frames at all", "Waiting for the meter: only others heard", "No telegram for 2 min", "Telegrams, but no
values: check the key" or "OK". Until it reads "OK" once after a start, the display shows the same problem as a setup
screen ([Troubleshooting](troubleshooting.md#setup-screens)). **Meters nearby** lists the meters the board hears, strongest
first, for example `0A1B2C3D SAG electricity -71 dBm`: the address, the manufacturer code and the meter type, all from
the unencrypted part of each telegram. Your electricity meter is the one marked `electricity`, usually the strongest.
These are also your neighbors' addresses: do not share them, and consider excluding the sensor from the Home
Assistant [recorder](https://www.home-assistant.io/integrations/recorder/).

## Display and PRG button

The display always shows the main page at low brightness: current power, today's kWh and two charts. The bottom chart
shows the day in quarter-hour bars. The column on the right shows the current quarter hour, one bar per minute. The day
chart fills from left to right, the minute chart from top to bottom. Ahead of the current bar, each chart still shows
the previous day or quarter hour in a dotted pattern. Between 21:00 and 07:00, the display is dark (**Night start**,
**Night end** and **Display off at night** in Home Assistant). The display needs the time from Home Assistant.

Both charts use a fixed scale. Set it with the **Chart max** number in Home Assistant (default 2.5 kW). A bar at full
length means that power for the whole quarter hour or minute. The scale is a square root, so small loads stay visible.
The day chart survives a restart or an update. The board saves it to flash once per quarter hour and blanks the quarter
hours it missed.

After the first flash, the display shows "kWh total" instead of "kWh today". It changes after the board sees the next
midnight with a valid time.

While something is wrong, the line under the power value alternates every 4 seconds between "kWh today" and a hint in
a white box. Several hints take turns. The board keeps decoding and filling the charts in each case.

| Hint          | When                                                                                         |
| ------------- | -------------------------------------------------------------------------------------------- |
| Wi-Fi offline | No Wi-Fi for 2 minutes. Nothing reaches Home Assistant. After 15 minutes the board restarts. |
| HA offline    | Wi-Fi is up, but no Home Assistant connection for 2 minutes. The kWh totals catch up later.  |
| No telegram   | Your meter has been silent for 5 minutes.                                                    |
| Few telegrams | Less than half the usual telegrams over 10 minutes. See **Telegrams per minute**.            |

Without Wi-Fi, "HA offline" is not shown, and without any telegram, "Few telegrams" is not shown. "No telegram" and "Few
telegrams" start once the meter was OK after a start; a problem before that shows as a setup screen
([Troubleshooting](troubleshooting.md#setup-screens)).

- **Press PRG** to make the display bright for a while (30 seconds by default, setting "Display bright time").
- **Press PRG again** while it is bright to show the next page: status, meter and battery.
- **Hold PRG for 5 to 15 seconds**, then release it. The board switches off. A longer hold does nothing.
- **Press PRG** to switch it on again.

The board has no power switch. "Off" is a deep sleep. In deep sleep, the board does not receive messages and does not
connect to Home Assistant.

## Battery

Use the board on USB power. The battery is a backup. In a test with the 1,100 mAh battery of the tested set, a full
charge lasted about 19 hours. That is an average of about 57 mA.

- **Low-battery cutoff:** at 3.4 V, the board switches itself off. Connect USB power. The battery charges while the
  board is off. Press PRG to switch the board on again.
- **No charging indicator:** the board has no pin that reports USB power or charging. The display shows a short
  "Charging" screen when you connect USB power. It cannot detect when you disconnect it.
- **Battery level:** the percentage follows a measured discharge curve. It shows the remaining runtime, not the raw
  voltage.

## Recipes

The [recipes](../recipes/) folder shows what you can do with the values in Home Assistant.
