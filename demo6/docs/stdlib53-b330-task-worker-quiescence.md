# 任务线程阻塞终结与真实产品验收

模块的阻塞终结必须等待已接纳任务及其清理尾部完成，不能以非阻塞 BUSY
代替完成。本批修复 Cgen 忽略 task pool 终结 `wait` 参数的缺陷，并把完整
任务线程测试迁到实际 CLI、DLL／SO 产物；不改变标准库公开 API 或运行时 ABI。

## 阻塞终结与非阻塞退役

此前 `xlir_module_prepare_resources(teardown, wait)` 的 task pool 分支始终调用
`xl6TaskPoolTryQuiesceV1`。任务已交付结果或析构通知，不代表所有任务清理尾部
已经结束；非阻塞准备可能返回 BUSY。终结因此提前失败，任务池却已关闭接纳，
后续一轮执行可能无法提交任务，或在模块仍持有资源时检查内存账本。

生成代码现在按已声明的等待合同选择现有驻留接口：

- `wait=true`：调用 `xlir_async_pool_quiesce`，等待任务及清理尾部的逻辑完成；
  失败保留独立状态，不进入 IR 全局终结程序。
- `wait=false`：继续调用 `xl6TaskPoolTryQuiesceV1`，保留可重试的 BUSY；
  不增加循环重试、隐藏延时或新的运行时包装。

资源等待仍由驻留 task runtime 实现。Cgen 只选择已确定的生命周期合同，
不生成 task pool 算法，不改变所有权图、结果布局或异步调度策略。
池内自终结的现有 checked 拒绝仍由驻留接口负责，不等待自身。

测试同时加强终结观察：不仅要求诊断为空，还必须检查独立成功位和模块已回到
未初始化状态。无诊断失败不能算作成功；失败时输出任务启动上下文和内存差额，
不放宽完整账本断言。

## 完整任务线程测试的真实产品入口

`check_stdlib_worker_completion_shipping.py` 使用普通发布 CLI 生成 C、Header
和 lowered IR，两次生成必须逐字节一致。六个工作线程及五种结果 ABI、捕获
所有权、先退役后发布合同均从真实 IR 检查，绑定只引用该 IR 的实际符号。

原生宿主分别链接运行库和编译库，导入检查必须只包含选定的那一个产品。
不编译编译器源码对象，不拼接原生 provider，不给隔离编译器提供源码兜底。
SDK 声明来自同次构建的资源清单；所有输入、产物和执行日志都有指纹。

测试保留完整的成功／失败／取消、析构首错、调用者诊断隔离、重复模块生命周期、
丢弃 Future 和精确内嵌 NUL 字符串结果。分配入口观察器只截获生成 C 的分配
调用，在真正的工作线程上设置 XRT TLS 一次性故障；实际分配器、TLS、Future
发布和清理仍在未修改的共享产品内，不由宿主伪造 NULL 代替 OOM。

故障扫描从生成结果载荷的第一次分配开始，覆盖五种有载荷任务至观察回调前的
执行／清理／发布；不覆盖启动、void 任务的全部分配、观察回调后的尾部或整个
调度器。模块逻辑完成不是 OS 线程 join，更不是共享库可卸载证明；产品保持加载。

两种错误对照分别删除测试回调的失败通知、改变真实结果分配故障点。它们必须
在相应语义断言处被拒绝，编译失败或超时不算有效对照。原有源码驱动的 IR
变异门仍独立保留，不能被新的运行门冒充或静默替代。

产品总门现在必需执行完整任务线程门和 Native Future 精确载荷策略门。
单个门通过不能外推为全部 53 模块、所有函数／分支或最终发布完成。

## 双平台专项结果

Windows／Linux 均从精确选择的 Git 树重新构建普通 TCC、XRT、VFS、CLI、
运行库与编译库，没有纳入其他工作区修改。两种共享产品各用 O0／O2 原生宿主
执行完整人口；优化等级指原生宿主，不是 TCC JIT 模式。

八个宿主配置的结果一致，每个配置通过：

- 3,978 次完整执行、90 次真实结果分配失败、972 次丢弃 Future。
- 五条完整分配故障前缀，共 35 次执行、30 次真实工作线程 TLS 故障；每条
  连续扫描到第一个未触发点，逐次核对结果、输入析构、发布、观察者和堆账本。
- 删除回调失败通知、改变真实结果分配故障点的两项已编译对照均被语义断言拒绝。

每端完整线程门 24 阶段、四项完整人口证明和四项拒绝对照全部通过。
Native Future 精确策略门另在每端四个配置验证八次平衡生命周期、缺策略拒绝
及冻结期无分配。Linux 实际编译器 SO 还执行原有 IR 专项，64 项变异必须在
C／Header 输出前全部被拒绝；没有用源码编译器对象替代该 SO。

修复前的实际产物在双平台共八个宿主中，均被“终结成功且模块状态归零”断言
拒绝，不依赖超时或编译错误。旧报告保留失败状态，不回写成通过。

两端独立 AOT 的 312 候选均完成：302 项通过、1 项预期拒绝、8 项专用宿主、
1 项平台条件；已运行程序经过位置迁移与独立依赖检查。

产品总门 Windows 632 阶段／13 个子门、Linux 633 阶段／14 个子门全部通过，
包含 204 模式拥有图／续体、原有 40 项 worker body 故障前缀、析构首错、线程、
同步、独立 AOT 图退役及终端就绪／资源账本。每端 312 个候选保留 303 项
JIT／模块通过、1 项预期拒绝、7 项专用宿主和 1 项平台条件，输入指纹不变。
没有把专用宿主或平台条件改记为普通通过。

## 可复核证据与剩余范围

- 普通构建：`D:/xlang-validation/stdlib53-b330-build-win/build.shipping.json`、
  `/var/tmp/xlang-stdlib53-b330-build-linux/build.shipping.json`。
- 完整任务线程门：
  `D:/xlang-validation/stdlib53-b330-products-win/gate-worker-completion/results.json`、
  `/var/tmp/xlang-stdlib53-b330-products-linux/gate-worker-completion/results.json`。
- 产品总门：`D:/xlang-validation/stdlib53-b330-products-win/results.json`、
  `/var/tmp/xlang-stdlib53-b330-products-linux/results.json`。
- 独立 AOT：`D:/xlang-validation/stdlib53-b330-aot-win/results.json`、
  `/var/tmp/xlang-stdlib53-b330-aot-linux/results.json`。
- 旧产物拒绝：`D:/xlang-validation/stdlib53-b330-old-control-win/results.json`、
  `/var/tmp/xlang-stdlib53-b330-old-control-linux/results.json`。
- 实际 SO 的 IR 专项：`/var/tmp/xlang-stdlib53-b330-ir-contract-linux/results.json`。
- 选择树、逐项指纹复核和提交范围：`D:/xlang-validation/stdlib53-b330-dev/`。

后续仍需收敛 53 模块覆盖表与最终发布条件。Windows 指定 IANA DST、Linux
旧 cmd 夹具继续保持平台条件；没有修改系统时区或把 GUI 关联启动算作成功。
CRT 未决策 0、XRT 19 组未决策及对象池最后的顺序不变。
