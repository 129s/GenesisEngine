#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
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


def _fmt(v: Any) -> str:
    if v is None:
        return "n/a"
    return str(v)


def _fmt_float(x: Any, digits: int = 2) -> str:
    try:
        v = float(x)
    except Exception:
        return "n/a"
    return f"{v:.{digits}f}"


@dataclass(frozen=True)
class Event:
    step: int
    type: str
    payload: dict[str, Any]


def _iter_events(rows: Iterable[dict[str, Any]]) -> tuple[dict[str, Any], list[Event]]:
    meta: dict[str, Any] | None = None
    events: list[Event] = []
    for r in rows:
        kind = r.get("kind")
        if kind == "runtime_eventline_meta" and meta is None:
            meta = r
            continue
        if kind == "runtime_eventline_event":
            events.append(
                Event(
                    step=int(r.get("step", 0) or 0),
                    type=str(r.get("type", "")),
                    payload=dict(r.get("payload", {}) or {}),
                )
            )
    if meta is None:
        meta = {}
    events.sort(key=lambda e: e.step)
    return meta, events


def _render_event(e: Event) -> list[str]:
    p = e.payload
    t = e.type
    if t == "initial_state":
        needs = p.get("needs", {}) or {}
        parts = []
        for name, nv in needs.items():
            parts.append(f"{name}={_fmt_float(nv.get('value'))}{'(!)' if nv.get('critical') else ''}")
        return [f"- t={e.step} 初始状态：map={_fmt(p.get('mapId'))} target={_fmt(p.get('plannerTarget'))} action={_fmt(p.get('action'))} needs: " + ", ".join(parts)]

    if t == "map_change":
        return [f"- t={e.step} 跨图：{_fmt(p.get('from'))} → {_fmt(p.get('to'))}"]

    if t == "planner_target_change":
        extra = " (agentTarget)" if p.get("toIsAgentTarget") else ""
        return [
            f"- t={e.step} 目标切换：{_fmt(p.get('from'))} → {_fmt(p.get('to'))}{extra} travel={_fmt_float(p.get('travelCost'))} score={_fmt_float(p.get('score'))}"
        ]

    if t == "action_change":
        base = f"- t={e.step} 动作切换：{_fmt(p.get('from'))} → {_fmt(p.get('to'))} q={_fmt(p.get('queueLength'))}"
        to = p.get("to", "")
        if to == "SocializeWithAgent":
            return [base + f" partner={_fmt(p.get('targetEntityId'))}"]
        if to in ("ConsumeResource", "TakeResource", "ProduceResource", "MoveToInteraction"):
            return [base + f" target={_fmt(p.get('target'))} res={_fmt(p.get('resourceType'))} amount={_fmt(p.get('amount'))}"]
        return [base]

    if t == "need_critical_transition":
        return [f"- t={e.step} 临界变化：{_fmt(p.get('need'))} critical {p.get('from')} → {p.get('to')} value={_fmt_float(p.get('value'))}"]

    if t == "need_delta":
        dv = p.get("delta")
        sign = ""
        try:
            sign = "↑" if float(dv) > 0 else "↓"
        except Exception:
            sign = ""
        return [
            f"- t={e.step} 需求波动：{_fmt(p.get('need'))} {sign} delta={_fmt_float(dv)} ({_fmt_float(p.get('from'))}→{_fmt_float(p.get('to'))})"
        ]

    if t == "resource_attempt":
        ok = (p.get("failureReason") in (None, "")) and int(p.get("obtainedUnits", 0) or 0) > 0
        status = "成功" if ok else f"失败({p.get('failureReason') or 'n/a'})"
        return [
            f"- t={e.step} 资源尝试：{_fmt(p.get('action'))} {status} {p.get('resourceType')}@{_fmt(p.get('interactionId'))} want={_fmt(p.get('wantedUnits'))} got={_fmt(p.get('obtainedUnits'))} recoveryPlanned={_fmt(p.get('recoveryPlanned'))}"
        ]

    if t == "workshop_attempt":
        ok = (p.get("failureReason") in (None, "")) and int(p.get("producedUnits", 0) or 0) > 0
        status = "成功" if ok else f"失败({p.get('failureReason') or 'n/a'})"
        return [
            f"- t={e.step} 生产尝试：{status} out={_fmt(p.get('outputType'))} workshop={_fmt(p.get('interactionId'))} wantUnits={_fmt(p.get('wantedUnits'))} got={_fmt(p.get('producedUnits'))}"
        ]

    if t == "social_attempt_start":
        return [f"- t={e.step} 社交开始：partner={_fmt(p.get('partnerEntityId'))} intendedRelief={_fmt_float(p.get('intendedRelief'))} social={_fmt_float(p.get('socialValue'))}"]

    if t == "social_attempt_end":
        return [
            f"- t={e.step} 社交结束：partner={_fmt(p.get('partnerEntityId'))} inferredSuccess={_fmt(p.get('inferredSuccess'))} socialDelta={_fmt_float(p.get('socialDelta'))} intendedRelief={_fmt_float(p.get('intendedRelief'))}"
        ]

    return [f"- t={e.step} {t}: {json.dumps(p, ensure_ascii=False)}"]


def render_markdown(eventline_path: Path, from_step: int | None, to_step: int | None, max_events: int) -> str:
    rows = _read_jsonl(eventline_path)
    meta, events = _iter_events(rows)

    if from_step is not None:
        events = [e for e in events if e.step >= from_step]
    if to_step is not None:
        events = [e for e in events if e.step <= to_step]

    if max_events > 0 and len(events) > max_events:
        events = events[:max_events]

    counts: dict[str, int] = {}
    for e in events:
        counts[e.type] = counts.get(e.type, 0) + 1

    social_starts = sum(1 for e in events if e.type == "social_attempt_start")
    social_ends = [e for e in events if e.type == "social_attempt_end"]
    social_success = sum(1 for e in social_ends if bool(e.payload.get("inferredSuccess")))

    resource_attempts = [e for e in events if e.type == "resource_attempt"]
    resource_fail = 0
    for e in resource_attempts:
        p = e.payload
        ok = (p.get("failureReason") in (None, "")) and int(p.get("obtainedUnits", 0) or 0) > 0
        if not ok:
            resource_fail += 1

    out: list[str] = []
    out.append("# Agent Lens（对象演化视角）\n")
    out.append("## 元信息\n")
    out.append(f"- eventline: `{eventline_path.as_posix()}`")
    out.append(f"- agentEntityId: `{_fmt(meta.get('agentEntityId'))}` agentIndexSorted: `{_fmt(meta.get('agentIndexSortedByEntityId'))}`")
    out.append(f"- worldSeed: `{_fmt(meta.get('worldSeed'))}` worldgenConfig: `{_fmt(meta.get('worldgenConfig'))}`")
    out.append(f"- stepsRequested: `{_fmt(meta.get('stepsRequested'))}` telemetrySchemaVersion: `{_fmt(meta.get('telemetrySchemaVersion'))}`\n")

    out.append("## 摘要\n")
    out.append(f"- 事件总数: `{len(events)}`（截断上限 maxEvents={max_events}）")
    out.append(f"- 资源尝试: `{len(resource_attempts)}` 失败: `{resource_fail}`")
    out.append(f"- 社交尝试: start `{social_starts}` end `{len(social_ends)}` inferredSuccess `{social_success}`\n")

    out.append("## 事件计数（按类型）\n")
    for k in sorted(counts.keys()):
        out.append(f"- `{k}`: `{counts[k]}`")
    out.append("")

    out.append("## 时间线（按 step）\n")
    for e in events:
        out.extend(_render_event(e))
    out.append("")

    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--eventline", type=Path, required=True, help="Path to runtime_eventline JSONL")
    ap.add_argument("--out", type=Path, default=None, help="Output markdown path")
    ap.add_argument("--from-step", type=int, default=None)
    ap.add_argument("--to-step", type=int, default=None)
    ap.add_argument("--max-events", type=int, default=600)
    args = ap.parse_args()

    md = render_markdown(args.eventline, args.from_step, args.to_step, args.max_events)
    if args.out is None:
        print(md)
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(md, encoding="utf-8")
    print(f"Wrote: {args.out.as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

