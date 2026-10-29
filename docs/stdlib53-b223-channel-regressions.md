# Channel 存量回归迁移与空值、错误清理修复

公开 Channel 的存量语言夹具已改用独立拥有的 `(status, T?)` 接收结果。
Windows/Linux 各通过 223 个回归阶段，原有 34 阶段公开接线、IR 损坏
合同和组件裁剪专项也重新通过。测试发现并修复了三处真实边界缺陷；
本批不是 WaitQueue、网络消费者、shipping SDK 或 53 模块总验收。

## 修复范围

- 语言 `var`／nullable bytes 的空槽可以是 NULL，但 XRT retain 接口要求
  真实 value shell。共用 RETAIN 指令与共用 runtime retain 调用现在先
  检查空槽，不再调用 `xrtValueRetain(NULL)`。正常、async 和 generator
  共享这些机械渲染路径，没有添加 Channel 专用空值转换。
- 语义检查允许向 `Channel<var>` 发送 null，IR 的资源输入准入却没有
  接受这一转换。现在以规范动态值类型身份接受 null→var，仍由既有
  boxing/lowering 完成转换；不放宽任意资源或名义类型的输入合同。
- Channel 失败结果的清理作用域此前先锁定一个没有诊断的失败，再
  安装原错误，因而丢失内存不足诊断。现在先安装原错误，再进入清理
  作用域。拥有结果的释放顺序、失败状态和原错误优先级保持不变。

拥有图遍历不是空值失败的根因：XRT visitor 原本允许空引用表示无边。
没有修改全局 trace 生成器、XRT 或 xruntime 快照，也没有更换其合同。
新宿主测试接受空引用，检查非空引用具有 ops，并验证 visitor 拒绝时
遍历立即停止。测试在无并发写入的冻结读作用域内执行，不声称通用
freeze 本身保证任意第三方对象图的全局静止。

## 存量测试与验收入口

原来的 `recvInto/tryRecvInto/recvTimeoutInto` 正例改为接收拥有结果，
随后按成功状态和非空值显式修改语言变量。闭包 cell 不再被当成原生
输出地址。旧 Into 保留一个明确的语义拒绝夹具，不保留兼容别名。
显式容量 1 的夹具也不再假设被扩大到 2。

`check_stdlib_channel_regressions.py` 是 Windows/Linux 共用批量入口；
原 Channel 资源门、历史 async storage-address 门和 `07c_channel.bat`
委托这一入口。它检查真实 typed/lowered 拥有结果、确定性 C、严格 C、
源码 JIT、模块重定位和独立 EXE。中央负诊断收集器读取同一个 25 项
清单，Windows 包装器只导出本轮实际验证、哈希未变的诊断日志。
Linux 源码驱动补齐附加 C 宿主文件的输入能力，与 Windows 一致。

两份静态总门的 Channel 条目改为拥有接收合同，不再要求旧输出地址
或旧生成实现。这些修改过的条目单独检查通过；总门本身仍有原有的
`XL6_IR_RUNTIME_COUNT` ownership 枚举及 thread gate 旧实现匹配失败，
不在本批宣称总门已通过。负诊断接线检查通过，没有降低覆盖阈值。

## 已验证证据

每个平台分别完成：

- 全源码驱动构建，Windows 303／Linux 302 个翻译单元；真实编译输入、
  私有 SDK、夹具、产物与日志记录哈希。只复用现有真实 TCC 产品，
  不把它或 shipping 库标记为本批重建结果。
- 18 个正例的 JIT、模块、typed/lowered IR、双次确定性 C、O0/O2
  严格 C 与独立 EXE 构建／运行；覆盖 int/bool/float/time/string/var、
  nullable、char/bytes、类引用、泛型、闭包、async、task 和 signal。
- 完整 Channel 与捕获变量接收的独立 EXE 各增加 5 次重复运行。
- 25 项语义／语法拒绝，检查具体诊断并保证不产生 C 文件。
- 原样保留混合 handfeel 夹具的 O0/O2 严格 C 检查，但只计编译验收。
- 原生全运行时故障宿主 O0/O2 各覆盖 35 个逻辑分配前缀，最后一个
  为首个未命中点 34；每次失败保留 MEMORY 错误，释放后存活计数和
  字节、分配／释放、非法释放及 UAF 计数平衡，二次 fini 无错误。
- 原有 34 阶段专项重新通过：39 个接收计划、6 个逻辑泛型合同、
  7 条 private lowering 链、1695 项损坏合同及按需组件裁剪。

回归证据为 `E:/xlang-validation/stdlib53-b223-check-win-dev8` 和
`/var/tmp/xlang-stdlib53-b223-check-linux-dev8`；原专项证据使用相应的
`stdlib53-b223-owned-*-dev8` 目录。提交前逐项对比最终选择性源码与
实际编译／测试输入，并检查两个平台各 21 个独立 EXE 不依赖
`xl.dll/xlc.dll` 或 `libxl.so/libxlc.so`。全运行时故障宿主不等于裁剪
后的 AOT 运行时，不能用其链接体积替代独立产物证据。

## 保留的边界与后续

`cgen_dev_handfeel_regression.xl` 在 Channel 之前仍有 Template.render
断言失败。去掉全部 Channel 代码的原有前缀在双平台独立复现相同
断言，故本批只验收它的编译；未决策的 X24 模板模块没有被修改。

WaitQueue 的原生 backend／拥有接收、网络组内的旧 Channel 消费者、
更广的无诊断故障和拥有图／模块卸载压力，以及 shipping DLL/SDK、
全量测试总门继续。WaitQueue 消费者拓扑存在旧文档 MPSC 与详细设计
MPMC 的冲突，已请求用户确认，不在本批自行变更。
CRT 0、XRT 19 组未决策和对象池最后的范围保持不变。
