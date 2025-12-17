#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import statistics
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from genesis_lenses.eventline import load_eventline, rank_turning_points, windowize


def _expand_inputs(input_arg: str, pattern: str) -> list[Path]:
    p = Path(input_arg)
    if p.is_file():
        return [p]
    if p.is_dir():
        return sorted(p.glob(pattern))
    return sorted(Path().glob(input_arg))


def _to_int(v: Any) -> int | None:
    try:
        return int(v)
    except Exception:
        return None


def _to_str(v: Any) -> str:
    if v is None:
        return "n/a"
    s = str(v)
    return s if s else "n/a"


def _fmt_float(x: Any, digits: int = 3) -> str:
    try:
        v = float(x)
    except Exception:
        return "n/a"
    if math.isnan(v) or math.isinf(v):
        return "n/a"
    return f"{v:.{digits}f}"


def _fmt_pct01(x: Any, digits: int = 1) -> str:
    try:
        v = float(x)
    except Exception:
        return "n/a"
    return f"{100.0 * v:.{digits}f}%"


def _median(xs: list[float]) -> float | None:
    if not xs:
        return None
    return float(statistics.median(xs))


def _quantile(xs: list[float], q: float) -> float | None:
    if not xs:
        return None
    if q <= 0.0:
        return float(min(xs))
    if q >= 1.0:
        return float(max(xs))
    ys = sorted(float(x) for x in xs)
    pos = (len(ys) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return float(ys[lo])
    t = pos - lo
    return float(ys[lo] * (1.0 - t) + ys[hi] * t)


@dataclass(frozen=True)
class RunSummary:
    path: Path
    seed: int | None
    worldgen_config: str
    agent_entity_id: int | None
    agent_index: int | None
    steps_requested: int | None
    windows: int
    totals: dict[str, float]
    turns: list[dict[str, Any]]
    top_partner: tuple[str, int, float] | None


def _summarize_eventline(path: Path, window_steps: int, top_turns: int, k_features: int) -> RunSummary:
    el = load_eventline(path)
    meta = el.meta

    seed = _to_int(meta.get("worldSeed"))
    worldgen_config = str(meta.get("worldgenConfig", "") or "n/a")
    agent_entity_id = _to_int(meta.get("agentEntityId"))
    agent_index = _to_int(meta.get("agentIndexSortedByEntityId"))
    steps_requested = _to_int(meta.get("stepsRequested"))

    windows = windowize(el.events, window_steps=window_steps, steps_requested=steps_requested)

    totals: dict[str, float] = {}
    social_events = 0
    social_partners: dict[str, int] = {}
    res_attempts = 0
    res_failed = 0
    prod_attempts = 0
    prod_failed = 0
    action_changes = 0
    planner_changes = 0

    for w in windows:
        m = w.metrics
        c = w.counts
        social = c.get("social", {}) or {}
        social_events += int(social.get("events", 0) or 0)
        for k, v in (social.get("partners", {}) or {}).items():
            social_partners[str(k)] = social_partners.get(str(k), 0) + int(v)

        res_attempts += int(m.get("resourceAttempts", 0.0))
        res_failed += int(m.get("resourceFailed", 0.0))
        prod_attempts += int(m.get("productionAttempts", 0.0))
        prod_failed += int(m.get("productionFailed", 0.0))
        action_changes += int((c.get("eventTypes", {}) or {}).get("action_change", 0))
        planner_changes += int((c.get("eventTypes", {}) or {}).get("planner_target_change", 0))

    totals["events"] = float(len(el.events))
    totals["windows"] = float(len(windows))
    totals["socialEvents"] = float(social_events)
    totals["uniquePartners"] = float(len(social_partners))
    totals["resourceAttempts"] = float(res_attempts)
    totals["resourceFailed"] = float(res_failed)
    totals["resourceFailRate"] = float(res_failed / res_attempts) if res_attempts > 0 else 0.0
    totals["productionAttempts"] = float(prod_attempts)
    totals["productionFailed"] = float(prod_failed)
    totals["productionFailRate"] = float(prod_failed / prod_attempts) if prod_attempts > 0 else 0.0
    totals["actionChangeEvents"] = float(action_changes)
    totals["plannerTargetChangeEvents"] = float(planner_changes)

    turns = rank_turning_points(windows, top=top_turns, k_features=k_features)

    top_partner: tuple[str, int, float] | None = None
    if social_events > 0 and social_partners:
        partner, cnt = max(social_partners.items(), key=lambda kv: kv[1])
        top_partner = (partner, int(cnt), float(cnt / social_events))

    return RunSummary(
        path=path,
        seed=seed,
        worldgen_config=worldgen_config,
        agent_entity_id=agent_entity_id,
        agent_index=agent_index,
        steps_requested=steps_requested,
        windows=len(windows),
        totals=totals,
        turns=turns,
        top_partner=top_partner,
    )


def _fmt_ref(xs: list[float]) -> str:
    med = _median(xs)
    q25 = _quantile(xs, 0.25)
    q75 = _quantile(xs, 0.75)
    if med is None or q25 is None or q75 is None:
        return "n/a"
    return f"median={med:.3f} IQR=[{q25:.3f},{q75:.3f}]"


def render_markdown(paths: list[Path], window_steps: int, top_turns: int, k_features: int) -> str:
    runs = [_summarize_eventline(p, window_steps=window_steps, top_turns=top_turns, k_features=k_features) for p in paths]
    runs.sort(key=lambda r: (r.worldgen_config, r.seed if r.seed is not None else 0, r.path.as_posix()))

    def col(key: str) -> list[float]:
        xs: list[float] = []
        for r in runs:
            v = r.totals.get(key)
            if v is None:
                continue
            xs.append(float(v))
        return xs

    out: list[str] = []
    out.append("# Agent Eventline Compare（跨 seed 对比）\n")
    out.append("## 输入\n")
    out.append(f"- runs: `{len(runs)}` windowSteps: `{window_steps}`\n")

    out.append("## 参考点（跨 runs 的分布）\n")
    for k, label in [
        ("events", "事件总数"),
        ("socialEvents", "社交事件数"),
        ("uniquePartners", "社交对象数"),
        ("resourceAttempts", "资源尝试数"),
        ("resourceFailRate", "资源失败率"),
        ("productionAttempts", "生产尝试数"),
        ("productionFailRate", "生产失败率"),
        ("actionChangeEvents", "动作切换事件数"),
        ("plannerTargetChangeEvents", "目标切换事件数"),
    ]:
        out.append(f"- {label}（`{k}`）：{_fmt_ref(col(k))}")
    out.append("")

    out.append("## 每个 run 摘要\n")
    for r in runs:
        seed = _to_str(r.seed)
        out.append(f"### seed={seed} agentIndex={_to_str(r.agent_index)} entityId={_to_str(r.agent_entity_id)}\n")
        out.append(f"- eventline: `{r.path.as_posix()}`")
        out.append(f"- worldgenConfig: `{r.worldgen_config}` stepsRequested: `{_to_str(r.steps_requested)}` windows: `{r.windows}`")
        out.append(
            "- totals: "
            + f"events `{int(r.totals['events'])}`"
            + f", social `{int(r.totals['socialEvents'])}` (unique `{int(r.totals['uniquePartners'])}`)"
            + f", resourceAttempts `{int(r.totals['resourceAttempts'])}` failRate `{_fmt_pct01(r.totals['resourceFailRate'])}`"
            + f", productionAttempts `{int(r.totals['productionAttempts'])}` failRate `{_fmt_pct01(r.totals['productionFailRate'])}`"
        )
        if r.top_partner is None:
            out.append("- socialConcentration: (no social events)")
        else:
            partner, cnt, share = r.top_partner
            flag = " HIGH" if share >= 0.80 and cnt >= 5 else ""
            out.append(f"- socialConcentration: topPartner=`{partner}` count=`{cnt}` share=`{_fmt_pct01(share)}`{flag}")

        if not r.turns:
            out.append("- turningPoints: (none)\n")
        else:
            out.append(f"- turningPoints: top `{len(r.turns)}`（zΔ 变点候选；不命名、不分章）")
            for t in r.turns:
                idx = int(t.get("index", 0))
                score = float(t.get("score", 0.0))
                out.append(f"  - window#{idx} score=`{score:.2f}`")
            out.append("")

    return "\n".join(out).rstrip() + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description="Compare multiple per-agent eventlines across seeds/configs (Markdown report).")
    ap.add_argument("--input", required=True, help="Eventline file, folder, or glob (JSONL).")
    ap.add_argument("--pattern", default="eventline_seed*.jsonl", help="Pattern when --input is a folder.")
    ap.add_argument("--window-steps", type=int, default=100, help="Window steps for coarse comparison.")
    ap.add_argument("--top-turns", type=int, default=6, help="Turning point candidates to report per run.")
    ap.add_argument("--k-features", type=int, default=6, help="Top-K feature deltas per turning point (internal).")
    ap.add_argument("--out", default="", help="Output path. If empty, print to stdout.")
    args = ap.parse_args()

    paths = _expand_inputs(args.input, args.pattern)
    if not paths:
        raise SystemExit(f"No eventline files found for input={args.input!r}")

    text = render_markdown(
        paths,
        window_steps=max(1, int(args.window_steps)),
        top_turns=max(0, int(args.top_turns)),
        k_features=max(1, int(args.k_features)),
    )

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

