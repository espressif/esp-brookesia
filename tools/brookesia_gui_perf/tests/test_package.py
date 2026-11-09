# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import unittest

import brookesia_gui_perf


class PackageTests(unittest.TestCase):
    def test_version(self):
        self.assertEqual(brookesia_gui_perf.__version__, "0.1.0")


if __name__ == "__main__":
    unittest.main()
