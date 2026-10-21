# units

`units` 将字节数、秒数和普通计数格式化为可读文本，并解析带单位的字节数与时长。为消除旧实现的二进制/十进制歧义，`bytes` 使用 IEC `KiB/MiB/GiB/TiB/PiB/EiB`（1024 进制），`bytesDecimal` 使用 SI `kB/MB/GB/TB/PB/EB`（1000 进制）。`parseBytes` 按同样的单位解析，大小写不敏感；裸数或 `B` 表示字节，**不再接受含义不明的单字母 `K/M/...`**。这是包在首版发布前的口径收紧。
字节与计数的格式化结果固定使用点号小数，不受进程 `LC_NUMERIC` 影响；`parseBytes` 也只接受点号小数。

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

- XLang：`release/examples/extlibs/units.xl`
- 同输出 C：`release/extlibs/units/examples/units_c.c`

在 `demo6/release` 可直接运行 `xl run extlibs/units/examples/units_c.c`；
普通 C 编译仍按包清单单独链接 `units_native.c`。
- 边界：包内 `test/test_units.c`、`test/test_threads.c`、`test/boundaries.xl`，以及 `demo6/test/cases/extlib_units.xl`

在 `demo6/release` 运行 `xl run extlibs/units/test/boundaries.xl`；在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package units -Source release/extlibs/units/test/test_units.c`。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/units/test/check.sh`，以严格 GCC 和 ASan/UBSan 复验 C 边界、四线程并发与 C 范例，并运行中央及包内 XLang 用例、逐字节比较双语范例输出。2026-10-14 修复整数显示中的负零；C/XLang 正负舍入边界、双语范例和无输出 AOT 核心均已复跑通过。
Windows 可在同目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/units/test/check.ps1`，联合验证严格 GCC、C ABI/边界/四线程、中央与包内 XLang 用例、双语范例同输出及无输出核心 AOT；`-SkipAot` 仅跳过最后的核心 AOT。每次门禁使用独占构建目录并在退出时清理，不写入发布目录临时文件。
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
