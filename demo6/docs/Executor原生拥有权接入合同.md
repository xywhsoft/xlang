# Executor 原生拥有权接入合同

这是五组长期任务 P1 的原生前置，不代表高层 `Executor`／`TaskPool` 标准库已经接入。
默认 async 固定工作线程、就绪队列与显式阻塞池的既有分工不变；本批没有 Cgen 实现。
原生实现对应 XRT 独立提交 `02e19fb137d1087cb4adb45cf52c6346d5de75a5`。

## 原生能力

新增 `xrtExecutorSubmitOwnedV1`、`xrtExecutorSubmitOwnedBatchV1`、
`xrtExecutorOwnership`、`xrtExecutorOwnershipAdapterV1`、
`xrtExecutorJobOwnershipAdapterV1`、`xrtExecutorOwnershipQuiescenceV1Get`、
`xrtExecutorJobOwnershipQuiescenceV1Get`。提交仍使用预分配槽位，不为每项申请通用堆。
每项受理时接管一个真实 Data 引用，固定的常驻合同声明 Proc、Drop 与物理 Data Ops；
整批失败不消费任何 Data。旧裸指针提交不自动获得拥有图认证。

队列到槽位、槽位到 Data、槽位到包含分配的引用都是真实边。执行体、取消摘链、
析构和原生入口尾声没有完成时拒绝静止证明；独立 outstanding 额度贯穿这些阶段，
不把瞬间的空队列或单个统计字段当作完成。Worker 与取消析构回调不能等待或销毁自身。

Prepare 关闭准入，让既有工作自然排空，通知 Worker 退出，在 freeze／mutation 外用
零超时 join 检查实际完成，不通过取消、代执行或跳过清理制造 READY。Clear 要求真实
join，Finish 退役线程资源。被 pin 的已完成槽位延迟复用，并保留包含它的分配；最后
实际引用释放后才销毁嵌入锁和分配。不能通过复制槽位或估算引用数替代此合同。

## 快照范围与验收

只同步 Executor 公共头、新内部头与两个实现段；未选中的生成段保持原样。上游当前
使用毫秒接口，DEMO6 固定快照仍保留微秒／deadline ABI；本次不升级其他 wait、内存或
文件系统内容，也不能直接覆盖为上游的完整单头。核心及 xruntime 同时更新声明、符号
表与快照元数据，低层 API 数变为 3229／3693，自有 xruntime API 仍为 464。

上游 `executor_ownership_tests` 检查模块化与单头、真实节点和边、错误合同拒绝、
并行回调、取消清理屏障、pin 阻止 ABA 复用、两种销毁顺序及调用线程的创建／快照
分配失败前缀，核对真实存活数量、字节和分配／释放账本。调用线程注入不等于 Worker
OOM 已认证。Windows 与 Linux 的 GCC/TCC 模块化及单头 O0/O2 均已通过。
Windows TCC 初次执行被 Defender 的 WinError 225 拦截；用户恢复并允许原路径
测试文件后，先运行恢复的原产物，再完成缺失的 TCC 两个优化级别。未修改防护
设置或绕过拦截。旧失败报告保留；此前没有隔离前的产物哈希，不能声称做过该比对。

DEMO6 的维护入口为 `tools/check_stdlib_executor_native.py --out <新证据目录>`，
支持 `--compiler`；实际 XRT／xruntime 两种单头和 O0/O2 都须通过七个 API、五个物理
节点、六条边及精确清理账本检查。另以同一上游完整夹具和仅测试侧时间单位适配器
检查两个实际快照。全部为普通离线 ABI／生命周期测试，没有网络或安全性质测试。

本次恢复验收在 `F:/xlang-validation/five-groups-20261019/p1-public-executor/`
的 `native-win-tcc-r3/results.json`（原产物及八个测试进程）和
`maintained-win-tcc-r2/results.json`（实际两种快照、四次构建及四次运行）。
两份报告均为 Complete，输入及日志哈希保持一致；不重跑已通过的 GCC/Linux 通道。

## 常驻 catalog 准入增量

`xl6OwnershipCatalogAddExecutorV1` 使用独立的 `xexecutordataownershipv1` 指针组，
不转换 TaskPool 的 descriptor 或函数指针；新增视图追加在现有 policy 组之后，
不移动原有组的字段。只有常驻 producer 的 Load 可登记，发布后的 catalog 不可追加。

`xl6OwnershipDomainNativeExecutorV1Get` 独立登记 shell／slot 的两个静止能力，
先拒绝运行与析构尾部，再按原生 Ops 和已登记 policy 的精确身份解析。内容相同但
地址不同的 policy 不被自动认可，旧 opaque 作业不被视为 leaf，不猜测 Data 的布局。
此域目前只作为显式部署根；默认 async 的 `xl6OwnershipDomainExecutorV1Get` 不变，
不会因语言已有 async 而拉入 detached Executor 的原生实现。未来受管理 producer
接入时须显式关联此域，而非改变默认执行服务。

维护入口 `tools/check_stdlib_executor_catalog.py --out <新目录> --compiler <编译器>`
支持多次指定编译器；ELF 环境可加 `--plan`。Windows 16、Linux 20 个阶段通过，
覆盖两种实际快照、GCC/TCC 和 O0/O2。每次运行检查 64 个排队图、32 个运行／析构
BUSY 阶段、错误类型及 policy 身份拒绝、封闭登记与 16 个分配预算，核对精确内存账本。
两个 ELF 源计划证明显式域要求原生 Executor、但不引入默认执行器或 TaskPool；
默认 async 的计划也不引入原生 Executor。
Windows 定点检查不把 PE 下无法独立裁除整个 compile_unit 的试验链接当作产品失败，
也不冒充完整 Windows 产品裁剪验收。报告在 `p1-executor-catalog/win-r2` 和
`p1-executor-catalog/linux-r2`，输入及日志哈希均未改变，没有网络／安全性质测试。

下一步仍须建立高层执行资源及 callback/code-lease 合同、语言类和方法、可达性裁剪
与真实 DLL/JIT/AOT 验收。此次只验证所选原生与常驻源码，未重建普通产品；不能仅因
原生符号和 catalog 能工作，就把公开执行器或整个长期任务改成完成。

## 已批准的语言回调错误策略

2026-10-19 用户确认：脱离式 Executor 的语言回调抛错时，保留首个错误和失败
计数，由 `wait()`／`throwIfFailed()` 显式 checked 报告；工作线程不因用户抛错
abort，也不静默丢弃错误。原生 Executor 仍是不返回 Future 的脱离式执行器，
不能冒充 TaskPool 的 Future 提交接口。

这不等于当前已经存在这两个语言方法。下文的常驻 producer 已实现首错存储、并发
发布、代码租约和回调释放；公开类、最后一个语言引用的退休与语言验收仍待接入。

## 显式 TaskPool 的常驻控制前置

池控制从 `xl6_task.c` 拆到 `xl6_task_pool.c`。前者仅负责语言任务帧、捕获与
结果转发；后者独立拥有不透明池布局、入口／activity 引用、提交信用、原生池和
资源登记。跨单元只使用私有 `xl6_task_pool_internal.h`，不共享布局或 include C
文件。现有 `XL6_TaskRuntimeV1` 不扩字段，也不重新解释旧函数指针。

新增独立 `XL6_TaskPoolControlV1` 描述符：Create、Start、Close、Cancel、
WaitFor、WaitUntil、Snapshot、Quiesce、TryQuiesce、Destroy。

- Create 只建 wrapper；Start 显式初始化。读取、关闭、等待空池不偷偷创建工作线程。
- Close 停止新的 wrapper 提交；此前取得的信用必须完成结算。它不销毁池，也不
  保证尚未进入原生 Submit 的旧信用一定被原生池接受。
- Cancel 同时关闭提交并锁存取消。延迟创建在锁外发生；创建者必须在发布原生
  指针前应用取消锁存，不能让另一条旧信用先拿到未取消的池。
- 等待必须先 Close／Cancel，包含提交信用及原生析构尾部。同池工作线程、同步
  取消析构和仍持有原生提交 scope 的调用者不能等待自身。
- 每个控制调用持有真实 wrapper 入口与控制信用，阻止原生池在调用期间被销毁。
  Quiesce 先关闭、等提交结算和登记资源关闭，再等控制信用及原生尾部，最后 join／
  destroy；TryQuiesce 对忙资源返回 BUSY，不在冻结或 wrapper 锁内等待。
- WaitFor 使用毫秒：-1 无限、0 轮询、正值有限；拒绝负值及溢出，不能把有限值
  饱和成无限哨兵。WaitUntil 是原生微秒时钟的绝对有限期限，拒绝 UINT64_MAX。
- bool 单独报告操作失败；成功时另行输出 READY／TIMEOUT／CANCELLED，不能用
  是否成功分配诊断对象判断操作成功。等待输出在失败前也初始化为 TIMEOUT。
- Snapshot 要求精确描述符大小；计数来自一次原生 Get，不从 Future 状态推算。
  显式成功 Quiesce 后保留最终统计。wrapper admission 标志与原生计数不承诺跨
  提供者的同一原子快照；没有原生实例时 initialized=false，不虚构配置已生效。

`TASK_POOL` 和 `OWNERSHIP_DOMAIN_TASK_POOL` 追加在部署 ID 表尾。控制根明确要求
TASK_POOL 与 ATOMIC；XRT TASK_POOL 自身不隐含公开 Atomic API 实现。独立拥有域
登记 wrapper／原生池的静止能力和适配器；旧 TASK 根依赖它，仍取得任务与池两个域。
仅控制池不引入语言帧、Capture、默认 async Executor 或脱离式原生 Executor。
动态导入的完整宿主继续消费同一部署表；VFS 的已有 runtime 头目录规则自然包含
新私有头，不新增第二份手工源码包。

维护入口为 `tools/check_stdlib_task_pool_control.py --out <新目录> --compiler <cc>`，
可重复指定编译器；Linux 可加 `--sanitize`。它分别检查：

- 真正部署／拥有计划产生的源码和 selector 独立编译，不加 ALL 或测试 feature
  掩盖缺失依赖；以及所选符号的准确归属和控制／帧分离。
- 两个实际单头、GCC/TCC、O0/O2 的取消／创建竞态、真实信用／控制退休交接、
  Run／Drop 屏障、32 个调用线程分配预算和精确内存账本。
- 真正 TASK 计划闭包和当前完整原生实现下，既有 280 次生命周期、240 个精确
  排队拥有图、520 次准备；白盒 outcome 的 400 个通知／释放／登记失败回调。
- Linux 所选控制闭包的 ASan／UBSan／泄漏检查。调用线程分配注入不代表 Worker
  OOM 已验收；全原生内核仅用于既有帧回归，没有执行网络或安全性质测试。

Windows 源计划测试关闭 unwind 表，并仅在该独立测试关闭 switch jump table：
MinGW 的共享 `.rdata` 中跳转表会反向保留无关的 IR planner，阻止独立链接；仍保留
O2，不改生产构建、不补假符号或替代实现。该检查不是完整产品的裁剪验收。

本批仅为常驻控制和原生生命周期前置；未新增公开语言 Executor／TaskPool 类，
未重建发布 DLL，也没有据此认定普通 DLL/JIT/AOT 或整个长期任务完成。

本批最终证据为 `F:/xlang-validation/five-groups-20261019/p1-task-pool-control/`
中的 `win-r5/results.json` 和 `linux-r5/results.json`：Windows 59 阶段／31 产物、
Linux 61 阶段／32 产物，输入与日志封印一致、无失败。Linux 原始执行目录为
`/var/tmp/xlang-task-pool-control-r5`，归档不修改报告中的原执行路径。
四种普通源码清单门各包含 559 个必需 provider，无缺失或重复，五个负控制均拒绝。
旧脚本的变更仅补拆分依赖；49 个相关 PowerShell 入口通过语法解析，两个受影响的
Shell 入口及 Linux 构建脚本通过 bash 语法检查，不表示已经执行所有这些测试。

## 常驻 detached 回调 producer 与错误账本

`xl6_executor_jobs.c` 复用 XRT 的 SubmitOwned／原子 SubmitOwnedBatch，不增加
第二套队列、工作线程、Future 或默认 async 回退。入口接收编译器验证过的
零形参 void 语言闭包，对应精确 C ABI `bool invoke(void* environment)`；这里
不能从 erased C 函数指针推断签名，未来公开语言接入仍须提供 XLIR 签名合同。

每个被受理作业拥有一个独立的真实 Data record，分别持有闭包与失败账本；闭包
持有代码租约和捕获。准备失败／原生拒绝仍由调用者拥有输入；逆序撤销已准备记录，
不运行 body，不当成取消或作业失败。批量受理保持原生的整组成功或整组失败，
不会把一个批次改成多次单项提交。单项提交不创建临时批量数组。

body 使用独立 checked 作用域调用。其结果保留到 Drop，先从元数据中拆出拥有槽，
再在锁和 mutation 外释放捕获；body 与捕获析构的失败合并为一个作业失败。直到
Drop 完成才登记账本，原生 Outstanding 继续覆盖这个尾部；仅返回 body 不算完成。
取消的排队作业不运行 body；正常清理不计失败，清理自身抛错则计一次。
没有改变 ThreadLocal 为“每作业清空”，也没有偷偷执行线程退出 hook。

账本锁内只做引用／计数发布，锁外释放错误；首错使用原始 Error 的实际引用，
不重新包装或复制消息。first 按完整作业清理后的登记顺序，而非提交顺序；并行
作业自然可能以不同顺序完成。首错可以没有诊断对象，这时仍返回 false 并计数，
后续已分配的错误不能替换它。uint64 计数溢出采用饱和且有独立 saturated 标志。
Snapshot 和 ThrowIfFailed 不清空历史；每次显式报告取得独立的错误引用。

`xl6ExecutorWaitV1` 先验证有限毫秒期限，原生关闭／等待合同不变。READY 描述
物理完成，即使账本错误导致 bool=false；TIMEOUT 不等于成功完成，不提前报告
尚未结束的 body／析构。普通 checked bool 调用把诊断交给语言错误边，不擅自污染
外层异常作用域。void 清理回调才使用显式失败传播。

共享的 `closure_release` 原来把 resident 环境 Drop 的 false 强制变成 StateError。
现在将原始 bool 失败（包括 NULL 诊断）传播到直接外层 checked 作用域，不伪造
一个错误来代替它；Closure 部署显式依赖 FOREIGN_ERROR，不靠其他组件偶然带入。
XRT 的 SetErrorKind 本来是静态、无分配诊断；不把它误当作分配型 fallback。

producer domain 精确登记 `xexecutordataownershipv1` 和 Data BUSY 能力，分别解析
账本与作业节点。运行和 Drop 尾部拒绝读取；排队记录可读，Prepare 不取消原作业，
只有完整清理后才 READY。检查 pin 使真正 Data allocation 在原生 Executor 已销毁
后仍存活；释放 pin 才回收，不制造假的 RC 节点。相同内容的 policy 副本不能代替
原 policy 身份。默认 async 不隐含拉入此 producer；它也不依赖 TaskPool。

维护入口：`tools/check_stdlib_executor_jobs.py --out <新目录> --compiler <cc>`，
可重复指定编译器；Linux 可加 `--sanitize`。原生实现和全部 provider 来自真正
部署／拥有计划，不使用 release DLL 或 ALL kernel 替代最小闭包。专项检查：

- GCC/TCC、O0/O2 的最小实际链接及 body／析构、首错、原子批量回滚、取消、
  期限、实际代码租约、读取方提前释放和精确分配账本。
- 每次运行保留 64 个 checked body／Drop／租约用例、8 个真实 pin 图、264 个
  多工作线程作业、24 个取消捕获、8 轮拒绝／放弃读取方及 8 个调用线程分配预算。
- 恢复旧 cleanup false 丢失、用 error 指针代替 first-failure bit、把未执行的取消
  误计为失败，三种实际错误实现均须编译成功并被断言拒绝。
- Linux 对所选原生实现、provider 和夹具整体运行 ASan／UBSan／泄漏检查。

`p1-executor-jobs/win-r1` 为 29 阶段／17 产物，`linux-r1` 为 31 阶段／18 产物；
三个非零退出都是有标记的预期负控制。四种 shipping 源码清单各需 561 provider。
输入／日志封印一致，没有网络或安全性质专项。个别清理用例固定注入一次 worker
分配失败，用于检查无诊断失败；这不代表完整 worker 分配失败前缀已经认证。
共享闭包改动另以 `legacy-win-r1`／`legacy-linux-r1` 各 33 阶段回归：GCC
O0/O2、两种实际单头、既有 280 次生命周期／240 个排队拥有图／520 次准备及
400 个 outcome 回调仍通过。它们是原生／常驻源码回归，不是公开执行器的语言验收。

该层借用 native Executor，其创建者仍须保证原生实例活到全部受理作业结束；没有
宣称解决在自身回调中释放最后一个语言拥有者的 join 问题。后续公开类必须组合
真正受管理的资源退休、配置／统计、批量公开面、错误账本和普通 DLL/JIT/AOT，
不能把这套 C producer 直接包装成无清理责任的 pointer 类。
