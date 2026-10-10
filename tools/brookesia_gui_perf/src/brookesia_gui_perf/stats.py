# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import math
import statistics
from typing import Dict, Sequence

CV_LIMIT = 0.05
MIN_SAMPLES = 30
DEFAULT_WARMUP = 3


def _percentile(ordered: Sequence[float], pct: float) -> float:
    """Linear-interpolation percentile (numpy default method)."""
    if not ordered:
        raise ValueError("percentile of empty sequence")
    if len(ordered) == 1:
        return float(ordered[0])
    k = (len(ordered) - 1) * (pct / 100.0)
    low = math.floor(k)
    high = math.ceil(k)
    if low == high:
        return float(ordered[int(k)])
    return float(ordered[low] + (ordered[high] - ordered[low]) * (k - low))


def aggregate(values: Sequence[float], *, min_samples: int = MIN_SAMPLES) -> Dict[str, float]:
    if len(values) < min_samples:
        raise ValueError(f"aggregate requires at least {min_samples} samples, got {len(values)}")
    ordered = sorted(float(v) for v in values)
    n = len(ordered)
    mean = statistics.fmean(ordered)
    std = statistics.stdev(ordered) if n >= 2 else 0.0
    if std == 0:
        cv = 0.0
    elif mean != 0:
        cv = std / mean
    else:
        cv = math.inf
    return {
        "mean": mean,
        "std": std,
        "cv": cv,
        "p50": _percentile(ordered, 50),
        "p75": _percentile(ordered, 75),
        "p95": _percentile(ordered, 95),
        "p99": _percentile(ordered, 99),
        "n": n,
    }


def cv_ok(stats: Dict[str, float], limit: float = CV_LIMIT) -> bool:
    return stats["cv"] <= limit
