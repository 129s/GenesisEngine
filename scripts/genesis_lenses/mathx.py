from __future__ import annotations

import math
import statistics
from typing import Iterable


def safe_div(num: float, den: float) -> float:
    if den == 0.0:
        return 0.0
    return num / den


def entropy_bits(counts: Iterable[int]) -> float:
    xs = [float(c) for c in counts if c > 0]
    total = sum(xs)
    if total <= 0.0:
        return 0.0
    h = 0.0
    for c in xs:
        p = c / total
        h -= p * math.log2(p)
    return h


def zscore_series(xs: list[float]) -> list[float]:
    if not xs:
        return []
    if len(xs) == 1:
        return [0.0]
    mean = statistics.fmean(xs)
    stdev = statistics.pstdev(xs)
    if stdev <= 1e-12:
        return [0.0 for _ in xs]
    return [(x - mean) / stdev for x in xs]


def mean_or_none(xs: Iterable[float]) -> float | None:
    ys = [float(x) for x in xs]
    if not ys:
        return None
    return statistics.fmean(ys)

