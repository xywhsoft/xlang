# IR 运行操作所有权与常驻线程验收

运行操作的输入所有权现在由真实编译后的枚举与生产函数验证，不再
用源码正则推断。线程验收也改为验证常驻原生接口与当前源码运行，
不要求旧线程算法回到生成 C。双平台各通过 78 个公共阶段及 140 个
线程语言阶段。本批没有新增标准库公开函数，没有修改 XRT 或运行时
实现；53 模块和发布总验收仍未完成。

## 编译后的所有权目录

旧静态门把 `XL6_IR_RUNTIME_COUNT` 计数哨兵当成运行操作，同时忽略
X-macro 展开的 SlotMap、树查询和 Channel 接收等操作，不能证明
所有权目录完整。新 `xlir_runtime_operand_catalog.c` 使用真正的 C
枚举和 `xl6IrRuntimeOperandOwnership`，覆盖 437 个操作的 8 个参数
位置，合计 3496 项 effect 检查，其中 49 项为 consume。NONE、COUNT、
负数和越界操作共 40 项检查均返回 invalid。

测试单独列出已有 consume 参数位置；生产函数漏掉一个枚举、错把
observe 改为 consume 或接纳未知操作，都会失败。参数 arity 仍由
IR verifier 检查；目录中的旧操作具有输入 effect 不代表它仍允许
进入语言或可执行 IR。该目录在 Windows/Linux 的 O0/O2 下均通过。

源码驱动只在实际编译输入与当前选择性源码逐项相同、SDK／对象／
驱动哈希未变时复用。比较包含所有编译单元，包括 XRT 单头实现；
本批复用了此前 303／302 单元的构建，不将它描述为本批重新构建。

## 线程验收与 Cgen 隔离

线程资源门现在委托跨平台公共验收与现有完整语言门。保留精确的
typed/lowered 操作数量、borrow 参数、拥有构造结果、失败合同、
TypeValueId、可调用签名、捕获持有和条件表达式合并检查。补充后的
合并检查验证两个 retain 分支与一个 move，而非仅检查最终返回值。

六个真实源码覆盖普通线程、默认空线程、可捕获线程、string/bytes/
char 捕获、线程容器和无 provider 控制项。每项均检查源码 JIT、模块
重定位、typed/lowered IR、双次确定性 C、O0/O2 严格 C、独立 EXE
构建运行和导入表。独立 EXE 不依赖 xlang 运行库；源码组件计划在
需要线程时只选一个常驻 provider，不需要时不选它。

原线程语言门的 140 阶段也重新通过，包含真正 JIT 生命周期、O0/O2
全运行时宿主、错误／故障夹具、容器回归、负例和实际 IR 损坏检查。
全运行时宿主不能替代按需 AOT 证据；它们不证明 OS-thread join、
image dlclose 或 shipping SDK 已完成。

旧 Cgen 隔离门将合法的 `xl6Sem*V1` 信号量接口误认为语义分析调用。
现在仅允许七个已存在的常驻信号量 ABI 名称，其余 `xl6Sem` 符号仍
拒绝。直接执行生产判断的专项确认 8 个合法调用接受、6 个 AST/HIR/
语义／旧后端形态拒绝，没有放开语义分析或未知旧 Cgen 名称。

## 验收入口与剩余清单

`check_xlir_runtime_operand_catalog.py` 是公共入口，PowerShell 包装器
支持传入已核对的源码构建。统一 IR 总门执行编译后的所有权目录；
原线程门执行完整公共／语言门。相对输出目录明确按 Root 解析，
不会因 PowerShell 的位置与进程 cwd 不同而写入错误目录。

证据为 `E:/xlang-validation/stdlib53-b224-check-win-dev2` 和
`/var/tmp/xlang-stdlib53-b224-check-linux-dev2`，其内部保留独立的
`thread-language/thread.language.json`。最终源码、实际输入、SDK、
日志和产物在提交前再次核对。

静态总门的完整诊断已一次性收集，仍有 56 条失败，涉及动态字段、
持久化、容器、字符串／索引、类值身份、同步、模块退役和测试接线。
其中既有过时的生成实现名称，也有需要继续核实的真实合同与接线
问题。诊断收集只在内存中替换 throw 以收集后续条件；其打印的
成功行不是正常总门通过证据，不能把 56 条诊断等同于 56 个运行缺陷。
清单保存在 `E:/xlang-validation/stdlib53-b224-dev/static-audit-dev4.json`。

WaitQueue 拓扑仍待确认，其原生 backend／拥有接收、上述合同收口、
更广故障与退役压力，以及 shipping DLL/SDK、53 模块总验收继续。
CRT 0、XRT 19 组未决策不变，对象池仍最后处理。
