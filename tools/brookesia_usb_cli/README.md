# Brookesia USB CLI

`brookesia-usb` controls the ESP-Brookesia USB service through the single CDC-ACM
channel provided by the USB Serial/JTAG controller. The protocol
version is `1`. The CLI does not require `/dev/ttyACM1`.

## Install

From this directory, install the CLI into the active Python environment:

```bash
python -m pip install -e .
```

The only host dependency is `pyserial`.

## Port selection

The CLI automatically selects an Espressif USB Serial/JTAG device by USB
identity and then verifies the device with the protocol `hello` command:

```bash
brookesia-usb devices
brookesia-usb status
```

When exactly one matching Serial/JTAG port is present, the CLI selects it
directly and performs `hello` only once for the requested command. When several
matching boards are present, discovery probes them using the configured
timeout. This avoids leaving a stale exclusive session when an application is
busy during startup.

To select the port explicitly, use `/dev/ttyACM0` (or the path reported by
`devices`):

```bash
brookesia-usb --port /dev/ttyACM0 status
```

The `--baudrate` option is accepted for pyserial compatibility; USB Serial/JTAG
does not use a physical baud rate. The default is `115200`. `--timeout` is the
per-read and write timeout in seconds, with a default of `10`:

```bash
brookesia-usb --port /dev/ttyACM0 --baudrate 115200 --timeout 10 status
```

## USB-UART Support

The CLI also supports USB-UART bridges (CP210x, CH340, FTDI, etc.) for devices
that only expose a USB-to-UART connection instead of native USB Serial/JTAG.
When a USB-UART device is used, the `--baudrate` parameter becomes significant
and must match the device's console baud rate.

Auto-discovery follows this order:
1. USB Serial/JTAG devices (VID:PID 0x303A:0x1001 or descriptive match)
2. USB-UART devices (common bridge VID:PID pairs or ttyUSB*/ttyACM* ports)

When a command needs a control session, the CLI sends `hello`, validates
`protocol_version: 1`, `transport` in `{"serial_jtag", "uart"}`, and
`session: "exclusive"`, then sends `goodbye` when it exits. Logs already in the
input buffer are discarded before `hello`; device logs are suppressed during
the control session so they cannot corrupt file frames.

When using USB-UART, note that:
- The `--baudrate` option must match the device's console baud rate
- Close `idf.py monitor` or other terminal programs before using the CLI
- The CLI does not automatically retry after disconnection
- Transport type can be verified via `brookesia-usb status` (look for the
  "transport" field in the JSON output)

To select the port explicitly, use `/dev/ttyACM0` (or the path reported by
`devices`):

```bash
brookesia-usb --port /dev/ttyACM0 status
```

For USB-UART devices, specify the appropriate port (e.g., `/dev/ttyUSB0`) and
matching baud rate:

```bash
brookesia-usb --port /dev/ttyUSB0 --baudrate 921600 status
```

When a command needs a control session, the CLI sends `hello`, validates
`protocol_version: 1`, `transport: "serial_jtag"`, and
`session: "exclusive"`, then sends `goodbye` when it exits. Logs already in the
input buffer are discarded before `hello`; device logs are suppressed during
the control session so they cannot corrupt file frames.

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
