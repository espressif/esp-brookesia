#!/usr/bin/env python
#
# G4 firmware / resource size gate for the ESP-Brookesia GUI performance work.
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Spec v1.8 section 3.7 requires every PR touching gui/, system/, app/, assets or
# sdkconfig to carry paired size data (app.bin bytes plus the idf.py size
# breakdown) under the same board and config, and section 6 item 9 rejects a
# performance/memory PR that omits it. Section 4 grades two size rows against the
# baseline: Pass-A <= -5%, Pass-B <= 0%, Fail > +10%, and the first round only
# records.
#
# Size figures must come from an idf.py target build. The repository-root
# CMakeLists.txt is a host aggregator for PC development and is explicitly not a
# valid source for these numbers.

import argparse
import math
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

# Spec v1.8 section 4, "体积" rows. direction is lower_better.
SIZE_PASS_A_PCT = -5.0
SIZE_FAIL_PCT = 10.0

# Resource trees staged into the littlefs image for the Golden Case product.
# Overridable so other products can reuse the script.
DEFAULT_RESOURCE_DIRS = [
    "system/brookesia_system_super/resource/startup",
    "system/brookesia_system_super/resource/shell",
    "system/brookesia_system_super/resource/fonts",
    "system/brookesia_system_super/resource/themes",
    "app/brookesia_app_settings/package/res",
    "app/brookesia_app_files/package/res",
    "app/brookesia_app_store/package/res",
]

# The v1.8 section 3.7 text names ".flash.bss" and ".dram0.data". IDF 6.2's
# "idf.py size --format json2" does not emit those names; it reports per-region
# parts instead. These are the region/part pairs that carry the same
# information, and they are what this script records.
REGION_PART_ALIASES = {
    "flash_bss": ("External RAM", ".tbss"),
    "dram0_data": ("DIRAM", ".data"),
    "dram0_bss": ("DIRAM", ".bss"),
    "iram0_text": ("HP core RAM", ".text"),
}


def parse_size_json(text: str) -> Dict[str, Any]:
    """Parse `idf.py size --format json2` output.

    The command prefixes non-JSON NOTE lines and may append trailing non-JSON
    text, so the first JSON object is extracted with a raw decoder instead of
    json.loads on the whole payload.
    """
    start = text.find("{")
    if start < 0:
        raise ValueError("no JSON object found in idf.py size output")
    doc, _ = json.JSONDecoder().raw_decode(text, start)
    layout = doc.get("layout") or []
    regions: Dict[str, Any] = {}
    parts: Dict[str, int] = {}
    for entry in layout:
        name = entry.get("name")
        if not name:
            continue
        regions[name] = {
            "total": entry.get("total"),
            "used": entry.get("used"),
            "free": entry.get("free"),
        }
        for part_name, part in (entry.get("parts") or {}).items():
            parts[f"{name}::{part_name}"] = part.get("size")
    return {
        "total_size": doc.get("total_size"),
        "regions": regions,
        "parts": parts,
    }


def aliased_parts(parsed: Dict[str, Any]) -> Dict[str, Optional[int]]:
    """Expose the spec-named size rows from the region/part map."""
    parts = parsed.get("parts") or {}
    out: Dict[str, Optional[int]] = {}
    for alias, (region, part) in REGION_PART_ALIASES.items():
        out[alias] = parts.get(f"{region}::{part}")
    return out


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def dir_bytes(path: Path) -> int:
    """Recursive byte total of a directory tree; 0 when absent."""
    if not path.is_dir():
        return 0
    return sum(p.stat().st_size for p in path.rglob("*") if p.is_file())


def find_single(root: Path, pattern: str) -> Optional[Path]:
    matches = sorted(root.glob(pattern))
    return matches[0] if matches else None


def measure(
    build_dir: Path,
    *,
    repo_root: Path,
    app_bin: Optional[str] = None,
    littlefs_bin: Optional[str] = None,
    sdkconfig: Optional[str] = None,
    profile: str = "debug",
    resource_dirs: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Collect one size record from an idf.py build directory."""
    if not build_dir.is_dir():
        raise ValueError(f"build dir not found: {build_dir}")

    app_path = Path(app_bin) if app_bin else find_single(build_dir, "*.bin")
    littlefs_path = Path(littlefs_bin) if littlefs_bin else find_single(build_dir, "littlefs_data.bin")
    sdkconfig_path = Path(sdkconfig) if sdkconfig else build_dir / "config" / "sdkconfig.json"

    if app_path is None or not app_path.is_file():
        raise ValueError(f"no app .bin found under {build_dir}")
    if sdkconfig_path is None or not sdkconfig_path.is_file():
        raise ValueError(f"sdkconfig not found: {sdkconfig_path}")

    record: Dict[str, Any] = {
        "app_bin": str(app_path),
        "app_bin_bytes": app_path.stat().st_size,
        "littlefs_bin_bytes": littlefs_path.stat().st_size if littlefs_path and littlefs_path.is_file() else None,
        "sdkconfig_sha256": sha256_file(sdkconfig_path),
        "build_profile": profile,
    }

    dirs = resource_dirs if resource_dirs is not None else DEFAULT_RESOURCE_DIRS
    per_dir = {}
    total = 0
    for rel in dirs:
        size = dir_bytes(repo_root / rel)
        per_dir[rel] = size
        total += size
    record["resource_dirs"] = per_dir
    record["resource_bytes"] = total
    return record


def add_idf_size(record: Dict[str, Any], size_text: str) -> Dict[str, Any]:
    """Attach the parsed idf.py size breakdown to a record (in place)."""
    parsed = parse_size_json(size_text)
    record["total_image_size"] = parsed["total_size"]
    record["regions"] = parsed["regions"]
    record["parts"] = parsed["parts"]
    record["spec_named_parts"] = aliased_parts(parsed)
    return record


def delta_pct(baseline: float, candidate: float) -> float:
    if baseline == 0:
        raise ValueError("zero baseline cannot produce a size ratio")
    return (candidate - baseline) / baseline * 100.0


def grade(delta: float, *, first_round: bool) -> str:
    """Grade a size delta per spec v1.8 section 4 (lower_better)."""
    if first_round:
        return "record"
    if delta <= SIZE_PASS_A_PCT:
        return "pass_a"
    # Section 4 defines Pass-B as <= 0% and Fail as > +10%. Growth inside (0, 10]
    # is a watch band, not a failure: grading it "fail" rejected every harmless
    # alignment change as soon as the first-round flag was removed.
    if delta <= SIZE_FAIL_PCT:
        return "pass_b"
    return "fail"


def _valid_size(value: object) -> bool:
    """A usable mandatory size row: a finite, positive number (bool is not a size)."""
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
        and value > 0
    )


def _valid_optional_size(value: object) -> bool:
    """A usable optional size row: a finite, non-negative measurement.

    0 is a legal reading: measure(resource_dirs=[]) on a product without staged
    resources measures resource_bytes as 0.
    """
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
        and value >= 0
    )


def compare(baseline: Dict[str, Any], candidate: Dict[str, Any], *, first_round: bool = False) -> Dict[str, Any]:
    """Paired size comparison over the spec's two size rows."""
    rows: Dict[str, Any] = {}
    compared_rows = 0
    for key in ("app_bin_bytes", "littlefs_bin_bytes", "resource_bytes", "total_image_size"):
        base = baseline.get(key)
        cand = candidate.get(key)
        if key == "app_bin_bytes" and not (
            _valid_size(base) and _valid_size(cand)
        ):
            # The app binary is the mandatory row: once the gate is armed, a pair
            # without it on either side has nothing firmware-shaped to compare, so
            # equal optional rows (resource_bytes) must not make it pass.
            rows[key] = {"baseline": base, "candidate": cand, "delta_pct": None,
                         "grade": "record" if first_round else "fail"}
            continue
        if base is None and cand is None:
            # Both arms omitted this optional row: nothing to compare, nothing to pass.
            rows[key] = {"baseline": base, "candidate": cand, "delta_pct": None, "grade": "record"}
            continue
        if not _valid_optional_size(base) or not _valid_optional_size(cand):
            # One-sided or unusable data must fail closed once the gate is armed:
            # silently recording it let a truncated candidate slip through. The
            # first round only records, so incomplete artifacts stay non-blocking.
            rows[key] = {"baseline": base, "candidate": cand, "delta_pct": None,
                         "grade": "record" if first_round else "fail"}
            continue
        if (base == 0) != (cand == 0):
            # Resource appeared or disappeared: no ratio can be defined, and the
            # image changed shape, so this is not an improvement to score. The
            # first round only records.
            rows[key] = {"baseline": base, "candidate": cand, "delta_pct": None,
                         "grade": "record" if first_round else "fail"}
            continue
        if base == 0 and cand == 0:
            # A resource-less product measured twice: legal 0 -> 0, no change.
            rows[key] = {"baseline": base, "candidate": cand, "delta_pct": None, "grade": "record"}
            continue
        delta = delta_pct(float(base), float(cand))
        compared_rows += 1
        rows[key] = {
            "baseline": base,
            "candidate": cand,
            "delta_pct": round(delta, 4),
            "grade": grade(delta, first_round=first_round),
        }

    base_sha = baseline.get("sdkconfig_sha256")
    cand_sha = candidate.get("sdkconfig_sha256")
    base_board = baseline.get("board_id")
    cand_board = candidate.get("board_id")
    # Two missing values compare equal, but prove nothing about a paired build.
    sdk_match = isinstance(base_sha, str) and bool(base_sha) and base_sha == cand_sha
    board_ok = isinstance(base_board, str) and bool(base_board) and base_board == cand_board
    fail_rows = [k for k, v in rows.items() if v["grade"] == "fail"]
    has_comparable_rows = compared_rows > 0
    return {
        # Section 4: the first round only records and must never Fail, even when
        # identity hashes or optional rows are missing. Once armed, a pair with no
        # comparable size row must not pass just because its identity fields match.
        "pass": True if first_round else (
            not fail_rows and sdk_match and board_ok and has_comparable_rows
        ),
        "sdkconfig_match": sdk_match,
        "board_match": board_ok,
        "compared_rows": compared_rows,
        "fail_rows": fail_rows,
        "rows": rows,
    }


def _cmd_measure(args: argparse.Namespace) -> int:
    record = measure(
        Path(args.build_dir),
        repo_root=Path(args.repo_root),
        app_bin=args.app_bin,
        littlefs_bin=args.littlefs_bin,
        sdkconfig=args.sdkconfig,
        profile=args.profile,
        resource_dirs=args.resource_dir,
    )
    if args.idf_size_json:
        record = add_idf_size(record, Path(args.idf_size_json).read_text(encoding="utf-8", errors="replace"))
    if args.board_id:
        record["board_id"] = args.board_id
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    for key in ("app_bin_bytes", "littlefs_bin_bytes", "resource_bytes", "total_image_size"):
        if record.get(key) is not None:
            print(f"  {key} = {record[key]}")
    return 0


def _cmd_compare(args: argparse.Namespace) -> int:
    baseline = json.loads(Path(args.baseline).read_text(encoding="utf-8"))
    candidate = json.loads(Path(args.candidate).read_text(encoding="utf-8"))
    result = compare(baseline, candidate, first_round=args.first_round)
    for key, row in sorted(result["rows"].items()):
        print(f"{key}: {row['baseline']} -> {row['candidate']} delta={row['delta_pct']}% grade={row['grade']}")
    if not result["sdkconfig_match"]:
        print("violation: sdkconfig_sha256 differs between the pair")
    if not result["board_match"]:
        print("violation: board_id differs between the pair")
    print(f"PASS={result['pass']}")
    if args.output:
        Path(args.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0 if result["pass"] else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="measure_size.py", description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    p_measure = sub.add_parser("measure", help="collect one size record from an idf.py build dir")
    p_measure.add_argument("--build-dir", required=True)
    p_measure.add_argument("--repo-root", default=".")
    p_measure.add_argument("--app-bin")
    p_measure.add_argument("--littlefs-bin")
    p_measure.add_argument("--sdkconfig")
    p_measure.add_argument("--profile", default="debug")
    p_measure.add_argument("--board-id")
    p_measure.add_argument("--idf-size-json", help="file holding `idf.py size --format json2` output")
    p_measure.add_argument("--resource-dir", action="append", help="repeatable; overrides the defaults")
    p_measure.add_argument("-o", "--output", required=True)
    p_measure.set_defaults(func=_cmd_measure)

    p_compare = sub.add_parser("compare", help="paired size comparison")
    p_compare.add_argument("baseline")
    p_compare.add_argument("candidate")
    p_compare.add_argument("--first-round", action="store_true", help="record only, never Fail")
    p_compare.add_argument("-o", "--output")
    p_compare.set_defaults(func=_cmd_compare)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except (ValueError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
