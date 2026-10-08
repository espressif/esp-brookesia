# System GSP App

[中文](./README_CN.md)

This example runs on the ESP32-P4 Function EV Board, on the 1024×600 screen. It opens a launcher. From there you can open Settings, adjust brightness and sound, connect to Wi-Fi, and choose the interface language and theme.

The interface is chosen in menuconfig:

- **GSP**, the default. Launcher and Settings, including brightness, Wi-Fi, sound, language, and theme.
- **LVGL**. The existing desktop and the original Settings screens.

The project is set up for ESP32-P4 revision v3.1.

## Build and flash

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

GSP pages need the scene compiler. If the build stops and asks for it, install the tool or point `GSPC_EXECUTABLE` at a `gspc` binary:

```bash
python -m pip install -U esp-gsp-tools
```

## Switch the interface

```bash
idf.py menuconfig
```

Open `ESP-Brookesia: System Core Configurations` and change `GUI backend`, then save. Build and flash again:

```bash
idf.py build
idf.py -p <PORT> -b 921600 flash
```

Both choices use `partitions_16m.csv`. The LVGL options in `sdkconfig.defaults` apply after LVGL is selected.

Both interfaces use the same capability configuration and AudioPlayback service. The Sound test uses local PCM/WAV playback through the board's shared audio processor. High-level recording is disabled; raw codec access remains available. This example does not expose speech, streaming, or video features. The partition table and GSP font coverage are unchanged.

Compiled GSP scenes are compressed losslessly in flash. Opening an app expands its scene bundle into PSRAM; closing it releases that memory. Font coverage is unchanged. To use uncompressed scenes, disable **Compress embedded Brookesia GSP bundles** under **ESP-Brookesia: GUI GSP Configurations**.

For an existing checkout that disabled **Audio Device → Audio Processor**, enable it under **ESP-Brookesia: Hal Adaptor Configurations**. Disable **Audio Processor Recording** and keep the example's other capability defaults. Switching the GUI backend does not require changing these settings.

## Try it

After the memory log, the screen shows **Brookesia** and a **Settings** button.

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

## Wi-Fi

Open **Settings → Wi-Fi**. Turn Wi-Fi on to scan nearby networks. Use **Refresh** to scan again and **Previous / Next** to browse the results. The page shows each network's security, signal level, and RSSI, together with the current connection status.

**Back** returns to Settings. Leaving during a scan cancels that scan; reopening the page reads the current service state and starts a fresh scan when Wi-Fi is on. Turning Wi-Fi off also disconnects the current network. The switch preference is saved across restarts.

Tap a network to connect. Open networks and networks with saved credentials connect directly. For a new personal network, choose **Edit password**, enter the password, confirm, and choose **Connect**. The keyboard includes letters, numbers, symbols, space, and delete; passwords remain masked. **Cancel** closes the keyboard without starting a connection.

Wait for **Connected to network** before returning to the list. A failed attempt can be retried. Leaving the page after a connection has been submitted lets the Wi-Fi service finish it; reopening reads the current status. **Disconnect** ends the current connection without deleting saved credentials. Successful connections are saved and can reconnect after a restart.

Enterprise network configuration, Chinese input, and saved-network management are not provided. The password keyboard accepts printable ASCII characters.

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

The GSP launcher provides a Settings entry rather than the Super desktop. My device, time zone, diagnostics, and saved-network management remain in LVGL. Both interfaces use the same system preferences and hardware services.

## Startup log

Reset prints a memory log before the launcher appears. The example opens and closes Settings 22 times: 2 to warm up, then 20 measured. This does not change the saved brightness.

To try a brightness change from the service as well, edit `main/main_gsp.cpp`:

```cpp
constexpr bool RUN_BRIGHTNESS_PROBE = true;
```

Rebuild and flash. The probe changes the brightness, then writes the previous percent back before the memory log.

For Wi-Fi acceptance testing, enable **Run Settings Wi-Fi hardware acceptance probe at startup** under **Brookesia system GSP example** in menuconfig. The probe uses real scans and simulated touches to exercise refresh, scan cancellation, reopening, and Wi-Fi toggles, and logs memory over 20 measured cycles after warmup. Disable it for normal use.

The probe also injects empty and failed scan outcomes and sample multilingual names to check the corresponding screens. These labeled cases are synthetic; the subsequent refreshes and repeated scans use nearby networks.

For LVGL, the same example menu provides **Run LVGL hardware acceptance probe at startup**. It checks desktop navigation, Settings, brightness, Wi-Fi, and hardware discovery, captures screens, and records memory over 20 cycles. Both acceptance probes are disabled by default. The LVGL probe also checks Sound controls and opens the language and theme choices.

Each probe has an optional password-and-connection mode. Use a dedicated personal test network and send a single JSON object with `ssid` and `password` fields when the serial console reports that it is ready. The firmware does not echo credentials; use a serial sender with local echo disabled. GSP checks password entry for an unsaved network, then tests stopping and restarting Wi-Fi during a direct connection from the list. If credentials are already saved, it runs only the direct-connection checks. LVGL can remove the test network's saved entry through Settings before entering its password again. A successful connection saves the credentials through the normal Wi-Fi service.

The GSP probe has a **Sound and appearance checks** mode for page navigation, a short test tone, mute restoration, and memory sampling. It also injects label-update failures to check Back, Exit, and reopening. Its temporary serial console accepts `touch X Y`, `frame`, and `state` commands for visual checks; it is absent from normal firmware.

Look at the panel to confirm the screens. The serial log records the service and the memory numbers.
