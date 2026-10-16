# 终端共享产品的完整资源账本

旧终端账本夹具已从生产源码拼接迁为真实共享产品观察器。它直接链接普通构建的
`xl.dll`／`xlc.dll` 或 `libxl.so`／`libxlc.so`，不再自行编译后端、Array 桥接
或其他运行时实现。生产源码、Cgen、XRT 快照和公开 API 均未改变。

## 完整寿命与错误断言

每个会话保留原有 13 项错误守卫，检查无效配置不改写原尺寸、边界尺寸、
空接收者、非法 resize、stderr 合流、重复 reader 租约及普通管道拒绝 resize。
真实 child 先确认 `terminal-ready`；父端只发送一次 resize，仍验证三个 TTY、
stderr、实际 96×32、`reply:hello`，并增加原生正常退出码 0 的检查。

父端释放 Process 包装后继续读写，证明流租约持有其真实寿命；最后等待原生
waiter 尾部，再检查完整分配／释放计数、存活字节、double/invalid free 和 UAF。
Windows 逐个检查自有进程、输入、输出句柄已关闭，另与直接系统 ConPTY 的
保留量对照；本机对照每会话保留一个系统进程句柄，实际产品没有额外增长。
Linux 每会话 descriptor 零增长。没有访问或释放 opaque HPCON 的内部字段。

观察器的唯一私有布局是 `xprocess`。`stage_stdlib_process_observer.py` 从选定
快照提取其完整声明，同时要求 xrt／xruntime 两份声明相同、普通产品启用全部
模块。布局随真实条件宏编译并记录指纹；不维护手写结构副本，不为测试新增
等待 API。裁剪后的独立 AOT 可能具有不同私有字段，因此明确拒绝用于此观察器。

## 双平台验收

`check_stdlib_terminal_ledger_shipping.py` 在每端分别链接两个共享产品，
各测 O0／O2，每个宿主连续执行 16 个会话：共四项证明、64 个终端会话，
均通过完整堆和自有句柄账本。导入表核实宿主只链接选定的那一个 xlang 产品。
每端共 21 阶段；缺失 resize、错误 95×31 在两份产品上均触发原尺寸断言，
共四项拒绝对照，不以超时或启动失败代替断言证据。

布局不一致、重复声明、非完整产品和裁剪 ABI 四项观察器控制均正确拒绝。
旧 PowerShell／Bash 原生入口同步改为生成布局并链接共享运行库，保留原来的
账本输出检查；两端原生命令配方另独立执行通过。Linux 配方的 ASan／UBSan
仅仪表化观察器宿主，不将其写成共享运行库已完整仪表化。
Windows 入口在启动原生宿主前就复制匹配的 DLL 并记录副本指纹，不依赖 PATH
碰巧提供运行库，也不等到后续语言合同测试才复制 DLL。

跨组总门增加必需的 `terminal-ledger` 子门；只有其真实产品账本通过，
`NativeTerminalLedgerAcceptance` 才为 true。B325 的
`NativeSourceLedgerAcceptance=false` 保持原值，不回写历史证据，也不再用
旧源码拼接实现替代产品测试。

本轮产品总门 Windows 629 阶段、10 个子门，Linux 630 阶段、11 个子门全部
通过。每端仍为 311 个候选，保留 6 项专用宿主和 1 项平台条件；不将这些状态
改写成普通通过。终端语言门另独立执行其原有 256 次压力和尺寸拒绝对照。

## 复测与证据

- Windows 专项：`D:/xlang-validation/stdlib53-b326-ledger2-win/results.json`。
- Linux 专项：`/var/tmp/xlang-stdlib53-b326-ledger-linux/results.json`。
- 观察器拒绝控制：同前缀 `stdlib53-b326-observer-win`／`-linux/results.json`。
- 旧入口原生配方：同前缀 `stdlib53-b326-legacy2-win`／`-linux/results.json`。
- 产品总门：`D:/xlang-validation/stdlib53-b326-products-win/results.json`、
  `/var/tmp/xlang-stdlib53-b326-products-linux/results.json`。
- 指纹复核：`D:/xlang-validation/stdlib53-b326-dev/certificate-win.json`、
  `certificate-linux.json`；选定提交范围见同目录 `commit-selection.json`。

使用新的输出目录，向 `check_stdlib_terminal_ledger_shipping.py` 传入真实
`--root`、`--build-report` 和 `--out`；默认 `--sessions 16`。构建和所有提供方
必须与报告指纹一致。首次缺少 Windows 类型声明的探索失败仍保留于
`D:/xlang-validation/stdlib53-b326-ledger-win/results.json`，不覆盖为通过。

本批关闭旧终端账本迁移，不修复系统 ConPTY，也不宣称 Windows 全进程句柄
零增长。完整 53 模块的函数边界、工作线程故障注入及发布验收仍继续。
CRT 无未决策；19 个 XRT 未决策组与对象池最后不变。
