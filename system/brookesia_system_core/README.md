# ESP-Brookesia System Core

* [中文版本](./README_CN.md)

## Overview

`brookesia_system_core` is the app, GUI, runtime, storage, timer, and service bridge framework for products.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/system/core/index.html).

## GSP system home and shared input

ESP builds select GSP or LVGL in Kconfig. A target must enable
`BROOKESIA_SYSTEM_CORE_ENABLE_GSP_BACKEND` before GSP is available. LVGL builds
omit the GSP shell, scene resources and renderer dependency; application
lifecycle and service contracts remain shared.

When `System::Config` supplies `gsp_panel` and `gsp_touch`, System Core installs
and starts its built-in GSP system home. The home lists six visible applications
per page and refreshes the list after installation or removal. It restores saved
language/theme preferences and holds the SNTP binding for the system lifetime.

Native and JavaScript applications use the existing System Core keyboard and
message-dialog APIs. A modal temporarily suspends the application GSP session;
closing it resumes that session. Switching applications closes pending modal
requests through their request manager. A failed GSP stop retains the session and
display/touch references so it can be retried before device release.

The compiled home, keyboard and dialog scenes belong to this component. Theme
preparation and the common Latin font come from the declared `brookesia_gui_gsp`
dependency; using this component does not require Settings or the GSP example's
source files.

Linux GSP builds use `System::Config::gsp_desktop_display` instead of ESP display
devices, and run the same shell, app manager and modal logic. The optional
`app_support_check` reports a launch limitation while keeping the installed App
visible with its error. The Toolkit entrypoint is
`simulator/tools/brookesia_gsp_system/run.py`; its Linux guide describes package
discovery, persistent data and hardware capabilities that are not connected.

On Linux, blocking JavaScript `ShowKeyboard` and `ShowMessageDialog` calls return
an accepted request ID before renderer startup. The ID can be used to cancel a
pending request. Message dialogs share the same FIFO with native/system requests;
cancelled pending dialogs are not displayed. If display fails after acceptance,
the normal close event completes that ID once and the App retains the actual
error. This does not change the synchronous display path on ESP.

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
