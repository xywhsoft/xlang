# units

2026-10-28 完整返回链复查：旧可空局部返回在接管原生文本后仍调用
`xlir_string_copy`。新 C/XL 对照预算先红后绿，五个格式化入口均只需
一次最终文本分配；预算 0 保持内存错误，预算 1 成功，不再生成第二份
正文。现以 `@owned string` 直接返回，C 的真正失败仍通过 XRT 传播，
公开 API、C ABI、舍入、区域设置与输入错误合同未变。专用 cgen 夹具
显式调用五个入口，核对各接管一次且没有后续 `string_copy`。
接管的 `strlen`、外部调用错误作用域和必要结果分配仍存在，不是整条
操作对手写 C 的零开销认证。

本次标准 `demo6/build.bat` 成功重建三件套，跨包拥有型导出的 JIT/AOT
用例通过；未合入新主线或更新 XRT。Windows/Linux 默认完整包门禁
退出 0，包含严格 C/ABI、Linux ASan/UBSan、四线程、中央/包内边界、
旧分配故障与新结果预算、双语固定输出以及上述语言用例与双语范例
独立 EXE。测试运行库只启用产品模块及线程/内存调试，不再使用 ALL；
未改变产品模块依赖。两平台记录输入指纹，失败时保留命令与现场。

2026-10-27 字符串所有权收口：五条格式化函数的 C 原型明确为
拥有型 `char*`，XLang 用 `@owned string?` 直接接管，去掉
`point` / `takeCstr` 中转。原生结果复制统一调用 XRT 的
`xrtStrDup`，不再保留本包自己的复制实现；manifest 显式声明
`XRT_MODULE_STRING`。生成 C 和 ABI 门禁分别固定直接接管及
返回类型，接管仍需一次 `strlen`。Windows 独立核心 EXE 从
842,752 增至 847,872 字节（增加 5,120 字节）。

2026-10-26 完整重建 Windows/Linux 发布物后，本包在两平台 x64
包级范围签收：默认门禁通过严格 C/ABI、Linux ASan/UBSan、线程、
语言边界与分配故障、C/XLang 固定输出，以及核心、XLang 范例和
C 单文件范例的独立 EXE AOT。旧 VFS 头路径和带文本 AOT 缺符号
记录均属历史发布物；不外推为全产品发布准入、其他架构或长时负载验收。

2026-10-25 新 Windows 发布版：从 `demo6` 根目录运行默认
`test/check.ps1`，严格 C/ABI、线程、XLang 边界与分配故障、C/XLang
同输出及核心/范例 AOT 全通过。下方同日 VFS 缺头失败是旧发布物
记录，不再是当前 Windows 门禁结果；Linux 独立 AOT 尚未验收。

2026-10-25 使用先前发布物执行 Windows 默认 `test/check.ps1`：
严格 C/ABI、线程、C 范例和 C 文件直跑均通过；首个 XLang 中央用例
因共享 TCC VFS 缺少 `include/xruntime_decl.h` 停止，后续语言和 AOT
阶段未执行。本次不能沿用下方早先同日的完整通过结果签收当前发布物。
最小复现与主线修复方向见
`../../../dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`；包内不加
头文件路径回退。

`units` 将字节数、秒数和普通计数格式化为可读文本，并解析带单位的字节数与时长。为消除旧实现的二进制/十进制歧义，`bytes` 使用 IEC `KiB/MiB/GiB/TiB/PiB/EiB`（1024 进制），`bytesDecimal` 使用 SI `kB/MB/GB/TB/PB/EB`（1000 进制）。`parseBytes` 按同样的单位解析，大小写不敏感；裸数或 `B` 表示字节，**不再接受含义不明的单字母 `K/M/...`**。这是包在首版发布前的口径收紧。
字节与计数的格式化结果固定使用点号小数，不受进程 `LC_NUMERIC` 影响；`parseBytes` 也只接受点号小数。
单位后缀只使用 ASCII 字母，大小写折叠不受进程 `LC_CTYPE` 影响。

## XLang API

- `bytes(value)` / `bytesDecimal(value)`：使用相应单位格式化；大于等于一种单位时保留两位小数，字节保留零位。
- `parseBytes(text)`：接受如 `1.5MiB`、`1.50 kB`、`-1 B`；返回浮点字节数。
- `duration(seconds)` / `durationShort(seconds)`：四舍五入到整秒，例如 `1h 2m 5s` / `1:02:05`；负时长保留一个整体负号。
- `parseDuration(text)`：接受由大到小、不重复的 `d/h/m/s` 组合，例如 `-1h30m`；裸数为秒。
- `count(value)`：`K/M/B/T` 计数缩写；`isPow2(value)` 判断正整数是否为 2 的幂。

非法输入抛 `ValueError`，不再使用与合法 `-1` 混淆的解析失败哨兵。数值必须有限；字节/计数限绝对值 `1e21`，时长限绝对值 `1e12` 秒。解析器不接受指数或任意单位词；格式化因显示舍入会损失精度，不应将输出作为精确持久化格式。`durationShort` 是展示格式，不是 `parseDuration` 的输入语法。

字节与计数的整数显示遵循当前 CRT 舍入模式；若结果舍入为零，统一显示 `0` 而不是 `-0`。例如默认模式下 `bytes(-0.4)` 为 `0 B`、`count(-0.4)` 为 `0`，`bytes(-0.6)` 仍为 `-1 B`。
当舍入后的显示值已达到下一单位时自动升档，例如 `bytes(1048575)` 为
`1.00 MiB`，`count(999960)` 为 `1.0M`；不输出 `1024.00 KiB` 或 `1000.0K`。
升档判断与小数写出共用 XRT 固定精度格式化结果；即使线程将 CRT 舍入模式
改为向下舍入，上述边界也不会停留在已经显示为满额的低一级单位。

## C 接口与所有权

公开声明在 `include/units_native.h`。文本结果由 XRT 独立分配，C 调用方用 `xrtFree`；格式化失败返回 `NULL`。解析函数要求显式字节长度，拒绝内嵌 NUL；失败返回 `NaN`，同时设置 XRT 错误对象。合法负数包括 `-1` 可正常返回。`xu_is_pow2` 返回 C `int` 0/1，与 XLang 桥的 `c_int` 对齐。包内不再使用共享轮转缓冲，可并发调用；XLang 自动接管文本所有权。

`xu_is_pow2` 的输入是 `int64_t`，对应 XLang `int`；解析函数的长度仍用 C `long long`，对应 XLang `c_longlong`。

每次公开 C 调用都会清除本线程上次遗留的 XRT 错误；成功结果不会与旧错误并存，失败后仍可通过 `xrtGetError()` 读取本次诊断。

## 范例与验证

2026-10-25：从当前源码重新构建 Windows 发布版后，默认
`test/check.ps1` 的严格 C/ABI、线程、语言边界、分配故障、
C/XLang 同输出及核心/公开范例 AOT 均通过。同轮 WSL
`test/check.sh` 的 C ASan/UBSan、线程、语言边界、分配故障和
双语对照通过。Linux 使用现有 `release/xl` 运行语言用例；
本轮没有重建 Linux 发布版或验证 Linux 独立 EXE。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C ABI、边界与四线程、C 文件直接运行、中央/包内 XLang 边界、
分配故障、双语同输出及核心/完整范例 AOT 均通过。旧的完整 AOT
缺符号记录属于先前发布物；本次没有重跑 Linux sanitizer，也不
扩大到包外的货币或物理单位系统。

- XLang：`release/examples/extlibs/units.xl`
- 同输出 C：`release/extlibs/units/examples/units_c.c`

在 `demo6/release` 可直接运行 `xl run extlibs/units/examples/units_c.c`；
普通 C 编译仍按包清单单独链接 `units_native.c`。
- 边界：包内 `test/test_units.c`、`test/test_threads.c`、`test/boundaries.xl`、`test/allocations.xl`、`test/owned_results.xl`，以及 `demo6/test/cases/extlib_units.xl`；`test/owned_probe.xl` 单独检查完整 cgen 返回链。

在 `demo6/release` 运行 `xl run extlibs/units/test/boundaries.xl`；在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package units -Source release/extlibs/units/test/test_units.c`。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/units/test/check.sh`，以严格 GCC 和 ASan/UBSan 复验 C 边界、四线程并发与 C 范例，运行中央及包内 XLang 用例、逐字节比较双语范例输出，并构建运行核心与双语范例独立 EXE。2026-10-14 修复整数显示中的负零；C/XLang 正负舍入边界、双语范例和无输出 AOT 核心均已复跑通过。
Windows 可在同目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/units/test/check.ps1`，联合验证严格 GCC、C ABI/边界/四线程、中央与包内 XLang 用例、双语范例同输出以及核心与双语范例独立 EXE；`-SkipAot` 仅供聚焦开发，跳过全部 AOT。每次门禁使用独占构建目录，成功时清理，失败时保留命令、输入指纹及产物，不写入发布目录临时文件。默认模式还执行包内三份语言回归的独立 EXE，而不只验收核心和公开范例。
同日补齐舍入后升档边界；Windows C/XLang 与 Linux ASan/UBSan 已通过。

2026-10-15 原先仅使用 XRT 基础分配和错误接口，空模块集下无输出核心 AOT 为 681,984 字节。现为区域无关的固定小数位写出声明 `XRT_MODULE_NUMBER_FORMAT`，同一核心 AOT 为 700,928 字节且运行通过；Windows C 边界、线程、XLang 边界和双语同输出范例通过。完整边界 AOT 仍缺共享符号 `xl6ValueDataSnapshotV1`，该阻断未变。

2026-10-15 修复逗号小数点区域设置下的格式化输出：原 `bytes(1536)` 写成 `1,50 KiB`，与 `parseBytes` 的点号输入不一致；C 区域设置负例先红后绿。新实现复用 XRT 的固定精度数字格式，不改变既有舍入和单位升档规则。Linux ASan/UBSan 复验通过。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与线程测试、双语同输出范例及 `test/cases/extlib_units.xl` 无输出核心 AOT。`xl build extlibs/units/test/boundaries.xl -o ../build/test/units_boundaries.exe` 仍因共享链接缺少 `xl6ValueDataSnapshotV1` 失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-17：Linux 包级脚本复用原生编译对象，将 C 边界、四线程并发、C 范例、中央/包内 XLang 边界和 C/XLang 逐字节同输出纳入一次门禁，ASan/UBSan 同次通过。公开 API 与实现未变；完整 AOT 的共享缺符号阻断仍未解除。

2026-10-18：新增 Windows 单入口包门禁，严格 GCC、C 边界和四线程、中央/包内 XLang、双语同输出及无输出核心 AOT 同次实跑通过；Linux ASan/UBSan 门禁也复跑通过。完整带文本 AOT 仍缺共享符号，未签收完整发布级验收。

2026-10-19：补 `duration`/`parseDuration` 的整数秒往返性质测试，C 用
固定种子扫描 4096 个正负时长，XLang 另扫描 200 个值，覆盖跨天与
大时长；Windows 严格 C/XLang、Linux C ASan/UBSan 和双语门禁通过。
这只验证长格式的整数秒合同；`durationShort` 仍是展示格式，不是
`parseDuration` 输入。完整带文本 AOT 的共享阻断未变。

2026-10-19：五条格式化入口新增 C/XLang 首次分配失败与清除故障后的
恢复回归；底层保留 `XERR_MEMORY`，XLang 桥抛 `MemoryError`，不误报
`ValueError`。`test/allocations.xl` 已登记到包清单；Windows 包级严格
C/XLang 与双语门禁、Linux C ASan/UBSan 及 XLang 联合门禁通过。
格式化实现未改，完整带文本 AOT 仍因共享 `xl6ValueDataSnapshotV1`
缺符号而未签收。

2026-10-20：`FE_DOWNWARD` 红测复现二进制、十进制字节与计数格式化在
升档判断和最终小数写出间使用不同舍入规则，可能产生满额低一级单位。
现直接依据 XRT 格式化后的显示值判断升档；正常模式输出不变。
Windows 严格 C/ABI/线程、XLang 边界与双语门禁，以及 Linux C
ASan/UBSan、线程和 XLang 联合门禁通过。完整 AOT 的共享阻断未变。

同日补强 Windows 默认门禁：无输出核心 AOT 后还需构建/运行带输出
XLang 范例，并与 C 范例逐字比较。`-SkipAot` 的严格 C、线程、
分配故障、XLang 边界及双语对照通过；默认门禁核心 AOT 通过，
完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺符号失败。本次独占
目录已清理，不能以过去仅核心 AOT 通过的结果签收发布级。

2026-10-23：单位后缀由区域设置相关的 CRT `toupper` 改为 ASCII 折叠，
保留小写 IEC 单位的 C/XLang 回归；C 测试在可用的土耳其语 `LC_CTYPE`
下额外核对 `i/I` 边界。Windows `-SkipAot` 严格 C、线程、XLang 与
双语门禁，以及 Linux C ASan/UBSan、线程与 XLang 门禁通过。
当前机器没有复现旧行为：Windows CRT 的土耳其语 locale 仍将 ASCII
`i` 转为 `I`，WSL 未安装该 locale。完整带输出 AOT 复验仍缺
`xl6ValueDataSnapshotV1`，不签收发布级。
