# 工作线程已捕获错误的传播缺陷与验收门

真实共享产品中，工作线程已经捕获 `MemoryError`、执行 `finally` 并返回 0，
原生线程仍报告退出码 1。Windows 与 Linux 都已复现；这是语言错误传播的
发布阻塞项，不是标准库缺少公开 API，也不能通过放宽线程退出断言解决。
本批增加规范回归和必需门禁，尚未修改生产实现，53 模块长期任务继续。

## 测试范围与结果

`stdlib_containers_worker_oom.xl` 在真实语言 `thread` 内调用
`xrtMemDebugFailAfter`，分别执行文本、Regex、Array、List、Dict、Set、Stack、
OrderedMap、SlotMap、MPMC／WaitQueue 十组操作。数据包括 UTF8、内嵌 NUL、
bytes、嵌套对象共享和拥有快照。只捕获 `MemoryError`，`finally` 核对真实
故障触发与捕获一致；不清空错误槽来掩盖问题。

`stdlib_worker_oom_product_host.c` 通过公开编译 API 调用真实产品，每组独立
进程收集结果。原生观察器只登记测试回调、保留真实线程引用和读取账本，
不替换分配器、线程入口、容器或运行时提供方。每个宿主预热后逐次注入
0 到首个未触发点，再确认下一点；成功才会输出完整前缀标记并检查堆账本。
目前失败会提前中止该组，不能称为已完成其所有故障前缀。

每端结果如下，两份产品分别是运行库和编译库；O0／O2 指原生宿主的优化等级，
不代表 TCC JIT 有这两种优化模式。

| 验证 | Windows | Linux |
| --- | --- | --- |
| 共享产品，2 产品 × 2 宿主优化等级 × 10 组 | 40 项失败 | 40 项失败 |
| 每个失败均记录已捕获且 finally 完成，原生退出 1 | 40 项 | 40 项 |
| 同一语言用例的生成 C，O0／O2 严格编译 | 2 项通过 | 2 项通过 |
| 独立 AOT 构建 | 通过 | 通过 |
| AOT，每组 budget 0 与无故障对照 | 18 通过、2 失败 | 18 通过、2 失败 |

AOT 的 budget 0 在 List 和队列组失败，其余 budget 0 及十组无故障对照通过。
每端总计 68 阶段、42 个失败，输入指纹保持不变。共享产品日志的共同诊断是
`worker-observed started=1 finally=1 hit=1 caught=1 native-exit=1`。
这是同一类错误边界问题在多个工作负载中的复现，不是 80 个独立根因。

故障注入覆盖从 worker 成功设置故障点起，到被调操作清理和 catch；不覆盖
线程启动、尾部、Future 原生工作线程或整个调度器。XRT 等待是逻辑完成，
不是 OS 线程 join。成功路径会等待真实 XRT 完成并释放保留引用，共享产品
保持加载；不能据此证明 OS 线程 join、运行库安全卸载或全运行时插桩。

## 已确认的错误边界

`src/api/xl_foreign_error.h` 已规定：`xl6ErrorScopePropagateFailure` 用于
void 原生回调／finalizer 的逃逸失败，不用于普通语言错误边。它会锁存立即
外层作用域的失败位，包括没有分配出诊断对象的失败；单纯 catch 和清空诊断
不会撤销这个状态。

`xl6ArrayProducerFinishV1` 的普通指针失败返回，以及 List／Set／Dict 等工厂
生成的普通失败分支，使用了这一回调传播操作。后续语言 CFG 可以捕获返回
失败，但 Thread 的外层执行作用域已经被标记失败。`xl6_thread.c` 会把这一
作用域结果与闭包返回值合并，所以出现“catch 已处理、线程仍失败”。
其他原生 helpers 和工厂 emitter 也有同类调用，需要逐一按物理返回合同审计。

后续修复应明确区分三种边界：

1. 普通 checked 返回由返回载体和当前诊断传递失败，语言 CFG 决定捕获或逃逸，
   不抢先锁存外层回调作用域。
2. void 原生回调／finalizer 无返回载体，真正逃逸失败必须继续锁存外层首错，
   包括 NULL 诊断；不能全局删除或静默化传播操作。
3. 清理事务继续执行所有步骤、保留原始首错，再由实际调用边界交付结果。

修复后需重新构建普通产品，跑完这十组完整前缀、错误作用域与析构回归、独立
AOT 和产品总门。不能让 Thread 忽略作用域失败，也不应在 Cgen 对特定库打补丁。

## 门禁与绑定覆盖

`check_stdlib_worker_fault_shipping.py` 隔离真实 CLI／DLL／SO，通过内嵌 VFS
构建 AOT；显式原生观察器仅编译匹配 SDK 声明。它保留全部独立失败，出错退出 1。
`DiscoveryComplete=true` 只表示已收集四十项，当前
`Complete=false`、`WorkerBodyPrefixAcceptance=false`。

跨组产品总门增加必需的 `worker-fault` 子门；只有它真实通过，
`WorkerBodyFaultPrefixAcceptance` 才可为 true。新语言夹具归为专用宿主，
旧六夹具的源码宿主证书不能将它标为通过。此前 B326 产品总门是其原有范围的
历史通过记录，不覆盖本次 worker 故障，也不回写为新门禁通过。

另用真实 Linux 编译库内部解析接口检查了 312 个既有候选：224 项登记产生的
228 个精确静态绑定都有源调用，所列托管类目录没有绑定缺项。此检查不执行
调用，也不包含所有原生泛型容器 intrinsic，因此不是全部函数／分支覆盖证明。

## 复测与证据

- Windows：`D:/xlang-validation/stdlib53-b327-worker3-win/results.json`。
- Linux：`/var/tmp/xlang-stdlib53-b327-worker3-linux/results.json`。
- 绑定目录：`/var/tmp/xlang-stdlib53-b327-discovery/results.json`。
- 输入、日志及提交选择复核：`D:/xlang-validation/stdlib53-b327-dev/`。

为 `check_stdlib_worker_fault_shipping.py` 提供匹配普通构建的 `--root`、
`--build-report` 和新的 `--out`，默认 `--jobs 2`。未修复产品应仍失败；
不把这些失败改为 expected、跳过或仅接受无故障对照。

CRT 未决策 0；XRT 19 组未决策及对象池最后的顺序不变。本轮不新增标准库设计。
