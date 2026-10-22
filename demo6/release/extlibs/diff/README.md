# diff

有界行级 LCS 差异。`diff.lines(oldText, newText)` 返回按原顺序排列的文本行，每行以前缀 ` `（相同）、`-`（删除）或 `+`（新增）开头；`diff.stats` 返回 `新增数,删除数,相同行数`。它**不是**带文件头、hunk 范围的 unified patch，不能直接交给 `patch` 命令应用。

行以 `\n` 分隔，行末 `\r` 会被规范化移除；输出行统一以 `\n` 结束。末尾单独的换行不额外形成空行，中间的空行则保留。按 UTF-8 字节比较整行，不做 Unicode 规范化。C 字符串 ABI 不支持嵌入 NUL 的内容；XLang 两个文本入口会明确拒绝，不静默截成前缀。
两侧输入都必须是有效 UTF-8；C 侧非法字节返回 `NULL` 和 `XERR_VALUE`，不会把非法文本交给 XLang string。长度上限先于 UTF-8 扫描检查，超限仍报告 `XERR_RANGE`。

## 资源边界

- 每侧输入最多 8 MiB、2048 行；输出由输入限额约束，不超过 16 MiB 加 8192 字节的行前缀与换行。超限或分配失败明确报错，不返回静默截断结果。
- C 接口仍要求有效的 NUL 结尾字符串；长度扫描最多检查到预算边界的下一字节，不会为了判断超限而无限扫描。恰好 8 MiB 的合法输入可用，超过预算则报 `XERR_RANGE`。
- C 的任一输入指针为 `NULL` 时报告 `XERR_ARGUMENT`，不将缺失参数当作有效空字符串；`""` 仍表示零行文本。
- 使用完整 LCS 表，时间 O(m×n)、内存 O(m×n)，m/n 是两侧行数。适合配置、源代码片段和中小文本，不是 GB 文件或流式 diff 引擎。
- 行视图按 XRT `xrtHash64` 预筛，哈希相等时仍做逐字节比较，避免碰撞造成错误匹配。

## C 接口和所有权

公开声明见 `include/diff_native.h`。`xdiff_lines` / `xdiff_stats` 返回独立分配的 NUL 结尾字符串指针，C 调用方转成 `char*` 使用后必须 `xrtFree`。失败返回 `NULL`，通过 `xrtGetError` 获取原因。XLang 包封装自动接管文本所有权，失败抛异常。函数没有静态结果缓冲，连续或并发调用不会覆盖旧结果。

两个公开 C 入口在每次调用开始时清理上一次的 XRT 线程错误；成功返回时 `xrtGetError()` 为 `NULL`。超限或内存失败后可直接调用另一入口，无需手动清错。

## 范例和验证

2026-10-22：C 边界补齐两侧同时达到 2048 行的完整 LCS 表用例，
核对统计结果及首尾输出行。Windows 聚焦门禁和 Linux C ASan/UBSan、
XLang 双语门禁均通过；完整范例 AOT 仍被共享缺符号阻断。

2026-10-21：`test/test_unterminated.c` 的关键边界调用位于 `assert` 内；现即使编译参数包含 `-DNDEBUG`，断言及其中的调用仍会执行。Linux 包门禁显式以 `-DNDEBUG` 编译此用例，严格 C、ASan/UBSan、XLang 与双语对照通过；Windows `-SkipAot` 聚焦门禁也通过。库算法未改，完整 AOT 的共享缺符号阻断仍在。

- XLang：`release/examples/extlibs/diff.xl`
- 同输出 C 对照：`release/extlibs/diff/examples/diff_c.c`
- 边界：`release/extlibs/diff/test/test_diff.c`、`test_threads.c`、`boundaries.xl` 与 `test/cases/extlib_diff.xl`

从 `demo6` 根目录可直接运行 `release/xl.exe run release/extlibs/diff/examples/diff_c.c`。
TCC 单文件路径由范例包含包内实现；正式包和严格 GCC 门禁仍分别编译原生源文件。

在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/diff/test/check.ps1`，联合检查严格 GCC 宏构建、C 边界/无终止符缓冲/线程、XLang 用例、三条范例路径的固定输出，以及完整范例 AOT。共享 AOT 链接阻断期间可加 `-SkipAot` 只验 C/JIT，不把它当成发布签收。Linux 上运行 `bash release/extlibs/diff/test/check.sh`，额外使用 ASan/UBSan。也可以单独运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package diff -Source release/extlibs/diff/test/test_diff.c`。2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 双语输出对照及并发调用；2026-10-13 补两条 NUL 红绿用例并复跑 C/XL 边界与范例。
2026-10-16 将包内 `test/boundaries.xl` 登记到 `coverage.tests`，从
`demo6` 工作目录运行通过；本轮未重跑 C/Linux/AOT 门禁。

`test/cases/extlib_diff.xl` 无输出核心 AOT 已构建并运行成功。`xl build extlibs/diff/test/boundaries.xl -o ../build/test/diff_xl_test.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败；完整 AOT 尚未验收。主线修复后重跑，见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 补齐恰好 2048 行的 C/XLang 正例，并对 `lines`、`stats` 的各分配点注入失败：失败必须报内存错误，清除故障后再次调用仍产生完整结果。定向测试通过。

2026-10-14 补超限失败→跨入口成功的错误槽红绿回归，以及包内 `test/check.sh` 的 Linux ASan/UBSan 门禁。Windows 严格 GCC、C/XL 边界与双语范例、Linux sanitizer、无输出 AOT 核心通过；带输出 AOT 主线阻断不变。

2026-10-15：新增 512 组固定种子的短序列对照，逐组从 `lines` 输出还原旧/新文本，用独立穷举子序列核对最短相同行数，并与 `stats` 对账。Windows 严格 GCC/C、XLang 边界、双语范例输出及 Linux ASan/UBSan 均通过；实现无需改动，完整 AOT 主线阻断不变。

2026-10-17：Linux `test/check.sh` 将 C ASan/UBSan 边界、并发测试、
中央和包内 XLang 用例、C/XLang 范例逐字输出对照接为同次门禁，
已通过。完整带输出 AOT 仍受共享缺符号阻断，不作发布签收。

2026-10-18：新增 8 MiB 正边界和 8 MiB + 1 字节无终止符反例。旧实现先调用无界 `strlen`，Linux ASan 复现堆越界读取；现改为预算内扫描。Windows C 定向测试及 Linux C ASan/UBSan、XLang 用例、双语范例输出对照通过。C 接口仍不接受任意无效指针；完整 AOT 共享阻断不变。

2026-10-19：C 教学范例原不能直接 `xl run`，缺少原生实现链接；现仅
在 TCC 单文件路径包含包实现，正式分离编译不变。Windows 直接运行与
包运行器输出一致；Linux 包门禁加入直跑输出对照，严格 GCC、C
ASan/UBSan、线程及 XLang 双语验证通过。完整 AOT 的共享阻断未变。

同日补非法 UTF-8 的输入边界：C 负例先复现 `lines` 把坏字节直接返回，现两条入口在有界测长后复用 XRT Unicode 校验。C 测试固定非法序列、错误分类和随后成功恢复；公开语义仍按有效 UTF-8 的整行字节比较。
Windows C 定向测试、Linux 严格 GCC/ASan/UBSan、线程和 C/XLang 双语门禁通过；新增 Unicode 模块后，无输出核心 AOT 构建运行通过（660,992 字节）。完整带输出 AOT 仍受共享缺符号阻断，未签收发布级。

2026-10-19：新增 Windows `test/check.ps1` 和固定范例预期，检查包运行器 C、直接运行的 C、XLang 三条输出路径；Linux `test/check.sh` 同步核对预期。两平台门禁通过，Linux 含 ASan/UBSan。算法未改，完整带输出 AOT 的共享缺符号阻断未解除，不签收发布级。

2026-10-20：Windows 门禁新增默认的完整范例 AOT 构建、运行和固定输出检查，并为原 C/JIT 路径提供显式 `-SkipAot`。快测通过；默认在 C/JIT 全部通过后，完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺符号非零退出，尚未执行到 AOT 输出检查。产物使用每次独占目录且失败后已清理；实现未改，整包不签收发布级。
