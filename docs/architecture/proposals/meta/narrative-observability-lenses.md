# 叙事可观测性（Narrative Observability Lenses）· Proposal

目标：在不引入预制剧情词表、不依赖 NLP/LLM 的前提下，让“叙事”以一种**可复现、可对比、可组合**的方式从模拟运行中被观测出来。

本提案只定义观测框架（离线分析），不规定具体“故事标题/章节/母题命名”。

## 核心直觉（函数式/可组合）

- 世界运行产生一个**事实流** `E = [e0, e1, ...]`（事件 JSONL）。
- 我们用纯函数对事实流做观测：`O(E) -> view`。
- “叙事对象”不是名字，而是一个**选择器/镜头（lens）**：从世界里切出一个子结构，再计算其随时间的演化轨迹。

因此：
- 不需要“章节”，只需要在时间轴上观察对象结构的变化；
- 粒度由观测算子决定（window/resample/refine），而不是由模拟硬切分；
- 命名是次要的：对象的结构与关联定义对象本身。

## 术语

- **Entity（实体）**：可被引用的对象（agent/交互点/地点/组织/制度等，具体类型可扩展）。
- **Event（事件）**：对叙事有意义的离散事实（不是每 tick 的全量日志）。
- **Lens（镜头/选择器）**：`L(E) -> E'`，选出某对象相关的事件子流或子图。
- **Observation（观测函数）**：`O(E') -> time series / graph / summary`，输出可比对的结构化结果。

## 数据输出层（离线）

建议新增一个“叙事史料”输出（JSONL），与现有 `worldline_*.jsonl` 并行：
- `eventline_*.jsonl`：只写叙事级事件（稀疏、可读、可索引）
- `worldline_*.jsonl`：窗口统计/宏观指标（稳定回归基线）

两者关系：
- worldline 适合做健康度、分布漂移、粗粒度涌现签名；
- eventline 适合做对象演化、因果链、可解释叙事。

## 观测算子最小集合（建议先用库，不做 DSL）

初版只需要一组可组合的函数（Python 库即可）：
- `select(entity_predicate)`：选对象
- `filter(event_predicate)`：选事件类型
- `map(transform)`：变换事件
- `fold(reducer, init)`：对事件折叠成状态/统计
- `window(size/step)`：重采样/变粒度
- `diff(metric)`：输出变化点（但不命名、不分章）
- `join(other_stream, key)`：把两个子流按 key/时间关联
- `graph_metrics(stream)`：从互动事件构图并计算中心性/团簇等结构指标

## 非目标（明确避免）

- 不在模拟核心中内置“章节/标题/母题词表”。
- 不强制给每个窗口/阶段命名。
- 不要求生成自然语言故事（可选渲染器可后置）。

## 需要拍板的开放问题（后续再定）

- 事件 schema 版本与兼容策略（建议沿用 `schema_version` + append-only 字段）。
- 初版要记录哪些事件类型（建议从既有 outcome/telemetry 可直接提取的事件开始）。
- Entity 标识体系：如何稳定引用 agent/交互点/组织等（避免依赖临时句柄）。

## 已落地（当前实现）

- Runtime：`genesis-runtime-cli soak` 支持 `--eventline-out/--eventline-agent/--eventline-agent-index`
- Schema：`docs/architecture/foundation/eventline-schema.md`
- 离线库：`scripts/genesis_lenses/`（纯标准库；可组合算子基建）
- 单对象渲染：`scripts/observe_agent_eventline.py`
- 跨 seed 对比：`scripts/compare_agent_eventlines.py`
- 批量流水线：`scripts/run_observability_batch.ps1`
