# Releasing

For maintainers. A full release builds the factory firmware, attaches it to the release, publishes the web installer
to GitHub Pages and moves the `stable` branch, which Device Builder users load.

1. Check that `main` is green and has been on a board.
2. Pick the version (SemVer, see `AGENTS.md`): major if existing boards need any action, minor for a feature, patch
   for a fix.
3. On GitHub: **Releases → Draft a new release**, tag `vX.Y.Z` on `main`, title `vX.Y.Z`. Use **Generate release
   notes** only as raw material and rewrite them with the template below.
4. Publish. A pre-release builds the firmware but reaches no board and does not move `stable`.

Before a major or minor release, run the release-candidate test below first.

## Release-candidate test

Run it on a real board, against a Home Assistant you use every day if you have no test instance. A spare board shows
up there as a new device (`wmbus-xxxxxx`). Your everyday board does not: Home Assistant knows it by its MAC address,
so the test runs the path of a user who moves an existing board to the web installer ([A board that was set up
before](web-installer.md#a-board-that-was-set-up-before)). Its entity IDs and history stay; expect a gap in its values
of about 30 minutes. Mask meter addresses and keys in every screenshot.

**Builds.** GitHub Pages only serves full releases, so the test runs from your computer. Build the factory firmware
twice, as `X.Y.Z-rc1` and `X.Y.Z-rc2`. For each build, set `version: dev` in `wmbus.factory.yaml` to the rc version
(do not commit it) and run (`scutil --get LocalHostName` gives the name on a Mac; not `localhost`, the board fetches
updates from your computer over Wi-Fi):

```bash
esphome -s update_manifest_url http://<your-computer>.local:8000/firmware/wmbus-listener.manifest.json compile wmbus.factory.yaml
```

Copy `firmware.factory.bin` and `firmware.ota.bin` from `.esphome/build/wmbus/build/` (older ESPHome versions: `.pioenvs/wmbus/`) into a folder
`firmware/` next to a copy of `static/`, and write `firmware/wmbus-listener.manifest.json` (the file the page loads) for the build you want to offer:

```json
{
  "name": "wM-Bus Meter",
  "version": "X.Y.Z-rc1",
  "home_assistant_domain": "esphome",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [{ "path": "firmware.factory.bin", "offset": 0 }],
      "ota": { "path": "firmware.ota.bin", "md5": "<md5 of firmware.ota.bin>", "summary": "Release candidate" }
    }
  ]
}
```

Serve the folder with `python3 -m http.server 8000` and open `http://localhost:8000/` in Chrome or Edge (Web Serial
needs https or localhost). Start with rc1; for the update test, replace the files and the manifest with rc2's.

| # | Test                     | Pass when                                                                                    |
| - | ------------------------ | -------------------------------------------------------------------------------------------- |
| 1 | Install from the page    | The local installer page flashes the board with **Erase device** ticked and asks for Wi-Fi  |
| 2 | Adopt in Home Assistant  | Home Assistant finds the board; **Add** (spare board) or **Reconfigure** (everyday board) works (restart Home Assistant if it asks for a key); the board keeps the API key after RST |
| 3 | Set the meter            | Meter status goes from "Not set up" via "Saving in 60 s" (10 s steps) to "OK" and values arrive           |
| 4 | Update                   | The "Firmware" entity offers rc2, the install finishes, settings and meter values are kept   |
| 5 | Interrupted update       | Start an update, stop the http server during the download: the old version keeps running, Meter status shows "Update failed, try again" |
| 6 | No battery               | Unplug the battery, run on USB power for a day: the board never switches itself off          |
| 7 | Back to your firmware    | Install your own firmware over USB: the board keeps the key Home Assistant gave it and reconnects without input (or delete the spare test device) |

Note any failure in the release candidate's pull request and fix it before the release.

## Release notes template

Home Assistant shows these notes in the firmware update dialog, so write them for users, not for developers.

```markdown
### New
- What you can do now, in one line each.

### Fixed
- What works again, in one line each.

### Before you update
(Major releases only.) What you must do, step by step. Example: "Set the meter key again with set_meter."

Built with ESPHome 2026.11.0.
```

Leave out a section that has nothing in it.
