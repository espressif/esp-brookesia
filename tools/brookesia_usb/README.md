# Brookesia USB CLI

`brookesia-usb` controls the ESP-Brookesia USB service over a single serial
transport: the CDC-ACM channel of USB Serial/JTAG, or a UART for boards that
only expose a USB-to-UART bridge. The protocol version is `1`.

## Install

```bash
python -m pip install brookesia-usb
```

The install pulls in `pyserial`. Check the installed version:

```bash
brookesia-usb --version
```

## Port selection

The CLI discovers the port automatically and verifies the device with the
protocol `hello` command. It prefers USB Serial/JTAG and falls back to a
USB-to-UART bridge when no Serial/JTAG port is present:

1. If exactly one USB Serial/JTAG candidate (Espressif VID:PID `0x303A:0x1001`
   or a matching port description) is present, it is used directly and is not
   probed during discovery; the protocol `hello` runs once when the command
   executes.
2. If several USB Serial/JTAG candidates are present, each is probed with
   `hello` until one reports the `serial_jtag` transport.
3. If no USB Serial/JTAG candidate is present, or all of them fail the probe,
   USB-to-UART candidates (common bridge VID:PID pairs, `/dev/ttyUSB*`, or
   non-Serial/JTAG `/dev/ttyACM*`) are probed, accepting either the
   `serial_jtag` or `uart` transport.

If a single USB Serial/JTAG port is present but is not the target device (for
example when both the USJ cable and the USB-to-UART bridge are connected), the
CLI selects it and reports an error; pass `--port` to select the port the
device actually uses.

```bash
brookesia-usb devices
brookesia-usb status
brookesia-usb --port /dev/ttyACM0 status
brookesia-usb --port /dev/ttyUSB0 --baudrate 921600 status
```

- `--port`: Serial/JTAG (`/dev/ttyACM*`) or USB-to-UART (`/dev/ttyUSB*`) path.
  Omitted means automatic discovery.
- `--baudrate`: ignored for USB Serial/JTAG, which has no physical baud rate.
  For a UART transport it must match the device console baud rate (for example
  `CONFIG_ESP_CONSOLE_UART_BAUDRATE`). Default `115200`.
- `--timeout`: per-read and write timeout in seconds. Default `10`.

When several matching devices are present, pass `--port` explicitly using a path
reported by `devices`. A single Serial/JTAG CDC configuration normally exposes
only `/dev/ttyACM0`; the absence of `/dev/ttyACM1` is expected.

Opening a USB-to-UART port resets the device, so the CLI retries `hello` until
the service is ready. Close `idf.py monitor`, minicom, or any other program that
reads the same port before running a command.

## Control sessions

Commands that need a control session send `hello` and validate protocol version
`1`, the transport reported by the device (`serial_jtag` or `uart`), and the
`exclusive` session state, then send `goodbye` on exit. Device logs are
suppressed for the duration of the session so they cannot corrupt JSON responses
or file frames.

## Commands

### List devices

List all serial devices and identify the Serial/JTAG candidate:

```bash
brookesia-usb devices
```

This command does not open a control session and returns non-zero when no serial
device is found.

### Get service status

Query the USB service, transport connection, session state, and active transfer:

```bash
brookesia-usb status
brookesia-usb --port /dev/ttyACM0 status
```

### Call a service function

Call any registered Brookesia service function whose arguments can be
represented by the service JSON schema. Use `Manager` to discover the
available services and functions:

```bash
brookesia-usb call Manager GetServiceNames '{}'
brookesia-usb call Manager GetServiceSchema '{"Name":"Storage"}'
brookesia-usb call SystemCore GetSystemInfo '{}'
brookesia-usb call SystemCore GetStorageLayout '{}'
brookesia-usb call Storage FSStat '{"Path":"/littlefs"}'
brookesia-usb call Storage FSList '{"Path":"/littlefs"}'
```

The JSON argument must be an object and parameter names and types must match
the function schema. ServiceManager applies required-parameter, default-value,
unknown-parameter, and type validation on the device. This includes service
functions that modify storage, such as remove and rename; the USB Serial/JTAG
connection is treated as a trusted control boundary.

Functions that require a `RawBuffer` argument cannot be called through this
JSON interface because a host pointer is not valid in device memory. Use
`put` or `install` for file and package data instead.

Calling the `Usb` service itself is rejected to prevent recursive calls into
the active USB control session.

### Upload a file

Upload a local file to a relative path under the configured device upload root
(`/littlefs/usb` by default):

```bash
brookesia-usb put ./logs/session.bin logs/session.bin
```

The CLI calculates the file size and SHA-256 digest, sends CRC-protected
16 KiB-or-smaller frames, waits for an ACK after every data frame, and reports
progress on stderr.

Absolute paths, `..` path components, symbolic-link escapes, and destinations
outside the upload root are rejected. Existing files are not overwritten by
default. To explicitly replace an existing file:

```bash
brookesia-usb put ./config/device.json config/device.json --overwrite
```

The device accepts files up to the configured maximum, initially 8 MiB. A
transfer is first written to a temporary file and is renamed or handed to the
system bridge only after size and SHA-256 verification succeeds.

### Install a BPK package

Send a complete BPK package to the device for validation and installation:

```bash
brookesia-usb install ./build/my_app.bpk
```

The package is staged in the USB temporary directory. The system_core bridge
performs the existing manifest, ZIP path-safety, staging, replacement, and
rollback checks. The host cannot select an application directory directly.

The CLI never retries an installation after a disconnect or an ambiguous
failure. Re-run the command only after checking the device status.

### Abort a transfer

Abort the active transfer by request ID:

```bash
brookesia-usb abort 42
```

`abort` is an emergency command and does not start a new control session. It
removes the temporary file and returns an `aborted` response when request `42`
is active. An unknown request ID returns a device error and a non-zero exit
status.

## Errors and exit status

The CLI prints errors to stderr and returns `0` only after the requested device
operation succeeds. It returns `1` for transport, protocol, validation, or
device errors. Common device error codes include:

- `invalid_command`: malformed JSON or unsupported operation;
- `busy`: another control session or transfer is active;
- `bad_frame`: invalid CRC, frame type, or sequence;
- `size_mismatch` / `hash_mismatch`: declared metadata does not match data;
- `path_denied`: unsafe path or overwrite not explicitly enabled;
- `storage_full`: temporary storage cannot be created or written;
- `install_failed`: system_core rejected or failed to install the package;
- `timeout`: no host activity within the configured timeout;
- `aborted`: the host or device cancelled the transfer.

If the board is not found, first check the Serial/JTAG Type-C cable and inspect
the available devices:

```bash
ls /dev/ttyACM*
brookesia-usb devices
```

Close `idf.py monitor`, minicom, or another program that is reading
`/dev/ttyACM0` before running a control command; Serial-JTAG has one shared
CDC channel and cannot safely multiplex competing readers.

The absence of `/dev/ttyACM1` is expected for the Serial-JTAG single-CDC
configuration.

## Development

Install from a source checkout together with the test extras, then run the lint
and unit tests:

```bash
python -m pip install -e "tools/brookesia_usb[test]"
python -m flake8 --config=.flake8 tools/brookesia_usb/src tools/brookesia_usb/tests
python -m pytest tools/brookesia_usb/tests
```

## Release

The package is published manually with `twine`.

1. Bump `version` in `tools/brookesia_usb/pyproject.toml`.
2. Build and validate the distribution:

   ```bash
   python -m pip install --upgrade build twine
   python -m build tools/brookesia_usb
   python -m twine check tools/brookesia_usb/dist/*
   ```

3. Upload with a PyPI API token (or an interactive prompt):

   ```bash
   python -m twine upload tools/brookesia_usb/dist/*
   ```

4. Tag the release, for example `usb-cli-v0.2.0`.

