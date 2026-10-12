# Device Control

[中文](./README_CN.md)

Device Control is an example app for ESP-Brookesia boards that use the GSP interface. It shows the screen brightness and changes it with a slider. The pages are GSP scenes and the logic is JavaScript, so you can change the app and install the new version over USB without rebuilding or flashing the firmware.

## Features

- A status page and a brightness page with a slider
- Reads and sets the backlight through the Display service
- Installs, updates, and uninstalls over USB while the board is running
- Can also be built into the firmware image as a preinstalled app

## Requirements

- ESP32-P4 Function EV Board running [System GSP App](../../system/gsp_app/README.md) with the GSP interface (the default)
- Python 3
- GSP tools: `python -m pip install -U esp-gsp-tools`
- USB tool: `python -m pip install brookesia-usb`

## Quick start

Run these commands from this directory.

1. Build the package:

   ```bash
   python3 tools/build_package.py --version 0.1.0
   ```

   The package is written to `build/brookesia.gsp.device_control.debug.0.1.0.bpk`.

2. Connect the board's USB Serial/JTAG port and close any serial monitor that uses it.

3. Install:

   ```bash
   brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
   ```

**Device Control** appears on the launcher next to **Settings**. Tap it, then tap **Brightness** and drag the slider. **Back** returns to the status page and **Exit** returns to the launcher.

## Update

Build with a higher version and install it the same way:

```bash
python3 tools/build_package.py --version 0.2.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.2.0.bpk
```

The new version replaces the old one and keeps the app's data. If the package is damaged or does not fit the firmware, the install is refused with the reason and the installed version keeps working.

## Uninstall

Find the app's `app_id`, then uninstall it:

```bash
brookesia-usb call SystemCore ListApps '{}'
brookesia-usb call SystemCore UninstallApp '{"AppId": <app_id>}'
```

Uninstalling also deletes the app's data. The `app_id` changes after every install, so list the apps first.

## Preinstall in the firmware

USB installation is the usual way to deliver this app. To ship a firmware image that already contains it, enable the option in System GSP App:

```text
idf.py menuconfig
  Brookesia system GSP example → Preinstall the Device Control app
```

The build compiles the package and adds it to the LittleFS image. The image is written only by a full `idf.py flash`, which **replaces every app installed over USB and their data**. `idf.py app-flash` leaves LittleFS untouched, so it neither adds nor removes the preinstalled app. A preinstalled app can still be updated or uninstalled over USB.

## Customize

| Path | Contents |
| --- | --- |
| `package/app/app.js` | App logic |
| `scenes/status.json`, `scenes/brightness.json` | The two pages |
| `package/manifest.json` | App ID, name, version, and required services |
| `tools/build_package.py` | Compiles the pages and creates the `.bpk` |

After a change, build with a new `--version` and install it. To check page layout without a board, run `python3 tools/build_package.py --preview`. The preview shows layout only and does not run `app.js`.

### JavaScript API

`app.js` assigns `globalThis.brookesia_app` with these callbacks:

| Callback | Called when |
| --- | --- |
| `on_start()` | The app opens. Return `false` or throw to cancel the start |
| `on_stop()` | The app closes |
| `on_action(action, path, payloadJson)` | A control is used (`gsp.call`) or a requested page is on screen (`gsp.scene`) |
| `on_event(serviceName, eventName, itemsJson)` | A subscribed service event arrives |

For `gsp.call`, the payload includes `arg` and `key`, the FNV-1a hash of the control's `callback` name; `app.js` shows how to compute it. Controls on a new page exist only after its `gsp.scene` action, so update them there.

| Function | Effect |
| --- | --- |
| `brookesia.gsp_set_text(name, text)` | Sets the text of a label that has a `bind` in the scene |
| `brookesia.gsp_set_value(name, value)` | Sets a slider's value |
| `brookesia.gsp_set_property(name, property, value)` | Sets a property the scene declares, such as `visible`, `enabled`, `color` (`"#RRGGBB"`) or a clock hand angle |
| `brookesia.gsp_show_page(index)` | Shows a page by its build order |
| `brookesia.call_service_function(service, function, argsJson, timeoutMs)` | Calls a service declared in `manifest.json` |

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `brookesia-usb` times out or the port is busy | Close any program using the board's serial port |
| The app list is empty right after a reboot | Wait for the launcher to appear, then retry |
| Install fails with a GUI backend message | The firmware uses the LVGL interface; build and flash the GSP interface |
| The build cannot find `gspc` | Set `GSPC_EXECUTABLE` or pass `--gspc <path>` |
| Tapping the app returns to the launcher | `on_start` failed; the reason is in the serial log |

## Limitations

- Runs only on firmware built with the GSP interface.
- Figma export produces LVGL JSON UI packages and cannot generate GSP pages.
