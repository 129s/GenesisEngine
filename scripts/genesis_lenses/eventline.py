from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .jsonl import iter_jsonl
from .mathx import entropy_bits, mean_or_none, safe_div, zscore_series


@dataclass(frozen=True)
class Event:
    step: int
    entity_id: int
    type: str
    payload: dict[str, Any]


@dataclass(frozen=True)
class Eventline:
    path: Path
    meta: dict[str, Any]
    events: list[Event]


@dataclass(frozen=True)
class Window:
    index: int
    step_start: int
    step_end: int
    metrics: dict[str, float]
    counts: dict[str, Any]


def load_eventline(path: Path) -> Eventline:
    meta: dict[str, Any] | None = None
    events: list[Event] = []

    for r in iter_jsonl(path):
        kind = r.get("kind")
        if kind == "runtime_eventline_meta" and meta is None:
            meta = dict(r)
            continue
        if kind == "runtime_eventline_event":
            payload = dict(r.get("payload", {}) or {})
            events.append(
                Event(
                    step=int(r.get("step", 0) or 0),
                    entity_id=int(r.get("entityId", 0) or 0),
                    type=str(r.get("type", "")),
                    payload=payload,
                )
            )

    if meta is None:
        meta = {}
    events.sort(key=lambda e: (e.step, e.type))
    return Eventline(path=path, meta=meta, events=events)


def filter_steps(events: Iterable[Event], from_step: int | None, to_step: int | None) -> list[Event]:
    out: list[Event] = []
    for e in events:
        if from_step is not None and e.step < from_step:
            continue
        if to_step is not None and e.step > to_step:
            continue
        out.append(e)
    return out


def _count_map_entropy(counts: dict[Any, int]) -> float:
    return entropy_bits(int(v) for v in counts.values())


def _to_int(v: Any) -> int:
    try:
        return int(v)
    except Exception:
        return 0


def _to_float(v: Any) -> float | None:
    try:
        return float(v)
    except Exception:
        return None


def windowize(events: list[Event], window_steps: int, steps_requested: int | None = None) -> list[Window]:
    if window_steps <= 0:
        raise ValueError("window_steps must be > 0")

    max_step = 0
    if events:
        max_step = max(max_step, max(e.step for e in events))
    if steps_requested is not None and steps_requested > 0:
        max_step = max(max_step, int(steps_requested) - 1)

    last_window = max_step // window_steps
    windows_events: list[list[Event]] = [[] for _ in range(last_window + 1)]
    for e in events:
        idx = max(0, e.step // window_steps)
        if idx >= len(windows_events):
            continue
        windows_events[idx].append(e)

    out: list[Window] = []
    for idx, evs in enumerate(windows_events):
        step_start = idx * window_steps
        step_end = min(max_step, (idx + 1) * window_steps - 1)
        out.append(_summarize_window(idx, step_start, step_end, evs))
    return out


def _summarize_window(index: int, step_start: int, step_end: int, events: list[Event]) -> Window:
    type_counts: dict[str, int] = {}
    action_to: dict[str, int] = {}
    planner_to: dict[str, int] = {}
    social_partner: dict[str, int] = {}
    res_fail: dict[str, int] = {}
    prod_fail: dict[str, int] = {}

    res_attempts = 0
    res_failed = 0
    res_obtained = 0
    prod_attempts = 0
    prod_failed = 0
    prod_units = 0

    social_events = 0
    social_delta: list[float] = []

    need_critical_transitions = 0
    need_delta_abs_sum = 0.0

    planner_travel_cost: list[float] = []
    planner_score: list[float] = []

    for e in events:
        type_counts[e.type] = type_counts.get(e.type, 0) + 1

        if e.type == "action_change":
            to = str(e.payload.get("to", "") or "")
            if to:
                action_to[to] = action_to.get(to, 0) + 1

        if e.type == "planner_target_change":
            to = str(e.payload.get("to", "") or "")
            if to:
                planner_to[to] = planner_to.get(to, 0) + 1
            tc = _to_float(e.payload.get("travelCost"))
            if tc is not None:
                planner_travel_cost.append(tc)
            sc = _to_float(e.payload.get("score"))
            if sc is not None:
                planner_score.append(sc)

        if e.type == "need_critical_transition":
            need_critical_transitions += 1

        if e.type == "need_delta":
            dv = _to_float(e.payload.get("delta"))
            if dv is not None:
                need_delta_abs_sum += abs(dv)

        if e.type == "resource_attempt":
            res_attempts += 1
            res_obtained += max(0, _to_int(e.payload.get("obtainedUnits")))
            reason = str(e.payload.get("failureReason", "") or "")
            ok = (reason == "") and (_to_int(e.payload.get("obtainedUnits")) > 0)
            if not ok:
                res_failed += 1
                key = reason if reason else "n/a"
                res_fail[key] = res_fail.get(key, 0) + 1

        if e.type == "workshop_attempt":
            prod_attempts += 1
            prod_units += max(0, _to_int(e.payload.get("producedUnits")))
            reason = str(e.payload.get("failureReason", "") or "")
            ok = (reason == "") and (_to_int(e.payload.get("producedUnits")) > 0)
            if not ok:
                prod_failed += 1
                key = reason if reason else "n/a"
                prod_fail[key] = prod_fail.get(key, 0) + 1

        if e.type in ("social_attempt_end", "social_attempt_start"):
            partner = str(e.payload.get("partnerEntityId", "") or "")
            if partner:
                social_partner[partner] = social_partner.get(partner, 0) + 1
            social_events += 1
            if e.type == "social_attempt_end":
                dv = _to_float(e.payload.get("socialDelta"))
                if dv is not None:
                    social_delta.append(dv)

    action_entropy = _count_map_entropy(action_to)
    planner_entropy = _count_map_entropy(planner_to)
    social_entropy = _count_map_entropy(social_partner)

    social_top_share = 0.0
    if social_events > 0 and social_partner:
        social_top_share = max(int(v) for v in social_partner.values()) / float(social_events)

    metrics: dict[str, float] = {
        "actionToEntropyBits": float(action_entropy),
        "plannerTargetEntropyBits": float(planner_entropy),
        "socialPartnerEntropyBits": float(social_entropy),
        "socialTopShare": float(social_top_share),
        "socialDeltaMean": float(mean_or_none(social_delta) or 0.0),
        "resourceAttempts": float(res_attempts),
        "resourceFailed": float(res_failed),
        "resourceFailRate": float(safe_div(float(res_failed), float(res_attempts))),
        "resourceObtainedUnits": float(res_obtained),
        "productionAttempts": float(prod_attempts),
        "productionFailed": float(prod_failed),
        "productionFailRate": float(safe_div(float(prod_failed), float(prod_attempts))),
        "productionUnits": float(prod_units),
        "needCriticalTransitions": float(need_critical_transitions),
        "needDeltaAbsSum": float(need_delta_abs_sum),
        "plannerTravelCostMean": float(mean_or_none(planner_travel_cost) or 0.0),
        "plannerScoreMean": float(mean_or_none(planner_score) or 0.0),
    }

    counts: dict[str, Any] = {
        "eventTypes": type_counts,
        "actionTo": action_to,
        "plannerTargets": planner_to,
        "social": {
            "events": social_events,
            "uniquePartners": len(social_partner),
            "partnerEntropyBits": float(social_entropy),
            "partners": social_partner,
        },
        "resourceFailureReasons": res_fail,
        "productionFailureReasons": prod_fail,
    }

    return Window(index=index, step_start=step_start, step_end=step_end, metrics=metrics, counts=counts)


def rank_turning_points(windows: list[Window], top: int = 8, k_features: int = 8) -> list[dict[str, Any]]:
    if top <= 0 or len(windows) < 2:
        return []

    feature_keys = [
        "actionToEntropyBits",
        "plannerTargetEntropyBits",
        "socialPartnerEntropyBits",
        "socialTopShare",
        "resourceFailRate",
        "productionFailRate",
        "needCriticalTransitions",
        "needDeltaAbsSum",
        "plannerTravelCostMean",
        "plannerScoreMean",
    ]

    series: dict[str, list[float]] = {k: [] for k in feature_keys}
    for w in windows:
        for k in feature_keys:
            series[k].append(float(w.metrics.get(k, 0.0)))

    z: dict[str, list[float]] = {k: zscore_series(vs) for k, vs in series.items()}

    scores: list[dict[str, Any]] = []
    for i in range(1, len(windows)):
        contributors: list[dict[str, Any]] = []
        score = 0.0
        for k in feature_keys:
            dz = z[k][i] - z[k][i - 1]
            adz = abs(dz)
            if adz <= 0.0:
                continue
            score += adz
            contributors.append({"feature": k, "absZDelta": float(adz), "zDelta": float(dz)})
        contributors.sort(key=lambda c: c["absZDelta"], reverse=True)
        scores.append(
            {
                "index": int(i),
                "score": float(score),
                "contributors": contributors[: max(1, int(k_features))],
            }
        )

    scores.sort(key=lambda r: r["score"], reverse=True)
    return scores[: min(len(scores), int(top))]

