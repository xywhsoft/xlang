# Continuation 拥有调用：当前源码与真实队列验收

本批只刷新 P0 的 consuming-call 合同与测试，不修改语言语义、生产 IR、
Cgen、驻留 runtime 或 XRT，也不宣布 P0/P1 或五组任务完成。

## 为什么旧夹具不能直接作为当前证据

普通只读 string 形参已经允许编译器内部借用／省略副本。旧夹具却假定每个
调用必有 owned staging；这不是生产缺陷，不能为了测试恢复 byval 源语法或
关闭副本消除。现在复用既有的私有 owned-parameter 构造工具，显式建立本测试
真正要覆盖的 consuming physical ABI，再形成三个 SSA owner/view region。

替换旧 staging 后，夹具自身必须为新增的 checked 终止边提供明确释放。
扩展的 test-only sink 构造保留 END/THROW 邻接和原 error scope；默认入口
保留其它历史夹具的行为。最终 IR 仍交给未改动的完整验证器检查，非法形状
不允许进入 C 或 header 输出。这不是生产 ownership repair pass。

另一个旧假设是 async 内联执行：在 Promise 生产线程设置的线程局部分配预算，
不会覆盖固定线程执行器里的 continuation。现在由测试源程序的显式 phase hook
在真实 worker 上注入，分开覆盖三处 await setup 与最后 result box。
启动路径则遍历调用线程的每一个真实分配前缀，直到首个未触发预算，不根据
并发的全局 AllocCount 猜测启动分配数。

等待 successful Watch registration 只说明可以推进下一输入，不代表已经释放
帧；只有最终真实队列 idle 才检查物理收尾。没有在 pending reservation 仍存活时
等待全队列 idle。Watch observer 直接包含原生产 transport，并替代其唯一同名
编译单元，既不改算法，也不把执行器换成 inline/fake scheduler。

原生 extern string 观察函数依然接收显式 C 文本指针；语言启动参数和 generator
item 则使用实际双字段 string，读取后走语言 string drop。两种 ABI 不混淆。

## 验收内容

入口：`tools/check_xlir_continuation_owner_calls.py`，PowerShell 包装仍由
`03_core_function_abi.bat` 调用。可显式传入完整源码构建目录；不传时包装会建立
新的完整源码构建，不能退回旧的“部分源码＋shipping DLL”混合验收。
输出目录必须全新。冻结检查覆盖源码构建的输入、头、对象与实际 driver，
测试／共用助手、工具链、生成 C/header/JSON/binding、日志及产物摘要。

两平台均对当前实际源码对象执行 GCC strict C，测试与宿主分别编译 O0/O2；
复用的完整生产源码对象保持它们原有的构建选项，不冒充全产品 O2 重建。
每次合同生成拒绝 331 个无效 IR，并验证 C/header 零输出；重复生成四种产物
字节一致。每个正常运行产物覆盖：

- 8,500 次正常执行、7,200 次真实 pending 挂起。
- 1,920 次三边界 setup 分配失败、1,600 次结果分配失败。
- 60 次 generator 与 260 次 async 调用线程启动分配前缀失败。
- 640 次空 Future 来源、每次两处真实 Watch 注册拒绝。
- 成功／失败／取消／关闭、ready/pending、两个 owned 实参同次消耗、alias 分支、
  原始调用错误、generator 提前关闭与生产者错误隔离。
- 每例及模块最终退出后的精确 live count/bytes、alloc/free、非法释放账本。

每种优化另构建“删除生成的显式 SSA release 调用”的反例：必须成功编译并由
运行时内存账本断言拒绝，编译失败不算反例通过。85 例 smoke 也独立运行。

证据根目录：`F:/xlang-validation/five-groups-20261020/p0-core-contract-refresh/`。
最终报告为 `owner-win-r3/results.json`、`owner-linux-r2/results.json`；每端 16
阶段，含两项预期运行拒绝。旧诊断与输入变化轮次保留，不计为最终验收。

## 边界与后续

本批是 current-source/native-host 限定证明，不声称 shipping DLL/SO/VFS 或
新 JIT/AOT 已验收。没有覆盖 release、推送、网络或安全性质专项测试。

发现的其它旧基线保持开放：owner views 夹具仍假定旧 yield staging；TaskPool
graph 夹具仍假定旧默认调度／V2/V3 退休 ABI；monolithic source guard 仍有从
旧 API 文件直接搜索 adapter 的位置假设。本批仅同步 owner-call 的新入口
路由，不凭这些旧断言修改生产实现，也不将它们记为已通过。

Executor 的首错＋失败计数、wait／throwIfFailed checked 报告合同维持不变，
详见 [Executor 线程退出验收](Executor线程退出验收.md)。其余 P0/P1、网络 40、
非对称密钥／ASN.1、X.509、TLS 和最终双平台联合验收继续按原范围推进。
