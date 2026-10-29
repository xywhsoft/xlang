# 脱离式回调与 Engine 生命周期底座

本批是受管理 Engine 的前置能力，不代表 `NetEngine` 的新公开面已经完成。
尚需接通配置对象、模块执行源、生命周期方法和精确 IR／拥有合同；旧的
未解析包装声明不能据此视为已可用。

## 统一回调节点

`xl6_detached_work` 从既有 Executor 驻留实现中提取，不另写调度算法。
它保存精确 nullary void closure／已认证 boxed callable、代码租约、捕获和
共享失败账本；与 Executor、TaskPool、Engine 的算法和控制线程分别裁剪。
Executor 保留原有类型化提交／批量提交 API，以原生 data policy 使用该节点。

准备成功仍由提交方拥有；仅原生接受后转移。拒绝使用 Abort 撤销准备，
不把拒绝或正常取消算成执行失败。Invoke 与 Drop 分开，节点一直保持 BUSY
到捕获清理及错误登记完成。每个已接受节点将函数体／清理结果合并记录一次。

脱离式执行不返回 Future。失败账本保留“首个失败事务”的错误和累计失败
次数，首个错误也允许是无诊断的失败，不能让后续错误替换。显式
`wait()`／`throwIfFailed()` 使用 checked 结果报告；观察不清空历史。
未观察的失败在退役边界继续报告。工作线程不因普通语言异常 abort，
也不静默丢弃异常；内部损坏的不变量仍独立处理。

通用节点独立登记拥有图 adapter／preparation／quiescence。图中的边是
实际拥有的 closure／boxed callable、账本及错误，不制造引用或减去真实 RC。
仍保留 Executor 既有驻留入口，不引入语言层兼容分支或 Cgen 包装函数体。

## XRT 的 Engine 前置接口

增补带拥有上下文的 worker-exit policy、V2 精确拥有图准入、非阻塞且可重试
的 TryStop，以及 requested config 和 actual worker backend 的独立查询。
退出上下文只在创建成功时转移一次，Stop／restart 不释放；实际 worker exit
与 TLS 清理返回、原生 join 和运行资源退役完成后，才允许最终释放上下文。
回调 Drop 不持生命周期锁或拥有图 mutation，退役保护仍有效。

同步只导入 net.h、内部 Engine 声明和 engine.c 的精确差量。固定快照的
微秒／绝对 deadline ABI 保留，不随上游毫秒包装接口整体升级。尾部未改变
的上游兼容包装只用于对齐比较基线；普通同步器的旧值匹配保护没有放宽。
公开 SDK、两份产品快照、符号表与能力矩阵保持一致。

## 验证范围

原生旧／新 Engine 生命周期，以及固定快照消费端，覆盖 Windows/Linux、
GCC/TCC、O0/O2。普通验证包括退出尾部阻塞、重启、真实单上下文图边、
错误策略拒绝、停止中的图准备、配置复制、实际后端、分配失败和内存平衡。

共享回调验收覆盖原有 64 个 checked body/drop 与代码租约结果、8 个固定图、
264 个并行作业、24 个取消捕获、拒绝／放弃读取及分配失败前缀；另有六项
受控错误实现必须被测试拒绝。Linux 额外进行 ASan/UBSan 与泄漏检查。
依赖计划证明单独使用共享节点不引入 Executor、TaskPool、Engine 或默认
async 执行器，普通 shipping 源码列表守卫检查两端产品接线。

冻结后的完整源码驱动为 Windows 638／Linux 637 编译单元；两端各通过
115 阶段公开 Executor 验收（20 个预期 checked／语义拒绝）、31 阶段后端
回归（1 个精确类型拒绝）。包含真实 JIT、module、严格 C、独立 AOT、
32 项 Executor／批量合同拒绝与后端每种优化 46 项无效合同拒绝。每端
16 个 AOT 产物的动态导入确认不依赖 xl.dll/xlc.dll/libxl.so/libxlc.so。

正式证据位于 `F:\xlang-validation\five-groups-20261020\p2-managed-engine`：
`source-*-r2`、`jobs-*-r2`、`executor-*-r1`、`backends-*-r1`、
`regress-*-r1`。输入、日志和产物摘要在提交前重新核对；早期输入变化的
源码构建保留作诊断，不计正式证明。构建响应模板仅提交两个新增单元行，
其它工作区整理及用户的 IR 空行不纳入此批。

这些是当前源码限定证明，不是已发布 DLL/SO/VFS 证明。本批无外部 DNS、
socket 连接、探测或网络安全专项；长期任务 P0/P1 与 P2—P7 尚未全部完成。
