# TaskPool 受管理线程退出与 checked 清理

本批接通已有原生拥有式 worker-exit 底座，复用相同共享执行失败账本，未新增
第二套线程调度器，也没有修改 Cgen 的算法职责。原长期任务 P0/P1 及网络、
非对称密钥、X.509、TLS 和最终联合验收仍须继续。

## 公开合同

- 语言 TaskPool 成功 wait 包含真实 worker join 和 ThreadLocal 清理。任务之间
  保留 TLS，不按作业重置；无法用任务计数清零冒充线程已经退出。
- body／捕获清理失败由任务 Future 交付；线程退出清理失败另计首错、失败数及
  饱和标志，由 wait／throwIfFailed checked 报告。第一次失败没有诊断时仍是
  失败，不能让后一个带诊断的失败覆盖它。计数单位是失败的 worker 退出清理
  事务，不是该事务中每个 ThreadLocal 析构的数量。
- 明确观察不会清空历史；后续失败重新触发未报告通知。未观察的退出错误必须
  在拥有图／宿主清理边界报告，不能静默丢掉，也不在工作线程 abort。
- wait 的期限／取消覆盖退出等待。完成优先；超时或取消不撤回既有退出操作。
  模块退役继续使用非阻塞 TryJoin，不在单个控制线程内无限等待用户析构。
- 原始 XRT／非拥有 wrapper 的 Wait 继续为 drain；不修改它们已有合同。

## 拥有权与裁剪

延迟创建前 wrapper 持有一个真实退出上下文引用；原生创建成功后一次性移交。
失败前缀不消费该引用。图 trace 精确列出资源拥有者和 wrapper／native 之间的
实际边，没有虚构第三份引用。退出 capability 必须是 catalog 已注册的精确
永久地址；复制描述符不能取得权限，未注册 policy 在调用回调前拒绝。

`xl6_execution_failures` 是独立叶组件，`xl6_execution_worker_exit` 是共用真实
线程清理，`xl6_task_pool_exit` 只安装拥有式上下文。TaskPool 最小部署不拉入
Executor、Closure 或默认 async 执行器；Executor 不反向拉入 TaskPool。
所有适配驻留 runtime；生成端仍只声明、调用和处理已有拥有清理。

任务与捕获尾部排空后先按已有注册协议关闭异步资源、归还原生执行信用，再
请求退出。不能靠忽略信用解决“TLS 拥有资源、资源阻止退出”的依赖环。

## 共用拥有图准备修复

实际独立 AOT 的 self-release 揭示：已开始异步 Prepare 的节点会暂时拒绝
Count/Trace；如果每次必须完整重读后才允许继续原 Prepare，就永远无法 join。

修复仍先尝试完整新图证明。仅遇 BUSY 时允许已 pin、已 claim、此前已授权并
开始的 Prepare 继续；BUSY 不能授权新节点或新准备。冻结下只允许该精确能力
的独立只读 Ready 查询；Prepare 在冻结外执行。READY 后必须刷新完整图，才
能进行下一项语义职责或提交。已经完成的通知不因之后的快照 OOM 重复执行。
后来增加的外部根／未知子节点仍阻止提交，不对 Count/Trace 开后门。

## 当前证据入口

`F:/xlang-validation/five-groups-20261019/p1-task-pool-managed-exit/`

- `source-win-r3`／`source-linux-r3`：当前完整源码编译器，624／623 编译单元。
- `language-*-r3`：严格生成 C、JIT、module、独立 AOT，真实 ThreadLocal
  持续／退出错误、期限／取消、未观察错误、自释放及既有 Executor 回归。
- `resource-*-r3`：真实原生图、两类共享错误账本、16 个未启动拥有图、32 个
  延迟启动分配预算、48 轮真实线程退出及 144 个持续作业、16 个真实异步文件
  在 join 期间仍持有语言引用；13 种坏实现须先编译成功再由运行断言拒绝。
- `jobs-*-r3`：detached Executor 共享账本回归与精确 selected providers。
- `control-*-r3`：两套固定头、原始 drain、实际非阻塞 ThreadKey 尾部、旧图与
  结果运输。旧 drain-before-destroy 的坏判断必须被运行断言拒绝。
- `ownership-*-r3`：共用准备／完成／结果图／代码租约回归；另有 96 个真实
  循环验证异步准备续行、后来外部根和未知子节点，两种坏准备实现运行拒绝。

所有测试都使用离线进程／内存／自己创建的临时文件，无网络或安全专项。
冻结报告须 Complete、输入未变、日志未变且无失败；提交前复核实际输入、日志
与产物。失败轮次保留，不替换为新轮次的成功结果。

最终上述十份报告共 667 个编排阶段，通过的预期拒绝为 60 项；两端语言各
116 阶段，XLIR 各保留 47 个 C/header 输出前错误合同拒绝。两端 native 均有
GCC/TCC 和 O0/O2；Linux 的资源、Executor jobs、控制与拥有图选定实际闭包
另通过 AddressSanitizer／UndefinedBehaviorSanitizer 及泄漏检查。全部正式
报告 Complete／InputsUnchanged 为 true，日志和实际产物复核一致、无失败。

这是当前源码的受控 JIT/module/AOT 与原生专项，复用真正的 TCC core；不是新
shipping DLL/SO/VFS 或完整五组最终验收，不提升 release。
