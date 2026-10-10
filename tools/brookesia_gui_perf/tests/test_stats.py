# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import math
import unittest

from brookesia_gui_perf.stats import aggregate, cv_ok


def make_values(n, base=100.0, span=10.0):
    return [base + (i % 7) / 6.0 * span for i in range(n)]


class StatsTests(unittest.TestCase):
    def test_min_samples_enforced(self):
        with self.assertRaisesRegex(ValueError, "at least 30"):
            aggregate(list(range(29)))

    def test_percentiles_exact_on_known_data(self):
        stats = aggregate(list(range(1, 101)), min_samples=1)  # 1..100
        self.assertEqual(stats["p50"], 50.5)
        self.assertEqual(stats["p95"], 95.05)
        self.assertEqual(stats["n"], 100)
        self.assertAlmostEqual(stats["mean"], 50.5)

    def test_cv_formula(self):
        stats = aggregate(make_values(30), min_samples=30)
        self.assertLessEqual(stats["cv"], 0.05)
        self.assertTrue(cv_ok(stats))
        self.assertFalse(cv_ok({"cv": 0.051}))

    def test_zero_variance_is_ok(self):
        stats = aggregate([0.0] * 30)
        self.assertEqual(stats["cv"], 0.0)
        self.assertTrue(cv_ok(stats))

    def test_zero_mean_with_spread_is_inf_cv(self):
        stats = aggregate([-1.0, 1.0] * 15)
        self.assertEqual(stats["cv"], math.inf)
        self.assertFalse(cv_ok(stats))


if __name__ == "__main__":
    unittest.main()
