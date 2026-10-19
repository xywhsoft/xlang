# TaskPool 原生线程退出合同

## 范围

本批补齐受管理 TaskPool 接入所需的 XRT 原生能力及固定快照，不等于语言层
worker-exit 已完成。公开 `TaskPool.wait()` 当前仍排空任务与捕获清理；随后
才接入真实工作线程上的 checked ThreadLocal 清理与错误报告。原长期任务的
其余并发、网络、密码学、X.509、TLS 和联合验收保持开放。

## 原生合同

- `xrtTaskPoolCreateOwnedExitV1` 成功接管一份上下文引用；所有真实工作线程
  共享借用它，每线程在任务和 finalizer 尾部之后执行一次 Exit。创建失败不
  接管、不调用 Exit/Drop，包括部分线程启动回滚；不能按每个作业清空 TLS。
- 原有 Close/Wait 保持排空语义。`xrtTaskPoolTryJoinV1` 在任务、资源 finalizer、
  真实资源信用和冲突原生入口排空后才关闭资源准入、发起停止；不提前杀死
  已受理资源仍需使用的线程。它不取消工作，也不归还创建者引用。
- 非阻塞查询返回 OK/BUSY/ERROR 对应原生的 OK/TIMEOUT/ERROR。OK 必须由实际
  线程完成检查证明，包含 worker Exit 和 XRT 线程包装器中的 ThreadKey 清理，
  不是只观察 worker body 的 Exited。退出回调与线程等待均不持有 ownership
  freeze/mutation 或池元数据锁；回调可以阻塞，但控制线程不随其无限阻塞。
- 原生统计只计任务，不将退出清理错误伪装成任务失败。错误由拥有上下文的
  checked 协议记录；native join 的物理状态与语言错误分开。
- 所有线程真正完成后，Destroy/图 Finish 才释放线程资源并调用一次上下文 Drop。
  collector pin 保留终态空壳直到真实 RC 归零，不猜测、修正或漏计引用。

`xrtTaskPoolOwnershipAdapterV2` 在完整图冻结下按注册 policy 的精确地址准入，
策略副本不是授权。V1 拒绝仍持有退出上下文的池；V2 额外 trace 一份实际上下文
拥有边，子节点仍需独立准入。停止开始到真实线程完成之间 Count 拒绝、Busy 为真，
避免在 Exit 或包装器 TLS 析构仍执行时错误回收。Prepare 复用同一物理停止协议，
Restore 不重新打开受理门；未知策略和忙碌查询不修改输出参数。

## 快照与 ABI

本次只同步 `include/xrt/task.h` 和 `src/concurrency/task_pool.c` 的明确增量。
原生独立仓库基线仍有旧毫秒/double 期限，而 xlang 固定快照已有微秒
`xdeadline` ABI，因此不能覆盖整个源 section。20 组准确上下文增量经过可逆
核对后分别同步 xrt/xruntime；原有期限差异完整保留。声明、符号表、快照元数据
与能力矩阵共同更新，新增 3 个 API。不导入原生工作区无关内存／文件系统改动。

## 验证

证据目录：`F:/xlang-validation/five-groups-20261019/p1-task-pool-worker-exit/`。

- 原生模块化／单头：Windows/Linux 的 GCC/TCC、O0/O2，重跑旧 TaskPool、
  真实资源尾部、拥有图、任务结果和 TaskGroupPool/OOM。新夹具每个 profile
  覆盖 16 轮拥有退出、16 轮资源信用、16 轮真实 ThreadKey 包装器尾部及 5 个
  创建分配前缀；Linux 另测所选原生闭包的 ASan/UBSan/泄漏检查。
- `tools/check_stdlib_task_pool_exit_native.py`：两套实际固定实现、相同双端
  编译器/优化组合；每个正例 16 轮精确策略、真实 worker ID、资源与晚于 Exited
  的 ThreadKey 清理、零分配 join、collector pin 和完整 count/bytes/free 账本。
  Linux 两套头各自另有 sanitizer。没有用 stub 冒充实际原生池。
- `check_stdlib_task_pool_control.py`：旧控制、任务图和结果运输回归；确保新增
  能力没有改变旧普通创建／排空行为。
- 六种坏实现分别去掉原生线程完成检查、Exit、精确策略授权，或错误地按线程
  Drop、在启动回滚执行 Exit、提前允许 Count；必须先编译成功，再由运行断言拒绝。

正式报告保留实际输入、日志、运行产物及哈希。首次 Windows 原生报告因测试
输出缓冲交错而未完成证据采集，保留失败目录，改用无缓冲的新报告；Linux
负控制首次因测试脚本漏 `-lm` 未编译，保留原记录并修正独立脚本后另跑。
没有降低产品断言、改写历史结果或执行网络／安全专项。

最终使用 `native-win-final2`／`native-linux-final`、`pinned-*-final`、
`control-*-final`、`mutants-win-final`／`mutants-linux-final2` 八份报告，共
189 个编排阶段（含 12 个预期运行拒绝）；原生官方矩阵共实际运行 153 个测试
程序，固定双头另有 18 个正例构建/运行配置。所有 Complete／InputsUnchanged／
LogsUnchanged 为 true、Failures 为空；提交前再次核对输入、日志和实际产物哈希。

## 后续语言接入（已单列实施）

把拥有退出上下文接入语言资源与延迟创建 wrapper，并正确 trace 创建前／移交后
的那一份引用；共享精简的 checked 错误账本，不为此拉入 detached Executor 调度。
语言 body/捕获错误仍由各自 Future 报告，退出清理错误单独 checked 交付。

wrapper 的非阻塞退休已改用真正 TryJoin，只有真实线程完成后才 Destroy；不再在
Wait(0) 之后假定销毁不会阻塞。新回归在真实 ThreadKey 析构中设屏障，验证控制
线程必须在屏障释放前返回 BUSY；释放后再验证真实完成、保留统计及完整账本。
旧 drain-before-Destroy 实现先编译成功，再被该非阻塞断言拒绝。该回归仅显式
增加测试用 ThreadKey 模块；无特性增量的最小部署仍使用实际裁剪计划。

后续 wrapper 批次使用同一证据目录下的 `wrapper-win-final`／`wrapper-linux-final`：
61／63 阶段通过，含两次旧实现的预期运行断言拒绝；两套头、GCC/TCC、O0/O2、
最小部署及旧任务图/结果运输保持，Linux 增加所选闭包 sanitizer。每个实际原生
控制 profile 保留 96 轮生命周期、16 轮 ThreadKey 阻塞尾部及 32 个分配预算。
两报告 Complete／InputsUnchanged／LogsUnchanged 为 true，Failures 为空。

同池异步资源必须按既有注册资源协议关闭并归还信用，不可跳过信用或强行丢资源，
也不能让“TLS 拥有资源、资源阻止线程退出”的依赖环永久悬挂。该语言集成及其
JIT/module/AOT、模块退休和 shipping DLL/SO/VFS 验收不能用本批原生结果代替。

后续受管理接入已实现，当前合同、共享账本、异步资源关闭次序和源码验收独立
记录于 [TaskPool 线程退出验收](TaskPool线程退出验收.md)。上文原生报告仅证明
本文件描述的原生底座；shipping 产品和整组验收仍未由这些专项代替。
