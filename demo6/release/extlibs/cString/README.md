# cString

## 当前状态：2026-10-29

CRT `strlen` 的直接调用、C ABI 与下述字节/NUL 合同已验证；新增普通
消费范例暴露了主线 M11：`c_size` 可以直接打印，但不能用于 f-string。
[无包最小复现与修复方向](../../../dev/extlib_blockers/native_c_scalar_interpolation.md)
已落盘。当前默认门禁保留该正例并退出 1，整包未完成本轮发布签收；
不能把下方旧通过记录当作当前完整绿灯，也不在包内强转为 int 隐藏问题。

旧 C 测试原全部是编译期常量，GCC 16.2 `-O2` 下 main 不含 strlen 调用。
现在通过易变函数指针实际调用 CRT，覆盖 32 种对齐与 16 种长度的 512 组
缓冲、16 组恰好止于结尾 NUL 的分配，以及空/ASCII/UTF-8/内嵌 NUL；
完整输入哨兵也检查无写入。新语言测试覆盖默认空串、动态 UTF-8、内嵌
NUL、切片、临时实参和全局/局部/传参/返回完整签名函数值。

Windows PowerShell 5.1 默认 25 阶段中的前 24 阶段通过，含实际生成的
`strlen` 原型与五条独立程序路径，最后 records 语言例在插值处失败；
Linux 原生快照中的 C ASan/UBSan 与同样 JIT/AOT 先过，最后同样失败。
两平台输入指纹不变，失败现场、命令与原始输出保留，未修改主线或冻结 XRT。

`cString.length(text)` 是 CRT `strlen` 的直接声明，用于显式的 C 字符串互操作：结果是首个 NUL 之前的**字节数**。它不代表 XLang 字符串的完整长度，也不代表 UTF-8 字符数。普通文本请使用 `string.len` 或 `string.byteLen`。

例如 `"a\0b"` 的 `string.byteLen` 为 3，而 `cString.length` 为 1；`"你好"` 的 `cString.length` 为 6。模块无原生封装层、状态和资源所有权。不要用返回值确定可含 NUL 数据的缓冲区长度。

范例分别位于 `release/extlibs/cString/examples/cString.xl` 和 `cString_c.c`。在 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/cString/test/check.ps1`，验证边界、双语输出和 AOT。
两种语言共用 `examples/cString.expected.txt`，不是只比较双方输出。
新 `examples/records_c.c` 展示记录完整字节数与 C 文本前缀长度的差别，
严格 C、直跑和独立程序均与 `records.expected.txt` 的六行结果一致；
对应的 `records.xl` 是保留的待修复正例，当前不能运行，不改写输出语法绕过。
`test/interop.xl` 已登记覆盖，可单独执行以复核已通过的字符串/函数值边界。

Windows JSON 报告在 `build/test/cString_guard/run_*.json`；成功只清理自己的
构建目录，失败保留目录。Linux 也只在成功时清理，失败保存命令、退出码
与输入指纹。本包不传入非法 C 指针，不为 `strlen(NULL)` 定义新语义。

以下保留历史验证记录，当前未闭环项以上面的 M11 为准。

Linux 可运行 `bash release/extlibs/cString/test/check.sh`；该门禁以
ASan/UBSan 运行 C 边界和范例，再运行 XLang JIT 与边界/范例两份独立
EXE，并将 C、JIT、AOT 范例输出逐字节比较。
2026-10-26 当前双平台发布版门禁均退出码 0。Linux 两份独立 EXE
包含内嵌 NUL 与 UTF-8 字节边界；包本体仍只有 CRT `strlen` 声明，
没有增加文本复制或原生包装。
同日新增 C 单文件范例的 Windows/Linux 独立 EXE AOT：GCC、TCC、
C AOT 和 XLang JIT/AOT 输出一致。本包不返回拥有型 C 字符串，
因此与 `cstring` 所有权接管无关。此结果使用现有发布版；更新后的
工作树重建新发布版被主线能力矩阵快照校验挡住，尚待复验。
2026-10-24 使用更新后的 Windows 发布版 `xl.exe` 复验，默认包门禁全部通过，
包括严格 C、C/XLang 同输出、边界和带输出范例 AOT。Linux C ASan/UBSan
及 XLang 门禁也通过。旧版 `xl6ValueDataSnapshotV1` 缺符号已解除；
这只是 cString 包级验证，不代表整个 xlang 发布版签收。

以下为历史验证记录：
2026-10-20：Windows/Linux 包级门禁现同时比较独立编译的 C 范例、
`xl run` 直跑同一 C 文件和 XLang 范例的输出；三条路径及原有 NUL、
UTF-8 字节边界通过。库仍是 CRT `strlen` 的直接声明，没有新增封装。
本轮未复验完整带输出 AOT，既有共享阻断不变。
