# 拥有值与槽复制的工作线程错误隔离

本批修复 List、Set、Stack、Tree、SlotMap 剩余的普通失败传播路径。
Windows／Linux 新构建的运行库与编译库，每端 40 项真实语言 worker
完整分配故障前缀全部通过；此前已捕获 MemoryError 却退出 1 的测试阻塞清零。
这证明指定工作负载和 worker body 边界，不等于全部调度器或 53 模块发布完成。

## 统一返回与清理合同

拥有值工厂、List 克隆／读取、Set 拥有结果、原生值装箱和 Array 原生值策略，
按已有 NULL 合同交付拥有诊断。字符串槽 Copy／Clone 和 Tree 查询使用已有
显式状态返回。它们结束自己的事务后，不再污染调用者的外层回调作用域。

合法数据可能为零或 false 的标量，以及依赖 FOREIGN_SCOPE 的 payload-only
接口，仍保留独立失败通知。没有机械删除所有传播调用，也没有让 Thread
忽略执行或捕获清理失败；真正逃逸的 void 回调／finalizer 仍保留首错及 NULL
诊断。List 头文件同步说明字符串返回与标量返回的区别。

SlotMap 将 checked 返回与内部清理传播分开。未发布的工厂／快照结果在本次
事务结束前释放；成对快照统一回滚两个结果，包括源 pin 释放后才发现的失败。
读出失败后的清理明确继承已知失败，不能用后续错误替换无诊断首错。

本批只修改 10 个驻留 C 实现、两个合同头和两个测试输入，不修改 Cgen、IR
枚举、公开函数签名或 XRT 实现，不增加标准库 API 或兼容分支。两端从精确
选择的 Git 树重新构建 TCC、XRT、VFS、CLI 和共享产品；不纳入其他工作区修改。

## 双平台 worker 故障前缀

十组工作负载均在真实语言 thread 内设置一次性分配故障，不在调用线程代设。
Stack 额外加入 bytes 和 C int 的独立快照／取出，避免只验证 string。
每端分别链接运行库和编译库，各使用 O0／O2 原生宿主，结果一致：

| 工作负载 | 每个宿主连续触发的分配故障点 |
| --- | --- |
| 文本 | 34 |
| Regex | 94 |
| Array | 41 |
| List | 37 |
| Dict | 55 |
| Set | 106 |
| Stack | 89 |
| Tree | 48 |
| SlotMap | 26 |
| MPMC／WaitQueue | 34 |

每个前缀逐点验证 MemoryError、一次 finally、线程退出 0、调用线程隔离及
完整拥有者堆账本，再确认首个未触发点和下一点。两种严格生成 C 编译、
独立 AOT 构建及十组各 budget 0／无故障的 20 项对照也全部通过。
O0／O2 是原生宿主优化等级，不是 TCC JIT 优化模式。

原生边界控制每个宿主检查 400 次 producer NULL／非 NULL 诊断返回、200 次
真实字符串复制分配失败、1,400 次 checked 参数拒绝、200 次真实句柄回滚，
并检查干净／已失败调用者及真正逃逸的 NULL 回调。既有首错身份、无诊断
优先级和堆平衡测试继续通过；八项原生控制均纳入必需 worker 门。
修复前候选的四个宿主均被外层状态断言拒绝，不靠编译失败或超时作反例。

每端 worker 门 84 阶段、40 项完整前缀、8 项原生边界控制全部通过，
`WorkerBodyPrefixAcceptance=true`。验证范围是 worker 成功设置故障点后的
body／被调操作清理／catch；不覆盖启动、原生 Future 工作线程或整个调度器。
XRT 逻辑完成不等于 OS join；共享产品保持加载，不能外推为安全卸载证明。

## 整批产品与独立程序回归

产品总门 Windows 630 阶段／11 个子门，Linux 631 阶段／12 个子门全部
通过，包含必需 worker 门、204 模式容器图／续体、析构首错、线程、同步、
独立 AOT 图退役、终端就绪及完整资源账本；输入指纹保持不变。

每端 312 个候选保留 303 项 JIT／模块普通通过、1 项预期拒绝、7 项专用
宿主和 1 项平台条件。独立 AOT 保留 302 项通过、1 项预期拒绝、8 项专用
宿主和 1 项平台条件；通过的程序已迁移位置并检查独立依赖。
Windows 指定 IANA DST 与 Linux 旧 cmd 夹具仍明确为平台条件，未改成通过。
这些清单的 Complete=true 不等于全部函数／分支或 53 模块发布验收；
Full53Acceptance、调度器全局 OOM、OS join 和整个运行时插桩均未宣称通过。

## 证据与剩余验收

- 普通构建：`D:/xlang-validation/stdlib53-b329-sealed-build-win/build.shipping.json`、
  `/var/tmp/xlang-stdlib53-b329-sealed-build-linux/build.shipping.json`。
- Worker 门：`D:/xlang-validation/stdlib53-b329-worker-sealed-win/results.json`、
  `/var/tmp/xlang-stdlib53-b329-worker-sealed-linux/results.json`。
- 产品总门：`D:/xlang-validation/stdlib53-b329-products-win/results.json`、
  `/var/tmp/xlang-stdlib53-b329-products-linux/results.json`。
- 独立 AOT：`D:/xlang-validation/stdlib53-b329-aot-win/results.json`、
  `/var/tmp/xlang-stdlib53-b329-aot-linux/results.json`。
- 旧候选拒绝：`D:/xlang-validation/stdlib53-b329-old-control3-win/results.json`、
  `/var/tmp/xlang-stdlib53-b329-old-control3-linux/results.json`。
- 选择、指纹复核与提交范围：`D:/xlang-validation/stdlib53-b329-dev/`。

首轮 Linux 候选的四项 List 读取失败仍保留在
`/var/tmp/xlang-stdlib53-b329-worker-linux/results.json`，不回写为通过。
早期参数拒绝并不分配内存，不能要求它触发分配故障；原生反例已区分真实
分配失败与 checked 参数拒绝，不为该测试假设修改产品行为。

完整目标仍需核对 53 模块当前覆盖、其他原生工作线程故障及最终发布证据。
CRT 未决策 0、XRT 19 组未决策、signal 注册线程显式泵、WaitQueue MPMC
和对象池最后的安排不变。
