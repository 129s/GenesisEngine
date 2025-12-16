#!/usr/bin/env python3
"""
Data-driven motif mining from Worldline Chronicle (JSONL) without a predefined motif taxonomy.

Inputs: one or more `worldline_*.jsonl` files produced by `genesis-runtime-cli soak --worldline-out`.
Outputs: a JSON report containing:
  - discovered directed lag-1 edges between numeric window features (time-lag correlation)
  - discovered length-2 motif chains (A->B->C over 3 consecutive windows) based on "high state" activation
  - per-motif D(w,m) (coverage over windows), aggregated across worlds

Design goals:
  - Avoid hand-crafted "phase types" and manual motif definitions.
  - Use only features that are already emitted by worldline windows (metrics + generic count summaries).
  - Keep dependencies to Python stdlib only.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import statistics
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from glob import glob
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


def _is_number(x: object) -> bool:
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(float(x))


def _quantile(sorted_values: Sequence[float], q: float) -> float:
    if not sorted_values:
        return 0.0
    if q <= 0:
        return float(sorted_values[0])
    if q >= 1:
        return float(sorted_values[-1])
    # Linear interpolation between closest ranks.
    n = len(sorted_values)
    pos = (n - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return float(sorted_values[lo])
    frac = pos - lo
    return float(sorted_values[lo]) * (1.0 - frac) + float(sorted_values[hi]) * frac


@dataclass(frozen=True)
class WorldlineWindow:
    index: int
    metrics: Dict[str, float]
    counts: Dict[str, Dict[str, float]]


@dataclass(frozen=True)
class WorldlineRun:
    path: str
    meta: Dict[str, object]
    windows: List[WorldlineWindow]


def _read_worldline(path: str) -> WorldlineRun:
    meta: Dict[str, object] = {}
    windows: List[WorldlineWindow] = []

    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            kind = obj.get("kind")
            if kind == "runtime_worldline_meta":
                meta = obj
                continue
            if kind != "runtime_worldline_window":
                continue

            idx = int(obj.get("index", len(windows)))
            metrics_in = obj.get("metrics", {}) or {}
            counts_in = obj.get("counts", {}) or {}

            metrics: Dict[str, float] = {}
            for k, v in metrics_in.items():
                if _is_number(v):
                    metrics[str(k)] = float(v)

            counts: Dict[str, Dict[str, float]] = {}
            for group_key, group_val in counts_in.items():
                if not isinstance(group_val, dict):
                    continue
                inner: Dict[str, float] = {}
                for k, v in group_val.items():
                    if _is_number(v):
                        inner[str(k)] = float(v)
                counts[str(group_key)] = inner

            windows.append(WorldlineWindow(index=idx, metrics=metrics, counts=counts))

    windows.sort(key=lambda w: w.index)
    return WorldlineRun(path=path, meta=meta, windows=windows)


def _list_inputs(input_path: str, pattern: str) -> List[str]:
    if os.path.isdir(input_path):
        return sorted(glob(os.path.join(input_path, pattern)))
    if os.path.isfile(input_path):
        return [input_path]
    # Allow passing a glob directly as input_path.
    expanded = sorted(glob(input_path))
    if expanded:
        return expanded
    raise FileNotFoundError(f"Input not found: {input_path}")


def _flatten_features(run: WorldlineRun) -> Dict[str, List[float]]:
    """
    Produce a purely numeric per-window feature vector without referencing domain labels.
    - Start from `metrics.*` numeric keys.
    - Add generic summaries for `counts.*` groups (totals, distinct kinds, top share).
    """
    by_name: Dict[str, List[float]] = {}

    def push(name: str, value: float) -> None:
        by_name.setdefault(name, []).append(float(value))

    for w in run.windows:
        # metrics
        for k, v in w.metrics.items():
            push(f"m.{k}", v)

        # counts summaries (domain-agnostic)
        for group, mapping in w.counts.items():
            total = sum(mapping.values())
            kinds = float(len([x for x in mapping.values() if x > 0.0]))
            top = max(mapping.values()) if mapping else 0.0
            top_share = (top / total) if total > 0 else 0.0
            push(f"c.{group}.total", total)
            push(f"c.{group}.kinds", kinds)
            push(f"c.{group}.top_share", top_share)

    # Drop features that are missing in some windows (ensure aligned lengths).
    n = len(run.windows)
    filtered: Dict[str, List[float]] = {k: v for k, v in by_name.items() if len(v) == n}
    return filtered


@dataclass
class CorrStats:
    n: int = 0
    sum_x: float = 0.0
    sum_y: float = 0.0
    sum_x2: float = 0.0
    sum_y2: float = 0.0
    sum_xy: float = 0.0

    def add(self, x: float, y: float) -> None:
        self.n += 1
        self.sum_x += x
        self.sum_y += y
        self.sum_x2 += x * x
        self.sum_y2 += y * y
        self.sum_xy += x * y

    def corr(self) -> float:
        if self.n < 3:
            return 0.0
        n = float(self.n)
        mean_x = self.sum_x / n
        mean_y = self.sum_y / n
        var_x = (self.sum_x2 / n) - mean_x * mean_x
        var_y = (self.sum_y2 / n) - mean_y * mean_y
        if var_x <= 1e-12 or var_y <= 1e-12:
            return 0.0
        cov = (self.sum_xy / n) - mean_x * mean_y
        return float(cov / math.sqrt(var_x * var_y))


def _build_lag_corr(runs: Sequence[WorldlineRun], feature_names: Sequence[str]) -> Dict[Tuple[str, str], float]:
    stats: Dict[Tuple[str, str], CorrStats] = {}

    for run in runs:
        f = _flatten_features(run)
        n = len(run.windows)
        if n < 3:
            continue
        # Only use features present in this run.
        present = [name for name in feature_names if name in f]
        for a in present:
            xa = f[a]
            for b in present:
                if a == b:
                    continue
                key = (a, b)
                st = stats.get(key)
                if st is None:
                    st = CorrStats()
                    stats[key] = st
                yb = f[b]
                # lag-1: x at t, y at t+1
                for t in range(0, n - 1):
                    st.add(xa[t], yb[t + 1])

    return {k: v.corr() for k, v in stats.items()}


def _sign(x: float) -> int:
    if x > 0:
        return 1
    if x < 0:
        return -1
    return 0


def _edge_support(runs: Sequence[WorldlineRun], a: str, b: str) -> float:
    """Fraction of runs where corr(a(t),b(t+1)) has the same sign as the pooled correlation."""
    pooled = 0.0
    pooled_n = 0
    for run in runs:
        f = _flatten_features(run)
        if a not in f or b not in f:
            continue
        xa = f[a]
        yb = f[b]
        st = CorrStats()
        for t in range(0, len(xa) - 1):
            st.add(xa[t], yb[t + 1])
        c = st.corr()
        if abs(c) < 1e-9:
            continue
        pooled += c
        pooled_n += 1
    if pooled_n == 0:
        return 0.0
    target = _sign(pooled / float(pooled_n))
    agree = 0
    total = 0
    for run in runs:
        f = _flatten_features(run)
        if a not in f or b not in f:
            continue
        xa = f[a]
        yb = f[b]
        st = CorrStats()
        for t in range(0, len(xa) - 1):
            st.add(xa[t], yb[t + 1])
        c = st.corr()
        if abs(c) < 1e-9:
            continue
        total += 1
        if _sign(c) == target:
            agree += 1
    return float(agree) / float(total) if total else 0.0


def _compute_thresholds(values: Sequence[float], q_high: float) -> Tuple[float, float]:
    sv = sorted(values)
    hi = _quantile(sv, q_high)
    lo = _quantile(sv, 1.0 - q_high)
    return hi, lo


def _edge_active_mask(values_a: Sequence[float], values_b: Sequence[float], hi_a: float, hi_b: float, lo_b: float, sign: int) -> List[bool]:
    n = min(len(values_a), len(values_b))
    active: List[bool] = []
    if n < 2:
        return active
    for t in range(0, n - 1):
        if values_a[t] < hi_a:
            active.append(False)
            continue
        if sign >= 0:
            active.append(values_b[t + 1] >= hi_b)
        else:
            active.append(values_b[t + 1] <= lo_b)
    return active


def _zscore(values: Sequence[float]) -> List[float]:
    if not values:
        return []
    mean = statistics.fmean(values)
    var = 0.0
    for v in values:
        dv = v - mean
        var += dv * dv
    var /= float(len(values))
    if var <= 1e-12:
        return [0.0 for _ in values]
    inv = 1.0 / math.sqrt(var)
    return [(v - mean) * inv for v in values]


def _alignment_mask(values_a: Sequence[float], values_b: Sequence[float], sign: int, q: float) -> List[bool]:
    """
    Data-driven activation for an edge a(t) -> b(t+1).

    We avoid hard-coding "source high" vs "source low" by using an alignment score:
      score_t = z(a_t) * (sign * z(b_{t+1}))
    where sign=+1 for positive relation, -1 for negative relation.

    Edge is active when score_t is in the top-q quantile of its own scores within the run.
    """
    n = min(len(values_a), len(values_b))
    if n < 2:
        return []
    za = _zscore(values_a)
    zb = _zscore(values_b)
    scores: List[float] = []
    for t in range(0, n - 1):
        scores.append(za[t] * (float(sign) * zb[t + 1]))
    sorted_scores = sorted(scores)
    threshold = _quantile(sorted_scores, q)
    return [s >= threshold for s in scores]


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, help="Worldline file, folder, or glob (JSONL).")
    ap.add_argument("--pattern", default="worldline_*.jsonl", help="Pattern when --input is a folder.")
    ap.add_argument("--top-edges", type=int, default=30, help="Keep top-K directed edges by |lag-corr|.")
    ap.add_argument("--top-motifs", type=int, default=40, help="Keep top-K motifs by support+coverage score.")
    ap.add_argument("--direction-margin", type=float, default=0.02, help="Minimum |corr_ab|-|corr_ba| to pick a direction.")
    ap.add_argument("--state-quantile", type=float, default=0.9, help="Quantile for 'high state' activation (low=1-q).")
    ap.add_argument("--min-windows", type=int, default=6, help="Ignore runs with fewer windows than this.")
    ap.add_argument("--out", default="", help="Output JSON path (default: out/worldline/analysis/motifs_*.json).")
    args = ap.parse_args(argv)

    input_paths = _list_inputs(args.input, args.pattern)
    runs: List[WorldlineRun] = []
    for p in input_paths:
        r = _read_worldline(p)
        if len(r.windows) >= args.min_windows:
            runs.append(r)
    if not runs:
        print("No usable worldline runs found (min-windows too high or inputs empty).", file=sys.stderr)
        return 2

    # Feature universe: union across runs, but keep only those present in >=50% of runs.
    feature_presence: Dict[str, int] = {}
    for r in runs:
        feats = set(_flatten_features(r).keys())
        for f in feats:
            feature_presence[f] = feature_presence.get(f, 0) + 1
    min_presence = max(1, len(runs) // 2)
    features = sorted([k for k, c in feature_presence.items() if c >= min_presence])
    if len(features) < 3:
        print("Not enough shared numeric features for motif mining.", file=sys.stderr)
        return 2

    lag_corr = _build_lag_corr(runs, features)

    # Direction picking and edge scoring.
    directed_edges: List[Dict[str, object]] = []
    seen_pairs = set()
    for a in features:
        for b in features:
            if a == b:
                continue
            key = tuple(sorted((a, b)))
            if key in seen_pairs:
                continue
            seen_pairs.add(key)
            cab = lag_corr.get((a, b), 0.0)
            cba = lag_corr.get((b, a), 0.0)
            if abs(cab) - abs(cba) > args.direction_margin:
                src, dst, c = a, b, cab
            elif abs(cba) - abs(cab) > args.direction_margin:
                src, dst, c = b, a, cba
            else:
                continue
            directed_edges.append(
                {
                    "from": src,
                    "to": dst,
                    "corrLag1": float(c),
                    "sign": "pos" if c >= 0 else "neg",
                }
            )

    directed_edges.sort(key=lambda e: abs(float(e["corrLag1"])), reverse=True)
    directed_edges = directed_edges[: max(1, args.top_edges)]

    # Add sign agreement support.
    for e in directed_edges:
        e["signSupport"] = _edge_support(runs, str(e["from"]), str(e["to"]))

    # Precompute per-run thresholds and edge activations for motif mining.
    # Activation is purely data-driven: alignment-score quantile within a run.
    run_features: Dict[str, Dict[str, List[float]]] = {}
    for r in runs:
        f = _flatten_features(r)
        run_features[r.path] = f

    # Edge activations per run: map edgeId->bool mask over t (t applies to window t -> t+1).
    edge_ids: List[Tuple[str, str, int]] = []
    for e in directed_edges:
        sgn = 1 if str(e["sign"]) == "pos" else -1
        edge_ids.append((str(e["from"]), str(e["to"]), sgn))

    edge_active: Dict[str, Dict[Tuple[str, str, int], List[bool]]] = {}
    for r in runs:
        f = run_features[r.path]
        per: Dict[Tuple[str, str, int], List[bool]] = {}
        for a, b, sgn in edge_ids:
            if a not in f or b not in f:
                per[(a, b, sgn)] = []
                continue
            per[(a, b, sgn)] = _alignment_mask(f[a], f[b], sgn, args.state_quantile)
        edge_active[r.path] = per

    # Mine length-2 chains: (A->B) then (B->C).
    # Motif active at t if edge1 active at t and edge2 active at t+1.
    motifs: Dict[Tuple[Tuple[str, str, int], Tuple[str, str, int]], Dict[str, object]] = {}
    edge_by_from: Dict[str, List[Tuple[str, str, int]]] = {}
    for a, b, sgn in edge_ids:
        edge_by_from.setdefault(a, []).append((a, b, sgn))

    for e1 in edge_ids:
        a, b, _ = e1
        for e2 in edge_by_from.get(b, []):
            if e2[1] == a:
                continue  # avoid trivial back-and-forth
            key = (e1, e2)
            motifs[key] = {"edges": [e1, e2], "supportWorlds": 0, "meanD": 0.0, "examples": []}

    # Compute D(w,m).
    for mkey, out in motifs.items():
        e1, e2 = mkey
        ds: List[float] = []
        support = 0
        examples: List[Dict[str, object]] = []

        for r in runs:
            a1 = edge_active[r.path].get(e1, [])
            a2 = edge_active[r.path].get(e2, [])
            n = min(len(a1), len(a2) - 1 if len(a2) > 0 else 0)
            if n <= 0:
                continue
            hits = 0
            first_t: Optional[int] = None
            for t in range(0, n):
                if a1[t] and a2[t + 1]:
                    hits += 1
                    if first_t is None:
                        first_t = t
            d = float(hits) / float(n) if n else 0.0
            ds.append(d)
            if hits > 0:
                support += 1
                if first_t is not None and len(examples) < 3:
                    examples.append({"worldline": r.path, "t": first_t})

        out["supportWorlds"] = support
        out["supportShare"] = float(support) / float(len(runs))
        out["meanD"] = float(statistics.fmean(ds)) if ds else 0.0
        out["medianD"] = float(statistics.median(ds)) if ds else 0.0
        out["examples"] = examples

    motif_list = []
    for (e1, e2), m in motifs.items():
        # Score prefers motifs that are frequent across worlds and cover non-trivial time.
        score = float(m["supportShare"]) * (0.25 + float(m["meanD"]))
        motif_list.append(
            {
                "id": f"{e1[0]}->{e1[1]}:{'+' if e1[2] > 0 else '-'} | {e2[0]}->{e2[1]}:{'+' if e2[2] > 0 else '-'}",
                "edges": [
                    {"from": e1[0], "to": e1[1], "sign": "pos" if e1[2] > 0 else "neg"},
                    {"from": e2[0], "to": e2[1], "sign": "pos" if e2[2] > 0 else "neg"},
                ],
                "supportWorlds": int(m["supportWorlds"]),
                "supportShare": float(m["supportShare"]),
                "meanD": float(m["meanD"]),
                "medianD": float(m["medianD"]),
                "score": score,
                "examples": m["examples"],
            }
        )

    motif_list.sort(key=lambda x: float(x["score"]), reverse=True)
    motif_list = [m for m in motif_list if (m["supportWorlds"] > 0 and m["meanD"] > 0.0)]
    motif_list = motif_list[: max(1, args.top_motifs)]

    out_dir = os.path.join("out", "worldline", "analysis")
    if not args.out:
        os.makedirs(out_dir, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
        args.out = os.path.join(out_dir, f"motifs_{stamp}.json")
    else:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)

    report = {
        "kind": "worldline_motif_report",
        "schema_version": 1,
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "params": {
            "input": args.input,
            "pattern": args.pattern,
            "top_edges": args.top_edges,
            "top_motifs": args.top_motifs,
            "direction_margin": args.direction_margin,
            "state_quantile": args.state_quantile,
            "min_windows": args.min_windows,
        },
        "runs": [{"path": r.path, "windows": len(r.windows), "meta": r.meta} for r in runs],
        "features": features,
        "edges": directed_edges,
        "motifs": motif_list,
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"Wrote: {args.out}")
    print(f"Runs: {len(runs)} Features: {len(features)} Edges: {len(directed_edges)} Motifs: {len(motif_list)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
