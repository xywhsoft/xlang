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
OOM 已认证。Windows GCC 与 Linux GCC/TCC 的 O0/O2 已通过；Windows TCC 的测试
执行被 Defender 的 WinError 225 拦截，仍待验收，没有修改防护设置或绕过拦截。

DEMO6 的维护入口为 `tools/check_stdlib_executor_native.py --out <新证据目录>`，
支持 `--compiler`；实际 XRT／xruntime 两种单头和 O0/O2 都须通过七个 API、五个物理
节点、六条边及精确清理账本检查。另以同一上游完整夹具和仅测试侧时间单位适配器
检查两个实际快照。全部为普通离线 ABI／生命周期测试，没有网络或安全性质测试。

下一步仍须把明确准入的 Data policy 接入常驻 runtime domain/catalog，建立高层执行
资源及 callback/code-lease 合同、语言类和方法、可达性裁剪与 JIT/AOT 验收。不能仅因
这七个原生符号已可链接，就把公开执行器或整个长期任务改成完成。
