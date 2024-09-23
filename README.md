[![Arduino Lint](https://github.com/espressif/esp-brookesia/actions/workflows/arduino_lint.yml/badge.svg)](https://github.com/espressif/esp-brookesia/actions/workflows/arduino_lint.yml) [![Version Consistency](https://github.com/espressif/esp-brookesia/actions/workflows/check_lib_versions.yml/badge.svg)](https://github.com/espressif/esp-brookesia/actions/workflows/check_lib_versions.yml)

**Latest Arduino Library Version**: [![GitHub Release](https://img.shields.io/github/v/release/espressif/esp-brookesia)](https://github.com/espressif/esp-brookesia/releases)

**Latest Espressif Component Version**: [![Espressif Release](https://components.espressif.com/components/espressif/esp-brookesia/badge.svg)](https://components.espressif.com/components/espressif/esp-brookesia)

# ESP-Brookesia

* [中文版本](./README_CN.md)

## Overview

ESP-Brookesia is a human-machine interaction development framework designed for AIoT devices. It aims to simplify the processes of user UI design and application development by supporting efficient development tools and platforms, thereby accelerating the development and market release of customers' HMI application products.

> [!NOTE]
> "[Brookesia](https://en.wikipedia.org/wiki/Brookesia)" is a genus of chameleons known for their ability to camouflage and adapt to their surroundings, which closely aligns with the goals of the ESP-Brookesia. This framework aims to provide a flexible and scalable UI solution that can adapt to various devices, screen sizes, and application requirements, much like the Brookesia chameleon with its high degree of adaptability and flexibility.

The key features of ESP-Brookesia include:

- Developed in C++, it can be compiled for `PC` or `ESP SoCs` platforms and supports `VSCode`, `ESP-IDF`, and `Arduino` development environments.
- Offers a rich set of standardized system UIs with support for dynamic UI style adjustments.
- Implements an app-based application management approach, ensuring UI isolation and coexistence across multiple apps, enabling users to focus on UI implementation within their target app.
- Application UIs are compatible with "[Squareline](https://squareline.io/) exported code" development methods.

The system UI functionality demonstration is as follows:

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_p4_function_ev_board_1024_600_2.gif" alt ="esp_ui_phone_p4_function_ev_board">
</div>

<div align="center">
    <a href="https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html">ESP32-P4-Function-EV-Board</a> running system UI - <a href="./docs/system_ui_phone_CN.md">Phone</a>
    <br>
    (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_1024_600_compress.mp4">Click to view the video</a>)
</div>
<br>

The functional block diagram of ESP-Brookesia is as follows, mainly consisting of the following components:

<div align="center">
    <img src="docs/_static/readme/block_diagram.png" alt="block_diagram" width="600">
</div>
<br>

- **System UI Core**: Implements the unified core logic of all system UIs, including app management, stylesheet management, event management, etc.
- **System UI Widgets**: Encapsulates common widgets for system UIs, including status bar, navigation bar, gesture, etc.
- **System UIs**: Implements various types of system UIs based on "System UI Core" and "System UI Widgets".
- **Squareline**: Contains multiple versions of *ui_helpers* files exported from "Squareline Studio" to avoid function name conflicts when used across multiple apps.
- **Fonts**: Contains the default fonts used by the system UIs.

## Usage

Please refer to the documentation - [How to Use](./docs/how_to_use.md).

## System UIs

Currently, ESP-Brookesia offers the following system UIs:

- [Phone](./docs/system_ui_phone.md)

## System UI Widgets

Please refer to the documentation - [System UI Widgets](./docs/system_ui_widgets.md).
