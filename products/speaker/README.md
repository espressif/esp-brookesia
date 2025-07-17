# EchoEar Factory Example

[中文版本](./README_CN.md)

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://espressif.github.io/esp-brookesia/launchpad.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

This example demonstrates how to run the ESP-Brookesia Speaker on the EchoEar development board, and supports voice wake-up and voice interaction with the Coze agent.

Please refer to the [EchoEar (Meow Companion) User Guide](https://espressif.craft.me/1gOl65rON8G8FK) for product information and firmware usage instructions.

## Getting Started Guide

### Hardware Requirements

* An EchoEar development board (with SD card inserted).

### ESP-IDF Requirements

- This example supports IDF release/v5.5 and later branches. By default, it runs on IDF release/v5.5.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) to set up your development environment. **We strongly recommend** you [build your first project](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and ensure your environment is set up correctly.

### Getting the esp-brookesia Repository

To get started with the esp-brookesia example, run the following command in the terminal to clone the repository to your local computer:

```
git clone https://github.com/espressif/esp-brookesia.git
```

### Configuration

Run `idf.py menuconfig` > `Example Configuration` to configure COZE related parameters.

Please refer to the [EchoEar (Meow Companion) User Guide - Getting Started - Developer Mode](https://espressif.craft.me/1gOl65rON8G8FK) for relevant configuration parameters.

## How to Use the Example

### Build and Flash the Example

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type `Ctrl-]`.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Known Issues

- Device repeatedly restarts after power-on or reset: This is a known issue with the PCBA version silk-screened as `v1.0`. You need to remove the `C1`, `C10` capacitors from the `Baseboard` and the `C4` capacitor from the `CoreBoard`, three `10uF` capacitors in total. These capacitors affect the power supply capability of the VCC power domain on the board and may cause the device to restart randomly.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
