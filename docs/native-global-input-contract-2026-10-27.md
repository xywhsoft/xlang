# 原生 C 输入的全局变量 ABI 合同

本批修复源码包与显式 C sidecar 的全局变量类型校验缺口。已有统一入口
负责将声明头与每个 C 翻译单元一起编译，但这个头使用公共导出视图，
按设计省略了私有 `extern` 全局变量，导致全局定义可能绕过类型校验。
不能通过把反例改为正例，或向公共头泄露私有变量来解决。

## 实现

增加独立的 `xl6_ir_cgen_native_header.h` 和原生输入声明头入口。公共与
原生头共享同一个发射实现；只有后者遍历经过验证的 ExternGlobal 记录，
调用生成 C 已使用的同一声明发射器。类型、C 名、访问和存储身份全部来自
XLIR，不读取 AST、不扫描生成文本，也不根据原生符号名猜测类型。

`xlCompileUnitCSourceFiles` 对源码包和额外 C 文件继续使用同一包装与编译
流程，只将声明头切换为原生输入视图。该入口位于 exe-compiler 配置门外，
runtime-only 配置不能绕过它。公开 C 头、带绑定记录的公开头以及 IDE
C 桥查询继续使用原公共视图，不包含导入变量。

没有改变语言／XRT ABI、时间单位或运行时拥有策略。目标程序不新增桥接
调用、分配、复制、扫描或同步；新增工作发生在编译期的声明生成和 C 类型
检查。本批校验参与编译的 C 源码，不据此宣称能够推断任意外部二进制的 ABI。

## 双平台前后对照

使用同一个语言输入和五份错误 C 定义，分别覆盖结构／位宽、符号性、
数组、const 和布尔类型，再加原有错误源码包。修改前 Windows／Linux
均实际生成了六个错误配置的可执行文件；这些反例文件没有执行。
修改后六组均在编译阶段拒绝。

Windows／Linux 各 30 项全部通过：

- 正确的 int64／C bool／size_t 定义通过 JIT、module 与独立 AOT。
- 五种错误定义分别经 GCC、公开 AOT、JIT、module 拒绝。
- 原有错误全局源码包经公开 AOT、JIT、module 拒绝。
- 失败的 AOT 调用保留原有 `KEEP` 输出字节；仅在先证明 standalone
  编译已拒绝后才运行同一配置的 JIT／module 负例。
- 严格 IR 合同核对三个精确声明；12 个坏合同在 C／公共头／原生头
  输出前拒绝，每端 36 次空输出检查。

公共头与带绑定记录的公共头逐字节相同，不包含三个导入变量名。用同一个
独立 dump 程序分别链接修改前、修改后的完整源码对象，两个平台的公共头
前后也逐字节一致，原导出函数保持可见。

首次 Windows 新测试将 IR 合同 C 文件误命名为语言样例的同名 sidecar，
因此加载了编译器测试并出现 `xl6ParseResultFree` 未解析。已改为独立的
`xlir_native_global_ir_contract.c`，保留首次失败；没有修改自动 sidecar
规则来迁就测试。

## 完整 guard 的当前状态

重新核对 R8 完整批次的物理终态：首阶段 73 个 worker 为 46 通过、
27 失败，依赖阶段未运行。若干失败有后续独立修复证据，不能回填旧结果。
剩余失败必须继续区分真实 ABI／语言缺口、测试前置和过时合同。

提交 `de97f5e07` 对应的 1,295 单元完整 Windows source-driver
构建通过，未复用源对象。加入本批七个源／测试输入的统一候选已另行冻结，
并为 HTTP guard 补齐同一 Git 快照的 226 个 `xhttp*` 源码 fixture 文件。
这是现有本地 body 测试的输入闭包，不是开发新的 HTTP 标准库，也不消费
其他会话未提交的扩展修改。构建结束后由同一控制器继续运行完整 74-worker
场景和依赖分支；尚未把它们计为通过。

Windows 证据根为 `E:/xlang-validation/five-groups-20261027/p7-shipping/`：

| 目录／文件 | 内容 |
|---|---|
| source-win-r12-guards.exit.json | 当前已提交源码的完整 1,295 单元构建 |
| native-global-cohort-r1 | 首次 Windows sidecar 命名失败及 Linux 初验 |
| native-global-cohort-r2 | 最终 API／Cgen 对象与同步的编译器源码包 |
| native-global-check-r3/results.json | 30 项准入／拒绝／输出保护检查 |
| native-global-before-r4/results.json | 六个旧配置实际接受、公共头前后对照 |
| native-global-proof-r5.json | 独立 reader 核对源码、对象、日志和产物 |
| freeze-r13-native-globals/candidate.json | 精确候选与额外 HTTP 源码 fixture |
| parameterized-r13.controller.json | 统一候选构建与完整参数化运行状态 |

Linux 原件在 `/home/ubuntu/xlang-validation/20261027-p7-shipping/` 同名
r2／r3／r4／r5。持久检查工具为 `tools/check_xlir_native_global_contract.py`。
没有替换 release，也不宣称 runtime-only 产品、全部 74 项 guard、最终
联合／嵌套门已经通过。长期任务 active；仅普通离线 ABI 检查，无网络或
密码学安全性质专项。
