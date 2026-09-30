# ESP-Brookesia Settings App

* [中文版本](./README_CN.md)

## Overview

`brookesia_app_settings` is the built-in native settings app for System Core and System Super products.

## Supported interfaces

Settings keeps the same application identity and device service preferences across both GUI backends.

| Interface | Available pages |
| --- | --- |
| JSON / LVGL | The existing Settings screens, including Wi-Fi connection setup |
| GSP | Home, display brightness, and Wi-Fi setup on a 1024 × 600 display |

The GSP Wi-Fi page supports turning Wi-Fi on or off, scanning, refreshing, and browsing nearby networks. Each network shows its signal strength and security status. The page also reports the current connection and handles scanning, empty results, and service errors.

Network names support common simplified and traditional Chinese, accented Latin, Greek, and Cyrillic characters.

Tap a network to connect. For a personal network that requires a password, choose **Edit password**, enter the password, confirm, and choose **Connect**. Passwords are masked. Open networks and networks with saved credentials connect directly. Connection status comes from the Wi-Fi service, which also saves successful connections and handles reconnection.

The GSP keyboard supports printable ASCII characters. Enterprise network configuration, Chinese input, and saved-network management are not available. Sound, language, and theme controls are reserved for a later release.

For a complete application with a launcher and hardware setup, see the [System GSP App example](../../examples/system/gsp_app/README.md).

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/app/settings.html).

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
