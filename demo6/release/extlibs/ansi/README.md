# ansi

纯文本 ANSI 控制序列构件；不会主动写终端、切换控制台模式或读取终端状态。适合构造终端样式文本、光标控制文本，以及从日志中去除已知的 CSI/OSC 序列。是否显示颜色取决于输出设备、终端模式和用户配置；写入普通文件时得到原始转义字节。

## API

| 功能 | XLang 函数 |
| --- | --- |
| 字符样式 | `bold`、`dim`、`italic`、`underline`、`blink`、`inverse`、`hidden`、`strike`、`reset` |
| 颜色 | `fg`、`bg`（16 色 SGR 码），`fg256`、`bg256`（0..255），`fgTrue`、`bgTrue`（RGB 每通道 0..255） |
| 光标 | `up`、`down`、`left`、`right`、`nextLine`、`prevLine`、`column`，参数必须为正数 |
| 屏幕 | `clearScreen`、`clearLine`、`clearToEnd`、`hideCursor`、`showCursor` |
| 辅助 | `title`、`strip`、`displayWidth` |

`fg` 仅接受 30..37、90..97；`bg` 仅接受 40..47、100..107。无效参数抛 `ValueError`，不再返回空字符串吞掉待输出的文本。所有 XLang 文本入口拒绝内嵌 NUL，避免 C 零结尾接口静默截断。`title` 拒绝控制字符及 U+2028/U+2029 行分隔符，避免标题文本注入额外终端指令。`strip` 识别完整 CSI 与 OSC（BEL 或 ESC `\\` 终止）；畸形、未知或未结束的 ESC 序列会报错，而非静默删除后续文本。

`displayWidth` 忽略 CSI/OSC，按 UTF-8 标量估算**单行**占用列数：常见组合附加符、零宽空格/连接符与变体选择符为零列，东亚宽字符和常见 emoji 为两列。它不是完整 Unicode grapheme/终端排版引擎：ZWJ 复合 emoji、区域旗帜、East Asian Ambiguous 字符及终端实现差异可能与真实显示不同。制表符、换行、U+2028/U+2029、控制字符、非法 UTF-8、双向控制符或畸形转义序列报错；标题和 OSC 载荷同样拒绝 Unicode 行分隔符和双向控制符，OSC 载荷中的 DEL/C1 控制字符也拒绝。`strip` 只剥离 ANSI 序列，不负责清洗普通文本里的 Unicode 格式字符。

## C 互操作

公开头文件为 `include/ansi_native.h`。文本函数返回独立分配的 `void*`，实际为 NUL 结尾 UTF-8/ANSI 文本；C 侧可转为 `char*`，使用后以 `xrtFree` 释放。失败返回 `NULL`，用 `xrtGetError` 读取原因。这里保留 `void*` 是为了匹配 XLang 的 `point` ABI；XLang 封装调用 `__xl_runtime.takeCstr` 自动接管所有权。`xansi_display_width` 失败返回 -1。库函数不共享可变结果槽，重复和多线程调用不会覆盖先前结果。

公开 C 调用会清除本线程上一次的 XRT 错误；成功返回时 `xrtGetError()` 为 `NULL`。前一次调用失败后可直接调用其他 ANSI 入口，无需调用方先手动清错。包级 C 门禁以四个工作线程交错验证独立结果、UTF-8 列宽与错误槽隔离。

## 范例与验证

- C/XLang 对照：`release/extlibs/ansi/examples/ansi_c.c`、`ansi.xl`。C 范例可直接 `xl run` 单文件执行；普通 C 构建仍独立链接 `ansi_native.c`。
- 对照范例按原始转义字节校验所有公开构件（样式、颜色、光标和屏幕控制），同时展示 UTF-8 宽度与去除转义；验证不依赖终端渲染或颜色配置。
- 完整展示：`release/examples/extlibs/ansi.xl`
- 边界：`release/extlibs/ansi/test/test_ansi.c`、`test_threads.c`、`boundaries.xl` 与 `test/cases/extlib_ansi.xl`

在 `demo6/release` 运行 `xl run extlibs/ansi/test/boundaries.xl`；在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package ansi -Source release/extlibs/ansi/test/test_ansi.c`。2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、XL JIT 和双语范例对照；2026-10-13 增加四种文本入口的 NUL 红绿用例，复跑 C/XL 主路径与范例。

Windows 包级门禁在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/ansi/test/check.ps1 -SkipAot`；它编译并运行严格 GCC 的 C 边界与范例、运行 XLang 边界及两份范例，并比较 C/XLang 输出。去掉 `-SkipAot` 时先构建运行完整范例 AOT 并与同次 C 范例对照，再构建运行边界 AOT；当前构建在下述共享缺符号处失败，不计为通过。

2026-10-14 补失败→成功的 C 错误槽回归：样式、控制序列、标题、剥离与宽度计算均不会保留旧错误。Windows 严格 GCC、C/XL 边界、双语范例及包内 `test/check.sh` 的 Linux ASan/UBSan 门禁通过；没有改变 ANSI 输出格式。

2026-10-15：`displayWidth` 原会跳过 OSC 载荷中的非法 UTF-8 或 DEL/C1 控制字符并返回正常宽度，和单行安全文本约束不一致。BEL 与 ESC `\\` 两种结束符的 C 负例先红后绿，共用 OSC 识别器现按 UTF-8 标量校验载荷；含中文标题的正例保持通过。Windows 严格 GCC、C/XLang 边界、C/XLang 范例同输出和 Linux ASan/UBSan 通过；完整 AOT 阻断不变。

当前 `xl build extlibs/ansi/test/boundaries.xl -o ../build/test/ansi_xl_test.exe` 被共享链接缺符号 `xl6ValueDataSnapshotV1` 阻断；AOT 尚未验收，见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-17：C/XLang 负例先复现零宽空格被误计为一列、双向覆盖字符被标题和列宽接口接受。现在零宽字符按零列计算，双向控制符在标题、OSC 载荷与列宽输入中明确报错；Windows 包级双语门禁及 Linux ASan/UBSan 已通过。完整 AOT 的共享缺符号阻断未变。

同日补 U+2028/U+2029 行分隔符边界：C/XLang 负例先复现标题、OSC 和列宽接口接受非单行文本，再统一拒绝。Windows 严格 GCC/C/XL 包级门禁与 Linux C ASan/UBSan、XLang 双语联合门禁通过；完整 AOT 仍受共享缺符号阻断。

2026-10-19 单文件范例：直接 `xl run release/extlibs/ansi/examples/ansi_c.c`
原报 `xansi_up` 未解析。现在仅在 TCC 单文件路径包含包实现，GCC
分离编译路径保持不变；Windows/Linux 包门禁均核对两条 C 路径与 XLang
范例同输出，Linux 包括 ASan/UBSan。完整带输出 AOT 阻断未变。

2026-10-20：Windows 默认门禁把完整带输出范例 AOT 加入边界 AOT 之前，
并复用同次 C 范例输出作对照；`-SkipAot` 仍只做 C/JIT 快测。完整范例
若因共享符号无法链接，门禁明确失败，不把快测通过当成发布验收。

2026-10-23：C/XLang 对照范例补齐所有公开 ANSI 构件的原始字节校验，
覆盖先前只在 API 表中列出的样式、颜色、光标及屏幕控制入口。
Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh` 的 C ASan/UBSan、
XLang 和双语同输出门禁通过；完整 AOT 的共享缺符号阻断未解除。
