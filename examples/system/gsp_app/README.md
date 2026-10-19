# System GSP App

[中文](./README_CN.md)

This example runs on the ESP32-P4 Function EV Board, on the 1024×600 screen. It opens a launcher. From there you can open Settings, adjust brightness and sound, connect to Wi-Fi, and choose the interface language and theme.

Two backend profiles are supported:

- **GSP**, the default. Launcher and Settings, including brightness, Wi-Fi, sound, language, and theme.
- **LVGL**. The existing desktop and the original Settings screens.

The default P4 configuration targets chip revision v3.1 or later. P4 v1.3 needs the [older-P4 configuration](#p4-revision-v13) below.

You can also install apps over USB while the board is running. They appear on the launcher next to Settings; see [Installable apps](#installable-apps).

## Build and flash

Activate the supported ESP-IDF environment (6.0 or later) and run commands from this example directory. For GSP, first install `esp-gsp-tools` with `python -m pip install -U esp-gsp-tools`, or pass `-D GSPC_EXECUTABLE=/path/to/gspc` with a compatible compiler; see the [GSP application setup](../../app/gsp_js_device_control/README.md#requirements). LVGL does not need the GSP compiler.

Use a new output directory for each board/backend combination. Existing `sdkconfig` choices take precedence over defaults; changing only the defaults does not convert an existing output to another board.

### P4X (revision v3.1 or later)

Generate the P4X board inputs, then choose one backend:

```bash
idf.py gen-bmgr-config -b esp32_p4x_function_ev
idf.py -B build-gsp -D IDF_TARGET=esp32p4 -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build-lvgl -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.lvgl' build
```

`sdkconfig.defaults.esp32p4` and `sdkconfig.ci.lvgl` select the revision ≥3 path with a minimum of v3.1. The CI file `sdkconfig.ci.board.esp32_p4_function_ev` also targets v3.1 despite its historical board name; it is not an old-P4 profile.

### P4 revision v1.3

Generate the old-P4 board inputs:

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
```

For GSP, create an output-local defaults file and load it last to override the v3.1 settings in `sdkconfig.defaults.esp32p4`:

```bash
mkdir -p build-gsp-p4-v1
cat > build-gsp-p4-v1/board.defaults <<'EOF'
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
EOF
idf.py -B build-gsp-p4-v1 -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;build-gsp-p4-v1/board.defaults' build
```

For LVGL, use a separate output and select LVGL in its defaults file:

```bash
mkdir -p build-lvgl-p4-v1
cat > build-lvgl-p4-v1/board.defaults <<'EOF'
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
CONFIG_BROOKESIA_GUI_BACKEND_GSP=n
CONFIG_BROOKESIA_GUI_BACKEND_LVGL=y
EOF
idf.py -B build-lvgl-p4-v1 -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;build-lvgl-p4-v1/board.defaults' build
```

Before flashing, check the selected output's `sdkconfig`: `CONFIG_ESP_BOARD_NAME="esp32_p4_function_ev"`, `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, and `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360`. Do not force-flash firmware requiring revision ≥3 onto v1.3.

### Flash and monitor

Use the same `-B` as the build. For example, after the P4X GSP build:

```bash
idf.py -B build-gsp -p <PORT> -b 921600 flash monitor
```

Replace `<PORT>` with the board's serial port; use `build-gsp-p4-v1`, `build-lvgl`, or `build-lvgl-p4-v1` for the other builds. Exit the monitor with `Ctrl+]`. A full flash replaces LittleFS, including installed apps and their data; back up anything you need first.

### Output directories

Each output owns `sdkconfig`, `dependencies.lock`, `dependencies/managed_components`, generated headers/bundles, and `littlefs_stage`. With no explicit defaults or external `SDKCONFIG`, a new output imports the project's existing sdkconfig once. Explicit defaults start a fresh configuration and include the generated board defaults first. Existing project configuration and downloads are preserved; each output's separate dependency cache uses additional disk space.

Board generation under `components/gen_bmgr_codes` is shared source input. Regenerate matching inputs when switching boards or ESP-IDF versions; back up custom inputs first. Use separate source checkouts for concurrent work on different boards. Give caller-specified external `SDKCONFIG` or resource-stage paths distinct locations too. The [CI output audit](../../../.gitlab/tools/README.md#build_appspy) checks the standard build-local layout.

## Switch the interface

Choose the corresponding output above; reusing an output for another backend is rejected. Keep `-B build-gsp` or `-B build-lvgl` on subsequent `menuconfig`, build and flash commands. To change settings within one backend:

```bash
idf.py -B build-gsp menuconfig
idf.py -B build-gsp build
```

Both profiles use `partitions_16m.csv`. The LVGL startup helpers are shared under `examples/system/common/lvgl`; neither backend reads sources or outputs from the Super example.

### Filesystem resources

The `littlefs_data` image is built from `<selected-build>/littlefs_stage`; this example no longer uses the source-tree `littlefs/` directory for that image.

With GSP selected, the image contains the sound-check WAV, Settings translations used for the application name, and any GSP apps configured for preinstallation. GSP scenes and their fonts are supplied by GSP bundles; the image does not include the LVGL desktop (`system/super`), shared LVGL fonts (`system/fonts`), or the Settings LVGL screens. With LVGL selected, the image includes the desktop resources and the complete Settings package.

### Flashing after a backend change

`idf.py app-flash` updates the application partition only; it does not update `littlefs_data`. After a backend change, build its output and use the full [flash command](#flash-and-monitor) above to deploy the matching filesystem resources.

Both interfaces use the same capability configuration and AudioPlayback service. The Sound test uses local PCM/WAV playback through the board's shared audio processor. High-level recording is disabled; raw codec access remains available. This example does not expose speech, streaming, or video features.

Compiled GSP scenes are compressed losslessly in flash. Opening an app expands its scene bundle into PSRAM; closing it releases that memory. To use uncompressed scenes, disable **Compress embedded Brookesia GSP bundles** under **ESP-Brookesia: GUI GSP Configurations**.

For an existing checkout that disabled **Audio Device → Audio Processor**, enable it under **ESP-Brookesia: Hal Adaptor Configurations**. Disable **Audio Processor Recording** and keep the example's other capability defaults. Switching the GUI backend does not require changing these settings.

## Try it

After startup, the public System Core home lists installed applications and provides a **Settings** entry.

```text
Settings
Display
move the brightness slider
Back
Exit
Settings
```

**Back** returns to the Settings home page. **Exit** returns to the launcher. Opening Settings again keeps the brightness you set.

Open **Sound**, **Language**, or **Theme** from the Settings home page. Theme is also accessible from Display.

With LVGL, the board shows the startup screen and then the desktop. Open Settings from there. Brightness uses the same display service.

## Installable apps

The system home shows six applications per page; use **Previous / Next** for additional pages. Install, update, and uninstall them over USB while the board is running. No firmware rebuild is needed, and installed apps stay after a restart.

```bash
python -m pip install brookesia-usb
brookesia-usb install <package>.bpk
```

A package installs only on the interface it was built for: a GSP package needs the GSP interface. [Device Control](../../app/gsp_js_device_control/README.md) is a ready-made example that reads and changes the brightness.

Installed apps live in the LittleFS partition. A full `idf.py flash` rewrites that partition and removes them along with their data; use `idf.py app-flash` to update the firmware and keep them.

To build Device Control into the firmware instead, enable **Brookesia system GSP example → Preinstall the Device Control app** in menuconfig and run a full `idf.py flash`. It is off by default.

## Wi-Fi

Open **Settings → Wi-Fi**. Turn Wi-Fi on to scan nearby networks. Use **Refresh** to scan again and **Previous / Next** to browse the results. The page shows each network's security, signal level, and RSSI, together with the current connection status.

**Back** returns to Settings. Leaving during a scan cancels that scan; reopening the page reads the current service state and starts a fresh scan when Wi-Fi is on. Turning Wi-Fi off also disconnects the current network. The switch preference is saved across restarts.

Tap a network to connect. Open networks and networks with saved credentials connect directly. For a new personal network, choose **Edit password**, enter the password, confirm, and choose **Connect**. The keyboard includes letters, numbers, symbols, space, and delete; passwords remain masked. **Cancel** closes the keyboard without starting a connection.

Wait for **Connected to network** before returning to the list. A failed attempt can be retried. Leaving the page after a connection has been submitted lets the Wi-Fi service finish it; reopening reads the current status. **Disconnect** ends the current connection without deleting saved credentials. Successful connections are saved and can reconnect after a restart.

Choose **Saved networks**, select the target SSID, then choose **Forget** to remove its saved credentials. Removing the current network also disconnects it. **Nearby networks** returns to scan results. Enterprise network configuration and Chinese input are not provided. The public system keyboard accepts printable ASCII passwords.

The P4 board uses its Wi-Fi companion chip through ESP-Hosted. If the service is unavailable, check the board configuration and companion firmware. A failed scan can be retried with **Refresh**; an empty result means no named networks were found.

## Brightness

The slider is a percent from 0 to 100, and that percent is saved.

Closing Settings leaves the display service running. Opening Settings again reads the current percent. It does not write a new one.

A reboot brings back the saved percent, then turns the backlight on. The first boot, with nothing saved, starts at 90.

The panel lamp uses a narrower range than 0–100, so a hardware log can show a different number. The slider shows the saved percent.

## Sound

Connect a speaker to the board, then open **Settings → Sound**. Adjust the volume or toggle mute. **Test sound** plays a short tone at the current settings; it remains silent when muted. If another sound is playing or paused, wait for it to finish before trying the tone. Volume and mute are saved by the playback service and restored after a restart. An unavailable device or a failed update is reported on the page.

Use a full `idf.py flash` after building this example so the filesystem contains the test clip. The clip is generated locally during the build.

## Language and theme

Choose **English** or **Chinese (Simplified)** in Language, or **Light** or **Dark** in Theme. Select **Restart now** to apply a saved choice, or **Later** to keep working with the current appearance. Selecting the current option again replaces any pending choice for that setting.

After restart, the selected language and theme apply to the launcher and all Settings pages, including Wi-Fi status and the password keyboard. Appearance selection does not change saved brightness, sound, or Wi-Fi settings. The keyboard continues to accept printable ASCII passwords; changing its labels does not enable Chinese input.

## Remaining interface differences

The host binds the SNTP service for as long as it runs, as Super does, so the wall clock is set from the network once Wi-Fi is up, whichever app opens first. Without a saved Wi-Fi network the clock stays unset and apps such as Clock show 1970.

System Core provides the GSP system home and public keyboard/message dialogs; this example configures the devices and starts the system. Settings includes My device, time zone with live SNTP state, and selection-based saved-network removal. Advanced diagnostics remain in LVGL. Both interfaces use the same system preferences and hardware services.

## Diagnostics

Normal startup goes straight to the launcher. The checks below are for development and are off by default. Enable them under **Brookesia system GSP example** in menuconfig.

| Option | What it does |
| --- | --- |
| **Open and close Settings repeatedly at startup to log heap use** | Opens and closes Settings 22 times, 2 to warm up and 20 measured, and logs memory before the launcher appears |
| **Log heap across short start and exit cycles of the installed GSP JS app** | Does the same for the installed Device Control app with 5 cycles |
| **Let USB callers inject touches and print the screen** | Adds a `GspProbe` service with `Tap`, `Drag`, `Component`, and `Frame`, callable through `brookesia-usb call`. `Frame` prints the framebuffers to the console after the USB command ends. Startup and app installation are unchanged |
| **Run Settings Wi-Fi hardware acceptance probe at startup** | Uses real scans and simulated touches to exercise refresh, scan cancellation, reopening, and Wi-Fi toggles, and logs memory over 20 cycles. It also shows synthetic empty and failed scan results and multilingual names |
| **Run Settings sound and appearance checks instead of scan checks** | Available with the Wi-Fi probe. Checks page navigation, the test tone, mute restoration, and memory, and adds temporary serial commands `touch X Y`, `frame`, and `state` |
| **Run password and connection checks instead of scan checks** | Available with the Wi-Fi probe. Tests password entry and connecting to a network |
| **Run LVGL hardware acceptance probe at startup** | For the LVGL interface: checks desktop navigation, Settings, brightness, Wi-Fi, sound, language, and theme, captures screens, and logs memory over 20 cycles. **Run LVGL password and connection checks instead of desktop checks** adds the connection test |

The connection checks need a dedicated personal test network. When the serial console reports that it is ready, send one JSON object with `ssid` and `password` fields. The firmware does not echo the credentials, so turn local echo off in your serial sender. A successful connection is saved by the normal Wi-Fi service.

To also change the brightness from the service at startup, set `RUN_BRIGHTNESS_PROBE` to `true` in `main/main_gsp.cpp`. The probe writes the previous percent back afterwards.

Look at the panel to confirm the screens. The serial log records service state and memory.
