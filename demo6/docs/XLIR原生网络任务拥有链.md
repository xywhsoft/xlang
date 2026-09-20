# XLIR 原生网络任务拥有链

2026-09-19，v213。网络立即任务和延迟任务已共用一个实际 continuation frame，
在独立 XRT 候选上通过聚焦回归和真实 XLIR 规划器验证。它不是生成模块全服务图、
完整语言集成或发布验收；DEMO6 生产集成仍为 v209。

## 此轮修复的合同

- Frame 实际拥有 Job 执行器引用、Engine 计数外壳、受理后的用户 Data 和取消
  Watch。Job 的 Data 实际拥有同一个 Frame；Future producer 仍为真实 Job。
  Bridge 借用 Job 的 Promise，不能再计一条 Promise/Future 强引用边。
- Post/Timer 与取消 Watch 分别消费一份 Frame 引用；Worker 仅在回调内借用。
  Engine 外壳引用不增加 LiveObjects，避免任务自身阻止 Engine 关闭和排空。
- Job/Promise 和可能失败的取消监听装配先于 Engine 受理。受理失败不消费用户
  数据，不遗留暗中执行的命令或定时器；成功后只发布 Ready，没有后置装配失败。
- 已取消父令牌可能同步通知：先记录请求，在真实 Timer ID 发布后重放取消。
  取消尾部在锁内另取 Engine 引用，在锁外提交，避免与完成时释放 Engine 竞争。
- 用户数据析构先于 Future 终态；Frame 最后的物理释放单独处理，不能让图 pin
  延迟语言清理。成功结果必须符合独立 payload policy，借用的非空结果明确失败。
- 未发布、活动或含旧不透明数据的 frame 拒绝图检查；未知策略在用户 Count/Trace
  前拒绝。Prepare 只观察执行器清理，Engine 负责关门排空，不制造第二套取消语义。
  用户 Proc/Drop 和等待不包在内部 mutation 或锁中。

## 批量证据

首轮快速语法预检发现新测试对 `xrtCancelCreate(void)` 传参不匹配；源码实现
预检通过。重型编译没有启动，三个配置各 32 个程序如实记为未运行；原失败记录
位于 `build/tmp/xlir_launch_v213_native_discovery/results.json`，未删除。

修正该测试调用后，聚焦 GCC、GCC O2、TCC 各 **32/32**，包括旧 task-net、
task-group-net、OOM、Engine、服务退休、桥、任务核心和新拥有图程序。
每个新增普通／单头程序执行 30 个拥有图、6 次旧接口拒绝、64 个分配预算
（12 次真实失败）、96 次完成／取消竞争；用户 Proc 和 Drop 中均由另一个线程
成功取得 freeze，并观察活动 Frame/Engine 拒绝 Count。逐场景内存精确平衡。

持久入口 `tools/check_xlir_native_task_net_plan.cjs` 链接原样的
`src/runtime/xl6_ownership_plan.c`，三个配置各 **80 轮**：

- 80 个真实 pending 图；Root 实际拥有 Engine 创建者和 Future 观察端；
- 48 次用户运行，80 次 Data 析构，32 次 owned payload 释放；
- 32 次错误 Data policy 拒绝，40 次外部复活、撤销旧计划和新计划回收；
- 80 次语言 finalizer、80 次 Root 实际释放，精确内存平衡。

构造者引用保留到第一个有外部根的计划真正取得 pin，随后实际释放并重新判断
可达性；不伪造内部根或从 RC 推算减数。零时限定时器正例要求先实际成功完成，
不把规划器提前关闭导致的失败当作成功。独立结果是 Future 的真实拥有槽。

规划器记录：`build/tmp/xlir_launch_v213_plan_discovery/results.json`。

稳定后执行一次完整原生回归：GCC **162/162**、TCC **162/162**，零失败、
零未运行，保留此前全部 157 个程序。GCC 编译 152.806 秒、运行 33.220 秒；
TCC 编译 14.244 秒、运行 24.738 秒。8086 个输入记录和 8082 个当前工作区
来源文件前后指纹一致，结果位于 `build/tmp/xlir_launch_v213_native_verified/results.json`。

该最终候选与网络任务规划器已测头逐字节相同（SHA-256
`9FA2BA9B60AC1313096C5C8A6DB6827E4B618255E5ECBF6FAAC690FF0B3C707A`），
沿用其三通道证据，不重复同一测试。另在最终候选上复验既有 Engine 规划器，
三个配置各 96 轮通过，证明本轮没有破坏原有命令／定时器回收合同。
总检查点为 `build/tmp/xlir_launch_v213_checkpoint/results.json`。

## 当前源码和未关闭范围

私有候选从当前 XRT 工作区完整冻结，基线提交
`31cde60e4ef2eb824ea456451fc1c5aad68ff93a`；保留该提交及当前工作区的其他修复，
不再只向旧 v212 vendor 覆盖少量文件。新增测试和合同文档也明确纳入快照。
生成只发生在私有候选，不覆盖 DEMO6 的四份嵌入／声明头和三个发布文件。

裸 port/completion、存活 pool/external buffer、嵌入命令、transport 完整拥有关系
仍待认证。原 18 个生成模块服务图门、混合对象、实际 DLL 卸载、全语言／FFI、
同源发布／隔离部署、平台与旧路径退役继续开放。v211 ResolverFuture 规划器的
TCC 隔离缺口不会因本项 TCC 通过而消失。本轮没有改变默认 Cgen。
