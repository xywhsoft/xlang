# shellwords

将命令行**词法文本**拆成参数，或把参数安全地编码为单个 shell 词。
XLang 使用 `import shellwords`；C 使用 `include/xrt_shellwords.h`。
本库不执行命令，不做变量展开、通配符、命令替换或文件访问。

## 接口

- `split(text)`：按空白、引号与反斜杠拆分，保留 `''`/`""` 空参数；
  不平衡引号及末尾单独的反斜杠报 `ValueError`。
- `quote(word)`：产生一个可由 POSIX shell 按原文读取的参数；安全字符
  原样返回，其余使用单引号及 ` '\'' ` 序列编码。形如 `NAME=value`
  的 POSIX 赋值词与 Bash 的 `NAME+=value` 即使不含空白也会整体引用，
  避免位于命令名前时被识别为变量赋值；
  左侧不构成变量名的普通 `=` 词不额外引用。
  命令位置可识别的 POSIX 保留词（如 `if`、`case`）及可选保留词
  （如 `time`）、尾随 `:` 的词也整体引用；`a:b` 这类普通内部冒号不变。
- `join(words)`：对字符串数组逐项 `quote` 并用空格连接；非字符串项
  和内嵌 NUL 均报错。`split(join(words))` 保持参数序列不变。
- `quoteWindows(word)`：按 MSVCRT 的 `argv` 引号/反斜杠规则编码一个参数。
  **不**处理 `cmd.exe` 的 `&`、`|`、`%` 等元字符，也不等同于 PowerShell
  转义。启动 Windows 进程时应配合目标程序的参数解析约定使用。

拆分过程保持 UTF-8 字节原样；XLang 的 `split`、`quote`、
`quoteWindows` 遇内嵌 NUL 会在传给 NUL 结尾的 C 参数前抛 `ValueError`，
与长度感知的 `join` 拒绝规则一致。
POSIX C 接口按参数字节处理，不要求有效 UTF-8：除 NUL 外，非 UTF-8 字节也可经 `quote` 和 `split` 原样往返；这不是对 XLang 源码或文本编码的放宽。Windows `quoteWindows` 只负责 CRT 引号规则，不执行 UTF-8 到 UTF-16 转换；使用 `CreateProcessW` 时调用方仍须按自己的编码合同转换并拒绝无效文本。
没有固定 8192 字节/512 参数的静默截断；实际大小受 XRT 分配是否成功限制。

## C 所有权

`xrtShellwordsSplit` 返回拥有的 `xvalue*`，调用方用 `xrtValueRelease` 释放。
`xrtShellwordsQuote`、`xrtShellwordsQuoteWindows`、`xrtShellwordsJoin`
返回拥有的 `str`，调用方用 `xrtFree` 释放。失败时读取 XRT 错误。
每个公开调用开始时清理上次的线程错误；失败后直接成功调用另一入口，
不会把旧错误误认为本次操作失败。
各次调用返回独立存储，不使用共享静态缓冲区；调用方不得并发修改同一个
`xvalue` 参数数组。XLang 包装层接管原生字符串所有权。

## 范例与验证

`examples/basic.c` 与 `release/examples/extlibs/shellwords.xl` 逐项展示
`split`、`quote`、`join`、`quoteWindows`，输出一致。
`examples/basic.c` 包含包内原生实现，可从 `demo6` 目录直接运行
`release/xl.exe run release/extlibs/shellwords/examples/basic.c`；正式包仍按
清单编译原生源文件，Windows/Linux 门禁逐字比较直跑与严格 C 编译输出。
在 `demo6` 目录运行 `release/extlibs/shellwords/test/check.ps1` 与
`bash release/extlibs/shellwords/test/check.sh`；
`test/boundaries.xl` 与 `test/allocations.xl` 已登记到 `coverage.tests`，可用
`tools/smoke_extlib_catalog.ps1 -Package shellwords` 连同原主用例复验。
开发期间可用 `-SkipAot` 跳过当前主线的 AOT 链接缺符号问题。门禁包含
独立 GCC C 用例、已有及新增 XLang 边界、双语范例和 AOT 构建/执行。
Windows GCC 与 Linux ASan/UBSan 的 C 边界及双语范例均已验证；C 测试
逐字节检查 ASCII 1–127 的 `split(quote(word))` 往返；C/XLang 还覆盖
空参数、中文与引号混合、组合字符和多字节文本的 quote/join 往返。
Windows 门禁还会构建 `test/windows_argv.c`，把 `quoteWindows` 的结果交给
实际的 Windows 子进程解析，检查空参数、空格、tab、CR/LF、引号、连续及末尾反斜杠
以及垂直 tab、换页符的 `argv` 往返；该程序使用 UTF-8 到 UTF-16 转换和宽字符 CRT 入口，
另检查中文、引号混合及非 BMP 字符。这不扩张为 `cmd.exe`、PowerShell 或任意程序的
参数解析合同。
Linux C 门禁另将 `quote` 的九种代表性输入交给真实 `/bin/sh` 的 `printf`，
核对空串、空格、单引号、UTF-8、变量/命令替换文本和换行不会改变参数字节；
这与本包 `split` 的自测互为独立验证，不执行输入中的命令替换。
同一门禁还把 ASCII 1–127 的单字节参数用 `join` 一次编码，交给真实
`/bin/sh` 输出并逐字节核对，覆盖控制字符及相邻参数的引用边界。
2026-10-15 追加四个原生入口的逐点分配故障测试：`split`、`join`、
`quote`、`quoteWindows` 均在故障时返回内存错误，故障清除后可恢复；
Windows 严格 GCC/C/XLang 与 Linux ASan/UBSan 包级门禁通过。
当前 XLang AOT
边界构建仍因主线缺少 `xl6ValueIndexReadCopyV1` 未签收。

2026-10-17：Linux `test/check.sh` 加入中央与包内 XLang 边界，并将 C、
XLang 范例逐字节对照同一 golden 文件；与 C ASan/UBSan 同次通过。
完整 AOT 的共享链接阻断不变。

2026-10-17：C 测试增加 169 组相邻片段组合，覆盖单引号、双引号、
反斜杠、空白、shell 元字符和多字节 UTF-8 的 `split(quote(word))`
往返。Windows 严格 GCC、真实 `argv` 子进程与 Linux ASan/UBSan、
`/bin/sh` 对照及 C/XLang 范例门禁通过；未发现新的包内缺陷，
完整 AOT 的共享阻断不变。

2026-10-17：真实 Windows 子进程 `argv` 往返增加引号内 CR 与 LF 参数；严格 GCC、C 测试、XLang 边界及双语范例门禁通过。此用例只验证 ASCII 控制字符，不推断 Unicode 命令行转换或其他 shell 规则。

2026-10-18：子进程门禁改为 `CreateProcessW` 加宽字符 `wmain`，覆盖 UTF-8 中文、空格、引号和非 BMP 字符到真实 Windows `argv` 的往返；Windows `-SkipAot` 全部包级检查通过。只验证 Windows CRT 参数引用规则，不包含 shell 元字符扩展语义；完整 AOT 共享阻断未变。

同日修复命令名前的赋值词：`quote("A=B")` 原样输出时会被 POSIX shell 当作环境赋值，Bash 的 `A+=B` 也同理；两条 C 负例先红后绿，现分别输出 `'A=B'`、`'A+=B'`，而 `a-b=e` 等非变量名左侧保持原输出。C/XLang 成对范例扩至四个公开入口，并共享逐字输出预期。Windows 严格 C、真实 argv、XLang 中央及包内边界、双语范例，Linux C ASan/UBSan 与语言对照门禁均通过。依据 [POSIX shell token 规则](https://pubs.opengroup.org/onlinepubs/9799919799/utilities/V3_chap02.html)及 [Bash 手册](https://www.gnu.org/software/bash/manual/html_node/Major-Differences-From-The-Bourne-Shell.html)。完整 AOT 共享阻断未变。

2026-10-18：命令位置保留词原被 `quote` 当作普通安全词原样输出，
例如 `quote("case")`；POSIX 还规定尾随冒号的词在相应位置保留。
C/XLang 用例先复现，再要求这两类词整体引用；普通内部冒号和其他
安全词仍保持原输出。Windows `-SkipAot` 包级门禁、Linux ASan/UBSan
和真实 `sh -n` 命令位置检查通过；不跳过 AOT 的 Windows 门禁仍在
`xl6ValueIndexReadCopyV1` 未解析处停止，未签收发布级。此修正只涉及
词法引用，不承诺消除调用方 shell 环境中的别名或执行目标选择风险。

同日补 C 字节语义回归：含 `0x80`/`0xFF` 的单个参数先经过 `split`，再由 `quote` 交给真实 `/bin/sh` 的 `printf`，两处均逐字节核对。Windows 严格 C/真实 `argv` 与 XLang 包门禁、Linux C ASan/UBSan 和双语门禁通过；实现未改，完整 AOT 阻断不变。

2026-10-19：C 教学范例补单文件 `xl run`，Windows/Linux 包门禁
分别与严格 C 编译结果逐字比较；Windows 真实宽字符 CRT `argv`、
Linux C ASan/UBSan 与 `/bin/sh` 对照、XLang 双语门禁均通过。
包实现未改，完整 AOT 共享缺符号阻断不变。

2026-10-20：增加 XLang 层分配故障测试，逐一扫描 `split`、`quote`、
`quoteWindows`、`join` 的前 32 个分配点，检查错误归类为 `MemoryError`
且清除故障后可继续调用；此用例进入 Windows/Linux 包级门禁。

同日补 Bash 命令位置关键字 `coproc`：此前 `quote("coproc")` 原样返回，
可在 Bash 中被当作语法而不是普通词。C/XLang 红例先复现、修复后统一输出
`'coproc'`；Windows 包级、Linux ASan/UBSan、目录 smoke 3/3 和真实 Bash
语法检查通过。仅修正 POSIX 风格引用，不改变 Windows `argv` 规则；
完整 AOT 的共享缺符号阻断仍在。
