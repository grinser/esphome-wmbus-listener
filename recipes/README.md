# Recipes for Home Assistant

These recipes use the meter values in Home Assistant. Recipes 1–3 are Home Assistant packages. Recipe 4 uses the Energy
dashboard and needs no YAML.

The recipes come from one apartment. The thresholds fit that apartment. Check them against a day of your own
data before you rely on them.

| Recipe               | What it shows                                         | File                   |
| -------------------- | ----------------------------------------------------- | ---------------------- |
| 1. Hot water running | A hot tap is open (electric instantaneous heater)     | `hot_water.yaml`       |
| 2. Unmeasured load   | Load that no smart plug measures, for example a stove | `unmeasured_load.yaml` |
| 3. Meter reporting   | The meter values are current                          | `meter_reporting.yaml` |
| 4. Energy dashboard  | Usage per hour, per device, and the untracked rest    | (no file)              |

## Install the packages

The entity IDs in the files assume the device name from `wmbus.yaml` (`wM-Bus Meter`). If you renamed the device, change
the entity IDs.

1. Make sure that `configuration.yaml` loads packages:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

2. Copy `meter_reporting.yaml` into the `packages` folder. Recipes 1 and 2 need it.
3. Copy the recipes that you want into the same folder.
4. For recipe 2, replace the plug entities with your own plugs. The file explains how.
5. Open **Developer tools → YAML**. Select **Check configuration**.
6. Restart Home Assistant. The new sensors appear in **Settings → Entities**.

Use the sensors in your own automations. For example, send a notification when you leave home and "Stove on" or "Hot
water running" is on.

## Recipe 4: Energy dashboard

The Energy dashboard shows the meter, your smart plugs, and the difference between them.

1. Open **Settings → Dashboards → Energy**.
2. Add **Energy Import** as grid consumption.
3. Add **Energy Export** as return to grid, if you produce energy.
4. Add the energy sensors of your smart plugs as individual devices.
5. Open the Energy dashboard. Select a day.

The hourly bars show when you used energy. Usage that no plug measures shows as untracked consumption.

The bars between 10:00 and 16:00 show your usage in the time window of a time-of-use tariff, for example the Austrian
summer discount. This also works for past days.
