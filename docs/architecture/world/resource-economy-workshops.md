# 资源经济：工坊（Workshop）配方协议（v2）

目标：在不引入 GUI/复杂交易系统的前提下，让资源类型产生真实差异，并形成由 Agent 行为驱动的最小生产链闭环。

## 背景
- `InteractionKind::Resource` 既可以表示自然供给点（Source：被动 regen），也可以表示工坊（Workshop：不被动 regen，需要 Agent 执行生产动作）。
- `ResourceType::Water` 为新名称（legacy：`Drink` 仍可被解析为 `Water`）。
- `ResourceType::Ore`：用于“非需求型资源”，只参与生产链输入（不直接满足需求）。
- `ResourceType::Tool`：用于“中间品 / 生产力放大器”，只参与生产链输入（不直接满足需求）。

## 数据契约（WorldDB）
工坊通过 `Interaction.meta.workshop` 声明（JSON object）：

```json
{
  "workshop": {
    "initial": 0,
    "recipes": [
      {
        "outputUnits": 3,
        "inputs": [
          { "type": "Water", "units": 2 }
        ]
      },
      {
        "outputUnits": 5,
        "inputs": [
          { "type": "Water", "units": 1 },
          { "type": "Ore", "units": 1 }
        ]
      }
    ]
  }
}
```

字段说明：
- `initial`：初始库存（可选，默认 0；会被 `capacity` 上限截断）。
- `recipes[]`：配方列表（至少一个）；每个元素包含：
  - `outputUnits`：每次生产批次产出单位数（>= 1）。
  - `inputs[]`：输入资源列表；每个元素包含：
    - `type`：输入资源类型字符串（`Food` / `Water` / `Social` / `Ore` / `Tool`；legacy：`Drink`）。
    - `units`：每批次消耗单位数（>= 1）。

兼容：
- 旧协议（v1）仍可使用：直接在 `workshop` 下提供 `outputUnits + inputs[]`（等价于单配方）。

## 运行时语义
- 工坊资源点不再被动 regen：初始化时与运行中都视为 `regenPerStep = 0`。
- 产出注入：只有当 Agent 在该 `interactionId` 执行生产动作后，库存才增长。
- 消费：Agent 的消费仍然按“在目标资源点消耗库存”执行；当目标为工坊且库存不足时，Agent 会尝试获取输入、生产、再消费（见 action 规划）。

## Worldgen 配置（TOML）
基准世界通过 `worlddb.resources.workshop` 生成工坊 meta：

```toml
[[worlddb.resources.workshop]]
output = "Food"
initial = 0
chance = 1.0
recipes = [
  { output_units = 3, inputs = [{ type = "Water", units = 2 }] },
  { output_units = 5, inputs = [{ type = "Water", units = 1 }, { type = "Ore", units = 1 }] }
]
```

注意：
- `chance` 为 0..1：决定“该资源类型生成的资源点中，有多少会被标记为工坊”（用确定性哈希保证 `config + seed` 可复现）。
- `chance` 默认 1.0（保持历史行为），但建议在生态类基准世界里使用小于 1 的概率，让同一资源类型同时存在 Source 与 Workshop，以避免全局耦合导致形态单一。

兼容：
- 旧写法仍可用（单配方）：
  - `output_units = 3`
  - `inputs = [{ type = "Water", units = 2 }]`
