# ESP32-P4-Function-EV-Board Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) with a `1024 x 600` resolution UI stylesheet.

## Getting Started

### Hardware Requirements

* An ESP32-P4-Function-EV-Board with a `1024 x 600` resolution LCD screen.

### ESP-IDF Required

- This example supports IDF release/v5.3 and later branches. By default, it runs on IDF release/v5.3.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

### Get the esp-brookesia Repository

To start from the examples in esp-brookesia, clone the repository to the local PC by running the following commands in the terminal:

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### Configuration

Run `idf.py menuconfig` and modify the esp-brookesia configuration.

### Testing the RainMaker app

To build and run the RainMaker application on the device:

1. **Configure your Wi-Fi router credentials** so the board connects automatically after power-on: run `idf.py menuconfig`, open **Phone Auto Wi-Fi** (visible when RainMaker support is enabled), and set **Wi-Fi SSID** and **Wi-Fi password** to match your **own** access point (same SSID/PSK as on the router). Ensure the network is reachable by the device (typically 2.4 GHz). Optionally adjust timezone under **Phone clock** and SNTP server under **Phone Auto Wi-Fi**.

Then run `idf.py build` and flash the firmware as usual. After reset, the device will try to join that Wi-Fi on boot so RainMaker features that need internet/DNS can work without a separate connection step.

For more information about the RainMaker app, see [README](../../../apps/brookesia_app_rainmaker/README.md).

## How to Use the Example

### Build and Flash the Example

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type `Ctrl-]`.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
