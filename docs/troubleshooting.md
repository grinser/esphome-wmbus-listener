# Troubleshooting

| Problem                                             | Cause and fix                                                                                                                                 |
| --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| The log shows no messages at all                    | Set `log_level` to `DEBUG`. If the log stays empty, the customer interface is not active yet.                                                 |
| Messages arrive, but none from your meter           | You use the label serial number as `meter_address`. Find the radio address ([Setup step 3](setup.md#3-find-the-radio-address-of-your-meter)). |
| The radio starts, but receives nothing              | `rf_switch: true` in `packages/meter.yaml` is missing. The V3 needs it.                                                                       |
| The log stops after boot                            | The logger uses the wrong port. Keep `hardware_uart: UART0` in `packages/device.yaml`.                                                        |
| The logger fails after a config change              | Keep the `components:` list under `external_components` in `packages/meter.yaml`. It excludes an outdated `esp32` override in the component.  |
| Unavailable in Home Assistant, but the display runs | The board no longer restarts while Home Assistant is away. If it stays unavailable after Home Assistant is back, press RST on the board.      |
| Home Assistant actions are slow                     | `wifi_power_save: HIGH` causes delays of some seconds. Change it to `LIGHT`.                                                                  |
| WiFi Signal is below −75 dBm                        | Increase `wifi_output_power` in `wmbus.yaml`.                                                                                                 |

Do not set `frequency:` for the radio. That option is for the CC1101 only.

## Setup screens

Until the meter is OK once after a start, the display shows what is missing instead of the main page. It stays on at
night. Wi-Fi and Home Assistant come first, then the meter; their screens can show right after the start. During the
first two minutes the main page shows; then the board decides between "no radio" and "meter not heard". "Wrong key?"
shows after three telegrams of your meter without a value. Once the meter was OK, the main page stays, also if the meter
goes quiet later.

| Screen                           | Meter status                             | What to do                                                                                                                                                                                                                       |
| -------------------------------- | ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Wi-Fi                            | –                                        | The board has no Wi-Fi yet (web installer). Scan the QR code, connect USB and enter your Wi-Fi on the installer page ([Web installer](web-installer.md)).                                                                        |
| Wi-Fi not found                  | –                                        | The stored network is out of reach for 30 seconds. The screen shows its name. Check the router and the distance; the board keeps trying.                                                                                         |
| Home Assistant (add this device) | –                                        | Factory firmware: add the board in Home Assistant (**Settings → Devices & services**) within 30 minutes after a start. The bar shows the minutes left.                                                                           |
| Home Assistant (30 min are over) | –                                        | The window to add the board has closed. Unplug the board and plug it in again, then add it.                                                                                                                                      |
| Set up the meter                 | Not set up                               | Enter the address and key on the device page (or run `set_meter`) ([Usage](usage.md#set-the-meter-in-home-assistant)). The screen shows the strongest address nearby, or "Wi-Fi not connected" if the board is not on Wi-Fi yet. |
| No radio at all                  | No radio frames at all                   | No frame at all since the start. Move the board away from USB chargers and routers, and check the antenna.                                                                                                                       |
| Meter not heard                  | Waiting for the meter: only others heard | Other meters arrive, yours does not. Wrong address, too far away, or the customer interface is not active yet.                                                                                                                   |
| Wrong key?                       | Telegrams, but no values: check the key  | Your meter is heard, but nothing decodes. Check the key from your grid operator, and that your meter type sends the fields in `packages/meter.yaml`.                                                                             |
