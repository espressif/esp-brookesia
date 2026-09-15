# ChangeLog

## v0.8.1 - 2026-09-10

### Enhancements:

- feat(service): add a UART transport so boards without a routed USB Serial/JTAG port can use host control
- feat(service): report the active transport in the service `Status` and the `hello` response
- feat(config): restore the Serial/JTAG RX/TX buffer Kconfig options and raise the RX default to 32 KiB to avoid frame payload CRC failures on large transfers
- feat(config): restore the `BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS` Kconfig option
- fix(transport): validate the configured UART port number before touching UART driver APIs

### Documentation:

- docs(readme): document the UART transport and the shared-console time-division behavior
- docs(readme): remove the unimplemented UART RTS/CTS flow-control options

## v0.8.0 - 2026-08-04

### Initial Release:

- Initial release of `brookesia_service_usb` with USB Serial/JTAG host control,
  service calls, file transfers, and BPK runtime app installation.
- Includes bounded command parsing, safe non-overwriting uploads, temporary-file
  cleanup, and documentation for the service configuration and host CLI.
