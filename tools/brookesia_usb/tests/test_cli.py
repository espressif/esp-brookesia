import json
import types
import unittest

from brookesia_usb import cli


class FakeSerial:
    def __init__(self, *args, **kwargs):
        self.responses = []
        self.writes = []
        self.closed = False
        self.hello_transport = "serial_jtag"
        self.timeout = 1.0

    def reset_input_buffer(self):
        pass

    def write(self, payload):
        self.writes.append(payload)
        request = json.loads(payload.decode("utf-8"))
        if request["op"] == "hello":
            self.responses.append(
                json.dumps({
                    "version": 1,
                    "op": "hello",
                    "request_id": request["request_id"],
                    "ok": True,
                    "protocol_version": 1,
                    "transport": self.hello_transport,
                    "session": "exclusive",
                }).encode() + b"\n"
            )
        elif request["op"] == "goodbye":
            self.responses.append(json.dumps({
                "version": 1,
                "op": "done",
                "request_id": request["request_id"],
                "ok": True,
                "command": "goodbye",
            }).encode() + b"\n")

    def readline(self):
        return self.responses.pop(0) if self.responses else b""

    def close(self):
        self.closed = True

    def flush(self):
        pass


class UartFakeSerial(FakeSerial):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.hello_transport = "uart"


class UnknownTransportFakeSerial(FakeSerial):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.hello_transport = "unknown"


class TimeoutSerial(FakeSerial):
    def readline(self):
        return b""


class BusySerial(FakeSerial):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.busy_from_hello = 0

    def write(self, payload):
        request = json.loads(payload.decode("utf-8"))
        if request["op"] == "hello" and self.hello_count() >= self.busy_from_hello:
            self.responses.append(json.dumps({
                "version": 1,
                "op": "error",
                "request_id": request["request_id"],
                "ok": False,
                "error_code": "busy",
            }).encode() + b"\n")
            return request["request_id"]
        super().write(payload)

    def hello_count(self):
        return sum(1 for payload in self.writes if json.loads(payload.decode("utf-8"))["op"] == "hello")


class LostReplyBusySerial(FakeSerial):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.busy_from_hello = 1

    def write(self, payload):
        request = json.loads(payload.decode("utf-8"))
        if request["op"] == "hello":
            count = sum(1 for w in self.writes if json.loads(w.decode("utf-8"))["op"] == "hello")
            self.writes.append(payload)
            if count == 0:
                self.responses.clear()
                return
            self.responses.append(json.dumps({
                "version": 1,
                "op": "error",
                "request_id": request["request_id"],
                "ok": False,
                "error_code": "busy",
            }).encode() + b"\n")
            return
        self.writes.append(payload)
        if request["op"] == "goodbye":
            self.responses.append(json.dumps({
                "version": 1,
                "op": "done",
                "request_id": request["request_id"],
                "ok": True,
                "command": "goodbye",
            }).encode() + b"\n")


class CliTests(unittest.TestCase):
    def test_serial_jtag_identity_is_accepted(self):
        port = types.SimpleNamespace(
            vid=0x303A, pid=0x1001, description="USB JTAG/serial", interface=None, hwid=""
        )
        self.assertTrue(cli.is_serial_jtag_port(port))

    def test_otg_port_is_rejected(self):
        port = types.SimpleNamespace(
            vid=0x303A, pid=0x1002, description="USB OTG", interface=None, hwid=""
        )
        self.assertFalse(cli.is_serial_jtag_port(port))

    def test_usb_uart_port_identification(self):
        # Test CP210x
        port = types.SimpleNamespace(vid=0x10C4, pid=0xEA60, description="", interface=None, hwid="")
        self.assertTrue(cli.is_usb_uart_port(port))

        # Test CH340
        port = types.SimpleNamespace(vid=0x1A86, pid=0x7523, description="", interface=None, hwid="")
        self.assertTrue(cli.is_usb_uart_port(port))

        # Test FTDI
        port = types.SimpleNamespace(vid=0x0403, pid=0x6001, description="", interface=None, hwid="")
        self.assertTrue(cli.is_usb_uart_port(port))

        # Test ttyUSB device
        port = types.SimpleNamespace(device="/dev/ttyUSB0", vid=None, pid=None, description="", interface=None, hwid="")
        self.assertTrue(cli.is_usb_uart_port(port))

        # Test ttyACM device (non-USJ)
        port = types.SimpleNamespace(device="/dev/ttyACM0", vid=0x1234, pid=0x5678, description="", interface=None, hwid="")
        self.assertTrue(cli.is_usb_uart_port(port))

        # Test USJ device should NOT be identified as USB-UART by this function
        port = types.SimpleNamespace(vid=0x303A, pid=0x1001, description="USB JTAG/serial", interface=None, hwid="")
        self.assertFalse(cli.is_usb_uart_port(port))

    def test_hello_and_goodbye_validate_single_transport(self):
        fake = FakeSerial()
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with cli.UsbClient("/dev/ttyACM0", 115200, 1.0) as client:
                hello = client.hello()
                self.assertEqual(hello["protocol_version"], 1)
                self.assertEqual(hello["transport"], "serial_jtag")
                goodbye = client.goodbye()
                self.assertEqual(goodbye["command"], "goodbye")
            self.assertTrue(fake.closed)
        finally:
            cli._serial_module = original_loader

    def test_hello_accepts_uart_transport(self):
        fake = UartFakeSerial()
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with cli.UsbClient("/dev/ttyUSB0", 115200, 1.0) as client:
                hello = client.hello()
                self.assertEqual(hello["protocol_version"], 1)
                self.assertEqual(hello["transport"], "uart")
                goodbye = client.goodbye()
                self.assertEqual(goodbye["command"], "goodbye")
            self.assertTrue(fake.closed)
        finally:
            cli._serial_module = original_loader

    def test_hello_rejects_unknown_transport(self):
        fake = UnknownTransportFakeSerial()
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with cli.UsbClient("/dev/ttyUSB0", 115200, 1.0) as client:
                with self.assertRaises(RuntimeError) as cm:
                    client.hello()
                self.assertIn("device is not using a supported transport", str(cm.exception))
            self.assertTrue(fake.closed)
        finally:
            cli._serial_module = original_loader

    def test_single_port_discovery_does_not_probe_before_the_real_command(self):
        port = types.SimpleNamespace(
            device="/dev/ttyACM0", vid=0x303A, pid=0x1001,
            description="USB JTAG/serial", interface=None, hwid=""
        )
        original_loader = cli._serial_module
        cli._serial_module = lambda: (None, types.SimpleNamespace(comports=lambda: [port]))
        try:
            self.assertEqual(cli.discover_control_port(timeout=1.0), "/dev/ttyACM0")
        finally:
            cli._serial_module = original_loader

    def test_usb_uart_fallback_when_no_usj(self):
        # No USJ ports, but one USB-UART port that responds with uart transport
        uart_port = types.SimpleNamespace(
            device="/dev/ttyUSB0", vid=0x10C4, pid=0xEA60, description="CP210x USB to UART Bridge", interface=None, hwid=""
        )
        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=lambda *args, **kwargs: UartFakeSerial()),
            types.SimpleNamespace(comports=lambda: [uart_port]),
        )
        try:
            self.assertEqual(cli.discover_control_port(timeout=1.0), "/dev/ttyUSB0")
        finally:
            cli._serial_module = original_loader

    def test_usb_uart_fallback_selects_first_working(self):
        # No USJ ports, multiple USB-UART ports, first one works
        uart_port1 = types.SimpleNamespace(
            device="/dev/ttyUSB0", vid=0x10C4, pid=0xEA60, description="CP210x USB to UART Bridge", interface=None, hwid=""
        )
        uart_port2 = types.SimpleNamespace(
            device="/dev/ttyUSB1", vid=0x1A86, pid=0x7523, description="CH340 USB to UART Bridge", interface=None, hwid=""
        )

        # First port responds with uart transport, second port times out
        class FirstWorksSecondTimeoutSerial:
            def __init__(self, *args, **kwargs):
                self.call_count = 0
                self.responses = []
                self.writes = []
                self.closed = False
                self.timeout = 1.0
                super().__init__(*args, **kwargs)

            def reset_input_buffer(self):
                pass

            def write(self, payload):
                self.writes.append(payload)
                request = json.loads(payload.decode("utf-8"))
                if request["op"] == "hello":
                    self.call_count += 1
                    if self.call_count == 1:
                        # First call - respond with uart transport
                        self.responses.append(
                            json.dumps({
                                "version": 1,
                                "op": "hello",
                                "request_id": request["request_id"],
                                "ok": True,
                                "protocol_version": 1,
                                "transport": "uart",
                                "session": "exclusive",
                            }).encode() + b"\n"
                        )
                    else:
                        # Second call - simulate timeout by not responding
                        pass
                elif request["op"] == "goodbye":
                    self.responses.append(json.dumps({
                        "version": 1,
                        "op": "done",
                        "request_id": request["request_id"],
                        "ok": True,
                        "command": "goodbye",
                    }).encode() + b"\n")

            def readline(self):
                return self.responses.pop(0) if self.responses else b""

            def close(self):
                self.closed = True

            def flush(self):
                pass

        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=lambda *args, **kwargs: FirstWorksSecondTimeoutSerial()),
            types.SimpleNamespace(comports=lambda: [uart_port1, uart_port2]),
        )
        try:
            # Should select the first port that works
            self.assertEqual(cli.discover_control_port(timeout=1.0), "/dev/ttyUSB0")
        finally:
            cli._serial_module = original_loader

    def test_discovery_propagates_busy_from_uart_candidate(self):
        uart_port = types.SimpleNamespace(
            device="/dev/ttyUSB0", vid=0x10C4, pid=0xEA60, description="CP210x USB to UART Bridge", interface=None, hwid=""
        )
        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=lambda *args, **kwargs: BusySerial()),
            types.SimpleNamespace(comports=lambda: [uart_port]),
        )
        try:
            with self.assertRaises(RuntimeError) as cm:
                cli.discover_control_port(timeout=1.0)
            self.assertTrue(str(cm.exception).startswith("busy:"), str(cm.exception))
        finally:
            cli._serial_module = original_loader

    def test_discovery_propagates_busy_from_serial_jtag_candidates(self):
        usj_ports = [
            types.SimpleNamespace(
                device="/dev/ttyACM0", vid=0x303A, pid=0x1001,
                description="USB JTAG/serial", interface=None, hwid=""
            ),
            types.SimpleNamespace(
                device="/dev/ttyACM1", vid=0x303A, pid=0x1001,
                description="USB JTAG/serial", interface=None, hwid=""
            ),
        ]
        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=lambda *args, **kwargs: BusySerial()),
            types.SimpleNamespace(comports=lambda: usj_ports),
        )
        try:
            with self.assertRaises(RuntimeError) as cm:
                cli.discover_control_port(timeout=1.0)
            self.assertTrue(str(cm.exception).startswith("busy:"), str(cm.exception))
        finally:
            cli._serial_module = original_loader

    def test_discovery_keeps_busy_when_later_serial_jtag_candidate_times_out(self):
        usj_ports = [
            types.SimpleNamespace(
                device="/dev/ttyACM0", vid=0x303A, pid=0x1001,
                description="USB JTAG/serial", interface=None, hwid=""
            ),
            types.SimpleNamespace(
                device="/dev/ttyACM1", vid=0x303A, pid=0x1001,
                description="USB JTAG/serial", interface=None, hwid=""
            ),
        ]
        serials = [BusySerial(), TimeoutSerial()]

        def make_serial(*args, **kwargs):
            return serials.pop(0)

        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=make_serial),
            types.SimpleNamespace(comports=lambda: usj_ports),
        )
        try:
            with self.assertRaises(RuntimeError) as cm:
                cli.discover_control_port(timeout=0.05)
            self.assertTrue(str(cm.exception).startswith("busy:"), str(cm.exception))
        finally:
            cli._serial_module = original_loader

    def test_discovery_keeps_busy_when_later_uart_candidate_times_out(self):
        uart_port1 = types.SimpleNamespace(
            device="/dev/ttyUSB0", vid=0x10C4, pid=0xEA60, description="CP210x USB to UART Bridge", interface=None, hwid=""
        )
        uart_port2 = types.SimpleNamespace(
            device="/dev/ttyUSB1", vid=0x1A86, pid=0x7523, description="CH340 USB to UART Bridge", interface=None, hwid=""
        )
        serials = [BusySerial(), TimeoutSerial()]

        def make_serial(*args, **kwargs):
            return serials.pop(0)

        original_loader = cli._serial_module
        cli._serial_module = lambda: (
            types.SimpleNamespace(Serial=make_serial),
            types.SimpleNamespace(comports=lambda: [uart_port1, uart_port2]),
        )
        try:
            with self.assertRaises(RuntimeError) as cm:
                cli.discover_control_port(timeout=0.05)
            self.assertTrue(str(cm.exception).startswith("busy:"), str(cm.exception))
        finally:
            cli._serial_module = original_loader

    def test_no_candidates_raises_error(self):
        # No serial ports at all
        original_loader = cli._serial_module
        cli._serial_module = lambda: (None, types.SimpleNamespace(comports=lambda: []))
        try:
            with self.assertRaises(RuntimeError) as cm:
                cli.discover_control_port(timeout=1.0)
            self.assertIn("no serial ports found", str(cm.exception))
        finally:
            cli._serial_module = original_loader

    def test_timed_out_hello_sends_best_effort_cleanup(self):
        fake = TimeoutSerial()
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with self.assertRaises(TimeoutError):
                with cli.UsbClient("/dev/ttyACM0", 115200, 0.01) as client:
                    client.hello()
            operations = [json.loads(payload.decode("utf-8"))["op"] for payload in fake.writes]
            self.assertEqual(operations[0], "hello")
            self.assertEqual(operations[-1], "goodbye")
        finally:
            cli._serial_module = original_loader

    def test_first_hello_busy_fails_without_goodbye(self):
        fake = BusySerial()
        fake.busy_from_hello = 0
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with cli.UsbClient("/dev/ttyACM0", 115200, 1.0) as client:
                with self.assertRaises(RuntimeError) as cm:
                    client.hello()
                self.assertIn("busy", str(cm.exception))
            operations = [json.loads(payload.decode("utf-8"))["op"] for payload in fake.writes]
            self.assertNotIn("goodbye", operations)
        finally:
            cli._serial_module = original_loader

    def test_busy_after_timeout_cleans_up_stale_session(self):
        fake = LostReplyBusySerial()
        original_loader = cli._serial_module
        cli._serial_module = lambda: (types.SimpleNamespace(Serial=lambda *args, **kwargs: fake), None)
        try:
            with self.assertRaises(RuntimeError) as cm:
                with cli.UsbClient("/dev/ttyACM0", 115200, 0.05) as client:
                    client.hello()
            self.assertIn("busy", str(cm.exception))
            operations = [json.loads(payload.decode("utf-8"))["op"] for payload in fake.writes]
            self.assertIn("goodbye", operations)
        finally:
            cli._serial_module = original_loader

    def test_parser_exposes_all_commands(self):
        parser = cli.build_parser()
        for argv in (
            ["devices"],
            ["status"],
            ["call", "SystemCore", "GetSystemInfo"],
            ["put", "file.bin", "logs/file.bin"],
            ["install", "app.bpk"],
            ["abort", "42"],
        ):
            self.assertEqual(parser.parse_args(argv).command, argv[0])


if __name__ == "__main__":
    unittest.main()
