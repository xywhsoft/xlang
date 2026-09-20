# XLIR 原生传输缓冲与冻结边界

## 本轮实际改动

此次修改的是原生 `D:/GIT/xrt` 工作树，没有放宽 XLIR Cgen 对不完整拥有图的拒绝，也没有同步生产单头、发布 DLL 或切换默认 Cgen。

- Listener 的同步接受、队列追加和队列分离，统一先进入拥有关系变更域，再获取 AcceptLock。分离链上的 Next 拥有边也在变更域内转移；Abort、Destroy 及用户回调在域外执行。
- Stream 和 Listener 的终态 WaitClosed 写入沿用相同的变更域／队列锁顺序，与 Future 路径一致。
- Stream 的 ReadBuffer、WriteBuffer 各自真实持有一个缓冲池引用，关闭和初始化失败时使用 Unit 成对释放，清除池指针。不新增对象字段、额外分配或虚构的缓冲 RC 节点。
- 普通单节点发送和文件发送的缓冲块保存明确的不可变拥有策略，使用与调度任务相同的物理 send-owner 身份。原有资源引用转交缓冲块，调度引用仍独立存在。
- 新增内部文件块拥有接口 `__xrtNetBufAppendFileOwned`。缓冲层只描述真实上下文，不解释文件句柄；未提供描述的旧文件块仍拒绝进入拥有图。未知外部发送载荷继续由原有 Trace 明确拒绝。

## 验证证据

- `E:/xlang-goal-artifacts/xlir_launch_v231_buffer_discovery/results.json`：首批 Windows／Linux GCC 共 26 项通过。
- `E:/xlang-goal-artifacts/xlir_launch_v231_buffer_verified/results.json`：67 个定向组合通过，包括 Windows／Linux GCC、GCC O2、TCC，以及 Linux ASan／UBSan／泄漏检查；另外 8 个 NET_BUFFER、NET_TCP、NET_TCP_FUTURE、NET_TCP_DIAL 裁剪编译配置通过。每个组合独立工作目录，最多三个编译／测试进程并行。
- `E:/xlang-goal-artifacts/xlir_launch_v231_freeze_proof/results.json`：更新后的同一份 Listener 测试在旧 v230 运行时准确触发缺失变更域的目标断言，在当前运行时 7 种配置下通过。它观察被真实队列锁阻塞的 Accept 调用已经进入线程变更域，随后验证原队列 Stream 引用确实转交；不是用睡眠猜测并发成功。

第一次旧版本对照触发了外层工作线程的通用看门狗，不能算作目标缺陷的证明。失败保存在 `xlir_launch_v231_freeze_baseline/results.json`，其 `DiagnosticComplete=false`。随后仅延长测试外层看门狗，保留目标断言期限；只重跑受影响的 Listener 测试。67 项通过之后，生产实现没有再修改，这一处测试差异由 freeze_proof 精确记录。

新增文件块测试保留了未知策略拒绝、真实上下文引用计数、OOM 不转交拥有关系、空区间拒绝、域外释放回调和精确内存平衡。实际复制文件句柄的关闭行为由既有 TCP File／send-job 测试覆盖。不能把缓冲层的描述符测试说成完整文件或传输对象图验收。

## 仍未完成的关键工作

Stream／Listener／UDP 的完整规范拥有图及退出准备尚未完成，对应 Trace 仍明确拒绝不完整图。这轮没有提供完整父对象收集适配器。

- Refs 多视图发送及 SendBuffer 共有载荷仍是资源组模型，不能将 Remaining 冒充引用计数，也不能把各 item 地址当成同一物理节点。
- Dial 的 attempt/control 需要明确真实拥有边和实际引用生命周期；现有释放函数本身不构成拥有描述。
- 父对象 Trace 需要完整的 Worker 静止条件及外部可变队列覆盖；Prepare 必须真实结束操作和回调，不能把关闭对象伪装为叶节点。
- 全量原生回归、完整 Engine／服务／传输／TLS 混合生命周期、FFI／动态库卸载、跨平台及工具链验收、发布与默认 Cgen 切换仍是独立门槛。

上一批 v229 的 1,518 次原生执行是历史批量基线，不是本轮修改后的全量验收。本轮检查点只标记定向验证完成。
