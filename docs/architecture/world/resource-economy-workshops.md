# 资源经济：工坊（Workshop）配方协议（v1）

目标：在不引入 GUI/复杂交易系统的前提下，让资源类型产生真实差异，并形成由 Agent 行为驱动的最小生产链闭环。

## 背景
- `InteractionKind::Resource` 既可以表示自然供给点（Source：被动 regen），也可以表示工坊（Workshop：不被动 regen，需要 Agent 执行生产动作）。
- `ResourceType::Water` 为新名称（legacy：`Drink` 仍可被解析为 `Water`）。

## 数据契约（WorldDB）
工坊通过 `Interaction.meta.workshop` 声明（JSON object）：

```json
{
  "workshop": {
    "outputUnits": 3,
    "initial": 0,
    "inputs": [
      { "type": "Water", "units": 2 }
    ]
  }
}
```

字段说明：
- `outputUnits`：每次生产批次产出单位数（>= 1）。
- `initial`：初始库存（可选，默认 0；会被 `capacity` 上限截断）。
- `inputs[]`：输入资源列表；每个元素包含：
  - `type`：输入资源类型字符串（`Food` / `Water` / `Social`；legacy：`Drink`）。
  - `units`：每批次消耗单位数（>= 1）。

## 运行时语义（v1）
- 工坊资源点不再被动 regen：初始化时与运行中都视为 `regenPerStep = 0`。
- 产出注入：只有当 Agent 在该 `interactionId` 执行生产动作后，库存才增长。
- 消费：Agent 的消费仍然按“在目标资源点消耗库存”执行；当目标为工坊且库存不足时，Agent 会尝试获取输入、生产、再消费（见 action 规划）。

## Worldgen 配置（TOML）
基准世界通过 `worlddb.resources.workshop` 生成工坊 meta：

```toml
[[worlddb.resources.workshop]]
output = "Food"
output_units = 3
initial = 0
inputs = [{ type = "Water", units = 2 }]
```

注意：
- 当前实现按 `output` 绑定到 `ResourceType`：当某资源类型存在 workshop 规格时，该类型生成的资源点会被标记为工坊并禁用被动 regen。

