# Setup

There are two ways to build and install the firmware. Both give the same result.

- **ESPHome Device Builder** in Home Assistant (recommended). You paste one file and the Device Builder loads everything
  else from GitHub. You need no Python and no git.
- **ESPHome command line** on your computer, for setups without the Home Assistant add-on.

Either way you need ESPHome 2026.11 or newer.

### 1. Get the key from your grid operator

You can install the firmware before this step. The meter starts to send only after the activation.

Linz Netz offers two ways to use the customer interface: its own Smart Meter Adapter (variant A) or a wireless M-Bus
receiver of your choice (variant B). This board is a variant B receiver.

1. Read OBIS code `0.2.0` on the meter display. The firmware version must be 4.50.16 or higher.
2. Open the
   [Kundenschnittstelle page](https://www.linznetz.at/portal/de/home/online_services/serviceportal/kundenschnittstelle)
   of Linz Netz.
3. Select **Verfügbarkeit prüfen** and log in to the Serviceportal.
4. Activate the customer interface. After the activation, you get your personal access code ("Zugangscode").
5. Use the access code as the AES key. It has 32 hexadecimal characters.
6. Keep the key secret. Anyone with the key can read your consumption from outside your home.

Other grid operators have similar steps. The names of the menus can be different.

### 2. Install the firmware

#### With the ESPHome Device Builder (recommended)

1. Install the ESPHome Device Builder add-on. See
   [Getting started with ESPHome and Home Assistant](https://esphome.io/guides/getting_started_hassio/).
2. Open the Device Builder and select **Secrets**. Add the four keys from
   [`secrets.yaml.example`](../secrets.yaml.example) that are not there yet:
   - `api_key`: a new 32-byte key in base64. The [API component page](https://esphome.io/components/api/) has a
     generator.
   - `meter_key`: the AES key from step 1.
   - `meter_address`: keep the example value `0A1B2C3D` for now. Step 3 shows how to find the correct value.
   - `wifi_ssid` and `wifi_password`, if your secrets do not have them yet.
3. Create a new device. Finish the wizard without installing.
4. Open the new device with **Edit**. Replace its whole content with [`device-builder.yaml`](../device-builder.yaml).
   Change the settings at the top if you want to (see [Usage](usage.md#settings)). Save.
5. Connect the board with a USB-A to USB-C data cable and select **Install**. The first build takes several minutes.
   - **Plug into this computer** works when you open Home Assistant over HTTPS.
   - Otherwise select **Manual download**, then flash the file with [ESPHome Web](https://web.esphome.io/).
   - If the computer does not find the board, see
     [Physical device connection](https://esphome.io/guides/physical_device_connection/) (it covers the CP210x driver
     for Windows).
6. Wait for the status page on the display. It shows the IP address and the Wi-Fi signal.

Later updates go over Wi-Fi (**Install → Wirelessly**). The Device Builder checks GitHub for a new version once a day.

#### With the ESPHome command line

Follow [Install from a terminal](install-terminal.md). It has every step as a command, so you can also hand it to an
AI assistant with terminal access.

### 3. Find the radio address of your meter

The meter sends under a radio address. **This address is not the serial number on the meter label.** The serial number
is only inside the encrypted message.

**Caution:** the log also shows the addresses of your neighbors' meters. Do not share logs or screenshots without
masking every address.

**Caution:** the online analyzer at wmbusmeters.org receives your AES key. Use the local command below to keep the key
on your computer.

Shortcut: once the board is in Home Assistant (step 4), its **Meters nearby** sensor lists the addresses it hears,
strongest first, with the meter type: yours is marked `electricity`. Enter that address and your key on the device page;
the board saves them 60 seconds later ([Usage](usage.md#set-the-meter-in-home-assistant)). If **Meter status** then shows "OK", the address is right.
Otherwise use the steps below.

1. In Home Assistant, enable the **Log level** setting on the device page and set it to `DEBUG`.
2. If the board is not in Home Assistant yet: set `log_level` to `DEBUG` in your config and install again (Device
   Builder: **Install → Wirelessly**; command line: `esphome run wmbus.yaml`).
3. Watch the log (Device Builder: **Logs**; command line: `esphome logs wmbus.yaml`). Every few seconds, a line
   `RSSI: … T: <hex>` shows one message.
4. Copy the hex text of one message.
5. Install [wmbusmeters](https://github.com/wmbusmeters/wmbusmeters) on your computer. **Windows:** wmbusmeters has no
   Windows version; run it in WSL (Windows Subsystem for Linux).
6. Decode the message with your key:

   ```bash
   wmbusmeters --analyze=<your AES key> <hex text>
   ```

7. Repeat with messages from other addresses until one decrypts. Its address is your meter's radio address.
8. Check that the decrypted serial number matches the number on your meter label.
9. Check the field names in the output. They must match the `field:` values in `packages/meter.yaml`.
10. Enter the radio address as `meter_address` in your secrets. Use 8 characters in uppercase, for example `0A1B2C3D`.
11. Set `log_level` back to `INFO`. Install the firmware again.

The white LED on the board blinks briefly each time a message from your meter arrives.

### 4. Add the board to Home Assistant

1. Open **Settings → Devices & services** in Home Assistant. The ESPHome integration shows the new device. See the
   [ESPHome integration](https://www.home-assistant.io/integrations/esphome/) for details.
2. Select **Configure**. Enter the `api_key` from your secrets.
3. Open the device page. The power and energy sensors show values within a few seconds.
4. Compare **Energy Import** with the total on the meter display. The values must match.
5. Optional: add **Energy Import** and **Energy Export** to the
   [Energy dashboard](https://www.home-assistant.io/docs/energy/electricity-grid/).
