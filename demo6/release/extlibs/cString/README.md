# cString

`cString.length(text)` 是 CRT `strlen` 的直接声明，用于显式的 C 字符串互操作：结果是首个 NUL 之前的**字节数**。它不代表 XLang 字符串的完整长度，也不代表 UTF-8 字符数。普通文本请使用 `string.len` 或 `string.byteLen`。

例如 `"a\0b"` 的 `string.byteLen` 为 3，而 `cString.length` 为 1；`"你好"` 的 `cString.length` 为 6。模块无原生封装层、状态和资源所有权。不要用返回值确定可含 NUL 数据的缓冲区长度。

范例分别位于 `release/extlibs/cString/examples/cString.xl` 和 `cString_c.c`。在 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/cString/test/check.ps1 -SkipAot`，可验证边界和双语输出；不带 `-SkipAot` 时检查 AOT。
Linux 可运行 `bash release/extlibs/cString/test/check.sh`；该门禁以
ASan/UBSan 运行 C 边界和范例，再运行 XLang 边界与范例并逐字节比较输出。
当前 C/XL 边界、双语输出及无输出边界 AOT 通过；带输出范例 AOT
仍缺共享符号 `xl6ValueDataSnapshotV1`，见
`dev/extlib_blockers/text_snapshot_link.md`。

2026-10-20：Windows/Linux 包级门禁现同时比较独立编译的 C 范例、
`xl run` 直跑同一 C 文件和 XLang 范例的输出；三条路径及原有 NUL、
UTF-8 字节边界通过。库仍是 CRT `strlen` 的直接声明，没有新增封装。
本轮未复验完整带输出 AOT，既有共享阻断不变。
