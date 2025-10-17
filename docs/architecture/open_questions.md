# Open Questions / 后续决策点

- Tilemap 范围与密度：是否全图覆盖，还是仅对部分 Room/Point 提供？是否支持多分辨率？
- 插值数据来源：在 Runtime 侧产出 `movement.progress` 即可满足 GUI 插值，是否引入更丰富轨迹（如速度/朝向）？
- 命令一致性：命令执行的事务性与回滚策略（必要吗？）
- 多世界/会话：Runtime 是否支持并发世界实例？接口如何区分？
- 网络化：未来是否考虑 Runtime 独立进程 + IPC/网络 API？（对 GUI/CLI 解耦）
- 度量与回放：是否内置录制/回放（快照序列）接口？

