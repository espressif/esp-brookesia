---
name: gsp-js-device-control
description: Build, preview, pack, and install the Brookesia GSP JavaScript device-control app. Use when changing this package, compiling its GSP pages, or checking that the GUI backend and the JavaScript execution backend are set correctly.
---

# GSP JavaScript device control

The package lives in `examples/app/gsp_js_device_control`. It has two separate backend settings: the GUI backend is `gsp`, and the execution backend is `JavaScript`. Do not treat them as one choice.

## Build and install

Run from the app directory:

```bash
python3 tools/build_package.py --version 0.1.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
```

Wait for the launcher before running a `brookesia-usb` command after a reset, and close any serial monitor first.

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

`python3 tools/build_package.py --preview` shows the compiled pages in the simulator. It does not run the JavaScript, call Display, or install the package. Say so when reporting a preview result.
