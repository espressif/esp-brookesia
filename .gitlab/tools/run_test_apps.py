# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Serial Test Runner for ESP Brookesia Test Applications

This script automates the execution of test cases through serial communication,
providing retry mechanisms, failure detection, and result logging.
"""

import argparse
import sys
import time
from enum import Enum
from pathlib import Path
from typing import List, Optional

try:
    import serial
except ImportError:
    print('Error: pyserial is not installed. Install it with: pip install pyserial')
    sys.exit(1)


# ============================================================================
# Constants
# ============================================================================

class TestResponse(Enum):
    """Test response patterns"""
    SUCCESS = '0 Failures'
    FAILURE = '1 Failures'
    ENTER = 'Enter test for running'
    REBOOT = 'Rebooting...'


class Config:
    """Configuration constants"""
    DEFAULT_BAUDRATE = 115200
    DEFAULT_TIMEOUT = 1
    DEFAULT_RETRY_LIMIT = 4
    DEFAULT_TEST_TIMEOUT = 120
    POLL_INTERVAL = 0.1
    INTER_TEST_DELAY = 1.0
    DEFAULT_OUTPUT_DIR = './build'
    DEFAULT_OUTPUT_FILE = 'failed_numbers.txt'


# ============================================================================
# Serial Communication
# ============================================================================

class SerialPort:
    """Wrapper for serial port operations"""

    def __init__(self, port: str, baudrate: int = Config.DEFAULT_BAUDRATE,
                 timeout: int = Config.DEFAULT_TIMEOUT):
        """
        Initialize serial port connection

        Args:
            port: Serial port path (e.g., '/dev/ttyUSB0')
            baudrate: Communication speed
            timeout: Read timeout in seconds
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.connection: Optional[serial.Serial] = None

    def open(self) -> bool:
        """
        Open serial port connection

        Returns:
            True if successful, False otherwise
        """
        try:
            self.connection = serial.Serial(
                self.port,
                self.baudrate,
                timeout=self.timeout
            )
            print(f'✓ Opened serial port {self.port} at {self.baudrate} baud')
            return True
        except serial.SerialException as e:
            print(f'✗ Failed to open serial port {self.port}: {e}')
            return False

    def close(self):
        """Close serial port connection"""
        if self.connection and self.connection.is_open:
            self.connection.close()
            print('✓ Serial port closed')

    def write(self, data: str):
        """Write data to serial port"""
        if self.connection:
            self.connection.write(data.encode())

    def readline(self) -> str:
        """Read line from serial port"""
        if self.connection:
            try:
                return self.connection.readline().decode(errors='ignore').strip()
            except UnicodeDecodeError:
                return ''
        return ''


# ============================================================================
# Test Execution
# ============================================================================

class TestResult(Enum):
    """Test execution result"""
    PASSED = 'passed'
    FAILED = 'failed'
    TIMEOUT = 'timeout'
    REBOOT = 'reboot'


class TestRunner:
    """Manages test execution through serial communication"""

    def __init__(self, serial_port: SerialPort, retry_limit: int = Config.DEFAULT_RETRY_LIMIT,
                 test_timeout: int = Config.DEFAULT_TEST_TIMEOUT):
        """
        Initialize test runner

        Args:
            serial_port: SerialPort instance
            retry_limit: Number of retry attempts per test
            test_timeout: Timeout for each test in seconds
        """
        self.serial = serial_port
        self.retry_limit = retry_limit
        self.test_timeout = test_timeout
        self.failed_tests: List[int] = []
        self.stats = {
            'total': 0,
            'passed': 0,
            'failed': 0,
            'timeout': 0,
            'reboot': 0
        }

    def wait_for_menu(self) -> int:
        """
        Wait for test menu to appear and parse test count

        Returns:
            Number of tests detected in the menu
        """
        import re

        print('[Menu] Sending enter command...')
        self.serial.write('\n\n')

        test_count = 0
        menu_lines = []

        while True:
            response = self.serial.readline()
            if response:
                print(f'[Menu] {response}')
                menu_lines.append(response)

                # Parse test numbers like "(1)", "(23)", etc.
                match = re.match(r'\((\d+)\)', response.strip())
                if match:
                    num = int(match.group(1))
                    test_count = max(test_count, num)

            if TestResponse.ENTER.value in response:
                print(f'[Menu] ✓ Test menu ready (detected {test_count} tests)')
                break
            time.sleep(Config.POLL_INTERVAL)

        return test_count

    def run_single_test(self, test_num: int, custom_failure_string: Optional[str] = None) -> TestResult:
        """
        Run a single test

        Args:
            test_num: Test number to execute
            custom_failure_string: Optional custom failure pattern to detect

        Returns:
            TestResult indicating the outcome
        """
        self.serial.write(f'{test_num}\n')
        start_time = time.time()

        while True:
            # Check timeout
            if time.time() - start_time > self.test_timeout:
                return TestResult.TIMEOUT

            response = self.serial.readline()
            if not response:
                time.sleep(Config.POLL_INTERVAL)
                continue

            print(f'  [{test_num}] {response}')

            # Check for reboot
            if TestResponse.REBOOT.value in response:
                return TestResult.REBOOT

            # Check for custom failure
            if custom_failure_string and custom_failure_string in response:
                return TestResult.FAILED

            # Check for standard failure
            if TestResponse.FAILURE.value in response:
                return TestResult.FAILED

            # Check for success
            if TestResponse.SUCCESS.value in response:
                return TestResult.PASSED

    def run_test_with_retry(self, test_num: int, custom_failure_string: Optional[str] = None) -> bool:
        """
        Run a test with retry mechanism

        Args:
            test_num: Test number to execute
            custom_failure_string: Optional custom failure pattern

        Returns:
            True if test passed, False otherwise
        """
        self.stats['total'] += 1

        for attempt in range(1, self.retry_limit + 1):
            print(f'\n[Test {test_num}] Attempt {attempt}/{self.retry_limit}')

            result = self.run_single_test(test_num, custom_failure_string)

            if result == TestResult.PASSED:
                print(f'[Test {test_num}] ✓ PASSED')
                self.stats['passed'] += 1
                return True
            elif result == TestResult.TIMEOUT:
                print(f'[Test {test_num}] ✗ TIMEOUT (exceeded {self.test_timeout}s)')
                self.stats['timeout'] += 1
                return False  # Don't retry on timeout
            elif result == TestResult.REBOOT:
                print(f'[Test {test_num}] ✗ REBOOT DETECTED')
                self.stats['reboot'] += 1
                return False  # Don't retry on reboot
            else:  # FAILED
                if attempt < self.retry_limit:
                    print(f'[Test {test_num}] ✗ Failed, retrying...')
                else:
                    print(f'[Test {test_num}] ✗ FAILED after {self.retry_limit} attempts')
                    self.stats['failed'] += 1

        return False

    def run_test_range(self, start: int, end: int, custom_failure_string: Optional[str] = None) -> List[int]:
        """
        Run a range of tests

        Args:
            start: Starting test number
            end: Ending test number
            custom_failure_string: Optional custom failure pattern

        Returns:
            List of failed test numbers
        """
        self.failed_tests = []

        print(f'\n{"=" * 70}')
        print(f'Starting test execution: Tests {start} to {end}')
        print(f'Retry limit: {self.retry_limit}')
        print(f'Test timeout: {self.test_timeout}s')
        print(f'{"=" * 70}\n')

        try:
            for test_num in range(start, end + 1):
                if not self.run_test_with_retry(test_num, custom_failure_string):
                    self.failed_tests.append(test_num)

                # Delay between tests
                if test_num < end:
                    time.sleep(Config.INTER_TEST_DELAY)
        except KeyboardInterrupt:
            # Preserve failed tests collected so far
            raise

        return self.failed_tests

    def print_summary(self, failed_tests: List[int]):
        """Print test execution summary"""
        print(f'\n{"=" * 70}')
        print('TEST EXECUTION SUMMARY')
        print(f'{"=" * 70}')
        print(f'Total tests:    {self.stats["total"]}')
        print(f'Passed:         {self.stats["passed"]} ({self.stats["passed"]/self.stats["total"]*100:.1f}%)')
        print(f'Failed:         {self.stats["failed"]}')
        print(f'Timeout:        {self.stats["timeout"]}')
        print(f'Reboot:         {self.stats["reboot"]}')

        if failed_tests:
            print(f'\nFailed test numbers: {", ".join(map(str, failed_tests))}')
        else:
            print(f'\n✓ All tests passed!')
        print(f'{"=" * 70}\n')


# ============================================================================
# Result Logging
# ============================================================================

def save_failed_tests(failed_tests: List[int], output_dir: str = Config.DEFAULT_OUTPUT_DIR, output_file: str = Config.DEFAULT_OUTPUT_FILE):
    """
    Save failed test numbers to file

    Args:
        failed_tests: List of failed test numbers
        output_dir: Output directory path
        output_file: Output file name
    """
    if not failed_tests:
        return

    output_path = Path(output_dir) / output_file
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'a') as f:
        f.write(f'\n\n# Test run at {time.strftime("%Y-%m-%d %H:%M:%S")}\n')
        for num in failed_tests:
            f.write(f'{num}\n')

    print(f'✓ Failed test numbers saved to: {output_path}')


# ============================================================================
# Main Entry Point
# ============================================================================

def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Serial Test Runner for ESP Brookesia Test Applications',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Auto-detect all tests (recommended)
  python %(prog)s -p /dev/ttyUSB0

  # Run specific test range
  python %(prog)s -p /dev/ttyUSB0 -s 1 -e 10

  # Run with custom retry limit
  python %(prog)s --port /dev/ttyUSB0 --start 1 --end 10 --retry-limit 5

  # Run with custom failure pattern and longer timeout
  python %(prog)s -p COM3 -s 1 -e 10 -c "Error" -t 180

  # Auto-detect with custom settings
  python %(prog)s -p /dev/ttyUSB0 -r 3 -t 180
        '''
    )

    # Required arguments
    parser.add_argument('-p', '--port', required=True, help='Serial port (e.g., /dev/ttyUSB0, COM3)')

    # Optional test range (auto-detect if not specified)
    parser.add_argument(
        '-s', '--start', type=int, default=None, help='Starting test number (default: auto-detect from menu)'
    )
    parser.add_argument(
        '-e', '--end', type=int, default=None, help='Ending test number (default: auto-detect from menu)'
    )

    # Optional arguments with short options
    parser.add_argument('-c', '--custom-failure', help='Custom failure string to detect')
    parser.add_argument(
        '-r', '--retry-limit', type=int, default=Config.DEFAULT_RETRY_LIMIT,
        help=f'Number of retry attempts (default: {Config.DEFAULT_RETRY_LIMIT})'
    )
    parser.add_argument(
        '-t', '--timeout', type=int, default=Config.DEFAULT_TEST_TIMEOUT,
        help=f'Timeout per test in seconds (default: {Config.DEFAULT_TEST_TIMEOUT})'
    )
    parser.add_argument(
        '-b', '--baudrate', type=int, default=Config.DEFAULT_BAUDRATE,
        help=f'Serial baudrate (default: {Config.DEFAULT_BAUDRATE})'
    )
    parser.add_argument(
        '-o', '--output-dir', default=Config.DEFAULT_OUTPUT_DIR,
        help=f'Output directory for failed tests (default: {Config.DEFAULT_OUTPUT_DIR})'
    )
    args = parser.parse_args()

    # Validation
    if (args.start is None) != (args.end is None):
        parser.error('Both --start and --end must be specified together, or both omitted for auto-detection')

    if args.start is not None and args.end is not None:
        if args.start > args.end:
            parser.error('Start number must be less than or equal to end number')
        if args.start < 1:
            parser.error('Start number must be at least 1')

    if args.retry_limit < 1:
        parser.error('Retry limit must be at least 1')
    if args.timeout < 1:
        parser.error('Timeout must be at least 1 second')

    return args


def main():
    """Main execution function"""
    args = parse_arguments()

    # Open serial port
    serial_port = SerialPort(args.port, args.baudrate)
    if not serial_port.open():
        sys.exit(1)

    runner = None
    interrupted = False

    try:
        # Create test runner
        runner = TestRunner(serial_port, args.retry_limit, args.timeout)

        # Wait for menu and get test count
        detected_test_count = runner.wait_for_menu()

        # Determine test range
        if args.start is None or args.end is None:
            # Auto-detect mode
            if detected_test_count == 0:
                print('✗ Error: Could not auto-detect test count from menu')
                sys.exit(1)
            start_test = 1
            end_test = detected_test_count
            print(f'\n✓ Auto-detected test range: {start_test} to {end_test}')
        else:
            # Manual mode
            start_test = args.start
            end_test = args.end
            if detected_test_count > 0:
                print(f'\nℹ Menu shows {detected_test_count} tests, but will run {start_test} to {end_test} as specified')
                if end_test > detected_test_count:
                    print(f'⚠ Warning: Specified end ({end_test}) exceeds detected tests ({detected_test_count})')

        # Run tests
        runner.run_test_range(
            start_test,
            end_test,
            args.custom_failure
        )

    except KeyboardInterrupt:
        print('\n\n⚠ Test execution interrupted by user')
        interrupted = True
    except Exception as e:
        print(f'\n✗ Unexpected error: {e}')
        import traceback
        traceback.print_exc()
    finally:
        serial_port.close()

        # Print summary and save results even if interrupted
        if runner and runner.stats['total'] > 0:
            runner.print_summary(runner.failed_tests)

            # Save failed tests
            if runner.failed_tests:
                save_failed_tests(runner.failed_tests, args.output_dir)

        # Exit with appropriate code
        if interrupted:
            sys.exit(130)
        elif runner and runner.failed_tests:
            sys.exit(1)
        elif runner is None or runner.stats['total'] == 0:
            sys.exit(1)


if __name__ == '__main__':
    main()
