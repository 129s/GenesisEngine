# 代码约定（Coding Conventions）

本文档用于固化“新增代码默认遵循”的工程约定，避免风格分裂导致的维护成本与重构摩擦。

## 1) 命名空间（Namespace）

目标：对外/跨模块引用统一使用 `Genesis::` 根命名空间；内部实现仍允许保持 `genesis::`，并以渐进方式迁移。

### 1.1 规则（新增代码必须遵守）
- **跨模块引用优先使用 `Genesis::`**（而不是 `genesis::`）。
- **使用 `Genesis::X` 前必须包含对应模块的 `genesis/<module>/Namespace.hpp`**（避免靠“碰巧某个头文件定义了别名”）。
- **不要新增新的全局 alias 聚合头**（避免与已有 `Namespace.hpp` 发生冲突）。

示例（推荐写法）：
```cpp
#include "genesis/world/Namespace.hpp"
#include "genesis/world/WorldTypes.hpp"

Genesis::World::MapId map = 1;
```

Agents（带 Components 子命名空间）：
```cpp
#include "genesis/agents/Namespace.hpp"
#include "genesis/agents/Needs.hpp"

Genesis::Agents::NeedType need = Genesis::Agents::NeedType::Hunger;
```

### 1.2 可用的 Namespace 过渡头
- `include/genesis/agents/Namespace.hpp` → `Genesis::Agents`（含 `Genesis::Agents::Components`）
- `include/genesis/base/Namespace.hpp` → `Genesis::Base`
- `include/genesis/core/Namespace.hpp` → `Genesis::Core`
- `include/genesis/diagnostics/Namespace.hpp` → `Genesis::Diagnostics`
- `include/genesis/messaging/Namespace.hpp` → `Genesis::Messaging`
- `include/genesis/simulation/Namespace.hpp` → `Genesis::Simulation`
- `include/genesis/telemetry/Namespace.hpp` → `Genesis::Telemetry`
- `include/genesis/world/Namespace.hpp` → `Genesis::World`
- `include/genesis/worldgen/Namespace.hpp` → `Genesis::Worldgen`

> 迁移策略与阶段计划见：`docs/architecture/proposals/meta/namespace-strategy.md`。

