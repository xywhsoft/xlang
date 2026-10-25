# md4c

2026-10-26 新发布物复验：Windows 默认 `test/check.ps1` 和 Linux
`test/check.sh` 均通过；严格 C、Linux ASan/UBSan、语言边界与分配故障
JIT/AOT、公开 XLang 范例 AOT 及 C 单文件范例 AOT 都已覆盖。C 范例
经 GCC、TCC 和 `xl build` 运行的 HTML 逐字节一致。渲染结果仍由
`takeCstr` 直接接管 XRT 分配，无需改写为 `cstring`。这仅签收文中
MD4C 渲染能力，不把 `no_html_flag` 当成完整 HTML 净化策略。

2026-10-25 使用本日标准 `build.bat` 新生成的 Windows 发布版复验：
默认 `test/check.ps1` 的严格 C、C 边界与分配故障、C/XLang 同输出、
公开范例以及边界和范例的独立 EXE 构建运行均通过。三个 HTML 返回
仍由 `takeCstr` 直接接管 XRT 分配，不因编译器支持 `cstring` 导出而
改写已有的零拷贝路径。使用当前 Linux 发布版补跑 `test/check.sh`：
C ASan/UBSan、XLang JIT、边界与范例的独立 EXE AOT 均通过，范例
HTML 在 C、JIT、AOT 三条路径上逐字节一致。

基于随包携带的 md4c 0.5.2，将 CommonMark/GitHub 方言文本渲染为 HTML。`md4c.html(markdown)` 使用 CommonMark；`html_flags(markdown, parserFlags, rendererFlags)` 可选择 GitHub 方言、禁用原生 HTML、XHTML 输出等；`file_html(path, parserFlags, rendererFlags)` 读取 UTF-8 路径对应的 Markdown 文件。相关标志可通过 `github_flags`、`no_html_flag` 等函数取得。

C 接口见 `md4c_native.h`：渲染函数返回调用方拥有的结果，用 `xrtFree` 释放；失败返回 `NULL`，可用线程内 `xmd4c_last_error()` 查错。XLang 包装层在失败时抛出异常；成功结果会被接管并释放，不占用全局轮换缓冲。文本和路径含内嵌 NUL 时，XLang 入口抛出 `ValueError`；文件内容含内嵌 NUL 时，C 返回 `NULL` 并说明原因，不静默截断。C 字符串入口无法表达内嵌 NUL 的完整输入，调用方须传入正常终止的字符串。
`html_flags` 和 `file_html` 只接受随包 md4c 0.5.2 实际定义的解析/渲染标志位；负数或未知位在扫描正文、读取文件之前报 `XERR_VALUE` / `ValueError`，不会静默忽略。合法组合仍可按位合成。

Markdown 输入必须是合法 UTF-8；内存与文件入口都会在解析前检查，非法内容返回 `XERR_VALUE`，合法多字节文本正常保留。输入上限为 64 MiB。文件入口由 XRT 在整文件读取前检查大小，超过上限返回 `XERR_RANGE`，不会先把大文件读入内存；内存文本入口也采用相同上限，C 字符串入口先在预算内扫描长度，超限时不再无限扫描。C 调用方仍需传入有效的 NUL 结尾字符串。
`last_error` 直接借用 XRT 当前线程的结构化错误消息，下一次操作可能改变它；
需要保留时由 C 调用方自行复制。文件读取失败保留 XRT 原始诊断，缺失诊断时补
`XERR_IO`；非法内容及内存失败分别标记为 `XERR_VALUE`、`XERR_MEMORY`。
成功渲染会清除旧错误，失败后可立即重试。

渲染原始 HTML 默认允许；处理不可信内容时，必须按应用需求选择 `no_html_flag` 并另行审查链接与图片 URL。该标志不是完整 HTML 安全策略，也不是 HTML sanitizer。

双语范例：`release/extlibs/md4c/examples/md4c.xl` 与 `md4c_c.c`。`release/examples/extlibs/md4c.xl` 使用只读测试夹具演示文件渲染，不产生输出文件。包级门禁：在 `demo6` 目录执行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/md4c/test/check.ps1`；Linux 运行 `bash release/extlibs/md4c/test/check.sh`。
2026-10-24 使用更新后的 Windows 发布版复验：默认门禁的严格 GCC、C/XLang 边界、分配故障、双语 HTML 对照和公开范例 AOT 全部通过；Linux C ASan/UBSan、XLang 边界及同输出门禁通过。旧版 `xl6ValueDataSnapshotV1` 缺符号已解除；这是包级验证，不等于整个 xlang 发布版签收。
`md4c_c.c` 自包含上游渲染器与包内桥接源码，可直接运行
`xl run release/extlibs/md4c/examples/md4c_c.c`；正式包仍按清单分别编译。
包 manifest 声明 XRT 缓冲、整文件读取和 UTF-8 校验模块；先前同一边界 AOT 从
3,769,856 缩至 849,920 字节，执行通过。Linux 脚本只编译一次 XRT
再分别链接测试和范例，并构建运行边界与范例 AOT；Windows/Linux
包级内存与错误恢复门禁均通过。

2026-10-24：使用更新后的 Windows 发布版 `xl.exe` 执行默认
`test/check.ps1`，严格 C、C 边界、内存故障注入、直接运行 C 范例、
XLang 边界、C/XLang 同输出及完整范例 AOT 均通过。三个 HTML 返回
仍由 `takeCstr` 直接接管 XRT 分配，不需要为了新 `cstring` 语法重写；
版本号与错误文本是借用结果，继续按借用合同复制。本次只签收该包的
Windows 门禁，不把先前 Linux 结果冒充为本次复验。

以下为历史验证记录，其中共享缺符号均指当时的旧发布二进制。

2026-10-15：中央 `test/cases/extlib_md4c.xl` 现使用包内固定夹具，不再为测试写文件；缺失文件按公开 `IOError` 合同验证。中央 JIT 与包内非 AOT 门禁通过。中央带输出 AOT 当时仍缺共享 `xl6ValueDataSnapshotV1`，发布验收未完成。

2026-10-16：包内 `test/boundaries.xl` 与包级门禁统一以 `demo6` 为工作目录，
固定夹具路径不再依赖从 `release` 启动；该用例已登记到 `coverage.tests`。
根目录复现先因找不到夹具失败，调整后中央主用例、包内边界和 Windows
`check.ps1 -SkipAot` 的 C/XLang 对照均通过。完整 AOT 阻断不变。

2026-10-16：Linux `test/check.sh` 在 C ASan/UBSan 测试之后运行包内及中央
XLang 边界用例，并将 C/XLang 范例 HTML 逐字节对照；整套门禁通过。
这不解除带输出 AOT 的主线链接阻断。

2026-10-17：C 内存入口先复现非法 UTF-8 被当作成功渲染；现于 md4c 调用前统一校验内存及文件内容，拒绝非法字节并保留 `XERR_VALUE`。新增非法文件内容及合法中文的 C/XLang 对照；Windows 严格 GCC、包级双语、统一目录 smoke 与 Linux ASan/UBSan 均通过。完整 AOT 的共享阻断未解除。

2026-10-18：两个 C 内存入口原先在 64 MiB 上限检查前调用无界 `strlen`。64 MiB + 1 字节无终止符反例先触发 Linux ASan 堆越界读取；现共用有界长度扫描并返回 `XERR_RANGE`。Windows `check.ps1 -SkipAot` 与 Linux C ASan/UBSan、XLang 和双语对照均通过；完整 AOT 的共享阻断仍在。

2026-10-19：C 教学范例补单文件 `xl run` 入口；合并上游 C 编译单元时
仅在范例包含边界清除冲突的内部宏，未改 vendored 源或正式包构建。
严格 GCC 范例编译与运行、TCC 直跑、Windows 定向 XLang 用例和 Linux
C ASan/UBSan/XLang 全门禁通过，Linux 门禁还逐字比较两种 C 运行输出。
本机 Windows 总门禁在重编 XRT 对象时遇 GCC 内存不足，本轮不计通过；
完整带输出 AOT 主线缺符号阻断不变。

2026-10-19：公开 flags 入口的负数和未知位原可进入上游解析器；C
负例先红后绿，现按 vendored md4c 标志掩码在输入扫描或文件读取前
拒绝。XLang 负例验证 `ValueError`，合法 GitHub/XHTML/no-HTML 组合
仍可渲染。当前 Windows `check.ps1 -SkipAot` 的严格 C、XLang 边界、
直接 `xl run` C 和双语范例通过；Linux C ASan/UBSan 与 XLang 联合
门禁通过。此前 Windows XRT 编译内存不足是历史记录，本次复跑已通过；
完整 AOT 的共享缺符号阻断未解除。

2026-10-20：Windows 包级门禁现在每次使用独占编译目录，退出后只清理
本次产物；C 文件负例用 XRT 排他临时目录，不再写固定的
`build/test/md4c_guard` 夹具。Windows `-SkipAot` 与 Linux C
ASan/UBSan、XLang 和双语对照门禁通过。完整带输出 AOT 仍待主线修复。

2026-10-21：C 渲染测试新增逐分配点故障扫描，覆盖普通文档和长输出的
缓冲增长、失败错误分类及故障清除后的重试。XLang 新增 `test/allocations.xl`，
对 `html`、`html_flags`、`file_html` 三个入口验证故障时只抛
`MemoryError`，并登记到包清单及 Windows/Linux 门禁。Windows 严格 C、
XLang、双语对照和 Linux C ASan/UBSan、XLang 门禁均通过；库实现未改，
带输出 AOT 的共享缺符号阻断仍在，整包尚未发布签收。
