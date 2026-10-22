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

## 当前产品重新核查与共用前置（2026-10-22）

上述 v213 是历史私有候选的证据，不能作为当前 xlang 固定运行时已接入网络任务
的证明。当前 `lib/xrt/xrt.h` 的 `src/network/task_net.c` 仍用旧的任务 Data 包装、
借用式 Post／Timer 和 FutureBridge Watch；原生 Job 未绑定封闭 Data policy，
延迟操作受理之后仍有可失败的取消桥装配。`task_group_net` 仍组合该旧路径。
当前标准库只有 Engine 的 detached post／after，没有受管理网络任务结果接口。
`net_file` 也仍是 Worker-only、借用缓冲／Completion 的四个入口，不直接导出裸
Completion 或把在途缓冲当成语言 leaf。TCP 的 sendFile 已有实际驻留实现，不能
与 Engine 原生文件完成接口混为一项。

另需保留版本边界：xlang 固定快照使用微秒／Until；独立 XRT 当前公开等待已经
是 int64 毫秒／For，内部定时值为 xrtTimer 的 double 秒。不能整份覆盖新头后
继续使用旧实现。原生增补应按两端各自时间合同定点维护同一拥有权算法，而不是
顺带升级全部时间 ABI；这项同步方式已提交用户确认，本批未更改 XRT 或快照。

共用目录前置已经实现：新增强类型 NetTask／NetTimer／NetWorkerExit capability
组，由选中 domain 在 Load 时登记，创建结束后封闭；内建 detached-work 三种
策略也走同一路径。Native Engine 查询不再写死单项数组，而把封闭目录的完整
typed 切片传给 XRT 的身份检查器。相同指针去重，同形副本不是原策略的授权，
不猜 Count／Trace，也不让 Engine 单独使用时拉入 TaskNet 或其他生产者。

验收为 `tools/check_stdlib_net_engine_policy_registry.py`，只创建 Engine／定时器／
工作记录，不创建应用 Socket 或应用网络流量。实际原生拥有对象、待完成 Timer、worker
exit、策略反序加载、目录增长／去重／封闭、16 个构造分配前缀、SDK 强类型声明
及 API 符号编译均验证；通用 ABI 参数探针先检查实际 typed 策略切片，再调用
未改动的 XRT 检查器，不制造图节点或引用数。漏传三种列表、丢失去重、重新开放
目录及吞掉加载失败的错误实现均必须被测试拒绝，并保留原 Engine 整图回归。

最终选择 policy-win-r6 与 policy-linux-r6，共 130 个阶段（Windows 64／Linux 66），
包含 GCC／TCC O0／O2、Linux ASan／UBSan、13 类成功编译后的错误实现反证和
3 类 SDK 类型拒绝；构建、拒绝和优化重复不是独立语言程序。输入、日志和产物
哈希全部复核，正式回执为证据根 `p3-net-task/policy-verified-commit.json`。

受影响的 UDP 驻留回归另有 Windows 43／Linux 48 阶段，通过 select／IOCP／epoll、
GCC／TCC、Linux ASan／UBSan 和未 await 控制的退役图；仅自有字面量回环，
不实际入组或发送多播。合计 221 阶段（107／114），不将其冒充源码 JIT／AOT
或发布产品验收。两端正式目录均在 `p3-net-task` 证据根，失败轮次保留但不计通过。

这只关闭共用目录前置，不关闭 TaskNet／TaskGroupNet／NetFile 或完整网络目标。
原生拥有记录、结果及取消／完成时序、语言 generic profile 接入和实际产品验收
仍须继续。没有发布库、VFS、外网、DNS、协议或网络安全专项验收。

## 拥有型 TaskNet 底座定点落地（2026-10-23）

用户已明确批准保留两端各自时间合同的定点增补，本项不再等待同步方式决策。
独立 XRT 提交 `79f5da3c3c0859508329e42a2db79afd8f9e6fde` 增加封闭 Data／
结果 policy、立即／延迟任务、真实 Frame／Job／Engine 外壳／Watch 拥有关系，
以及 exact-identity adapter 和 quiescence。xlang 对应增补四份嵌入／声明头：
微秒／Until 保持不变，独立 XRT 仍为毫秒／For，没有整套时间 ABI 升级。
定点同步器逆向移除新增片段后逐字节比较原头，证明未带入其他原生改动。
快照公共 API 数现为 XRT 3352／xruntime 3817（xruntime 自有仍为 465）。

所有可失败的取消桥装配在 Engine 受理前完成。成功后只发布 Ready；预先取消
先记录请求，再对真实 Timer ID 重放。Job 在 Future 终态之前释放用户 Data，
而 Frame 的物理释放可等待 graph pin；两者不混淆。取消尾部另持 Engine 外壳
引用，外壳不增加 LiveObjects，不阻止任务自身的停止排空。未知用户 policy
在 Count／Trace 前拒绝，用户 Proc／Drop 和等待不在内部锁／mutation 中执行。

正式底座验收 Windows 48／Linux 64 阶段，共 112 阶段。四种构建形态为固定
xrt、固定 xruntime、独立 XRT 单头与模块化；GCC O0／O2、TCC 及 Linux
ASan／UBSan 都运行真实当前实现和原样 XLIR 规划器。每种配置运行 80 轮真实
pending 图，覆盖成功、停止、取消、错误策略拒绝和实际外部复活／重建计划；
不是推算引用数或伪造节点。生命周期夹具另检查 64 个分配预算、12 次实际失败、
16 次受理前取消、16 次未启动 Engine 拒绝和 callback／Drop 中独立 freeze，
逐场景核对实际内存账本。

9 个新 API 的真实产物模块查找在双端 O0／O2 检查，共 8 阶段。准备轮发现
native symbol 表必须保持二分查找排序，已排序并完整重跑该专项，不改变查找器。
快照门禁 Windows 10／Linux 5 阶段（包括真实陈旧资产拒绝），合计本批 135 阶段。
夹具准备轮还修正旧 BufPool adapter 参数以及重复取消已终态 Future 的错误断言；
准备／失败轮保留，不计成功。没有放宽拥有权 verifier 或安全默认值。

证据根 `F:/xlang-validation/five-groups-20261022/p3-net-task-owned`；
正式底座 `native-win-r4`／`native-linux-r3`，映射 `map-*-r3`，
快照 `snapshot-win-r2`／`snapshot-linux-r1`。原始 Linux 证据与 F 盘镜像均保留。
入口为 `check_stdlib_task_net_owned_native.py`、`check_stdlib_task_net_owned_artifact.py`、
`check_stdlib_snapshot_guard.py`；定点同步入口 `sync_task_net_owned_snapshot.py`。
回执为 `verified-commit.json`，backport 身份在 `lib/xrt/backports/task_net_owned.json`。

本项只关闭拥有型原生底座与定点同步。语言 generic profile／结果投影、封闭目录
family 接入、TaskGroup 拥有型组合、NetFile、完整服务图及 shipping 仍开放。
旧借用路径不因新接口存在而获得认证；本批没有修改 Cgen 实现、发布库或 VFS，
没有应用 Socket、DNS、应用网络流量、协议攻击样本或网络安全专项。

