# ESP-Brookesia GUI LVGL Backend

* [中文版本](./README_CN.md)

## Overview

`brookesia_gui_lvgl` is the LVGL backend and image packaging helper set for GUI Interface documents.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/gui/lvgl/index.html).

## How to Use

### Runtime Image Formats

The LVGL backend supports LVGL ``.bin``, PNG, and JPEG files referenced by JSON UI ``imageSet`` assets. JPEG
resources use ``.jpg`` or ``.jpeg`` extensions, are validated and preloaded into a reference-counted cache, and can
provide their dimensions from the JPEG frame header.

On ESP, enable ``CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER``. On PC and WASM, enable ``LV_USE_TJPGD`` and
``LV_USE_FS_MEMFS`` in the LVGL configuration. Loading a JPEG returns an explicit path-based error when the decoder is
unavailable or the file is invalid.

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
