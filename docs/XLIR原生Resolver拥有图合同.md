# XLIR 原生 Resolver 拥有图合同

后续 v211 已迁移本文下一阶段的实际 Resolver Future 链路。真实规划器还发现
本版本退出阶段的 Joined/Count 循环等待，已通过非阻塞只读线程完成观察修复，
仍拒绝活动清理并保留下面全部 v210 证据。新验证与未完成边界见
[Resolver Future 拥有链](XLIR原生ResolverFuture拥有链.md)。

收尾复核更正：初次统一批次通过后，新补的“已认领服务自身 worker 请求退休”
反例在排队、部分取消、全部取消和复活四组均发现 ERROR，而合同要求 BUSY。
该反例保留于 `build/tmp/xlir_launch_v210_probe_claimed_worker_regression/results.json`。
已调整共享 Join 的身份判断，worker 仍不能 join 或消费创建者引用。修复后聚焦
三通道各 8/8、最终统一 GCC/TCC 各 139/139 均已通过，所有输入前后不变。
新断言保留在模块化和单头程序中，要求每个已认领回调返回 BUSY 并保留原诊断身份。
本轮实际执行两次完整原生批次：初次结果及新增反例保留，不把初次通过当最终证据。

当前状态：2026-09-16，v210 原生候选已验证。DEMO6 当前语言集成仍为 v209，
没有把本次候选同步到四份嵌入/声明头，也没有覆盖发布 DLL。完整 native 服务图、
混合对象生命周期及最终语言/发布验收仍未完成。

## 本轮实现

修改独立 XRT 的 `net.h`、Resolver 内部布局、`resolver.c` 和 `dns.c`，提供独立的
Resolver、ResolveOp、不可变地址列表物理拥有视图及版本化准入适配器。

- 服务的 Count 读取实际 creator/operation/plan 强引用；线程数、Outstanding 和
  队列索引不冒充引用计数。查询组中的每个已接纳请求和 ready 队列中的每个请求
  各对应一次真实拥有引用；哈希、链表头尾及工作线程借用指针不重复报边。
- 操作描述实际 Resolver、结果、错误和认证 callback context 槽。缓存各自的结果/
  错误引用独立报边，地址列表是无回调的不可变叶节点，独立结果引用不虚构反向边。
- 新的 OwnedV1 查询/完成策略成功才消费一个 context 引用，失败不消费；未知策略
  在子 context Count/Trace 前拒绝，旧借用型自定义回调保持不透明。
- 认证策略必须是常驻、独立管理所有权转换的精确合同；Drop 只负责合同中的物理
  引用释放，不能作为图 Finish 阶段新启动语言语义析构的后门。调用方不得仅凭一个
  Trace 函数或当前无活动请求就把任意外部 callback 放入准入表。
- 队列与引用转换在同一 mutation domain 内完成；阻塞查询、完成回调、context Drop
  和 native join 在域外执行。条件等待醒来先释放队列锁，再重新进入 mutation，
  避免与冻结形成锁顺序反转。运行中查询/派发、join、物理退休及未完成 TLS 的线程
  退出窗口均拒绝采集，不把暂时无法证明计为可释放。
- Claim 阻止新接纳，但不删除已受理请求或合成取消。Prepare 排空原语义工作并
  非等待 join，保留拥有槽；Clear 后的 Finish 幂等退休实际资源。服务创建者引用
  与 plan pin 分开，公开 Destroy 成功才消费创建者引用。

## 批量发现与验证

初次三通道发现批次保留于 `build/tmp/xlir_launch_v210_native_discovery/results.json`。
GCC 的新测试触发严格缩进告警；TCC 发现新图计数断言漏算了入口拥有槽，原有六个
程序均通过。修正测试后分别断言有/无入口槽的精确边数，没有改变生产源码来绕过
断言。独立分组探针覆盖 11 类行为，随后统一验证。

- 修复后聚焦批次：GCC、GCC O2、TCC 各 8/8，包含模块化与单头新测试、原服务退休
  测试及原 resolver 正常/OOM/压力测试。每个新增进程执行 24 个空闲退休、32 个
  真实排队/取消/复活场景、16 个未知回调拒绝、8 个运行中查询、52 个分配预算，
  其中实际注入失败 14 次；另有独立叶结果和缓存共享计数检查。每案精确内存平衡。
- 外线程在 Lookup/Done/Drop 执行时实际取得 freeze，验证运行中节点拒绝采集，
  而非因整个 callback 占着 mutation 导致采集器永远无法执行。8 次回调复活均产生
  真实外部根，恢复 Claim 后按正常生命周期释放；不把复活误算为可清理。
- 首次完整原生批次：GCC **139/139**、TCC **139/139**，无失败或未运行。
  逐项保留上一轮全部 129 个程序，增加 DNS、TCP dial 相关覆盖及两个新入口。
  GCC 编译 111.377 秒、运行 27.417 秒；TCC 编译 10.809 秒、运行 22.223 秒。
  输入前后哈希一致，使用新的私有完整候选，原 v208/v209 封存不覆盖。

聚焦入口：`build/tmp/xlir_launch_v210_native_repaired/results.json`，SHA-256：
`C6F0B03F6B7CC8F859B18597853DEF8AFB99291C83AAA85CE707AF827081A506`。
完整入口：`build/tmp/xlir_launch_v210_native_verified/results.json`，SHA-256：
`DB9661052DAC96510AE2F310DBD8C9220AA884D286274889092B290E435D96CD`。

收尾修复后最终聚焦入口：`build/tmp/xlir_launch_v210_native_post_audit/results.json`，
SHA-256：`835485B547DDFBC2D53D3D93E0E7610F7D081F11D48B7E8C90A51EF4564E8E81`。
最终完整入口：`build/tmp/xlir_launch_v210_native_post_audit_verified/results.json`，
SHA-256：`3F57C1ADF9C447D5BA1F291EDDF7593A1EFDBF07C956BE3239488AB24593F82A`。
该快照 GCC 编译 112.747 秒、运行 27.088 秒；TCC 编译 10.352 秒、运行 22.162 秒。
原 139 个程序全部保留，仅增加回调中的真实回归断言。最终封存入口为
`build/tmp/xlir_launch_v210_post_audit_checkpoint/results.json`，保留初次封存和失败版本。

## 下一接入及不允许省略的边界

1. `resolver_future.c` 仍使用旧 bridge、借用 Done/取消上下文和无认证结果发布。
   要将真实 producer、Promise、operation、cancel watch、共享地址结果及装配/完成
   竞争接入统一拥有协议；本轮 Resolver API 不替代该实际调用链的迁移。
2. Engine 的 worker/command、port、buffer、timer 及句柄需要独立物理拥有合同，
   不能把 LiveObjects 当作通用 RC，或用 TryDestroy 成功替代图完整性。
3. 准入表、精确产物依赖及 host/module trace 必须在以上合同完整后接入同源 XLIR
   运行时，再原样复测 18 个实际模块场景及混合图/DLL 卸载，不提前删除服务拒绝。
4. v209 的语言验证证据本轮只重核沿用，不虚报重跑语言批次。全语言/FFI、同源发布、
   隔离部署、平台及旧路径退役仍保持原范围，不因本原生组件通过而宣称长期任务完成。
