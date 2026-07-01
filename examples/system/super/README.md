# System Super Example

[中文版本](./README_CN.md)

This example demonstrates how to start a complete ESP-Brookesia System Super product shell. It integrates HAL, Display/Audio/Wi-Fi/HTTP/Storage/SNTP/Video/Device services, the GUI LVGL backend, the JavaScript runtime, and the built-in Settings, App Store, and Files apps to validate app lifecycle, resource packaging, system overlay, and launcher flows.

## 📑 Table of Contents

- [System Super Example](#system-super-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
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

Hardware resources are managed through the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component.

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

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, open a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)
