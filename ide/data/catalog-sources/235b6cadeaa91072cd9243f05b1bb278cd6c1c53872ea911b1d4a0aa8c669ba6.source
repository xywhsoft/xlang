# cString

`cString.length(text)` 是 CRT `strlen` 的直接声明，用于显式的 C 字符串互操作：结果是首个 NUL 之前的**字节数**。它不代表 XLang 字符串的完整长度，也不代表 UTF-8 字符数。普通文本请使用 `string.len` 或 `string.byteLen`。

例如 `"a\0b"` 的 `string.byteLen` 为 3，而 `cString.length` 为 1；`"你好"` 的 `cString.length` 为 6。模块无原生封装层、状态和资源所有权。不要用返回值确定可含 NUL 数据的缓冲区长度。

范例分别位于 `release/extlibs/cString/examples/cString.xl` 和 `cString_c.c`。在 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/cString/test/check.ps1`，验证边界、双语输出和 AOT。
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
