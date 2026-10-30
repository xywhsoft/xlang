# ansi

2026-10-29：17 个文本入口的私有 NUL 校验改为只校验，不再返回输入
字符串。此前即使原生结果已经直接接管，返回输入仍会额外分配并复制
整段正文；现在各入口校验后直接使用原参数。公开签名、30 个 C ABI
及原生实现均未改变，所有入口仍拒绝内嵌 NUL，不增加辅助接管层。

固定非空输入的调用点故障测试共 67 项：原绑定 17 项失败，新绑定
归零。该预算**包含**调用处构造 `"x"`：16 个文本生产者首次成功预算
从 4 降至 3，`displayWidth` 从 3 降至 2；去掉的是旧校验返回的输入
副本。余下分别是参数构造、NUL 针串构造和（文本生产者才有的）
原生结果分配。C 借用常量，同输入 33 项预算验证文本结果一次分配、
标量宽度零分配；两边不是同样的参数构造成本，不能宣称全链零开销。
生成 C 的专用完整输入/返回夹具中，正文复制赋值从 1 处降到 0，
另核对 29 条结果直接接管、全部 30 条实际原型与正式 C 头一致。
NUL/UTF-8 校验、搜索、结果 `strlen`、错误作用域等成本仍存在，
本轮未测大文本吞吐或端到端延迟，也不外推其他编译器与架构。

最终 Windows 默认门禁与 Linux 原生快照门禁均通过：严格 C、四线程、
Linux ASan/UBSan、旧新 OOM 与恢复、17 条 NUL 负例、双语八行黄金
输出、公开范例，以及**各九条**独立 AOT 构建/执行。测试运行库只
启用产品的三个模块及实际测试使用的线程/内存诊断，不再全开 XRT。
冻结编译器、XRT 和 xruntime 未改；历史记录不替代本次固定基线证据。

2026-10-27：29 个文本结果的 C 原型明确为 `char*`，XLang 通过
`@owned string` 直接接管 XRT 分配，不再经过 `point`/`takeCstr`。
样式与颜色入口现在在 C 侧拒绝无效 UTF-8，避免直接 C 调用返回
不符合文本合同的结果；有效文本仍按原格式生成。Windows 默认门禁
与 Linux C ASan/UBSan、XLang JIT/AOT 全通过，并新增代表性分配故障
和生成 C 直接接管检查。校验输入和取得结果长度仍会扫描字节，
不宣称整个调用零开销。

2026-10-26 重建发布物后，Windows/Linux x64 默认包门禁均退出 0：
严格 C、线程隔离、Linux ASan/UBSan、XLang 边界、C/XLang 固定输出、
完整范例与边界独立 EXE AOT，以及 C 单文件范例独立 AOT。
此前共享快照缺符号的记录仅属于旧发布物。签收范围是这些短时
包级路径，不承诺终端实际渲染、所有 Unicode 字素簇或其他架构。

2026-10-24：当前 Windows 发布版默认 `test/check.ps1` 全通过：严格 C、
线程、边界与 C/XLang 同输出、公开范例，以及范例和边界独立 EXE
的 AOT 构建与执行。下文共享链接缺符号的记录属于旧发布物；
Linux 验收仍以独立平台门禁为准。

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

`displayWidth` 忽略 CSI/OSC，按 UTF-8 标量估算**单行**占用列数：常见组合附加符、零宽空格/连接符与变体选择符为零列，东亚宽字符和常见 emoji 为两列。它不是完整 Unicode grapheme/终端排版引擎：ZWJ 复合 emoji、区域旗帜、East Asian Ambiguous 字符及终端实现差异可能与真实显示不同。制表符、换行、U+2028/U+2029、控制字符、U+00AD 软连字符、非法 UTF-8、双向控制符或畸形转义序列报错；标题和 OSC 载荷同样拒绝 Unicode 行分隔符和双向控制符，OSC 载荷中的 DEL/C1 控制字符也拒绝。`strip` 只剥离 ANSI 序列，不负责清洗普通文本里的 Unicode 格式字符。

## C 互操作

公开头文件为 `include/ansi_native.h`。文本函数返回独立分配的 NUL 结尾 `char*`，C 调用方使用后以 `xrtFree` 释放；XLang 以 `@owned string` 直接接管。失败返回 `NULL`，用 `xrtGetError` 读取原因。样式及颜色函数拒绝无效 UTF-8；传入 `NULL` 文本仍按空串处理。`xansi_display_width` 失败返回 -1。库函数不共享可变结果槽，重复和多线程调用不会覆盖先前结果。

公开 C 调用会清除本线程上一次的 XRT 错误；成功返回时 `xrtGetError()` 为 `NULL`。前一次调用失败后可直接调用其他 ANSI 入口，无需调用方先手动清错。包级 C 门禁以四个工作线程交错验证独立结果、UTF-8 列宽与错误槽隔离。

## 范例与验证

2026-10-25：C/XLang 负例确认 `displayWidth` 原把软连字符算作固定一列；
现在拒绝 U+00AD，避免终端换行行为改变列宽。Windows 默认门禁
与 Linux C ASan/UBSan、XLang 边界和双语门禁复验此限制。

- C/XLang 对照：`release/extlibs/ansi/examples/ansi_c.c`、`ansi.xl`。C 范例可直接 `xl run` 单文件执行；普通 C 构建仍独立链接 `ansi_native.c`。
- 对照范例按原始转义字节校验所有公开构件（样式、颜色、光标和屏幕控制），同时展示 UTF-8 宽度与去除转义；验证不依赖终端渲染或颜色配置。
- 完整展示：`release/examples/extlibs/ansi.xl`
- 边界：`release/extlibs/ansi/test/test_ansi.c`、`test_threads.c`、`boundaries.xl`、`allocations.xl`、`input_budgets.xl`、`input_budgets_c.c`、`owned_probe.xl` 与 `test/cases/extlib_ansi.xl`

在 `release` 运行 `xl run extlibs/ansi/test/boundaries.xl`；在仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package ansi -Source release/extlibs/ansi/test/test_ansi.c`。2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、XL JIT 和双语范例对照；2026-10-13 增加四种文本入口的 NUL 红绿用例，复跑 C/XL 主路径与范例。

Windows 包级门禁在仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/ansi/test/check.ps1`；它编译并运行严格 GCC 的 C 边界、线程测试与范例，运行 XLang 边界和双语范例，并与 `test/ansi.expected.txt` 的八行输出比较。默认独立构建并运行中央核心、包内边界、旧 OOM、新语言预算、接管夹具、新 C 预算、包内 C/XLang 范例及公开范例共九条 AOT 路径；`-SkipAot` 仅供日常快测，不用于签收。Linux 对应门禁为 `bash release/extlibs/ansi/test/check.sh`，包含 C ASan/UBSan 与相同的独立 AOT 路径。

两门禁使用本次独占目录和子进程超时，前后核对包/运行库/编译器/公开头/范例等输入指纹；新增或变化的输入使结果无效。Windows 保存同名目录外 JSON（命令、原始输出、状态、输入），Linux 保存 `.report.txt` 与 `.inputs.sha256`，命令失败时另保留工作目录中的 `failure.txt`。成功才删除本次中间产物，失败保留现场；缺编译器会非零退出，不视为跳过。Linux 建议复制所需文件到原生目录执行并核对原位置与快照哈希，避免把 DrvFS 行为误作产品问题。

2026-10-14 补失败→成功的 C 错误槽回归：样式、控制序列、标题、剥离与宽度计算均不会保留旧错误。Windows 严格 GCC、C/XL 边界、双语范例及包内 `test/check.sh` 的 Linux ASan/UBSan 门禁通过；没有改变 ANSI 输出格式。

2026-10-15：`displayWidth` 原会跳过 OSC 载荷中的非法 UTF-8 或 DEL/C1 控制字符并返回正常宽度，和单行安全文本约束不一致。BEL 与 ESC `\\` 两种结束符的 C 负例先红后绿，共用 OSC 识别器现按 UTF-8 标量校验载荷；含中文标题的正例保持通过。Windows 严格 GCC、C/XLang 边界、C/XLang 范例同输出和 Linux ASan/UBSan 通过；完整 AOT 阻断不变。

历史阻断：旧发布物构建边界 AOT 时曾缺少共享符号 `xl6ValueDataSnapshotV1`，见 `dev/extlib_blockers/text_snapshot_link.md`；2026-10-26 两平台默认门禁已验证不再复现。

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

2026-10-24：重新运行 Windows `test/check.ps1 -SkipAot` 和 Linux
`test/check.sh`，严格 C、线程、包内边界、C 单文件、XLang 范例及双语
输出均通过，Linux 同时通过 ASan/UBSan。Windows 默认门禁继续在
公开范例 AOT 链接 `xl6ValueDataSnapshotV1` 时非零退出；因此这次只
签收 C/JIT 与 Linux sanitizer 路径，不将整包标为发布级通过。
