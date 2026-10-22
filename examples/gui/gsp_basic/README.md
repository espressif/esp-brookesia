# GSP Basic

[中文](./README_CN.md)

Board example that runs one ESP-GSP scene on the ESP32-P4 Function EV Board through Brookesia HAL. The scene source is `main/scenes/home.json`. This firmware does not load Brookesia JSON UI and does not install a `.bpk`.

The checked-in profile links the GSP library for ESP32-P4 revision v3.1.

## Build and flash

Activate ESP-IDF 6.0 or later and run from this example directory. In the active IDF Python environment (Python 3.10 or later), install the GSP tool manager:

```bash
python -m pip install -U esp-gsp-tools
```

CMake obtains the compiler version pinned by the ESP-GSP component. The first build needs network access unless that compiler is already cached.

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

Replace `<PORT>` with the serial port identified for the connected Function EV Board. Exit the monitor with `Ctrl+]`. After reset the panel shows `Brookesia · GSP`, a build id, a moving activity bar, and `+1` / `Reset` buttons.
