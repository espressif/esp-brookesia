# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

PREFIX = "[BENCH] "

# ESP-IDF serial header "I (73321) Tag: : msg". Deliberately unanchored: a lost
# newline shows up as a second header later in the same line, and an anchored
# pattern could never observe it.
LOG_HEADER_RE = re.compile(r"[A-Z] \((\d+)\) ")


@dataclass
class RunRecords:
    metadata: Optional[Dict[str, Any]] = None
    samples: List[Dict[str, object]] = field(default_factory=list)
    malformed: int = 0


def assess_log_integrity(text: str) -> Dict[str, int]:
    """Count serial-capture defects that would bias any timing taken from a log.

    Callers must surface these instead of aggregating regardless: watchdog
    register dumps alone reach ~13% of lines on this board, and the resulting
    UART congestion both concatenates lines and reorders timestamps.
    """
    total = 0
    concatenated = 0
    regressions = 0
    last_ts: Optional[int] = None
    for line in text.splitlines():
        if not line.strip():
            continue
        total += 1
        headers = LOG_HEADER_RE.findall(line)
        if len(headers) > 1:
            concatenated += 1
        if headers:
            ts = int(headers[0])
            if last_ts is not None and ts < last_ts:
                regressions += 1
            last_ts = max(last_ts, ts) if last_ts is not None else ts
    return {
        "lines": total,
        "concatenated_lines": concatenated,
        "timestamp_regressions": regressions,
    }


def parse_log(text: str) -> RunRecords:
    records = RunRecords()
    for line in text.splitlines():
        idx = line.find(PREFIX)
        if idx < 0:
            continue
        # UART congestion puts two records on one line whenever the emitter writes
        # back-to-back, so the tail after the first prefix is not necessarily one
        # JSON document. Each subsequent prefix starts the next record.
        payloads = line[idx:].split(PREFIX)
        for payload in payloads:
            payload = payload.strip()
            if not payload:
                continue
            try:
                obj = json.loads(payload)
            except json.JSONDecodeError:
                # The framework logs through an unconditional printf
                # (BROOKESIA_LOGI_IMPL_FUNC has no level gate), so it interleaves with
                # these records on the shared stdout and a long record can be torn
                # mid-JSON. Losing one record is not a reason to discard every intact
                # one, and discarding it silently is not acceptable either, so it is
                # counted here and reported as log_integrity.malformed_records.
                records.malformed += 1
                continue
            rec_type = obj.get("record")
            if rec_type == "metadata":
                if records.metadata is not None:
                    raise ValueError("duplicate metadata record")
                records.metadata = obj
            elif rec_type == "sample":
                if "scenario" not in obj or "metrics" not in obj:
                    raise ValueError(f"sample record missing scenario/metrics: {obj!r}")
                records.samples.append(obj)
            else:
                raise ValueError(f"unknown record type: {rec_type!r}")
    # A log with zero [BENCH] lines (e.g. a boot log captured before the
    # device-side emitter lands) yields empty records; only bench samples
    # without a metadata record are an error.
    if records.metadata is None and records.samples:
        raise ValueError("no metadata record in log")
    return records
