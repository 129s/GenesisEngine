#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import os
import statistics
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            rows.append(json.loads(s))
    return rows


def _safe_div(num: float, den: float) -> float:
    if den == 0.0:
        return 0.0
    return num / den


def _entropy_bits(counts: Iterable[int]) -> float:
    xs = [float(c) for c in counts if c > 0]
    total = sum(xs)
    if total <= 0.0:
        return 0.0
    h = 0.0
    for c in xs:
        p = c / total
        h -= p * math.log2(p)
    return h


def _zscore_series(xs: list[float]) -> list[float]:
    if not xs:
        return []
    if len(xs) == 1:
        return [0.0]
    mean = statistics.fmean(xs)
    stdev = statistics.pstdev(xs)
    if stdev <= 1e-9:
        return [0.0 for _ in xs]
    return [(x - mean) / stdev for x in xs]


@dataclass(frozen=True)
class Window:
    index: int
    step_start: int
    step_end: int
    steps: int
    metrics: dict[str, float]
    counts: dict[str, Any]


def _feature_series(windows: list[Window]) -> dict[str, list[float]]:
    series: dict[str, list[float]] = {}

    def push(key: str, value: float) -> None:
        series.setdefault(key, []).append(float(value))

    for w in windows:
        m = w.metrics
        c = w.counts

        for k in (
            "criticalNeedRate",
            "stockoutShareAny",
            "stockoutShareSources",
            "stockoutShareWorkshops",
            "actionEntropyBits",
            "plannerTargetEntropyBits",
            "plannerTravelCostMean",
            "resourceAttempts",
            "resourceFailed",
            "productionAttempts",
            "productionFailed",
            "stepsWithAnyConsumption",
            "stepsWithAnyDecay",
            "stepsWithAnyRegen",
        ):
            push(f"m.{k}", float(m.get(k, 0.0)))

        action_types = c.get("actionTypes", {}) or {}
        action_total = float(sum(int(v) for v in action_types.values()))
        action_kinds = float(sum(1 for _, v in action_types.items() if int(v) > 0))
        action_top_share = float(max((int(v) for v in action_types.values()), default=0)) / action_total if action_total > 0 else 0.0
        push("c.actionTypes.total", action_total)
        push("c.actionTypes.kinds", action_kinds)
        push("c.actionTypes.top_share", action_top_share)

        planner_targets = c.get("plannerTargets", {}) or {}
        planner_total = float(sum(int(v) for v in planner_targets.values()))
        planner_kinds = float(sum(1 for _, v in planner_targets.items() if int(v) > 0))
        planner_top_share = float(max((int(v) for v in planner_targets.values()), default=0)) / planner_total if planner_total > 0 else 0.0
        push("c.plannerTargets.total", planner_total)
        push("c.plannerTargets.kinds", planner_kinds)
        push("c.plannerTargets.top_share", planner_top_share)

        res_fail = c.get("resourceFailureReasons", {}) or {}
        res_fail_total = float(sum(int(v) for v in res_fail.values()))
        res_fail_kinds = float(sum(1 for _, v in res_fail.items() if int(v) > 0))
        res_fail_top_share = float(max((int(v) for v in res_fail.values()), default=0)) / res_fail_total if res_fail_total > 0 else 0.0
        push("c.resourceFailureReasons.total", res_fail_total)
        push("c.resourceFailureReasons.kinds", res_fail_kinds)
        push("c.resourceFailureReasons.top_share", res_fail_top_share)

        prod_fail = c.get("productionFailureReasons", {}) or {}
        prod_fail_total = float(sum(int(v) for v in prod_fail.values()))
        prod_fail_kinds = float(sum(1 for _, v in prod_fail.items() if int(v) > 0))
        prod_fail_top_share = float(max((int(v) for v in prod_fail.values()), default=0)) / prod_fail_total if prod_fail_total > 0 else 0.0
        push("c.productionFailureReasons.total", prod_fail_total)
        push("c.productionFailureReasons.kinds", prod_fail_kinds)
        push("c.productionFailureReasons.top_share", prod_fail_top_share)

        social = c.get("social", {}) or {}
        social_events = float(social.get("events", 0.0))
        social_unique = float(social.get("uniquePartners", 0.0))
        social_entropy = float(social.get("partnerEntropyBits", 0.0))
        top_share = 1.0
        if social_events > 0.0 and social_unique > 0.0:
            top_share = float(2.0 ** (-social_entropy))
            top_share = max(0.0, min(1.0, top_share))
        push("c.social.total", social_events)
        push("c.social.kinds", social_unique)
        push("c.social.top_share", top_share)

    return series


def _rank_turning_points(windows: list[Window], top_turns: int, k_features: int) -> list[dict[str, Any]]:
    features = _feature_series(windows)
    z = {k: _zscore_series(v) for k, v in features.items()}

    candidates: list[dict[str, Any]] = []
    for i in range(1, len(windows)):
        contrib = []
        score = 0.0
        for key, zxs in z.items():
            dz = abs(zxs[i] - zxs[i - 1])
            if dz <= 0.0:
                continue
            contrib.append((key, dz))
            score += dz
        contrib.sort(key=lambda x: x[1], reverse=True)
        candidates.append(
            {
                "index": i,
                "score": score,
                "contributors": [{"feature": k, "absZDelta": v} for k, v in contrib[:k_features]],
            }
        )

    candidates.sort(key=lambda c: c["score"], reverse=True)
    return candidates[: max(0, top_turns)]


def _fmt_pct01(x: float) -> str:
    if not math.isfinite(x):
        return "n/a"
    return f"{max(0.0, min(1.0, x)) * 100.0:.1f}%"


def _top_k(d: dict[str, Any], k: int) -> list[tuple[str, int]]:
    items = [(str(name), int(count)) for name, count in (d or {}).items()]
    items.sort(key=lambda p: p[1], reverse=True)
    return items[:k]


def chronicle_markdown(worldline_path: Path, top_turns: int, k_features: int, keep_partials: bool) -> str:
    rows = _read_jsonl(worldline_path)
    if not rows:
        return "# Worldline Chronicle\n\n(empty)\n"

    meta = rows[0]
    window_steps = int(meta.get("windowSteps", 0) or 0)
    windows: list[Window] = []
    for r in rows[1:]:
        if r.get("kind") != "runtime_worldline_window":
            continue
        steps = int(r.get("steps", 0) or 0)
        if not keep_partials and window_steps > 0 and steps != window_steps:
            continue
        windows.append(
            Window(
                index=int(r.get("index", 0)),
                step_start=int(r.get("stepStart", 0)),
                step_end=int(r.get("stepEnd", 0)),
                steps=steps,
                metrics=dict(r.get("metrics", {}) or {}),
                counts=dict(r.get("counts", {}) or {}),
            )
        )

    if not windows:
        return "# Worldline Chronicle\n\n(no windows)\n"

    turns = _rank_turning_points(windows, top_turns=top_turns, k_features=k_features)

    all_actions: dict[str, int] = {}
    all_targets: dict[str, int] = {}
    social_events_total = 0
    for w in windows:
        for name, count in (w.counts.get("actionTypes", {}) or {}).items():
            all_actions[str(name)] = all_actions.get(str(name), 0) + int(count)
        for name, count in (w.counts.get("plannerTargets", {}) or {}).items():
            all_targets[str(name)] = all_targets.get(str(name), 0) + int(count)
        social_events_total += int((w.counts.get("social", {}) or {}).get("events", 0))

    action_total = sum(all_actions.values())
    target_total = sum(all_targets.values())
    action_entropy = _entropy_bits(all_actions.values())
    target_entropy = _entropy_bits(all_targets.values())

    def avg_metric(key: str) -> float:
        return statistics.fmean(float(w.metrics.get(key, 0.0)) for w in windows)

    out: list[str] = []
    out.append("# Worldline Chronicle（世界线编年史）\n")
    out.append("## 元信息\n")
    out.append(f"- worldline: `{worldline_path.as_posix()}`")
    out.append(f"- worldSeed: `{meta.get('worldSeed', meta.get('lastSeed', 'n/a'))}`")
    out.append(f"- worldgenConfig: `{meta.get('worldgenConfig', 'n/a')}`")
    out.append(f"- agentCount: `{meta.get('agentCountInitial', meta.get('agentCountRequested', 'n/a'))}`")
    out.append(f"- stepsRequested: `{meta.get('stepsRequested', 'n/a')}` windowSteps: `{meta.get('windowSteps', 'n/a')}` windows: `{len(windows)}`\n")

    out.append("## 整体概览（宏观统计）\n")
    out.append(f"- 平均 criticalNeedRate: `{_fmt_pct01(avg_metric('criticalNeedRate'))}` 平均 stockoutShareAny: `{_fmt_pct01(avg_metric('stockoutShareAny'))}`")
    out.append(f"- 总行动数: `{action_total}` 行动熵: `{action_entropy:.2f} bits`；总目标数: `{target_total}` 目标熵: `{target_entropy:.2f} bits`")
    out.append(f"- 社交事件总数: `{social_events_total}`（SocializeWithAgent）\n")

    out.append("## 行动与目标（Top）\n")
    out.append("- actionTypesTop: " + ", ".join([f"`{n}`={c}" for n, c in _top_k(all_actions, 6)]) if all_actions else "- actionTypesTop: (none)")
    out.append("- plannerTargetsTop: " + ", ".join([f"`{n}`={c}" for n, c in _top_k(all_targets, 6)]) if all_targets else "- plannerTargetsTop: (none)")
    out.append("")

    out.append("## 转折候选（数据驱动，不注入剧情）\n")
    if not turns:
        out.append("- (none)\n")
    else:
        for t in turns:
            idx = int(t["index"])
            w = windows[idx]
            prev = windows[idx - 1]
            out.append(f"### Window #{w.index}（steps {w.step_start}..{w.step_end}）\n")

            def mv(k: str) -> float:
                return float(w.metrics.get(k, 0.0))

            def pv(k: str) -> float:
                return float(prev.metrics.get(k, 0.0))

            out.append(f"- score(zΔ sum): `{t['score']:.2f}`")
            out.append(
                "- 关键指标："
                + f" criticalNeedRate `{_fmt_pct01(pv('criticalNeedRate'))}`→`{_fmt_pct01(mv('criticalNeedRate'))}`"
                + f", stockoutShareAny `{_fmt_pct01(pv('stockoutShareAny'))}`→`{_fmt_pct01(mv('stockoutShareAny'))}`"
                + f", plannerTargetEntropy `{pv('plannerTargetEntropyBits'):.2f}`→`{mv('plannerTargetEntropyBits'):.2f}`"
                + f", actionEntropy `{pv('actionEntropyBits'):.2f}`→`{mv('actionEntropyBits'):.2f}`"
            )

            cur_actions = w.counts.get("actionTypes", {}) or {}
            prev_actions = prev.counts.get("actionTypes", {}) or {}
            cur_social = w.counts.get("social", {}) or {}
            prev_social = prev.counts.get("social", {}) or {}
            out.append(
                "- 行动Top："
                + ", ".join([f"`{n}`={c}" for n, c in _top_k(cur_actions, 4)])
                + f"（prev Social={int(prev_social.get('events', 0))} cur Social={int(cur_social.get('events', 0))}）"
            )

            contrib = t.get("contributors", []) or []
            if contrib:
                out.append("- 贡献特征Top: " + ", ".join([f"`{c['feature']}` zΔ={c['absZDelta']:.2f}" for c in contrib]))
            out.append("")

    return "\n".join(out).rstrip() + "\n"


def _expand_inputs(input_arg: str, pattern: str) -> list[Path]:
    p = Path(input_arg)
    if p.is_file():
        return [p]
    if p.is_dir():
        return sorted(p.glob(pattern))
    return sorted(Path().glob(input_arg))


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate a human-readable worldline chronicle (Markdown) from JSONL.")
    ap.add_argument("--input", required=True, help="Worldline file, folder, or glob (JSONL).")
    ap.add_argument("--pattern", default="worldline_*.jsonl", help="Pattern when --input is a folder.")
    ap.add_argument("--top-turns", type=int, default=8, help="Turning point candidates to report per run.")
    ap.add_argument("--k-features", type=int, default=8, help="Top-K feature deltas per turning point.")
    ap.add_argument("--keep-partials", action="store_true", help="Keep partial last windows (steps < windowSteps).")
    ap.add_argument("--out", default="", help="Output path. If empty, print to stdout.")
    args = ap.parse_args()

    paths = _expand_inputs(args.input, args.pattern)
    if not paths:
        raise SystemExit(f"No worldline files found for input={args.input!r}")

    outputs: list[str] = []
    for path in paths:
        outputs.append(
            chronicle_markdown(
                path,
                top_turns=max(0, args.top_turns),
                k_features=max(1, args.k_features),
                keep_partials=bool(args.keep_partials),
            )
        )

    text = "\n---\n\n".join(outputs)
    if args.out:
        out_path = Path(args.out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")
        print(f"Wrote: {out_path.as_posix()}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
