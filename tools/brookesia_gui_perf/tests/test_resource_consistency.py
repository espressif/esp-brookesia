# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Guard the Golden Case carrier: res/ -> perf_eval_res.c -> baseline.

The embedded document tree, the compiled-in ``doc_complexity`` constants and the
published baseline must describe the same corpus. Regenerating one and not the
others silently changes what a capture claims to have measured, and because
``check_gate`` compares ``doc_complexity`` strictly, a stale constant also makes
the baseline unusable for the next paired run.
"""

import importlib.util
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GENERATOR = ROOT / "examples/system/perf_eval/main/gen_perf_eval_res.py"
CARRIER = ROOT / "examples/system/perf_eval/main/perf_eval_res.c"
BASELINE = ROOT / "reports/baseline/esp32_p4_function_ev/Baseline.esp32_p4_function_ev.json"

FIELDS = ("nodes", "styles", "images", "bytes")


def load_generator():
    """Import the resource generator without running its __main__."""
    spec = importlib.util.spec_from_file_location("perf_eval_gen_res", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def measured_complexity():
    module = load_generator()
    return dict(zip(FIELDS, module.measure_complexity(module.collect_files())))


def carrier_complexity():
    text = CARRIER.read_text(encoding="utf-8")
    out = {}
    for name in FIELDS:
        match = re.search(r"perf_eval_res_doc_%s\s*=\s*(\d+)" % name, text)
        if match is None:
            raise AssertionError("perf_eval_res.c has no perf_eval_res_doc_%s constant" % name)
        out[name] = int(match.group(1))
    return out


class ResourceConsistencyTest(unittest.TestCase):
    def test_committed_carrier_matches_res_tree(self):
        """Editing res/ without rerunning gen_perf_eval_res.py must fail here."""
        measured = measured_complexity()
        committed = carrier_complexity()
        self.assertEqual(
            committed, measured,
            "perf_eval_res.c is stale: run "
            "`python3 examples/system/perf_eval/main/gen_perf_eval_res.py` "
            "and commit the result (measured=%s, committed=%s)" % (measured, committed),
        )

    def test_baseline_describes_the_committed_carrier(self):
        """A baseline captured from a different corpus cannot pair with this build."""
        self.assertTrue(BASELINE.is_file(), "baseline is missing: %s" % BASELINE)
        metadata = json.loads(BASELINE.read_text(encoding="utf-8"))["metadata"]
        recorded = metadata.get("doc_complexity")
        self.assertIsInstance(recorded, dict, "baseline metadata.doc_complexity is missing")
        committed = carrier_complexity()
        self.assertEqual(
            {k: recorded.get(k) for k in FIELDS}, committed,
            "baseline doc_complexity does not match the embedded carrier; "
            "re-capture the baseline from the current firmware",
        )


if __name__ == "__main__":
    unittest.main()
