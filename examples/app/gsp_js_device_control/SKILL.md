---
name: gsp-js-device-control
description: Build, preview, pack, and install the Brookesia GSP JavaScript device-control app. Use when changing this package, compiling its GSP pages, or checking that the GUI backend and the JavaScript execution backend are set correctly.
---

# GSP JavaScript device control

The package lives in `examples/app/gsp_js_device_control`. It has two separate backend settings: the GUI backend is `gsp`, and the execution backend is `JavaScript`. Do not treat them as one choice.

## Build and install

Follow [Requirements](README.md#requirements) to prepare Python and obtain GSPC 0.5.0 with the public tool manager. Keep `GSPC_EXECUTABLE` exported for builds, updates, and previews. Run from the app directory:

```bash
python3 tools/build_package.py --version 0.1.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
```

Wait for the launcher before running a `brookesia-usb` command after a reset, and close any serial monitor first.

USB install is the normal path. The directory is also an optional ESP-IDF component: System GSP App's `CONFIG_BROOKESIA_GSP_PREINSTALL_DEVICE_CONTROL` builds the package with the same script and adds it to the LittleFS image. Only suggest it when the user wants a preinstalled firmware image. Follow [Preinstall in the firmware](README.md#preinstall-in-the-firmware): enter the host directory, retain its existing `-B` for menuconfig/build/flash, and warn that a full flash replaces every USB-installed app and its data.

To update, build with a higher `--version` and install again. The host firmware does not change.

To uninstall, list the apps and pass the `app_id` of `brookesia.gsp.device_control`:

```bash
brookesia-usb call SystemCore ListApps '{}'
brookesia-usb call SystemCore UninstallApp '{"AppId": <id>}'
```

## What the build rejects

- `--gui-backend lvgl`: this package is GSP only. LVGL packages keep the JSON UI `profile.json` path.
- `--from-figma`: Figma export cannot produce GSP pages.
- A staged `manifest.json` or `profile.json` whose GUI backend is not `gsp`.
- A `profile.json` `root` that is not the compiled `res/device_control.gspb`.

## Preview limit

Follow [Customize](README.md#customize) to obtain simulator 1.4.0 and export `GSP_SIM_EXECUTABLE` before running `python3 tools/build_package.py --preview`. It does not run the JavaScript, call Display, or install the package. Say so when reporting a preview result.
