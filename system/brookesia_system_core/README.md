# ESP-Brookesia System Core

* [中文版本](./README_CN.md)

## Overview

`brookesia_system_core` is the app, GUI, runtime, storage, timer, and service bridge framework for products.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/system/core/index.html).

## USB Host Bridge

On supported ESP32-P4 targets, system core can bind the USB Serial/JTAG
service for exclusive host control. It supports service calls, protected file
uploads under the configured upload root, and BPK runtime app installation.
Unsupported manifest fields are reported as warnings and do not prevent a
package from being installed.

See the [USB service documentation](../../service/system/brookesia_service_usb/README.md)
and the [host CLI documentation](../../tools/brookesia_usb/README.md) for
configuration and usage details.

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
