# AGENTS.md

ESPHome firmware for a Heltec WiFi LoRa 32 V3 that receives an encrypted wM-Bus electricity meter (T1, 868.95 MHz),
decodes it on the device and reports to Home Assistant. User docs are in `README.md` and `docs/`; this file is for
working on the code.

## Layout

| What                                   | Where                                                              |
| -------------------------------------- | ------------------------------------------------------------------ |
| All settings (substitutions)           | `wmbus.yaml` (local build) and `device-builder.yaml` (remote)      |
| Board, Wi-Fi, API, OTA, clock          | `packages/device.yaml`                                             |
| Radio, meter decoder, sensors, LED     | `packages/meter.yaml` (pins the external component)                |
| OLED pages, brightness, PRG button     | `packages/display.yaml`                                            |
| Status bar and setup screens logic     | `packages/status.h` (host-tested)                                  |
| Daily kWh and the two load charts      | `packages/load_chart.yaml`, drawing code in `packages/loadchart.h` |
| Battery, low-battery cutoff, power off | `packages/power.yaml`, pin hold in `packages/power_off.h`          |
| Optional Bluetooth proxy               | `packages/bluetooth.yaml`                                          |
| Home Assistant examples                | `recipes/`                                                         |
| Host tests                             | `tests/`                                                           |
| Display preview (PNG of a screen)      | `tools/render_display.py`, state in `tools/display-state.json`     |

`wmbus.yaml` includes the packages with `!include`; `device-builder.yaml` loads the same files from GitHub (`ref: stable`, the latest release).
`wmbus.factory.yaml` is the web-installer firmware: it includes `wmbus.yaml` and removes everything personal (Wi-Fi,
API key, meter address). It has no network OTA and no hotspot on purpose: without a key at build time both would be
open to anyone in range. Do not add them back. `.github/workflows/release.yml` builds it with `esphome/build-action` and attaches it to a
published release.

## Installing for a user

Follow [`docs/install-terminal.md`](docs/install-terminal.md) step by step. Ask the user to plug in the board and to
fill in `secrets.yaml` themselves. Never ask for, read, print or commit its content.

## Commands

```bash
tests/run.sh                          # host tests: load chart, night hours, status bar, settings check
cp secrets.yaml.example secrets.yaml  # then put a real base64 key into api_key (see below)
esphome config wmbus.yaml
esphome compile wmbus.yaml
```

ESPHome 2026.11 or newer. The placeholder `api_key` in the example does not compile; generate a random one as the
comment in `secrets.yaml.example` shows (CI does the same).

## Rules

- Settings a user may change go into `substitutions:`, never as literals in a package. Change them in **both**
  `wmbus.yaml` and `device-builder.yaml`; `tests/check_device_builder.py` fails if they differ.
- Keep entity names and IDs stable. Home Assistant history and the `recipes/` depend on them.
- The external component (SzczepanLeon/esphome-components) is pinned to a reviewed commit in `packages/meter.yaml`.
  To bump it: read the diff since the pinned commit, update the comment with the date and what changed, compile.
- Many substitutions are only start values: the board stores the Home Assistant setting (template `number`/`select`
  with `restore_value`) and the lambdas read `id(...).state`, not `${...}`. See the blocks headed
  `# --- Settings in Home Assistant` in `device.yaml` and `display.yaml`, and the LED numbers in `meter.yaml`.
  Selects take their `initial_option` from a substitution through `| upper`, so lower-case values still build.
- Night mode uses the "Night start"/"Night end" numbers through `is_night_hour()` in `packages/settings.h` (Home
  Assistant time); no valid time counts as day.
- New entities and globals go into an existing package, not a new file: copies of `device-builder.yaml` that users
  already have list the package files and would not load a new one.
- Device Builder users load the `stable` branch (`refresh: 1d`), the release workflow moves it to every full release.
  `main` is development, but keep it buildable: a release is cut from it.
- README stays short (around 100 lines); details go to `docs/`.

## Versions and releases

SemVer, tags `vX.Y.Z`. The release workflow puts the version into the factory firmware (`project.version`).

| Part  | When                                                                                       |
| ----- | ------------------------------------------------------------------------------------------ |
| Major | Existing boards need action: an entity name or ID changed, a substitution removed, USB flash |
| Minor | New feature                                                                                |
| Patch | Fix                                                                                        |

The GitHub release notes are the changelog: Home Assistant shows them in the firmware update dialog. There is no
`CHANGELOG.md`. Write them for users with the template in `docs/releasing.md`. Pre-releases reach no board.

## Gotchas

- `packages/load_chart.yaml` adds to `ha_time`, `energy_import` and `power_import` with `!extend`. Moving those IDs
  breaks it.
- `tests/extract.py` pulls the lambdas out of `load_chart.yaml` by their `id:` anchors. Keep those lines intact.
- `includes:` resolve first against the config directory, then beside the package. That is why the `.h` files can
  live in `packages/` and still work remotely.
- Lists merge across packages (`sensor:`, `globals:`, …); a key defined twice as a mapping overrides.
- After an OTA, wait for "Boot seems successful" (about 40 s) before a USB or EN reset. Opening the serial port resets
  the board; a reset before that line rolls back to the previous firmware.
- Many Heltec V3 boards lack the USB-C CC resistors: a USB-C to USB-C cable gives no serial port on a Mac. Use a
  USB-A to USB-C data cable.
- GCC 15 (CI) needs `using std::isnan;` in the host test; macOS clang does not.
- Substitutions inside lambdas are plain text: `${display_dim}f` becomes `0.15f`, so keep numeric values quoted
  strings in `substitutions:`.
