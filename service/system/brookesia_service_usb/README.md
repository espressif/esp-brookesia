# ESP-Brookesia USB Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_usb` provides exclusive host control over a serial
transport on supported ESP targets: the USB Serial/JTAG CDC channel, or a UART
for boards that only route a USB-to-UART bridge.

It exposes typed service status and transfer events, JSON service calls, file
uploads, and BPK runtime app installation. The host protocol and CLI are
documented in [`brookesia_usb`](../../../tools/brookesia_usb/README.md).

## Environment Requirements

- An ESP target with either USB Serial/JTAG or a UART routed to the host.
- The selected transport reserved for the control session.
- `brookesia_service_helper` and `brookesia_service_manager` dependencies.

The service is ESP-only because it depends on the ESP-IDF USB Serial/JTAG and
UART drivers. Protocol checks run in the host CLI test suite.

## Add to Your Project

Add `espressif/brookesia_service_usb` to the project component dependencies and
select a transport. USB Serial/JTAG is the default on targets that expose it;
select UART for boards that only route a USB-to-UART bridge. System Core enables
the bridge by default when automatic USB service registration is enabled.

## Configuration

The main Kconfig options are:

- `BROOKESIA_SERVICE_USB_TRANSPORT`: `SERIAL_JTAG` (default where supported) or
  `UART`.
- `BROOKESIA_SERVICE_USB_ENABLE_AUTO_REGISTER`: register the service with
  `ServiceManager` automatically.
- `BROOKESIA_SERVICE_USB_UPLOAD_ROOT`: absolute root for host uploads and
  temporary files; defaults to `/littlefs/usb`.
- `BROOKESIA_SERVICE_USB_MAX_TRANSFER_SIZE`: maximum upload or BPK size;
  defaults to 8 MiB.
- `BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS`: host session timeout;
  defaults to 30 seconds.

UART transport options:

- `BROOKESIA_SERVICE_USB_UART_PORT`: ESP-IDF `uart_port_t`; defaults to `0`,
  the usual USB-to-UART console port on Espressif dev boards.
- `BROOKESIA_SERVICE_USB_UART_BAUDRATE`: used only when the selected port is
  not the console UART; a shared console port keeps
  `CONFIG_ESP_CONSOLE_UART_BAUDRATE` so console logs stay readable.
- `BROOKESIA_SERVICE_USB_UART_RX_BUFFER_SIZE`: defaults to 16384, matching the
  default frame payload so a delayed RX pump cannot drop a frame.
- `BROOKESIA_SERVICE_USB_UART_TX_BUFFER_SIZE`: defaults to 4096.

## Shared Console Port

A board without USB Serial/JTAG uses the same UART for firmware download,
console logs, and control sessions, but never at the same time:

- Firmware download runs in ROM download mode before the application starts.
- Console logs are emitted while idle and suppressed for the duration of a
  control session, then restored on `goodbye`, timeout, or disconnect.
- Non-JSON input received on a UART transport is discarded silently so terminal
  typing does not produce protocol error responses.

Do not enable an interactive console REPL (`esp_console`) on the same UART as
the control transport; both would compete for the RX stream.

## Security and Ownership

The USB connection is an exclusive trusted control boundary. JSON calls can
invoke registered service functions, so applications should not expose
destructive services to an untrusted physical host.

File uploads reject absolute paths, parent components, and symbolic-link
escapes. Existing files are protected by default; pass `overwrite` explicitly
to replace one. BPK installation uses the System Core package validation and
rollback flow.
