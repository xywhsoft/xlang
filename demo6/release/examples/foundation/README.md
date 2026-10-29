# 基础查询、诊断与对象拥有组

在 demo6 目录使用同一次构建的产品运行：

```text
release\xl.exe run release\examples\foundation\system.xl
release\xl.exe run release\examples\foundation\memory.xl
release\xl.exe run release\examples\foundation\pools.xl
```

Linux 使用 `release/xl` 和 `/` 路径分隔符。

system 查询实际执行进程的 ABI 及系统在线 CPU 数，不把它等同于 affinity
或容器 quota。memory 需要显式导入；快照只读，统计开关影响同进程的全部
XRT 使用者，例子在 finally 中恢复开关。debug 是否开启由宿主/产品决定。

pools 例子清理池根之后仍能访问 actor；它在最后一个外部引用释放时才析构。
ObjectPool<T> 按固定类型持有对象，ObjectArena 可混合类类型；不要求继承
动态字段 object。池操作限制在创建线程。它们复用现有 XRT 共享堆，提供
批量生命周期管理，不是独立 slab 分配器，也不复用仍然存活的对象实例。

完整合同见 [基础九组标准库边界与验收](../../../docs/基础九组标准库边界与验收.md)。
