# Schema Versioning（协议版本策略）

本文档定义 GenesisEngine 对外只读协议的版本演进规则，目标是让 GUI/工具链在协议变更时 **可检测、可拒绝、可升级**，避免“静默错读”。

## 1. 版本类型

- `TickTelemetry.schema_version`：遥测协议版本（GUI/分析工具消费）。
- `WorldAtlas.schema_version`：Atlas 协议版本（世界静态结构与可视元数据）。
- `WorldAtlas.world_version`：世界数据版本（随 `WorldDatabase` 的静态结构变更递增）。

## 2. 变更与版本提升

当前仓库采用偏保守策略：
- **新增字段也提升 `schema_version`**（即使字段是可选的），用版本号显式驱动消费端适配。
- 删除/重命名/语义变化属于破坏性变更：必须提升版本，并更新迁移说明。

## 3. 消费端约定（强制）

- GUI/工具链在启动或首帧读取时校验 `schema_version`：
  - 版本不匹配：应拒绝解析或降级到“只显示原始 JSON/错误提示”，避免错误展示。
- Headless/CLI 默认启用 schema 校验（可用 `--no-schema-check` 临时绕过，仅用于排障）。

## 4. 文档与测试门禁

当版本号变化时，必须同步更新：
- `docs/architecture/foundation/telemetry-schema.md`
- `docs/architecture/foundation/runtime-api.md`

仓库测试 `tests/test_contract_docs_schema_versions.cpp` 会检查文档中声明的版本号与代码常量一致，用于防止“代码变了文档没变”。
