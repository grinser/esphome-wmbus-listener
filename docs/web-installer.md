# Web installer

The web installer puts a prebuilt firmware on the board from the browser. You do not build anything and you need no
ESPHome installation. Wi-Fi, the Home Assistant key and the meter values go onto the board after the install, so the
firmware holds nothing personal.

Installer page: <https://grinser.github.io/esphome-wmbus-listener/>

## What you need

- Chrome or Edge on a computer. Safari and Firefox cannot flash over USB.
- A USB-A to USB-C **data** cable. Some cables only charge, and many Heltec V3 boards stay dark on a USB-C to USB-C
  cable (the board lacks the resistors a USB-C port looks for). On a Mac with only USB-C ports, use an adapter or hub.
- On Windows: the [CP210x driver](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers) for the USB
  chip of the board.
- The AES key of your meter (setup step 1).

## Install

1. Open the installer page and connect the board.
2. Select **Connect**, pick the serial port of the board, then **Install**. If the board ran any other firmware before,
   tick **Erase device**: otherwise it keeps its old Home Assistant key.
3. When the page asks, enter your Wi-Fi name and password. The board learns Wi-Fi only over USB: it has no hotspot.
4. In Home Assistant, open **Settings → Devices & services**. The ESPHome integration shows the new device. Select
   **Add**. Home Assistant sends the board a new API key; the board stores it. Do this within 30 minutes of plugging
   the board in: after that, the board stops accepting a new key until its next start. Until then, any device in
   your network could claim it.
5. **Meter status** shows "Not set up". **Meters nearby** lists the radio addresses the board hears, strongest first.
6. On the device page under **Configuration**, enter **New meter address** and **New meter key**. The board saves
   both 60 seconds later and restarts ([Usage](usage.md#set-the-meter-in-home-assistant)). Then **Meter status** shows
   "OK".

All settings are on the device page under **Configuration** ([Usage](usage.md#settings)).

## A board that was set up before

Installing this firmware on a board that Home Assistant already knows (built with your own secrets) keeps its history.
Home Assistant knows the board by its MAC address, so it does not show a new device:

1. Install with **Erase device** ticked (Install, step 2).
2. The existing device asks for attention. If Home Assistant asks for an encryption key, close the dialog and restart
   Home Assistant (**Settings → System → Restart**). After the restart it offers **Reconfigure**: confirm that the
   device no longer uses encryption.
3. Home Assistant then gives the board a new API key at once; the connection is encrypted again. Do this within 30
   minutes of the install, or press RST on the board to start the 30 minutes again.
4. Entity IDs, history and your settings in Home Assistant stay. Some settings are hidden by default (display
   brightness, log level, Wi-Fi power save, factory reset): enable them on the device page if you need them.
5. Enter the meter again (step 6).

## Updates

The **Firmware** update entity checks GitHub once a day. Install an update from Home Assistant like any other update.
Your settings, the meter values and Wi-Fi stay on the board. If the download breaks off, the board keeps running the
old version without a restart, and **Meter status** shows "Update failed, try again" for a minute. Install again.

## Move to the Device Builder later

To change more than the settings, adopt the board in the ESPHome Device Builder: it shows the board as "discovered".
Select **Adopt**. The Device Builder then builds the firmware from `device-builder.yaml`.

1. Before you adopt, add all keys from [`secrets.yaml.example`](../secrets.yaml.example) to the Device Builder's
   secrets: `wifi_ssid`, `wifi_password`, `api_key`, `meter_address`, `meter_key`. For the meter keys, any value in
   the right format works: the values stored on the board come first.
2. Install the first Device Builder firmware **over USB** (**Install → Plug into this computer**). The web-installer
   firmware accepts updates only from GitHub, not over the network. Later installs go over Wi-Fi.
3. In Home Assistant, the device asks for the new `api_key` once. Enter the one from your secrets.

## Security

- The firmware holds no personal value. Wi-Fi, the API key and the meter key are stored on the board, in flash.
- Nobody can install firmware over the network: updates come only from this project's GitHub releases, over HTTPS.
- Anyone with the board in hand can read the flash, and anyone with USB access can change its Wi-Fi. Keep the board
  where strangers cannot reach it.

## If it does not work

| Problem                     | Fix                                                                                     |
| --------------------------- | --------------------------------------------------------------------------------------- |
| No serial port to pick      | Use a USB-A to USB-C data cable (see above). Windows: install the CP210x driver.        |
| Install stops or fails      | Hold PRG, press RST, release PRG. Then select **Connect** again.                        |
| No Wi-Fi prompt             | Keep USB connected, reload the page, **Connect** → **Change Wi-Fi**.                    |
| Home Assistant does not see | Same network? More than 30 minutes since the start? Unplug the board and plug it in.    |
| Asks for an encryption key  | Close the dialog and restart Home Assistant. Still asking: install again with **Erase device** ticked. |
| Display shows a setup screen | **Meter status** and the screen say what is missing: see [Setup screens](troubleshooting.md#setup-screens). |
