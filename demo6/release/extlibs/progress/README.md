# progress

`progress` 管理任务总量、当前量与单行进度文本。它只负责模型和字符串渲染，不读取终端宽度、不自动输出、也不使用光标控制序列；应用自行决定是打印、覆盖当前行还是写日志，因此在服务端、重定向与无终端环境中行为一致。

## XLang API

`progress.create(total)` 创建 `ProgressBar`，`total` 必须为正整数。对象提供：

`ProgressBar` 是导出类型，可写 `progress.ProgressBar bar = progress.create(10)`；也可使用 `auto`。

- `advance(count = 1)`：非负步进，超过总量时饱和到总量；负数抛 `ValueError` 且不修改状态。
- `set(current)`：设置当前量，钳制到 `[0, total]`。
- `setPrefix(text)`：设置最多 1024 字节的有效 UTF-8 前缀；拒绝 C0/C1 控制字符、DEL、Unicode 行分隔符及常见不可见格式符和双向控制符，保持单行纯文本输出。
- `setWidth(width)`：柱体宽度 10 到 100，越界抛 `ValueError`，旧宽度保持不变。
- `current`、`total`、`percent`、`done`：只读属性；未达到总量时百分比最多 99，达到时为 100。
- `render()`：返回独立字符串，例如 `Processing [====>     ]  50% (5/10)`；多次调用互不覆盖。

对象销毁后调用成员会抛 `RuntimeError`。计数采用有界计算，不会因接近 64 位上限而执行溢出的 `current + count` 或 `current * 100`。
`percent` 与进度格数按 `floor(current * scale / total)` 的整数阈值计算；接近 `int64` 上限时也不会因平台 `long double` 精度而提前跨过百分比边界。

## C API 与所有权

声明见 `include/progress_native.h`。`xprog_create` 返回 XRT 分配的句柄，调用方用 `xprog_destroy`；`xprog_render` 返回独立的 NUL 结尾字符串，调用方用 `xrtFree`。原生布尔操作返回 `1/0` 并在失败时设置 XRT 错误；`xprog_percent` 返回 `-1` 表示无效句柄。`xprog_is_done`、`xprog_current`、`xprog_total` 遇到空句柄也设置 XRT 错误，C 调用方可区分无效句柄与正常的零值。前缀长度使用 `_n` 接口，避免内嵌 NUL 静默截断。不同对象没有共享渲染缓冲。

总数、步进与当前值的 C 接口使用 `int64_t`，与 XLang `int` 的 ABI 一致，包括 Linux LP64 平台。

公开 C 操作在开始时清除本线程旧 XRT 错误；成功调用不保留前一次失败的错误。`xprog_destroy` 是资源释放操作，不清除调用方已有的诊断。

## 范例与验证

2026-10-22：C 边界再补 512 组全 `int64` 范围的固定种子输入，
含 `INT64_MAX` 的零进度、半程、末格与完成状态。独立参照算法逐次累加余数，
不计算可能溢出的 `current * scale`，逐格核对随机栏宽的渲染结果和百分比。
Windows 聚焦门禁及 Linux C ASan/UBSan、XLang 联合门禁通过；
完整带输出 AOT 的共享缺符号阻断未变。

2026-10-22：C 边界新增 512 组固定种子的总量、当前值和栏宽组合，
以有界整数算术独立核对百分比、完成状态及渲染柱体的每一格。
Windows 聚焦门禁和 Linux C ASan/UBSan、XLang 联合门禁均通过；
完整带输出 AOT 仍受共享缺符号阻断。

- 同输出 XLang/C 范例：`release/examples/extlibs/progress.xl`、`release/examples/extlibs/progress_c.c`，两者均可用发布目录中的 `xl.exe run` 执行。
- C 边界：`release/extlibs/progress/test/test_progress.c`
- XLang 边界：`release/extlibs/progress/test/boundaries.xl`、`release/extlibs/progress/test/allocation_oom.xl`、`demo6/test/cases/extlib_progress.xl`
- `xl check extlibs/progress/test/private_native_negative.xl` 应因类内原生入口不可由导入方访问而失败；该负例只编译、不运行。

在 `demo6/release` 执行 `.\xl.exe run extlibs/progress/test/boundaries.xl`；在 `demo6` 执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package progress -Source release/extlibs/progress/test/test_progress.c`。测试覆盖极大总量、饱和步进、非法前缀、UTF-8、宽度边界和独立结果。

Linux/WSL 可在 `demo6` 目录运行 `bash release/extlibs/progress/test/check.sh`。脚本以严格 GCC 和 ASan/UBSan 运行 C 边界及 C 范例，同时执行中央/包内 XLang 用例，并逐字节比较 C/XLang 范例输出。

Windows 可在 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/progress/test/check.ps1`。脚本执行严格 GCC、C/XLang 边界、分配故障、私有原生入口负例、双语范例同输出、边界 AOT 和完整范例 AOT/C 输出对照；`-SkipAot` 可只运行动态路径。每次门禁使用独占构建目录并在退出时清理，不写入 `release/tmp`。

2026-10-12：Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与双语范例通过；`test/cases/extlib_progress.xl` 和包内 `test/boundaries.xl` 的无输出 AOT 均构建运行成功。2026-10-14：补齐只读 C API 空句柄错误信号及定向回归。带输出范例 AOT 仍因共享 `xl6ValueDataSnapshotV1` 缺失失败，发布验收未完成。

2026-10-14 追加：原前缀校验允许 tab、ESC 与 Unicode 行分隔符进入渲染结果；C/XLang 红绿边界后改为按 XRT UTF-8 标量拒绝这些控制字符。Windows 严格 GCC、C/XLang 边界与同输出范例、包内新增 Linux ASan/UBSan 门禁通过。完整带输出 AOT 的主线阻断不变。

2026-10-14 追加：公开 C 入口先失败后成功仍保留旧错误；逐入口连续调用回归先红后绿。Windows C/XLang 边界、Linux ASan/UBSan、C/XLang 同输出范例已复跑通过；运行本地 `release/xl.exe` 以免系统 PATH 指向另一份发行物。完整带输出 AOT 阻断不变。

2026-10-15：包清单显式声明所需 XRT Unicode 模块，原生源码的模块宏改为条件定义。中央无输出 AOT 用例构建并运行通过，产物由 3,670,528 字节降至 691,712 字节；C/XLang 边界、双语同输出范例及 Linux ASan/UBSan 门禁复跑通过。完整带输出 AOT 仍缺主线符号 `xl6ValueDataSnapshotV1`，不据此签收发布级验收。

2026-10-17：补前缀不可见格式符边界。U+200B 的 C 负例先红后绿，
U+2060/U+FEFF 与 XLang 负例同步固定；拒绝后旧前缀及渲染结果不变。
带输出 AOT 仍等待共享符号修复。

2026-10-16：`LLONG_MAX` 总量下 1–99% 的每个阈值前后一格 C 用例先复现旧浮点实现提前进位，现改用无溢出的整数阈值比较；XLang 补 50% 精确边界。Windows C/XLang、Linux ASan/UBSan、包内边界 AOT 和双语范例同输出通过；带输出范例 AOT 阻断未变。

2026-10-16：Linux XLang 导入复现公开 `long long` 与生成的 `int64_t` 原型不一致；五个受影响的 C 入口改为 `int64_t`，补独立 ABI 头文件门禁。Windows/Linux XLang、Windows C、Linux ASan/UBSan 和双语范例复验通过；带输出 AOT 的主线阻断未变。

2026-10-17：C 范例在设置前缀或宽度失败时补析构，避免泄漏已创建的句柄；Linux 包级门禁合并 C ASan/UBSan、中央/包内 XLang 边界和双语范例逐字节对照，同次通过。大整数比例算法未改；完整带输出 AOT 共享阻断不变。

同日 C/XLang 负例先复现 U+202E 被前缀接受；现拒绝双向控制符，失败时旧前缀保持不变。Windows C/XLang 边界与 Linux 包级门禁通过，完整带输出 AOT 阻断未变。

2026-10-17：补 C/XLang 分配故障回归，覆盖构造的两次分配及前缀替换的失败、错误分类、旧状态保持和失败后恢复；新用例已登记到包覆盖清单并接入 Linux 门禁。Windows C/XLang 定向运行及 Linux C ASan/UBSan、XLang、双语门禁通过。原实现无需修改；带输出 AOT 的共享缺符号阻断仍在。

2026-10-18：`setPrefixNative(text,size)` 原可从导入方调用并伪造长度，
`renderNative()` 也直接返回需由包装层释放的指针。编译负例先确认可达，
现九个 `*Native` 成员均仅供类内部调用；公开属性及方法保持原样。
Windows XLang 边界/分配故障、Linux 严格 GCC、C ASan/UBSan、
XLang 与双语同输出门禁通过。完整带输出 AOT 共享阻断未变。

2026-10-18：新增 Windows 单入口包门禁；严格 GCC、C 边界、中央及包内 XLang 边界、分配故障、私有入口负例、双语同输出、无输出 AOT 均实跑通过。带输出 AOT 仍受共享缺符号阻断，不据此签收完整发布级验收。

2026-10-20：默认 Windows 门禁在边界 AOT 后增加完整 XLang 范例的构建、运行和 C 输出对照。`-SkipAot` 的 C/XLang 聚焦检查通过；默认门禁边界 AOT 通过后，完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺符号非零退出，新增的运行/对照阶段尚未执行到。成功与失败均无本次独占目录残留，包实现未改；不签收整包发布级。

2026-10-21：为十档进度范例增加独立的固定输出
`test/progress.expected.txt`。Windows/Linux 包级门禁现在分别要求 C 与
XLang 范例匹配该结果，再比较双语输出；Windows 完整 AOT 分支也使用
同一预期。两平台 C/XLang 聚焦门禁通过，Linux 包含 ASan/UBSan。
渲染实现未改，完整 AOT 仍受共享缺符号阻断，不签收整包发布级。

2026-10-21：补 `render()` 分配失败的 C/XLang 双侧回归，确认 C 保留 `XERR_MEMORY`、XLang 抛 `MemoryError`，解除故障后同一进度对象仍可读取并渲染。Windows `-SkipAot` 和 Linux C ASan/UBSan、XLang 联合门禁通过；实现无需修改，完整 AOT 的共享缺符号阻断仍在。
