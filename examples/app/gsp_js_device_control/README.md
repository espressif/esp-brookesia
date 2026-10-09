# GSP JavaScript Device Control

[中文](./README_CN.md)

A small app for the ESP-Brookesia GSP interface that you install while the board is running. It shows the screen brightness and lets you change it with a slider. The logic is JavaScript and the pages are GSP scenes, so you can change either one, build a new package, and update the app over USB without rebuilding or flashing the firmware.

## Features

- Two pages: a status page and a brightness page with a slider
- Reads and changes the backlight through the Display service, and shows the value the service reports
- Install, update, and uninstall over USB
- Appears on the launcher next to Settings

## Requirements

- An ESP32-P4 Function EV Board running the [System GSP App](../../system/gsp_app/README.md) with the GSP interface, which is the default
- Python 3
- The GSP scene compiler: `python -m pip install -U esp-gsp-tools`
- The USB command line tool: `python -m pip install brookesia-usb`

## Build

```bash
python3 tools/build_package.py --version 0.1.0
```

The package is written to `build/brookesia.gsp.device_control.debug.0.1.0.bpk`.

If the build cannot find `gspc`, set `GSPC_EXECUTABLE` to its path or pass `--gspc <path>`.

## Install

Connect the board's USB Serial/JTAG port, wait until the launcher is on screen, and close any serial monitor. Then run:

```bash
brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
```

**Device Control** appears on the launcher next to Settings. The launcher shows up to four installed apps, and they stay installed after a restart.

## Update

Build again with a higher version and install it the same way. The installed copy is replaced.

```bash
python3 tools/build_package.py --version 0.2.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.2.0.bpk
```

A package that is damaged or built for another interface is rejected, and the installed version keeps working.

## Uninstall

```bash
brookesia-usb call SystemCore ListApps '{}'
brookesia-usb call SystemCore UninstallApp '{"AppId": <id>}'
```

Use the `app_id` of `brookesia.gsp.device_control` from the first command. The id can change after a reinstall, so list the apps again before uninstalling.

## Using the app

1. Tap **Device Control** on the launcher.
2. The status page shows the package version and the current brightness.
3. Tap **Brightness** and drag the slider. The screen brightness changes, and the value shown is read back from the Display service.
4. Tap **Back** to return to the status page, or **Exit** to return to the launcher.

## Customize

| Path | Purpose |
| --- | --- |
| `package/app/app.js` | App logic |
| `scenes/status.json`, `scenes/brightness.json` | The two pages |
| `package/manifest.json` | Name, version, and required services |
| `tools/build_package.py` | Compiles the pages and creates the `.bpk` |

After editing, build with a new `--version` and install again.

### JavaScript interface

`app.js` defines `globalThis.brookesia_app` with these callbacks:

| Callback | Called when |
| --- | --- |
| `on_start()` | The app opens |
| `on_stop()` | The app closes |
| `on_action(action, path, payloadJson)` | A page control is used or a page change finishes |
| `on_event(serviceName, eventName, itemsJson)` | A subscribed service event arrives |

`on_action` receives two kinds of action:

- `gsp.call`: a control with a `callback` was used. The payload holds `scene`, `action_id`, `arg`, and `key`, which is the FNV-1a hash of the callback name from the scene JSON. `app.js` shows how to compute it.
- `gsp.scene`: the page requested with `gsp_show_page` is now on screen. Controls on the new page only exist from this point, so update them here.

To change what is on screen:

| Function | Effect |
| --- | --- |
| `brookesia.gsp_set_text(name, text)` | Sets a label's text |
| `brookesia.gsp_set_value(name, value)` | Sets a slider's value |
| `brookesia.gsp_show_page(scene)` | Switches to the page with that index, in build order |

A label's text can be changed only if it has a `bind` in the scene JSON. Services are called with `brookesia.call_service_function`, and the services an app needs are listed in `manifest.json`.

## Notes

- The package runs only on firmware built with the GSP interface. Installing it on an LVGL build is rejected.
- `python3 tools/build_package.py --preview` shows the pages in the ESP-GSP simulator, `gsp_sim_host` from esp-gsp-tools. It shows layout only. It does not run `app.js` or talk to the Display service.
- Figma export produces LVGL JSON UI packages and cannot create GSP pages.

## Troubleshooting

| Symptom | What to do |
| --- | --- |
| `brookesia-usb` times out or reports that the port is busy | Close any serial monitor or other program using the board's port |
| The first command after a reset fails or lists no apps | Wait until the launcher is on screen, then run it again |
| Install fails with a message about the GUI backend | The firmware uses the LVGL interface. Build and flash the GSP interface |
