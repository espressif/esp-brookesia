# System GSP App

[中文](./README_CN.md)

Brookesia System runs one native app on the ESP32-P4 Function EV Board. The GUI backend is chosen in menuconfig:

- **GSP** (default): the 1024×600 Showcase (Command, Gallery, Lab), compiled into the firmware.
- **LVGL**: the existing Super desktop, with its resources written to the `littlefs_data` partition.

The checked-in profile links the GSP library for ESP32-P4 revision v3.1.

## Build and flash

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

## Switch backend

```bash
idf.py menuconfig
```

Open `ESP-Brookesia: System Core Configurations → GUI backend`, then save. Both choices use `partitions_16m.csv`. LVGL options in `sdkconfig.defaults` apply after LVGL is selected.

```bash
idf.py build
idf.py -p <PORT> -b 921600 flash
```

GSP scenes need the scene compiler. Menuconfig still opens without it. The GSP build then stops and tells you how to install it: `python -m pip install -U esp-gsp-tools` in the IDF Python environment, or set `GSPC_EXECUTABLE` to a `gspc` binary.

## Expected behavior

GSP: after reset the panel shows the Command scene. Swipe horizontally to Gallery and Lab. The clock hands move. `ALL SYSTEMS READY` opens quick settings. The display slider changes the backlight and the brightness text. `Exit` blanks the backlight for about 1.5 seconds, then the Command scene starts again. In Lab, the first `TRIGGER` after entering the scene shows the component page, including after leaving and entering Lab again.

LVGL: after reset the panel shows the startup screen, then the Super desktop with the Settings app.
