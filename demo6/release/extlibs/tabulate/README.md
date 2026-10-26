# tabulate

2026-10-26：当前发布版的 Windows 默认包门禁新增顶层对象边界 AOT，
在严格 C、线程、XLang JIT 和核心 AOT 通过后，独立 EXE 于退出清理
阶段报 `RuntimeError: operation is not valid in the current state`；
Linux 默认包门禁的 C ASan/UBSan、线程与语言 JIT 亦通过，随后
在同一边界 EXE 打印 PASS 后报同错并退出 1。
Windows/Linux 最小复现均证明顶层 `Table` 的 JIT 正常、独立 EXE
退出失败；详见 `demo6/dev/extlib_blockers/tabulate_aot_top_level.xl`
及 `global_native_class_aot_fini.md`。此包暂不签收完整发布级；
保留红色边界门禁，不在包内改写顶层对象生命周期。

2026-10-25：从当前源码完整重建 Windows 发布版后，默认
`test/check.ps1` 通过严格 C/ABI、线程、XLang 边界与分配故障、
私有入口负例、C/XLang 固定输出及核心/公开范例 AOT。
WSL `test/check.sh` 通过 C ASan/UBSan、线程、XLang 边界与分配故障、
双语输出对照。本轮未改表格实现或 API；此前发布物的 AOT 缺符号
记录仅作历史追溯，不再是当前包的阻断。

`tabulate` 把二维文本排版为纯文本终端表格。公开 XLang API 使用**单元格数组**，例如 `table.addRow(["Alice", "30", "Beijing, China"])`；不再把一行字符串当作伪 CSV 解析。这样空单元格、逗号和前后空格都有明确语义，也不会重复实现 CSV 协议。需要从 CSV 文件构建表格时，请先用独立的 `csv` 扩展库解析，再把字段数组交给本包。

## XLang API

- `tabulate.create()`：创建 `Table`；底层分配失败时抛 `MemoryError`，不返回空句柄对象。`valid` 表示底层句柄仍存在。
- `tabulate.Table`：导出类型，可用 `tabulate.Table table = tabulate.create()` 显式声明。
- `table.setHeaders(array<string> cells)`：替换表头，`[]` 清除表头；替换不删除已有数据行。
- `table.addRow(array<string> cells)`：追加非空行；列数可少于或多于当前表头，缺少的格子按空格显示。
- `table.rowCount` / `table.colCount`：当前数据行数及渲染列数。
- `table.render()`：返回独立的 ASCII 边框文本；空表返回空串，非空结果以换行结束。

单元格必须是有效 UTF-8，不能包含 NUL、换行（包括 Unicode U+2028/U+2029 行与段落分隔符）、制表、ESC 等控制字符，也拒绝常见不可见格式符和双向控制符（如 U+00AD soft hyphen、U+200B、U+202E、U+2066）。每格最多 4096 字节，最多 32 列、256 数据行、1 MiB 已提交单元格文本；渲染输出最多 4 MiB，超限或分配失败显式报错，事务自动回滚，已提交内容不变。此范围面向日志/CLI 的单行文本表格，不是电子表格或富文本排版。

宽度计算将常见汉字、假名、韩文和全角字符视为两列，常见组合附加符及基本/补充变体选择符视为零列，其余字符按一列估算。不同终端对模糊宽度、emoji 字素簇的呈现可能不同；本包不承诺完整 Unicode grapheme/wcwidth 排版，也不接受 ANSI 转义序列。

## C 接口与所有权

公开声明在 `include/tabulate_native.h`。C 便捷接口 `xtab_set_headers(table, cells, count)`、`xtab_add_row(table, cells, count)` 接受 NUL 结尾的字符串数组；底层 `xtab_begin_headers` / `xtab_begin_row`、`xtab_append_cell`、`xtab_commit` 接受显式字节长度，失败自动 `xtab_abort`。`xtab_render` 返回 XRT 独立分配的文本，C 调用方用 `xrtFree`。不同表对象可在不同线程使用；**同一表对象的并发读写需要调用方同步**。

新的公开操作清除当前线程的旧 XRT 错误；合法查询或渲染成功不会携带前一次失败。`xtab_abort` 和析构只负责清理，不清除导致本次事务失败的错误，调用方可在清理后读取它。

## 范例与验证

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C/ABI、边界及独立实例并发、XLang 事务与分配故障、私有原生
入口拒绝、三条范例路径固定输出以及核心/公开范例 AOT 全部通过。
先前的完整 AOT 缺符号记录只对应旧发布物；本次没有重跑 Linux
sanitizer，Unicode 宽度仍遵守上文的有限估算合同。

- XLang：`release/examples/extlibs/tabulate.xl`
- 同输出 C：`release/extlibs/tabulate/examples/tabulate_c.c`

在 `demo6/release` 可直接运行 `xl run extlibs/tabulate/examples/tabulate_c.c`；
普通 C 编译仍按包清单分别链接两个实现单元。
- 边界：包内 `test/test_tabulate.c`、`test/test_threads.c`、`test/boundaries.xl`，以及 `demo6/test/cases/extlib_tabulate.xl`
- `xl check extlibs/tabulate/test/private_native_negative.xl` 应拒绝导入方调用原生单元格追加入口；该负例仅编译，不执行。

在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/tabulate/test/check.ps1`，联合验证严格 GCC 清单宏和 ABI、C/XL 边界与分配故障、私有原生入口编译负例，以及包运行器 C、单文件 C 和 XLang 三条范例路径的固定输出。也可单独运行 `.\release\xl.exe run release/extlibs/tabulate/test/boundaries.xl` 或 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package tabulate -Source release/extlibs/tabulate/test/test_tabulate.c`。测试覆盖空值/逗号、中文宽度、表头替换、异常回滚、最大行数/字段/总预算、独立结果与多线程独立实例。

默认 Windows 包门禁还构建运行无输出核心与公开 XLang 范例 AOT，并将范例输出与固定预期比较；`-SkipAot` 仅运行 C/JIT 聚焦门禁，不代表发布签收。AOT 产物使用每次独占的构建目录，门禁结束时只清理本次目录。

C 测试还注入创建时的首次分配失败，验证返回空句柄且清除故障后可再次创建。

Linux 包级门禁：从 `demo6` 运行 `bash release/extlibs/tabulate/test/check.sh`，以严格 GCC、ASan/UBSan 和泄漏检查执行 C 边界及四线程独立实例测试，同时运行中央、包内 XLang 用例和双语范例逐字输出对照。

公开 C 接口的行列计数返回 `int64_t`，与 XLang `int` 的 ABI 一致；`append_cell` 的长度则保持 C `long long`，对应 XLang `c_longlong`。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与线程测试、双语同输出范例及 `test/cases/extlib_tabulate.xl` 无输出核心 AOT。`xl build extlibs/tabulate/test/boundaries.xl -o ../build/test/tabulate_boundaries.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 修复 C 便捷接口的单元格数组含 `NULL` 时仅回滚、未设置错误的问题。现在 `set_headers` / `add_row` 均报告 `XERR_VALUE`，已提交数据保持不变，失败后仍可继续写入；C/XLang 边界、C 并发及双语范例通过。

2026-10-15：失败后新建、查询、渲染、重启事务原会留下旧 XRT 错误；C 连续调用回归先红后绿。新增单元格复制、行数组扩容、表头替换和渲染扩容的分配故障测试，验证错误分类、事务回滚与重试，并固定 Linux ASan/UBSan 包级脚本。Windows 严格 GCC、C/XLang 边界、四线程测试、双语同输出和无输出核心 AOT（872,448 字节）复跑通过。完整边界 AOT 仍缺 `xl6ValueDataSnapshotV1`，未签收发布级。

2026-10-16：XLang 的 `setHeaders`/`addRow` 将整次构行放入 `try/finally`，
即使中途由语言层抛错也会撤销待提交行。包内用例补错误表头保持旧内容、
超列数回滚与随后继续写入，并登记为 `coverage.tests`。当前 C 边界、
四线程、中央和包内 XLang 用例及双语范例通过；完整 AOT 本轮未重跑。

2026-10-17：C/XLang 负例先复现 U+202E 等终端格式控制符被接受，现于
宽度校验中拒绝常见零宽/双向控制符，失败的行事务不改变表内容。
新增 XLang 创建和追加行的逐点分配故障测试，Windows/Linux 均验证
`MemoryError`、回滚与恢复；该用例已登记到包清单。Linux 联合门禁
（C sanitizer、语言边界、双语范例）通过；完整 AOT 阻断未变。

2026-10-17：C 便捷入口原先用无界 `strlen` 扫描单元格，超过 4096 字节的
未终止输入会在长度校验前越界读取。现最多探测 4097 字节，超限仍按原
`XERR_VALUE` 路径回滚；ASan 负例先红后绿，Linux 包级门禁及 Windows
定向 C 测试通过。显式长度入口和 XLang 语义不变。

2026-10-18：`appendCellNative(text,length)` 原可由导入方伪造长度，
绕过 `setHeaders/addRow` 的事务收尾。编译负例先确认可达，八个
`*Native` 方法现仅供类内部调用，公开表格 API 不变。Windows XLang
边界/分配故障和 Linux 严格 GCC、ASan/UBSan、四线程、XLang 双语对照
通过；完整 AOT 仍受共享快照符号阻断。

2026-10-19：新增 Windows `test/check.ps1` 和中英混排固定预期，合并 C 边界/线程、XLang 边界/故障、私有入口编译负例与三条范例路径；Linux `test/check.sh` 同步固定输出。Windows 与 Linux ASan/UBSan 联合门禁通过。渲染算法未改，完整 AOT 的共享缺符号阻断仍在，不签收发布级。

2026-10-22：补充变体选择符 U+E0100–U+E01EF 原被算作一列，导致含该字符的汉字单元格多补空格。C/XLang 用完整两列表格固定文本先红后绿；Windows 包级严格 C/线程/XLang/范例门禁与 Linux C ASan/UBSan、XLang、双语门禁通过。其他 Unicode 宽度估算边界不变；完整带输出 AOT 仍受共享缺符号阻断，不签收整包发布级。

2026-10-22：Windows 默认包门禁不再只凭 C/JIT 返回全绿，现另构建、运行无输出核心和公开 XLang 范例 AOT，并对照固定输出。`-SkipAot` 的严格 C、线程、语言及三条范例路径通过，Linux C ASan/UBSan、线程、语言与双语门禁通过；默认 Windows 门禁的核心 AOT 通过，范例构建因共享 `xl6ValueDataSnapshotV1` 未解析而准确返回失败。本次独占构建目录已清理，整包仍未签收发布级。

2026-10-23：渲染分配故障从首点扩到多行表格的 C/XLang 各前 64 点，核对 `XERR_MEMORY`/明确的 `MemoryError`、成功输出与基线一致、失败后表格行数不变及解除故障后的恢复。现有创建与追加行用例也改为明确的类型捕获。Windows `test/check.ps1 -SkipAot` 的严格 C/线程/XLang 与 Linux C ASan/UBSan、线程、XLang/双语门禁通过；渲染实现/API 未改，完整 AOT 的共享阻断仍在。
