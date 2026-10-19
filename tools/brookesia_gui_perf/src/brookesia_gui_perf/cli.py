# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import json
import pathlib
import sys
from typing import List, Optional

from brookesia_gui_perf.compare import compare_reports
from brookesia_gui_perf.gate import check_gate
from brookesia_gui_perf.parser import assess_log_integrity, parse_log
from brookesia_gui_perf.report import build_report, validate_report
from brookesia_gui_perf.spec_coverage import (
    COLLECTED,
    NOT_APPLICABLE,
    PENDING,
    assess_coverage,
    assess_dimensions,
    assess_exemptions,
    summarize,
)
from brookesia_gui_perf.stats import DEFAULT_WARMUP


def _load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _cmd_aggregate(args: argparse.Namespace) -> int:
    records = parse_log(pathlib.Path(args.log).read_text(encoding="utf-8"))
    if not records.samples:
        print(
            "aggregate: log contains no [BENCH] sample records "
            "(device-side emitter not present in this firmware)",
            file=sys.stderr,
        )
        return 1
    report = build_report(records, warmup=args.warmup)
    violations = validate_report(report)
    if violations:
        for v in violations:
            print(f"violation: {v}", file=sys.stderr)
        return 1
    out = pathlib.Path(args.output)
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {out} ({report['sample_count']} samples)")
    return 0


def _cmd_gate(args: argparse.Namespace) -> int:
    violations = check_gate(
        _load_json(pathlib.Path(args.baseline)), _load_json(pathlib.Path(args.optimized))
    )
    for v in violations:
        print(f"violation: {v}")
    return 1 if violations else 0


def _run_gate_and_compare(baseline_path: pathlib.Path, optimized_path: pathlib.Path) -> Optional[dict]:
    baseline = _load_json(baseline_path)
    optimized = _load_json(optimized_path)
    violations = check_gate(baseline, optimized)
    if violations:
        for v in violations:
            print(f"gate: {v}")
        return None
    return compare_reports(baseline, optimized)


def _cmd_compare(args: argparse.Namespace) -> int:
    result = _run_gate_and_compare(pathlib.Path(args.baseline), pathlib.Path(args.optimized))
    if result is None:
        return 1
    for scenario, metrics in sorted(result["metrics"].items()):
        for name, row in sorted(metrics.items()):
            print(
                f"{scenario}/{name}: improvement={row['improvement_pct']} "
                f"base={row['baseline_grade']} opt={row['optimized_grade']} ok={row['ok']}"
            )
    print(f"PASS={result['pass']}")
    if args.output:
        pathlib.Path(args.output).write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )
    return 0 if result["pass"] else 1


def _cmd_verify(args: argparse.Namespace) -> int:
    bdir = pathlib.Path(args.baseline_dir)
    odir = pathlib.Path(args.optimized_dir)
    pairs = []
    for baseline_path in sorted(bdir.glob("Baseline.*.json")):
        board = baseline_path.name[len("Baseline."):-len(".json")]
        optimized_path = odir / f"Optimized.{board}.json"
        pairs.append((board, baseline_path, optimized_path))
    if not pairs:
        print("verify: no Baseline.*.json found", file=sys.stderr)
        return 2
    all_pass = True
    for board, baseline_path, optimized_path in pairs:
        if not optimized_path.exists():
            print(f"{board}: missing optimized report", file=sys.stderr)
            all_pass = False
            continue
        result = _run_gate_and_compare(baseline_path, optimized_path)
        if result is None:
            print(f"{board}: GATE-FAIL")
            all_pass = False
            continue
        print(f"{board}: {'PASS' if result['pass'] else 'FAIL'}")
        all_pass = all_pass and result["pass"]
    return 0 if all_pass else 1


def _cmd_doc_load(args: argparse.Namespace) -> int:
    """v1.8 section 3.1: T_doc_load comes from parsing the PROFILE log.

    Section 3.1 states these segments are not hand-written on the device, so
    this reads only the section 1.3 probe output. A capture with no profile
    lines yields a report with no stages rather than an error, because a run
    that never loaded a document is a legitimate result.
    """
    from brookesia_gui_perf.profile import parse_profile_log

    with open(args.log, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    result = parse_profile_log(text)
    payload = {
        "test_suite": "ESP-Brookesia-Perf",
        "doc_load": result.as_dict(),
        "log_integrity": assess_log_integrity(text),
    }
    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2, sort_keys=True)
        print(f"wrote {args.output} ({len(result.segments)} segments)")
    else:
        print(json.dumps(payload, indent=2, sort_keys=True))

    if result.undecodable:
        print(
            f"warning: {result.undecodable} profile line(s) could not be decoded",
            file=sys.stderr,
        )
    return 0


def _cmd_coverage(args: argparse.Namespace) -> int:
    path = pathlib.Path(args.report)
    report = _load_json(path)
    coverage = assess_coverage(report)
    if args.sizes:
        sizes = _load_json(pathlib.Path(args.sizes))
        if sizes.get("app_bin_bytes") is not None:
            coverage["firmware_bin_bytes"] = {
                "status": COLLECTED, "value": sizes["app_bin_bytes"]
            }
        if sizes.get("resource_bytes") is not None:
            coverage["resource_image_bytes"] = {
                "status": COLLECTED, "value": sizes["resource_bytes"]
            }
    if args.write:
        report.setdefault("metadata", {})["acceptance"] = coverage
        report["metadata"]["dimension_coverage"] = assess_dimensions(report)
        report["metadata"]["stat_exemptions"] = assess_exemptions(report)
        path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
    for field, entry in coverage.items():
        reason = f" -- {entry['reason']}" if entry.get("reason") else ""
        print(f"{field}: {entry['status']}{reason}")
    counts = summarize(coverage)
    print(
        f"collected={counts[COLLECTED]} not_applicable={counts[NOT_APPLICABLE]} "
        f"pending_instrumentation={counts[PENDING]}"
    )
    dims = report.get("metadata", {}).get("dimension_coverage") or assess_dimensions(report)
    for name, body in dims["dimensions"].items():
        print(f"{name}: {body['status']} -- {body['question']}")
    print(f"dimension_complete={not dims['missing']} missing={dims['missing']}")
    if args.strict and counts[PENDING]:
        print(f"strict: {counts[PENDING]} field(s) pending instrumentation", file=sys.stderr)
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="brookesia-gui-perf")
    sub = parser.add_subparsers(dest="command", required=True)

    p_agg = sub.add_parser("aggregate", help="serial log -> spec51 report")
    p_agg.add_argument("log")
    p_agg.add_argument("-o", "--output", required=True)
    p_agg.add_argument("--warmup", type=int, default=DEFAULT_WARMUP)
    p_agg.set_defaults(func=_cmd_aggregate)

    p_prof = sub.add_parser(
        "doc-load",
        help="spec33 T_doc_load segments parsed from section 1.3 PROFILE log",
    )
    p_prof.add_argument("log")
    p_prof.add_argument("-o", "--output")
    p_prof.set_defaults(func=_cmd_doc_load)

    p_gate = sub.add_parser("gate", help="spec52 metadata gate")
    p_gate.add_argument("baseline")
    p_gate.add_argument("optimized")
    p_gate.set_defaults(func=_cmd_gate)

    p_cmp = sub.add_parser("compare", help="gate + improvement verdict")
    p_cmp.add_argument("baseline")
    p_cmp.add_argument("optimized")
    p_cmp.add_argument("-o", "--output")
    p_cmp.set_defaults(func=_cmd_compare)

    p_ver = sub.add_parser("verify", help="all-board gate + compare")
    p_ver.add_argument("--baseline-dir", required=True)
    p_ver.add_argument("--optimized-dir", required=True)
    p_ver.set_defaults(func=_cmd_verify)

    p_cov = sub.add_parser(
        "coverage", help="v1.8 5.1 canonical-field coverage / N-A declarations"
    )
    p_cov.add_argument("report")
    p_cov.add_argument("--write", action="store_true", help="embed acceptance block into report")
    p_cov.add_argument("--sizes", help="measure_size.py record: fills firmware/resource bytes")
    p_cov.add_argument(
        "--strict", action="store_true", help="fail if any field is pending_instrumentation"
    )
    p_cov.set_defaults(func=_cmd_coverage)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (ValueError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
