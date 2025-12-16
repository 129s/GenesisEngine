# Proposal · Open Questions / 后续决策点

- **Tilemap 规模与分辨率**  
  - Root map 是否需要全覆盖 Tilemap？大世界是否采用多分辨率（外部低分辨率、内部高分辨率）？  
  - Chunk 化后如何定义最小/最大尺寸、内存预算、LRU 策略？

- **位置/插值数据**  
  - Telemetry 目前建议输出 `position(x,y)` 与可选 `movement{from,to,t01}`。是否需要额外的速度、朝向以支持更精细的动画？  
  - 可视化与逻辑步是否需要锁步约束，还是允许可视化延迟若干帧？

- **命令事务性**  
  - 世界生成/刷新已在模拟线程串行执行，但是否需要事务/回滚模型？  
  - 命令失败时如何反馈到 GUI（弹窗/日志/Telemetry 标记）？

- **多世界/会话管理**  
  - Runtime 是否需要支持并发世界实例？如果需要，API 如何区分 `worldId`？  
  - GUI 是否有必要实现多会话切换或标签页？

- **网络化与远程运行**  
  - 是否计划将 Runtime 独立进程化，通过 IPC/网络协议与 GUI 通信？  
  - 如果是，Telemetry/Atlas 的序列化协议与版本控制需要提前设计。

- **二进制 Tilemap 协议**  
  - `.tmb` 格式的压缩方式（RLE/LZ4）、跨平台字节序约定、版本升级策略尚未定稿。  
  - 资产打包流程与 CI 校验需要何种工具链？

- **录制/回放与度量**  
  - 是否内建快照录制/回放接口？如何处理大型 Telemetry 序列的存储？  
  - 指标阈值、报警机制、性能基线（例如模拟 1h 的吞吐量）需不需要标准化？

- **Scene 布局描述与编辑器**  
  - `layout` 描述是否统一使用 JSON Schema？运行时是否支持热加载或增量覆写？  
  - 是否需要提供可视化编辑器/调试面板，查看 Scene → 子节点坐标与布局生成参数？  
  - 复杂场景（噪声生成/脚本生成）如何保证可复现并写回 `coord_local/coord_global`？

- **Portal 锚点与渲染协同**  
  - Portal `anchors{at_from, at_to}` 是否需要在 `WorldAtlas` 中单独索引，供 GUI/调试快速查询？  
  - SceneView 中的 Portal/资源标记是否需要对齐全局坐标，还是只依赖 `coord_local`？  
  - 传送时的动画/插值是否需要额外的 Telemetry 字段（如跨 Scene 过渡 t 值）？
