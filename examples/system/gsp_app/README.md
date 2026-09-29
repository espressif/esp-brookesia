# System GSP App

[中文](./README_CN.md)

This example runs on the ESP32-P4 Function EV Board, on the 1024×600 screen. It opens a launcher. From there you can open Settings and change the screen brightness.

The interface is chosen in menuconfig:

- **GSP**, the default. Launcher, Settings home, and the brightness page.
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

Wi-Fi, sound, language, and theme are on the page and marked **Unavailable**. A line under the list says they are not in this version.

With LVGL, the board shows the startup screen and then the desktop. Open Settings from there. Brightness uses the same display service.

## Brightness

The slider is a percent from 0 to 100, and that percent is saved.

Closing Settings leaves the display service running. Opening Settings again reads the current percent. It does not write a new one.

A reboot brings back the saved percent, then turns the backlight on. The first boot, with nothing saved, starts at 90.

The panel lamp uses a narrower range than 0–100, so a hardware log can show a different number. The slider shows the saved percent.

## Startup log

Reset prints a memory log before the launcher appears. The example opens and closes Settings 22 times: 2 to warm up, then 20 measured. This does not change the saved brightness.

To try a brightness change from the service as well, edit `main/main_gsp.cpp`:

```cpp
constexpr bool RUN_BRIGHTNESS_PROBE = true;
```

Rebuild and flash. The probe changes the brightness, then writes the previous percent back before the memory log.

Look at the panel to confirm the screens. The serial log records the service and the memory numbers.
