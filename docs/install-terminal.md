# Install from a terminal

This path needs no browser and no Home Assistant add-on: every step is a command. Use it on your own computer, or hand
this file to an AI assistant with terminal access (for example Claude Code) and ask it to install the firmware for you.
The assistant needs you for two things only: plugging in the board and filling in `secrets.yaml` yourself (step 2).
Do not give it your passwords or keys in the chat.

**For AI assistants:** never ask for, read, print or log the content of `secrets.yaml`. Let the user edit it in an
editor. The commands below never show a secret.

You need Python 3.11 or newer, git, a USB-A to USB-C data cable (many Heltec V3 boards do not work on USB-C to USB-C) and the AES key of your meter ([Setup step 1](setup.md#1-get-the-key-from-your-grid-operator)).

## 1. Get the firmware and ESPHome

```bash
git clone https://github.com/grinser/esphome-wmbus-listener.git
cd esphome-wmbus-listener
python3 -m venv .venv && . .venv/bin/activate
pip install "esphome>=2026.11"
```

## 2. Fill in the secrets

This copies the template and writes a random API key into it, without showing the key:

```bash
key=$(python3 -c 'import secrets,base64;print(base64.b64encode(secrets.token_bytes(32)).decode())')
sed "s|REPLACE_WITH_GENERATED_BASE64_KEY=|$key|" secrets.yaml.example > secrets.yaml && unset key
```

Open `secrets.yaml` in an editor and fill in:

| Key             | Value                                                                              |
| --------------- | ---------------------------------------------------------------------------------- |
| `wifi_ssid`     | your Wi-Fi name                                                                    |
| `wifi_password` | your Wi-Fi password                                                                |
| `api_key`       | already filled in; Home Assistant asks for it later                                |
| `meter_key`     | the AES key from your grid operator, 32 hex characters                             |
| `meter_address` | keep `0A1B2C3D` if you do not know it yet; step 4 finds it                         |

`secrets.yaml` stays on your computer: it is in `.gitignore`. Never paste its content into a chat or a log.

## 3. Build and flash

Connect the board with the USB-A to USB-C cable.

```bash
esphome run wmbus.yaml --no-logs --device /dev/cu.usbserial-0001   # macOS; Linux: /dev/ttyUSB0
```

Find the port with `ls /dev/cu.usbserial* /dev/ttyUSB* 2>/dev/null`. Without `--no-logs`, the command keeps showing the
log until you press Ctrl+C. The commands here are for macOS and Linux; on Windows, use WSL or the
[Device Builder](setup.md#with-the-esphome-device-builder-recommended). The first build takes several minutes. The display shows
the status page with the IP address when the board is on Wi-Fi.

## 4. Find the meter's radio address

The **Meters nearby** sensor lists the addresses the board hears, strongest first. Read it from the terminal:

```bash
python3 - <<'PY'
import asyncio, yaml
from aioesphomeapi import APIClient, TextSensorState

key = yaml.safe_load(open("secrets.yaml"))["api_key"]

async def main():
    cli = APIClient("wmbus.local", 6053, None, noise_psk=key)  # your device_name + .local
    await cli.connect(login=True)
    entities, _ = await cli.list_entities_services()
    names = {e.key: e.name for e in entities}
    def on_state(s):
        if isinstance(s, TextSensorState) and names.get(s.key) in ("Meters nearby", "Meter status"):
            print(f"{names[s.key]}: {s.state}")
    cli.subscribe_states(on_state)
    await asyncio.sleep(40)
    await cli.disconnect()

asyncio.run(main())
PY
```

Your meter is usually the strongest address. Put it into `secrets.yaml` as `meter_address` and install again, now over
Wi-Fi:

```bash
esphome run wmbus.yaml --no-logs --device wmbus.local
```
After a minute, **Meter status** reads `OK`. If it reads "Telegrams, but no values: check the key", `meter_key` is
wrong. If it reads "Waiting for the meter: only others heard", try the next address. Other texts:
[Setup screens](troubleshooting.md#setup-screens). [Setup step 3](setup.md#3-find-the-radio-address-of-your-meter)
shows the slower, exact way with `wmbusmeters`.

The list also holds your neighbors' addresses. Do not share it.

## 5. Add the board to Home Assistant

[Setup step 4](setup.md#4-add-the-board-to-home-assistant). Home Assistant asks for the `api_key` from your secrets.

## Updates

`git pull`, then `esphome run wmbus.yaml --no-logs --device wmbus.local`.
