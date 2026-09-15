# System Super Example

[中文版本](./README_CN.md)

This example demonstrates how to start a complete ESP-Brookesia System Super product shell. The default build integrates HAL, Display/Audio/Wi-Fi/HTTP/Storage/SNTP/Video/Device services, NES emulation, Coze and Xiaozhi agents, the GUI LVGL backend, the JavaScript runtime, and the built-in Settings, App Store, and Files apps.

## 📑 Table of Contents

- [System Super Example](#system-super-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
  - [⚡ Build Performance](#-build-performance)
  - [📊 Build Analysis](#-build-analysis)
  - [🚀 Quick Start](#-quick-start)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🧭 **System shell experience**: Starts the System Super Shell with desktop background, status bar, App Launcher, and system overlay
- 📦 **Built-in app integration**: Preinstalls Settings, App Store, and Files to validate native app install, launch, and restore flows
- 🧩 **Framework integration**: Combines Service Manager, GUI LVGL, Runtime Manager, System Core/Super, and HAL board resources
- 🗂️ **Resource packaging**: Stages System Super resources, fonts, images, and the LittleFS partition through the build flow

## 🚩 Getting Started

### Hardware Requirements

This example currently supports the following development boards:

- `esp32_p4x_function_ev`
- `esp32_s31_korvo1`
- `esp_mosaico_v1_0`

Hardware resources are managed through the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component.

On ESP-Mosaico, expansion modules are optional and are not required to start System Super. See the [ESP-Mosaico V1.0 adaptation guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico) for the currently supported modules and camera-preview limitations.

> [!TIP]
> This example supports using an SD Card as an external storage volume. Please insert the SD Card before powering on the board.
> When using the SD Card, "App Store" and other apps will default to scanning it for storage or specific file directories, so it is recommended to use it to expand the system storage space.

### Development Environment

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

## 🔨 How to Use

<a href="https://espressif.github.io/esp-brookesia/index.html">
  <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
</a>

Please refer to [ESP-Brookesia Programming Guide - How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects).

### Console and Host-Control Port Pairing

The USB service host control (BPK install, file transfer) and the console log
output must use the same physical port, so that the host talks to the device and
reads its logs over one cable. The two settings are independent Kconfig options
and must always be switched together:

| Port | Console | USB service transport |
| --- | --- | --- |
| USB-to-UART bridge (UART0) | `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` | `CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_UART=y`, `..._UART_PORT=0` |
| USB Serial/JTAG (USJ) | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` | `CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_SERIAL_JTAG=y` |

The per-target `sdkconfig.defaults.esp32p4` and `sdkconfig.defaults.esp32s31`
files contain both profiles; enable exactly one. The default differs per target:

- **ESP32-P4** defaults to **USB Serial/JTAG**, matching the P4 function board
  defaults and the CI target-test environments. Switch to the USB-to-UART
  profile when the host connects through the on-board USB-to-UART bridge.
- **ESP32-S31** (Korvo1) defaults to **USB-to-UART**, matching the board's
  default UART0 console.

The ESP32-P4 exposes both ports, so the port must currently be selected at build
time; runtime auto-detection is not implemented yet.

The `brookesia-usb` host CLI auto-discovers the port, so no host change is
needed when the pairing is switched.

## ⚡ Build Performance

The example builds the complete dependency set by default. It enables ccache and limits simultaneous compile edges for first-party Brookesia C++ targets and `esp-boost`, whose template-heavy translation units have the highest peak compiler memory use. The same tuning is available to all first-party examples and test apps.

| CMake option | Default | Description |
| --- | --- | --- |
| `CCACHE_ENABLE` | `ON` | Reuses compiler results in repeat builds |
| `BROOKESIA_CXX_JOBS` | `6` | Ninja job-pool size for C++-bearing Brookesia and `esp-boost` targets; set to `0` to disable |
| `BROOKESIA_FAST_COMPILE` | `OFF` | Uses `-g1` for Brookesia C++ sources to reduce debug-information generation during local development |
| `BROOKESIA_COMPILE_TUNING_INCLUDE_ESP_BOOST` | `ON` | Include `esp-boost` in the shared C++ job pool when the target exists |

For a memory-constrained workstation, reduce `BROOKESIA_CXX_JOBS`, for example:

```bash
idf.py -B build -D BROOKESIA_CXX_JOBS=3 build
```

The existing GCC IPA-clone compile options remain enabled independently of the job-pool setting. The old `BROOKESIA_SUPER_CXX_JOBS` and `BROOKESIA_SUPER_FAST_COMPILE` names remain accepted as deprecated aliases.

## 📊 Build Analysis

After configuration or compilation, generate a report from the build metadata:

```bash
python3 tools/analyze_build.py build
```

The tool reads `project_description.json`, `compile_commands.json`, `.ninja_log`, and Ninja dependency metadata. It writes only `brookesia_build_analysis.json` and `brookesia_build_analysis.md` inside the selected build directory. Use `--skip-deps` when only translation-unit and Ninja timing statistics are needed.

## 🚀 Quick Start

After flashing the firmware, the device initializes general services, starts the display backend, and enters the System Super Shell. A normal boot shows the App Launcher and system status bar; tapping a built-in app icon starts that app, and a bottom-edge upward gesture returns from a foreground app to the launcher.

The following serial log indicates that system initialization and the example smoke flow have completed:

```text
=== System Example Completed ===
```

## 🔍 Troubleshooting

**Blank screen after boot**

Confirm that the LittleFS partition was flashed and that `littlefs_data.bin` was generated. System Super relies on the build flow to stage `system/super`, `system/fonts`, and `apps` resources.

**Shell or built-in app launch failure**

Confirm that `brookesia_system_super`, `brookesia_system_core`, `brookesia_gui_lvgl`, related service components, and built-in app components stay on the same release line.

**Cannot exit a foreground app with the gesture**

Confirm that the Display service has started and reports touch gestures, and that the gesture starts at the bottom edge and moves upward.

**No application logs on the USB-to-UART bridge**

The bootloader ROM always prints on UART0, but the second-stage bootloader and
the application print on the configured console. If the console is set to USB
Serial/JTAG, a monitor on the USB-to-UART bridge shows only the ROM banner and
then goes silent. This usually means the console and the host-control transport
are paired to different ports: enable the USB-to-UART profile described in
[Console and Host-Control Port Pairing](#console-and-host-control-port-pairing).

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, open a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)
