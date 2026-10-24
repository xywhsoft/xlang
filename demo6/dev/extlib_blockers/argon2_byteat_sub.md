# argon2 主线阻断：uint8 与 int 算术的 XLIR 路径

2026-10-25 复验：当前 Windows 发布版运行本页最小复现退出 0；
`test/cases/extlib_argon2.xl` 的 JIT 与专项门禁中的独立 EXE 均通过，
含 NUL 的完整输入也已验收。**Argon2 的这条阻断已解除。**
下述 2026-10-12 至 10-22 的失败、其他压缩包的复现和修复建议是
历史快照；本次没有据此推断它们各自当前的包级状态。

- 验证时间：2026-10-12；`demo6/release/xl.exe`（2026-10-07 构建）及同目录 DLL。
- 影响：`xl run examples/extlibs/argon2_basic.xl` 和 `xl run ../test/cases/extlib_argon2.xl` 均不能完成编译。定位到 `extlibs/argon2/src/argon2.xl:93` 的 `string.byteAt(info, 0) - 48`。
- 最小复现：`xl run ../dev/extlib_blockers/argon2_byteat_sub.xl`，在 `demo6/release` 目录执行。
- 期望：将字符 `0` 的字节值 48 减去 48，得到 `int 0`，程序退出码为 0。
- 实际：退出码 1，报 `:2:39: [ir] typed XLIR operator operand has no canonical physical-to-execution path`。
- 疑似原因：`string.byteAt` 返回 `uint8`，和 `int` 常量组成二元算术时，语义层已接受，但 typed XLIR 未提供该物理类型到算术执行类型的规范路径。诊断位于 `src/ir/xl6_ir.c` 的运算符操作数 ABI 校验处。
- 建议：在主线的共享数值拓宽/XLIR 物理转换路径补全 `uint8` 与 `int` 算术，确保直接返回、赋值、嵌套表达式和比较都一致；加最小正反例。此任务不修改主线，也不在每个扩展包内加显式转换绕行。
- 当时状态：blocked-upstream；当前 Argon2 最小复现与包级语言门禁已通过。

## 第二个触发面：bz2

`xl run ../test/cases/extlib_bz2.xl` 在第 68 行
`incremental[10] = incremental[10] ^ 1` 报同一条 typed XLIR 诊断。
这说明问题不只限于 `string.byteAt`，还覆盖 `Buffer` 字节索引与 `int` 的
位运算。主线修复时应把字节类型参与算术和位运算的物理转换统一测试；
`bz2` 的包级验收暂缓，不在测试里增加兼容性强制转换。

2026-10-20 复验：`powershell -NoProfile -File tools/check_extlib_bz2.ps1 -SkipAot`
在严格 C、269 项 Python 互通、文件边界与中央 C 范例 JIT 通过后，
仍于 `test/cases/extlib_bz2.xl:68:39` 报相同 XLIR 诊断并以非零状态退出。
包内文件范例的类型与临时目录问题已单独修复；它们不解释此共享错误。

`lz4` 也有相同触发面：`test/cases/extlib_lz4.xl:92` 与
`release/examples/extlibs/lz4.xl:78` 都使用 `Buffer` 字节索引 XOR 255，
当前 `xl run` 均报同一条 XLIR 操作数路径诊断。基本/文件范例的其他路径可运行，
但完整包级验收仍等待共享修复。

`lzma` 在 `test/cases/extlib_lzma.xl:92` 和
`release/examples/extlibs/lzma.xl:77` 使用同形态的字节 XOR，
更新文件 API 的 `Buffer.toBytes()` 调用后，测试仍被该 XLIR 诊断阻断。

同一诊断已在更新文件 API 调用后的三个包级测试再次确认：

| 包 | 复现位置 | 运行命令（在 `demo6/release`） |
| --- | --- | --- |
| `miniz` | `test/cases/extlib_miniz.xl:92` | `xl run ../test/cases/extlib_miniz.xl` |
| `snappy` | `test/cases/extlib_snappy.xl:90` | `xl run ../test/cases/extlib_snappy.xl` |
| `zstd` | `test/cases/extlib_zstd.xl:92` | `xl run ../test/cases/extlib_zstd.xl` |

这三处都对 `Buffer` 的单字节索引结果执行 XOR 255。

## 2026-10-22 复验范围校正

上面的 `miniz`、`snappy` 复现位置和 `lzma` 综合范例位置是旧版本测试记录，
不能作为当前包级阻断状态使用。当前 `extlib_miniz.xl` 与
`extlib_snappy.xl` 已改为直接写入损坏字节；两者 `xl run` 成功，
Windows 包级 `check.ps1 -SkipAot` 的 C、XLang 和双语对照门禁也通过。
`release/examples/extlibs/lzma.xl` 当前 `xl check` 通过，但
`test/cases/extlib_lzma.xl:92` 仍报相同 typed XLIR 诊断。
这些测试变化没有修复主线的混合字节算术；`argon2`、`bz2`、`lz4`、
`zstd` 等现存复现仍须在主线统一解决。`miniz`、`snappy` 默认 Windows
门禁均在完整 XLang AOT 构建时报缺少 `xl6ValueDataSnapshotV1`，
因此两包仍未完成发布级验收。
