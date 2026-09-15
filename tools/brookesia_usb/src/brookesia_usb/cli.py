"""Command-line client for the ESP-Brookesia USB Serial/JTAG control port."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

from . import __version__
from .protocol import (
    PROTOCOL_VERSION,
    BinaryFrame,
    FrameType,
    command,
    encode_frame,
    iter_file_frames,
    sha256_file,
)


SERIAL_JTAG_VID = 0x303A
SERIAL_JTAG_PID = 0x1001
MIN_DISCOVERY_TIMEOUT_SECONDS = 3.0
HELLO_ATTEMPT_TIMEOUT_SECONDS = 1.0
HELLO_RETRY_DEADLINE_SECONDS = 30.0
HELLO_MAX_ATTEMPTS = 30
TRANSFER_FINALIZE_TIMEOUT_SECONDS = 120.0


def _serial_module():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError as error:  # pragma: no cover - depends on optional host install
        raise RuntimeError("pyserial is required; install with: pip install pyserial") from error
    return serial, list_ports


def list_devices() -> int:
    _, list_ports = _serial_module()
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found")
        return 1
    for port in ports:
        vid_pid = ""
        if port.vid is not None and port.pid is not None:
            vid_pid = f" vid=0x{port.vid:04x} pid=0x{port.pid:04x}"
        marker = " [serial-jtag]" if is_serial_jtag_port(port) else ""
        print(f"{port.device}{marker}{vid_pid} {port.description or ''}".rstrip())
    return 0


class UsbClient:
    def __init__(self, port: str, baudrate: int, timeout: float) -> None:
        serial, _ = _serial_module()
        self.port = port
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=timeout, write_timeout=timeout)
        try:
            self.serial.dtr = False
            self.serial.rts = False
        except (OSError, AttributeError):
            pass
        self.timeout = timeout
        self.request_id = 1
        self.session_active = False
        self._hello_request_pending = False

    def __enter__(self) -> "UsbClient":
        return self

    def __exit__(self, *_: Any) -> None:
        if self.session_active:
            try:
                self.goodbye()
            except (OSError, RuntimeError, TimeoutError):
                pass
        elif self._hello_request_pending:
            self._send_pending_hello_cleanup()
        self.serial.close()

    def next_request_id(self) -> int:
        value = self.request_id
        self.request_id += 1
        return value

    def write_command(self, op: str, **fields: Any) -> int:
        request_id = self.next_request_id()
        self.serial.write(command(op, request_id, **fields))
        return request_id

    def read_json(self) -> dict[str, Any]:
        while True:
            line = self.serial.readline()
            if not line:
                raise TimeoutError("timed out waiting for device response")
            try:
                value = json.loads(line.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                if not self.session_active:
                    continue
                raise RuntimeError(f"invalid device response: {line!r}") from error
            if not isinstance(value, dict):
                if not self.session_active:
                    continue
                raise RuntimeError("device response is not an object")
            return value

    def wait_for(self, request_id: int, *operations: str) -> dict[str, Any]:
        while True:
            response = self.read_json()
            if response.get("request_id") != request_id:
                continue
            if response.get("op") in operations:
                return response

    def hello(self) -> dict[str, Any]:
        original_timeout = self.serial.timeout
        deadline = time.monotonic() + max(self.timeout, HELLO_RETRY_DEADLINE_SECONDS)
        response: dict[str, Any] | None = None
        try:
            self.serial.timeout = min(original_timeout, HELLO_ATTEMPT_TIMEOUT_SECONDS)
            attempts = 0
            while response is None:
                attempts += 1
                self.serial.reset_input_buffer()
                self._hello_request_pending = True
                request_id = self.write_command("hello")
                try:
                    response = self.wait_for(request_id, "hello", "error")
                except TimeoutError:
                    if time.monotonic() >= deadline or attempts >= HELLO_MAX_ATTEMPTS:
                        raise
                    continue
                if response.get("ok") is False and response.get("error_code") == "busy":
                    if attempts == 1:
                        self._hello_request_pending = False
                        self._raise_for_error(response)
                    if time.monotonic() >= deadline or attempts >= HELLO_MAX_ATTEMPTS:
                        self._hello_request_pending = False
                        self._raise_for_error(response)
                    self._send_pending_hello_cleanup()
                    response = None
        finally:
            self.serial.timeout = original_timeout
        self._hello_request_pending = False
        self._raise_for_error(response)
        if response.get("protocol_version") != PROTOCOL_VERSION:
            raise RuntimeError("unsupported USB protocol version")
        transport = response.get("transport")
        if transport not in ("serial_jtag", "uart"):
            raise RuntimeError(f"device is not using a supported transport (got {transport!r})")
        if response.get("session") != "exclusive":
            raise RuntimeError("device does not provide an exclusive control session")
        self.session_active = True
        return response

    def _send_pending_hello_cleanup(self) -> None:
        """Release a session if hello reached the device but its reply was lost."""

        try:
            self.write_command("goodbye")
            flush = getattr(self.serial, "flush", None)
            if flush is not None:
                flush()
        except (OSError, RuntimeError, TimeoutError):
            pass
        finally:
            self._hello_request_pending = False

    def goodbye(self) -> dict[str, Any]:
        if not self.session_active:
            return {"ok": True, "op": "done", "command": "goodbye"}
        request_id = self.write_command("goodbye")
        response = self.wait_for(request_id, "done", "error")
        self._raise_for_error(response)
        self.session_active = False
        return response

    def status(self) -> dict[str, Any]:
        request_id = self.write_command("status")
        response = self.wait_for(request_id, "status", "error")
        self._raise_for_error(response)
        return response

    def call(self, service: str, function: str, args: dict[str, Any]) -> dict[str, Any]:
        request_id = self.write_command("call", service=service, function=function, args=args)
        response = self.wait_for(request_id, "call", "error")
        self._raise_for_error(response)
        return response

    def transfer(self, operation: str, file_path: Path, remote_path: str | None, overwrite: bool) -> dict[str, Any]:
        file_path = file_path.expanduser().resolve()
        with file_path.open("rb") as file_obj:
            size, digest = sha256_file(file_obj)
        fields: dict[str, Any] = {"size": size, "sha256": digest}
        if remote_path is not None:
            fields["path"] = remote_path
            fields["overwrite"] = overwrite
        request_id = self.write_command(operation, **fields)
        ready = self.wait_for(request_id, "ready", "error")
        self._raise_for_error(ready)
        frame_size = int(ready.get("max_frame_payload", 16 * 1024))
        try:
            with file_path.open("rb") as file_obj:
                for frame in iter_file_frames(file_obj, request_id, frame_size):
                    if frame[3] == FrameType.DATA:
                        sequence = int.from_bytes(frame[8:12], "little")
                        for attempt in range(3):
                            self.serial.write(frame)
                            try:
                                response = self.wait_for(request_id, "ack", "error")
                            except TimeoutError:
                                if attempt == 2:
                                    raise
                                continue
                            self._raise_for_error(response)
                            if response.get("sequence") != sequence:
                                raise RuntimeError("device acknowledged an unexpected frame sequence")
                            sent = min((sequence + 1) * frame_size, size)
                            print(f"progress: {sent}/{size} bytes", file=sys.stderr)
                            break
                    else:
                        self.serial.write(frame)
        except KeyboardInterrupt:
            self.cancel_transfer(request_id)
            raise
        original_timeout = self.serial.timeout
        self.serial.timeout = max(original_timeout, TRANSFER_FINALIZE_TIMEOUT_SECONDS)
        try:
            response = self.wait_for(request_id, "done", "error")
        finally:
            self.serial.timeout = original_timeout
        self._raise_for_error(response)
        return response

    def cancel_transfer(self, request_id: int) -> None:
        self.serial.write(encode_frame(BinaryFrame(FrameType.CANCEL, request_id, 0)))
        try:
            response = self.wait_for(request_id, "error", "done")
        except (OSError, RuntimeError, TimeoutError):
            return
        if response.get("op") == "error" and response.get("error_code") == "aborted":
            return
        self._raise_for_error(response)

    def abort(self, request_id: int) -> dict[str, Any]:
        self.serial.write(command("abort", request_id))
        response = self.wait_for(request_id, "error", "done")
        if response.get("op") == "error" and response.get("error_code") == "aborted":
            return response
        self._raise_for_error(response)
        return response

    @staticmethod
    def _raise_for_error(response: dict[str, Any]) -> None:
        if response.get("ok") is False:
            code = response.get("error_code", "device_error")
            message = response.get("error", "unknown device error")
            raise RuntimeError(f"{code}: {message}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="brookesia-usb")
    parser.add_argument("--port", help="USJ or USB-UART device; omitted means auto-discovery (USJ first, then USB-UART)")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate for UART transport (ignored for USJ)")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("devices")
    subparsers.add_parser("status")

    call_parser = subparsers.add_parser("call")
    call_parser.add_argument("service")
    call_parser.add_argument("function")
    call_parser.add_argument("args", nargs="?", default="{}")

    put_parser = subparsers.add_parser("put")
    put_parser.add_argument("local_file", type=Path)
    put_parser.add_argument("remote_path")
    put_parser.add_argument("--overwrite", action="store_true")

    install_parser = subparsers.add_parser("install")
    install_parser.add_argument("package", type=Path)

    abort_parser = subparsers.add_parser("abort")
    abort_parser.add_argument("request_id", type=int)
    return parser


def run_main(args: argparse.Namespace) -> int:
    if args.command == "devices":
        return list_devices()
    if not args.port:
        args.port = discover_control_port(
            args.baudrate,
            max(args.timeout, MIN_DISCOVERY_TIMEOUT_SECONDS),
            probe=args.command != "abort",
        )
    with UsbClient(args.port, args.baudrate, args.timeout) as client:
        if args.command != "abort":
            client.hello()
        if args.command == "status":
            print(json.dumps(client.status(), indent=2, ensure_ascii=False))
        elif args.command == "call":
            parsed_args = json.loads(args.args)
            if not isinstance(parsed_args, dict):
                raise ValueError("call args must be a JSON object")
            print(json.dumps(client.call(args.service, args.function, parsed_args), indent=2, ensure_ascii=False))
        elif args.command == "put":
            print(json.dumps(client.transfer("put", args.local_file, args.remote_path, args.overwrite), indent=2))
        elif args.command == "install":
            print(json.dumps(client.transfer("install_bpk", args.package, None, False), indent=2))
        elif args.command == "abort":
            print(json.dumps(client.abort(args.request_id), indent=2))
    return 0


def is_busy_error(error: Exception) -> bool:
    return isinstance(error, RuntimeError) and str(error).startswith("busy:")


def discover_control_port(baudrate: int = 115200, timeout: float = 1.0, probe: bool = True) -> str:
    _, list_ports = _serial_module()
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("no serial ports found")

    # Phase 1: Try USJ candidates first (existing behavior)
    usj_candidates = sorted((port for port in ports if is_serial_jtag_port(port)), key=lambda item: item.device)
    if usj_candidates:
        # A single VID/PID-matched port is sufficient to select the transport. Do
        # the protocol hello only once in run_main(), using the user's normal
        # timeout, so discovery cannot create a stale exclusive session.
        if len(usj_candidates) == 1:
            return usj_candidates[0].device
        if not probe:
            if len(usj_candidates) != 1:
                raise RuntimeError("multiple USB Serial/JTAG ports found; pass --port explicitly")
            return usj_candidates[0].device

        # USB Serial/JTAG is a single CDC port. Probe the protocol after filtering
        # the hardware identity instead of assuming a ttyACM index.
        busy_error: Exception | None = None
        for port in usj_candidates:
            try:
                with UsbClient(port.device, baudrate, timeout) as client:
                    response = client.hello()
                    if response.get("transport") == "serial_jtag":
                        return port.device
            except (OSError, RuntimeError, TimeoutError) as error:
                if busy_error is None and is_busy_error(error):
                    busy_error = error
                continue
        if busy_error is not None:
            raise busy_error
        # If all USJ candidates failed, fall through to USB-UART phase

    # Phase 2: Fallback to USB-UART candidates
    uart_candidates = sorted((port for port in ports if is_usb_uart_port(port) and not is_serial_jtag_port(port)), key=lambda item: item.device)
    if not uart_candidates:
        if usj_candidates:
            # We had USJ candidates but they all failed the hello probe
            raise RuntimeError("cannot find Brookesia on the USB Serial/JTAG port; pass --port explicitly")
        else:
            raise RuntimeError("cannot find an ESP32 USB Serial/JTAG or USB-UART port")

    # Probe UART candidates with hello, accepting either transport type
    busy_error: Exception | None = None
    for port in uart_candidates:
        try:
            with UsbClient(port.device, baudrate, timeout) as client:
                response = client.hello()
                if response.get("transport") in ("serial_jtag", "uart"):
                    return port.device
        except (OSError, RuntimeError, TimeoutError) as error:
            if busy_error is None and is_busy_error(error):
                busy_error = error
            continue
    if busy_error is not None:
        raise busy_error
    raise RuntimeError("cannot find Brookesia on any serial port; pass --port explicitly")


def is_serial_jtag_port(port: Any) -> bool:
    if port.vid == SERIAL_JTAG_VID and port.pid == SERIAL_JTAG_PID:
        return True
    description = f"{port.description or ''} {port.interface or ''} {port.hwid or ''}".lower()
    return any(marker in description for marker in ("usb_jtag", "usb serial/jtag", "serial/jtag"))


def is_usb_uart_port(port: Any) -> bool:
    if is_serial_jtag_port(port):
        return False
    usb_uart_vid_pid = {
        (0x10C4, 0xEA60),  # CP210x
        (0x1A86, 0x7523),  # CH340
        (0x0403, 0x6001),  # FTDI
        (0x0403, 0x6010),  # FTDI
        (0x0403, 0x6011),  # FTDI
        (0x0403, 0x6014),  # FTDI
        (0x067B, 0x2303),  # Prolific
    }
    if port.vid is not None and port.pid is not None:
        if (port.vid, port.pid) in usb_uart_vid_pid:
            return True
    device = getattr(port, "device", "") or ""
    return device.startswith("/dev/ttyUSB") or device.startswith("/dev/ttyACM")


def main() -> int:
    parser = build_parser()
    try:
        return run_main(parser.parse_args())
    except (OSError, RuntimeError, TimeoutError, ValueError, json.JSONDecodeError) as error:
        print(f"brookesia-usb: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
