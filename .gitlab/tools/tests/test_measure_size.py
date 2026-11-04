# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Tests for the G4 size gate (.gitlab/tools/measure_size.py).

The size payload in ID_SIZE_JSON is a trimmed copy of real
`idf.py size --format json2` output from an ESP32-P4 rev v1.0 build of
examples/system/super, so the parser is exercised against the actual shape
IDF emits rather than a convenient mock.
"""

import importlib.util
import pathlib
import unittest

_SPEC = importlib.util.spec_from_file_location(
    "measure_size",
    pathlib.Path(__file__).resolve().parents[1] / "measure_size.py",
)
measure_size = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(measure_size)

ID_SIZE_JSON = """
NOTE: Loaded component extension from "managed_components/espressif__esp_board_manager"
{
  "version": 1.2,
  "total_size": 9242672,
  "layout": [
    {"name": "External RAM", "total": 134217696, "used": 9109376, "free": 125108320,
     "parts": {".text": {"size": 6800608}, ".rodata": {"size": 1682028},
               ".eh_frame": {"size": 625676}, ".init_array": {"size": 640},
               ".appdesc": {"size": 256}, ".tbss": {"size": 152}, ".tdata": {"size": 16}}},
    {"name": "DIRAM", "total": 572368, "used": 175870, "free": 396498,
     "parts": {".text": {"size": 94874}, ".data": {"size": 38996}, ".bss": {"size": 3872}}},
    {"name": "HP core RAM", "total": 8192, "used": 182, "free": 8010,
     "parts": {".text": {"size": 114}, ".data": {"size": 68}}},
    {"name": "LP RAM", "total": 32768, "used": 60, "free": 32708,
     "parts": {".force_slow": {"size": 36}, ".rtc_reserved": {"size": 24}}}
  ]
}
trailing non-JSON noise from the idf.py wrapper
"""


class ParseSizeJsonTests(unittest.TestCase):
    def setUp(self):
        self.parsed = measure_size.parse_size_json(ID_SIZE_JSON)

    def test_total_size_and_regions(self):
        self.assertEqual(self.parsed["total_size"], 9242672)
        self.assertEqual(len(self.parsed["regions"]), 4)
        self.assertEqual(self.parsed["regions"]["DIRAM"]["used"], 175870)

    def test_parts_are_region_qualified(self):
        parts = self.parsed["parts"]
        self.assertEqual(parts["DIRAM::.data"], 38996)
        self.assertEqual(parts["External RAM::.text"], 6800608)
        self.assertNotIn(".data", parts)

    def test_spec_named_aliases(self):
        """v1.8 section 3.7 names .flash.bss / .dram0.data; IDF 6.2 emits
        region-scoped parts instead, so the aliases map onto the equivalents."""
        aliased = measure_size.aliased_parts(self.parsed)
        self.assertEqual(aliased["dram0_data"], 38996)
        self.assertEqual(aliased["flash_bss"], 152)
        self.assertEqual(aliased["dram0_bss"], 3872)
        self.assertEqual(aliased["iram0_text"], 114)

    def test_rejects_non_json(self):
        with self.assertRaises(ValueError):
            measure_size.parse_size_json("no json here")


class GradeTests(unittest.TestCase):
    def test_bands_per_section4(self):
        self.assertEqual(measure_size.grade(-6.0, first_round=False), "pass_a")
        self.assertEqual(measure_size.grade(-5.0, first_round=False), "pass_a")
        self.assertEqual(measure_size.grade(-4.9, first_round=False), "pass_b")
        self.assertEqual(measure_size.grade(0.0, first_round=False), "pass_b")
        # Section 4: Fail is > +10%, so the (0, 10] band is a watch, not a failure.
        self.assertEqual(measure_size.grade(0.1, first_round=False), "pass_b")
        self.assertEqual(measure_size.grade(10.0, first_round=False), "pass_b")
        self.assertEqual(measure_size.grade(10.1, first_round=False), "fail")
        self.assertEqual(measure_size.grade(11.0, first_round=False), "fail")

    def test_first_round_only_records(self):
        for delta in (-50.0, 0.0, 99.0):
            self.assertEqual(measure_size.grade(delta, first_round=True), "record")

    def test_delta_pct_rejects_zero_baseline(self):
        with self.assertRaises(ValueError):
            measure_size.delta_pct(0, 10)


class CompareTests(unittest.TestCase):
    def _record(self, **over):
        rec = {
            "app_bin_bytes": 1000,
            "littlefs_bin_bytes": 500,
            "resource_bytes": 200,
            "total_image_size": 990,
            "sdkconfig_sha256": "abc",
            "board_id": "esp32_p4_function_ev",
        }
        rec.update(over)
        return rec

    def test_identical_pair_passes(self):
        result = measure_size.compare(self._record(), self._record())
        self.assertTrue(result["pass"])
        self.assertEqual(result["fail_rows"], [])

    def test_shrink_is_pass_a(self):
        result = measure_size.compare(self._record(), self._record(app_bin_bytes=900))
        self.assertEqual(result["rows"]["app_bin_bytes"]["grade"], "pass_a")
        self.assertTrue(result["pass"])

    def test_growth_fails(self):
        result = measure_size.compare(self._record(), self._record(app_bin_bytes=1200))
        self.assertEqual(result["rows"]["app_bin_bytes"]["grade"], "fail")
        self.assertFalse(result["pass"])
        self.assertIn("app_bin_bytes", result["fail_rows"])

    def test_first_round_never_fails(self):
        result = measure_size.compare(
            self._record(), self._record(app_bin_bytes=5000), first_round=True
        )
        self.assertEqual(result["rows"]["app_bin_bytes"]["grade"], "record")
        self.assertTrue(result["pass"])

    def test_first_round_tolerates_incomplete_artifacts(self):
        # size_gate.yml falls back to an app.bin-only record when size.json is
        # absent; that missing row must not turn the recording round red.
        result = measure_size.compare(
            self._record(), self._record(total_image_size=None), first_round=True
        )
        self.assertEqual(result["rows"]["total_image_size"]["grade"], "record")
        self.assertTrue(result["pass"])

    def test_sdkconfig_mismatch_refuses(self):
        result = measure_size.compare(self._record(), self._record(sdkconfig_sha256="def"))
        self.assertFalse(result["sdkconfig_match"])
        self.assertFalse(result["pass"])

    def test_board_mismatch_refuses(self):
        result = measure_size.compare(self._record(), self._record(board_id="esp32_s31_korvo1"))
        self.assertFalse(result["board_match"])
        self.assertFalse(result["pass"])

    def test_one_sided_missing_row_fails_closed(self):
        result = measure_size.compare(
            self._record(), self._record(littlefs_bin_bytes=None)
        )
        self.assertIsNone(result["rows"]["littlefs_bin_bytes"]["delta_pct"])
        self.assertEqual(result["rows"]["littlefs_bin_bytes"]["grade"], "fail")
        self.assertFalse(result["pass"])

    def test_both_sides_missing_row_is_record(self):
        base = self._record(littlefs_bin_bytes=None)
        result = measure_size.compare(base, dict(base))
        self.assertEqual(result["rows"]["littlefs_bin_bytes"]["grade"], "record")
        self.assertTrue(result["pass"])

    def test_empty_pair_is_rejected(self):
        result = measure_size.compare({}, {})
        self.assertFalse(result["sdkconfig_match"])
        self.assertFalse(result["board_match"])
        self.assertFalse(result["pass"])

    def test_missing_identity_fields_are_rejected(self):
        base = dict(self._record())
        base.pop("sdkconfig_sha256")
        result = measure_size.compare(base, dict(base))
        self.assertFalse(result["sdkconfig_match"])
        self.assertFalse(result["pass"])


class DirBytesTests(unittest.TestCase):
    def test_sums_recursively(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            (root / "a").mkdir()
            (root / "a" / "x.bin").write_bytes(b"x" * 100)
            (root / "y.bin").write_bytes(b"y" * 50)
            self.assertEqual(measure_size.dir_bytes(root), 150)

    def test_absent_dir_is_zero(self):
        self.assertEqual(measure_size.dir_bytes(pathlib.Path("/nonexistent-xyz")), 0)


class MeasureTests(unittest.TestCase):
    def test_measure_rejects_missing_build_dir(self):
        with self.assertRaises(ValueError):
            measure_size.measure(
                pathlib.Path("/nonexistent-build"), repo_root=pathlib.Path(".")
            )


if __name__ == "__main__":
    unittest.main()
