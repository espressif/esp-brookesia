# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Parse the v1.8 section 1.3 PROFILE log into T_doc_load stage segments.

v1.8 section 3.1 is explicit that T_doc_load segmentation, start_app stage
timings and T_resource_seg are *not* hand-written on the device:

    "T_doc_load 分段、start_app 阶段耗时、T_resource_seg 分段不写手工代码,
     直接解析 §1.3 开关输出的 PROFILE 日志"

So the device side only emits lines and the numbers are recovered here. The
probes live in gui/brookesia_gui_interface/src/parser.cpp and
gui/brookesia_gui_lvgl/src/style_font.cpp and are gated on
CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG.

Wire format, as emitted by the probes:

    I (73321) GuiIntf: GUI parser profile: base_dir(/app), stage(parse_root_json),
        bytes(2048), elapsed_ms(4412), total_ms(4412)

Fields are key(value) pairs, values are bare or single-quoted. A trailing
carriage return and the ESP-IDF log prefix are both stripped before matching,
because a capture routinely loses the newline and concatenates records.
"""

import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional

# "GUI parser profile: base_dir(...), stage(...), elapsed_ms(123), total_ms(456)"
# The prefix is matched loosely on purpose: probes in different components use
# their own tag, and a stricter match would silently drop whole stages.
_PROFILE_RE = re.compile(r"profile:\s*base_dir\(")

# key(value) where value is 'quoted' or bare-without-commas-or-parens.
_FIELD_RE = re.compile(r"(\w+)\(([^()]*)\)")


@dataclass
class StageSegment:
    """One parser stage, with the wall time it took."""

    stage: str
    base_dir: str
    elapsed_ms: float
    total_ms: Optional[float] = None
    #: Stage-specific counters the probe chose to report, e.g. assets/images.
    counters: Dict[str, float] = field(default_factory=dict)

    def as_dict(self) -> Dict[str, object]:
        out: Dict[str, object] = {
            "stage": self.stage,
            "base_dir": self.base_dir,
            "elapsed_ms": self.elapsed_ms,
        }
        if self.total_ms is not None:
            out["total_ms"] = self.total_ms
        if self.counters:
            out["counters"] = dict(self.counters)
        return out


@dataclass
class ProfileLog:
    segments: List[StageSegment] = field(default_factory=list)
    #: Lines that looked like a profile line but could not be decoded.
    undecodable: int = 0

    def stages(self) -> List[str]:
        seen: List[str] = []
        for seg in self.segments:
            if seg.stage not in seen:
                seen.append(seg.stage)
        return seen

    def by_stage(self) -> Dict[str, List[StageSegment]]:
        out: Dict[str, List[StageSegment]] = {}
        for seg in self.segments:
            out.setdefault(seg.stage, []).append(seg)
        return out

    def total_parse_ms(self) -> Optional[float]:
        """Wall time for a full document load, from the last stage's total_ms.

        The probes report a running total_ms, so the maximum seen is the
        document load time; summing stages would double count nested work.
        """
        totals = [s.total_ms for s in self.segments if s.total_ms is not None]
        return max(totals) if totals else None

    def as_dict(self) -> Dict[str, object]:
        out: Dict[str, object] = {
            "stages": [s.as_dict() for s in self.segments],
        }
        total = self.total_parse_ms()
        if total is not None:
            out["doc_load_ms"] = total
        if self.undecodable:
            out["undecodable"] = self.undecodable
        return out


def _to_float(text: str) -> Optional[float]:
    try:
        return float(text)
    except ValueError:
        return None


def _parse_one(chunk: str) -> Optional[StageSegment]:
    fields = dict(_FIELD_RE.findall(chunk))
    if "stage" not in fields or "elapsed_ms" not in fields:
        return None

    elapsed = _to_float(fields["elapsed_ms"])
    if elapsed is None:
        return None

    counters: Dict[str, float] = {}
    for key, value in fields.items():
        if key in ("stage", "base_dir", "elapsed_ms", "total_ms"):
            continue
        number = _to_float(value)
        if number is not None:
            counters[key] = number

    return StageSegment(
        stage=fields["stage"],
        base_dir=fields.get("base_dir", ""),
        elapsed_ms=elapsed,
        total_ms=_to_float(fields.get("total_ms", "")),
        counters=counters,
    )


def parse_profile_log(text: str) -> ProfileLog:
    """Recover T_doc_load stage segments from a serial capture.

    Never raises on a malformed line: a damaged capture is reported through
    ProfileLog.undecodable so the caller can refuse to use it, which is the
    same contract assess_log_integrity follows.
    """
    result = ProfileLog()
    for raw in text.splitlines():
        line = raw.rstrip("\r")
        starts = [m.start() for m in _PROFILE_RE.finditer(line)]
        if not starts:
            continue
        # A capture that lost a newline glues several records onto one line.
        # Splitting on every "profile:" occurrence recovers them all; scanning
        # the whole line as one record would let the later duplicate keys
        # overwrite the earlier ones via dict() and silently drop a stage.
        bounds = starts + [len(line)]
        for begin, end in zip(starts, bounds[1:]):
            segment = _parse_one(line[begin:end])
            if segment is None:
                result.undecodable += 1
            else:
                result.segments.append(segment)
    return result
