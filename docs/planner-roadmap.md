# Utility Planner Roadmap

## Vision
- Establish a modular decision layer that evaluates needs, goals, and context to produce action queues for NPCs.
- Support incremental complexity: begin with hunger-focused actions, expand to rest/social, then crimes/tasks.
- Maintain transparency for debugging via telemetry hooks and planners logs.

## Phase 1: Foundations (Current Focus)
- Define planner data structures: `Desire`, `ActionTemplate`, `PlanInstance`.
- Integrate with existing need telemetry and resource system.
- Implement hunger-driven planner prototype: evaluate hunger severity, check available food spawns, enqueue movement + consume actions.
- Extend NeedSatisfier into planner worker rather than direct consumer.
- Instrument planner decisions in telemetry summaries.

## Phase 2: Multi-Need Utility Evaluation
- Introduce utility scores combining hunger, energy, social needs with configurable weights.
- Add cooldown and inertia to avoid thrashing between actions.
- Support chained actions: walk -> interact -> wait.
- Validate with simulation tests ensuring resource usage stays balanced and needs remain within target bands.

## Phase 3: Context Awareness
- Incorporate location costs (path length, congestion), spawn availability forecasts, and time of day modifiers.
- Hook into event bus for dynamic triggers (low stock, social gossip) altering utility.
- Implement soft commitments so multiple agents can coordinate without deadlock.

## Phase 4: Advanced Behaviours
- Add goal templates for work shifts, social events, crime opportunities.
- Enable delegation/requests between NPCs (e.g., ask for food, share rumours).
- Track reputation and relationships influencing planner priorities.
- Expand telemetry with plan success/failure metrics and resource impacts.

## Tooling & Testing
- Create planner visualizer using existing telemetry buffer.
- Develop scenario-based tests: starvation edge case, overcrowded tavern, night-time rest behaviour.
- Integrate planner metrics into CI smoke simulations (24h run comparisons).

## Requirements & Dependencies
- Robust pathfinding system (nav graph or grid) with cost estimates.
- Location metadata (capacity, opening hours).
- Scheduled tick hooks (already available via `Engine::processStep`).
- Extensible action execution framework (future work to replace placeholders).

## Risks & Mitigations
- **Complexity creep:** guard with clear phase gates and MVP definitions.
- **Telemetry overload:** summarize key metrics, allow optional detailed dumps.
- **Performance:** profile planner runs; cache repeated utility calculations.

## Next Immediate Actions
1. Formalize planner interfaces (`IPlanner`, `PlanContext`).
2. Migrate hunger satisfier logic into planner as first action provider.
3. Prototype movement placeholder returning travel time based on location graph distances.

