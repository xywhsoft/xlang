# XLIR 原生端口拥有链

v218 更新：readiness 代次引用已实现；Select／epoll 已有真实运行证据，
kqueue 原生平台验证仍开放。见 [原生 Watch 拥有链](XLIR原生Watch拥有链.md)。

v217 更新：完成式 OS 请求已建立显式拥有提交和真实终态转移，Windows／Linux
实际 XLIR 规划器均已验证；readiness watch 与完整服务图仍开放。以下保留
v216 的 Post 里程碑，当前进展见 [原生 OS 请求拥有链](XLIR原生OS请求拥有链.md)。

2026-09-19，v216。已实现 Port Post → 交付事件 → 回调结束的真实拥有引用转移，
并用未修改的生产 XLIR 回收规划器验证。OS 请求、readiness watch、完整 Engine／
transport 与生成模块图仍未完成；这不是解除原 18 个服务图门的依据。

## 实现与语义

`xnetport` 仍是唯一拥有的存储，不制造一个虚假的 RC 节点。真实计数宿主投影
端口的拥有槽，以及自己保存的已交付事件槽。每个成功的 owned Post 消费一个
context 引用，失败不消费；User 是单独的借用派发身份，由 context 真实保护。
队列节点只借用 lease，端口链表才是拥有槽；出队在同一 mutation 内把该引用
转交给事件。没有额外添加只供图检查使用的引用。

`xrtNetPortEventReleaseV1` 先清空事件身份，再在冻结／mutation 区外释放 context；
重复释放无效。Accepted socket 仍有独立拥有权。Engine 在 completion 返回后，
以及没有 completion 的情况下，均释放交付引用。端口销毁不影响已交付事件的引用。
事件是 move-only，复制结构体不保活；Engine 回调借用的事件不能由用户再次释放。
这里的屏障退出指 API 自己建立的作用域；调用可能触发 Drop 的释放／销毁操作时，
宿主不得包裹一个仍活动的外层冻结或 mutation。宿主自身的用户语义析构必须在
Finalize／Prepare 阶段完成并重验可达性，不能挪到 Clear 之后的物理 Finish 中。

Trace 与适配器查找需要完整冻结和精确 policy 身份准入。排队 lease 的 Prepare
保持 BUSY，真实交付或物理排空才使其 READY。Host 的准备过程负责端口实际收敛，
Claim 不伪装成取消成功。collector pin 真正保活 lease/context，Clear 后 Finish
恰好一次解除 context 引用。旧 Post／Watch／Submit 使用仍使该端口拒绝完整图准入，
Engine 的 `OwnershipPortExposed` 边界没有删除。

## 集中发现与修复

- 新测试最初把既有 `Ready` ABI 写成 `IsPrepared`，编译失败源码和诊断已保存。
- 新规划器夹具最初要求一次 Finalize 必然 READY。真实规划器按初始节点数限制每轮
  工作量，因此最后一个回调完成时也可能正常返回 BUSY。夹具改为继续同一计划，
  通过 Commit／状态查询确认最终状态，没有修改规划器或重复语义析构。
- 源级并发审查发现两个独立问题：owner 线程的 Watch／Submit 与跨线程 Post 可以
  同时设置不透明标志，故其改为原子状态；Prepare 在规划器冻结区外调用，不可直接
  读取同时被出队线程修改的 Port 归属，故其自行取得非阻塞冻结，冲突返回 BUSY。

对第三项保留了实际旧代码反例：新测试链接修复前的真实单头实现，构建成功，运行
在“另一线程持有 mutation，已交付 lease 却返回 READY”的断言失败。修复后同一
测试通过。第一次反例构建的 include 路径错误、随后真实反例，以及源审查时中止的
扩大编译也分别保存，均不算成功回归或安全软件问题。

## 已验证证据

修复后 `xlir_launch_v216_native_repaired/results.json`：GCC、GCC O2、TCC 三配置
各 40/40，包括模块化和单头。每个端口程序在 Select／IOCP 上覆盖 104 次交付、
8 次回调重投、56 次拒绝、12 个分配预算（4 次真实失败）、16 次准备生命周期、
8 组并发发送者、16 个 Engine 回调／无回调场景和 16 次准备冲突。
外线程冻结探针确认 Drop 未继承全局屏障；活动分配数、字节、分配／释放和非法
释放／重复释放／释放后使用计数精确平衡。

持久入口 `tools/check_xlir_native_port_plan.cjs` 与
`test/cases/xlir_native_port_plan.c` 链接原样 `xl6_ownership_plan.c`。修复候选
`xlir_launch_v216_port_plan_fixed/results.json` 三配置各通过 64 个真实循环，
含 64 次策略拒绝、32 次外部交付引用保活、32 次析构复活、64 次真实 BUSY 准备、
160 次释放回调冻结探针；排队销毁、先交付后销毁、局部批次和 collector pin
期间移除拥有槽均覆盖。

相同候选的 Engine、Buffer、TaskNet 规划器分别三配置各通过 192、96、80 个
循环，报告为 `xlir_launch_v216_{engine,buffer,task_net}_plan_fixed/results.json`。
最终扩大回归 `xlir_launch_v216_native_verified/results.json` 已完成：GCC、TCC
各 307/307，0 失败、0 未运行，保留 v215 全部 294 个程序并加入 13 个端口后端／
拥有权程序，输入指纹前后相同。只有这一轮完成全批次执行；另一次因源审查中止的
`xlir_launch_v216_native_final` 保留为中止编译，不能充当成功证据。

## 下一步与未完成边界

1. IOCP／io_uring 请求描述符需要显式的借用 lease 元数据；只有终态完成才转移
   拥有引用，不能在取消请求成功时提前释放缓冲或 socket context。
2. Select／epoll／kqueue 的 watch 代次要有真实共享 lease 引用，支持分方向
   one-shot、同批合并及替换 watch 后旧事件仍保活旧 context。
3. Engine 的 parked 覆盖整个 Wait。后端仍在修改自己的队列时，不能据此扫描
   私有节点；必须把在途记录和已发布事件的引用转移纳入可冻结的显式协议。
4. TCP／UDP／TLS 需要同步迁移旧 continuation self-drop 与真实宿主 Count／Trace，
   再连接 XLIR 服务合同、常驻拥有槽和宿主精确 policy 准入；不能只删掉 Cgen
   的 live-service 拒绝。在 `xl6_network_service.c` 中 Engine／Resolver 都由真实
   服务槽拥有，当前 `xl6_ir_cgen_module_trace.c` 的安全拒绝保持不变。

语言生产集成仍为 v209，原 4233 个输入的证据须重核后复用。四份嵌入／声明头及
三个发布文件未由本批覆盖，默认 Cgen 未切换。原 ResolverFuture 与独立 layout
的 TCC 隔离缺口仍保留，端口 TCC 通过不替代它们。完整语言／FFI、混合生命周期、
DLL 卸载、同源发布／隔离部署、平台与旧路径退役仍开放。
