# 生成帧与异步存储门禁迁移

本批更新测试、驱动及依赖清单，未修改语言、Cgen 或运行时实现。
当前异步根先排入固定工作线程的就绪队列，然后由入口任务运行到第一个 await；
初始 admission 位于驻留 `xl6_continuation_frame.c`，已不在生成 C 内。

测试探针因此移到真实驻留 TU 的 admission 边界，包含实际实现，只替换测试
队列的入口选择。分配、producer 绑定、Watch、发布、后继转移、清理全部沿用
真实运行时。测试先驱动一次就绪入口，再检查原先的两个 await 状态。
异步存储 harness 显式初始化和终结实际模块；其全局内存平衡包含模块生命周期。
初始入口不计作受管测试的 await 帧，两个被观察对象仍是实际的两次挂起帧。

IR 合同检查当前 V3 布局和类型化后继工厂，同时保留 V3 Bind、V4 Observe、
trace 类型变异和 5 个存储 fixture 的全部 30 个拒绝变异。没有删除 fixture、
减少轮次或跳过失败结果。生成帧精确图计数按 await 阶段区分：cell 示例
既保留输入 capture，又通过 cell 持有当前数组；替换成 `[42]` 后，当前图增加
3 个值／容器节点和 2 条边。重复 capture、外部 alias 与根可达性断言均保留。

## 验证

Windows 的 `frame-workers-r2` 与 `async-storage-workers-r3` 全部 23 个配置通过：

- 生成帧 GCC O0/O2/TCC 各 1500 次事务、3100 次真实 Watch 退役；其余原生
  ownership、queue、handoff、executor、progress 配置一并通过。
- 异步存储三个配置各 100 次生成启动、200 次 Watch 退役、100 次真实 OOM
  回滚、100 次 blocked-cleanup 后继转移；冻结分配／释放人口保持。
- 受管帧三个配置各 240 次生命周期、580 次跨线程 freeze、820 次拒绝、
  120 次精确失败、20 次真实 DLL 卸载；producer 三配置各 100 个图、
  1040 次策略／身份拒绝、640 次后继回滚与转移、330 个分配预算。

独立重算 6999 个输入、工具、产物及日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/frame-storage-r1.proof.json`。
对应 4 个当前 IR 合同另在 Linux 完成 12 个构建／运行阶段，证据在
`class-contracts-linux-r1.proof.json`，不把它称为 Windows 原生运行人口的 Linux 复测。

这些是完整固定源码 cohort 的定点门禁证据；全体参数化门禁及普通发布产物
验收分别记录，不由本批的 23 个配置推断长期任务完成。
