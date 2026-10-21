# fuzzy

`fuzzy` 提供 Unicode 编辑距离、整数相似度、子序列评分和换行分隔候选中的最佳选择。编辑距离和比率直接复用 XRT `xrtUtf8Distance`，按 Unicode 标量而非 UTF-8 字节计算；`score` / `best` 的子序列评分也按标量前进，但**只对 ASCII 字母忽略大小写**，其他字符要求精确相等。不进行 Unicode 规范化或完整大小写折叠。

## API

- `levenshtein(a, b)`：最少插入、删除、替换次数。
- `ratio(a, b)`：`0..100` 整数，按 `floor((maxLen-distance)*100/maxLen)` 计算；两个空串为 100。
- `score(pattern, text)`：匹配分数；不构成子序列时为 -1。连续命中优于拆散命中，完整词优于仅有相同前缀的长文本；开头或分隔符后的命中也有加分。分数只用于排序，不保证固定数值。
- `isMatch(pattern, text)`：是否是子序列；空模式匹配任何文本。
- `best(pattern, candidates)`：从换行分隔列表中选最高分；精确词会优于拆散或更长的前缀候选，并列取首项，完全不匹配返回空串。仅在 CRLF 行尾去除 `\r`；末尾没有 `\n` 的孤立 `\r` 属于候选内容。

## 边界与所有权

`levenshtein` / `ratio` 每侧最多 4096 个 Unicode 标量，使用 XRT 的 O(m×n) 编辑距离算法。`score` 的模式和文本各最多 1 MiB；`best` 的模式/单个候选最多 1 MiB，总列表最多 8 MiB。输入必须是有效 UTF-8；超限或错误抛 `ValueError`，不静默截断。C 字符串 ABI 不支持嵌入 NUL；XLang 的全部文本入口会在调用 C 前拒绝 NUL，而不是只处理前缀。

C 接口在 `include/fuzzy_native.h`：三个整数结果均为 `int64_t`，与 XLang `int` 的 C ABI 在 Windows/Linux 上一致。编辑距离/比率失败为 -1；评分失败为 -2，不匹配为 -1。`xfuzzy_best` 返回独立 XRT 字符串，调用方用 `xrtFree` 释放；失败返回 `NULL`。XLang 封装自动接管所有权。无静态共享缓冲，可并发调用。

C 字符串入口会在各自字节上限内寻找 NUL，再建立视图；超过预算直接报 `XERR_RANGE`，不会先执行无界 `strlen`。C 调用方仍须提供有效、可读取的字符串内存；有界扫描不能使任意野指针安全。

四个公开 C 入口在调用开始时清理本线程上一次的 XRT 错误；成功结果及正常不匹配时 `xrtGetError()` 为 `NULL`。输入或内存失败后可直接重试，无需手动清错。

## 验证与范例

- XLang：`release/examples/extlibs/fuzzy.xl`
- 同输出 C：`release/extlibs/fuzzy/examples/fuzzy_c.c`，可直接用 `xl run` 单文件执行；正式 C 构建仍独立链接 `fuzzy_native.c`。
- 边界：包内 `test/test_fuzzy.c`、`test/test_threads.c`、`test/boundaries.xl`，以及 `demo6/test/cases/extlib_fuzzy.xl`

在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/fuzzy/test/check.ps1`，一次复验严格 C、三个现有 C 测试、XLang 边界、双语同输出、无输出核心 AOT 及带输出范例 AOT/C 对照；`-SkipAot` 只运行 C/JIT 聚焦门禁。Linux 对应入口为 `bash release/extlibs/fuzzy/test/check.sh`，包含 C ASan/UBSan 和四线程回归。需要单独定位时，可在 `demo6/release` 运行 `xl run extlibs/fuzzy/test/boundaries.xl`。

2026-10-22：Windows 默认门禁加入完整范例 AOT 构建、运行及 C 输出比较；两个 AOT 产物放在本次独占目录，成功或失败均清理。`-SkipAot` 严格 C、C/XLang 范例与语言边界通过，默认门禁核心 AOT 通过；完整范例构建仍因共享 `xl6ValueDataSnapshotV1` 缺符号而失败，新增的运行/输出比较尚未执行，整包不能签收发布级。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、XL JIT、C/XL 同输出范例及 `test/cases/extlib_fuzzy.xl` 无输出核心 AOT。2026-10-13 补四条 NUL 红绿用例，C/XL 边界及双语范例复跑通过。`xl build extlibs/fuzzy/test/boundaries.xl -o ../build/test/fuzzy_xl_test.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 补单候选恰好 1 MiB 成功、超 1 字节拒绝的 C/XLang 回归，以及 C 结果分配失败后可恢复调用的故障注入；定向边界与并发测试通过。

2026-10-14 补失败→成功、失败→正常不匹配的 C 错误槽红绿回归，新增包内 `test/check.sh` Linux ASan/UBSan 门禁。Windows 严格 GCC、C/XL 边界与双语范例、Linux sanitizer、无输出 AOT 核心通过；带输出 AOT 主线阻断不变。

2026-10-16：Linux XLang 导入曾在 `xfuzzy_levenshtein` 的 `long long` 声明处与生成的 `int64_t` 原型冲突；公开 ABI 统一为 `int64_t`，三个整数入口都纳入严格头文件契约检查。完整 AOT 阻断不变。

同日修复 C 范例三个 `int64_t` 结果在 LP64 下误用 `%lld` 的严格 GCC
编译失败，统一使用 `PRId64`。Linux 门禁现运行 C ASan/UBSan、包内 XLang
边界，并逐字比较 C/XLang 范例；Windows 严格 GCC 语法也通过。
完整 AOT 仍受共享缺符号阻断。

2026-10-17：`best` 曾把最后一个候选末尾的孤立 `\r` 误作 CRLF 的一部分删除；C 负例先红后绿，现仅在实际 `\n` 前剥离 `\r`。Windows C/XLang 边界、Linux ASan/UBSan 与双语对照通过；完整 AOT 阻断不变。

2026-10-18：ASan 用恰好 `上限 + 1` 字节、尚无 NUL 的缓冲区，先复现四个 C 入口在限额检查前调用 `strlen` 越界读；现在共用有界视图创建，保留合法边界与既有错误类别。独立反例、原有 C 测试、Windows C、Linux ASan/UBSan、XLang 边界和双语对照通过；完整 AOT 仍缺共享快照符号。

2026-10-19：新增 Windows 包内单入口门禁，并将现有四线程测试纳入
Linux ASan/UBSan 门禁。Windows 严格 GCC、三组 C、中央与包内 XLang、
双语同输出及无输出核心 AOT，Linux C ASan/UBSan、四线程、XLang 与
双语对照均通过；匹配算法未改。带输出的完整 AOT 仍受共享符号阻断。

2026-10-19 后续：C 测试以独立的字符索引动态规划参照核对 2048 组
确定性短字符串，混合 ASCII、中文和非 BMP 字符，逐组检查编辑距离
与整数 `ratio`；XLang 增加一条三字符重排的对照值。Windows `-SkipAot`
联合门禁及 Linux 严格 C、ASan/UBSan、线程、XLang 与双语输出均通过。
这不验证 `score/best` 的完整排序，也不构成长输入性能证明；库实现未改，
完整带输出 AOT 仍受共享缺符号阻断。

2026-10-19 单文件范例：原 C 范例仅在包运行器附加原生源后可运行，
`xl run release/extlibs/fuzzy/examples/fuzzy_c.c` 会报 `xfuzzy_match` 缺符号。
现在只在 TCC 单文件路径包含包实现，普通 GCC 构建继续独立链接。
Windows 包门禁核对三条范例路径，Linux ASan/UBSan 门禁同步核对
单文件 C、分离编译 C 与 XLang 输出；完整带输出 AOT 阻断未变。

2026-10-20：C/XLang 红例确认 `score("abc", "a_b_c")` 原高于完整词
`"abc"`，`best` 因而可能选错候选。连续命中权重现高于一次分隔符加分，
完整词对相同前缀的长候选再领先一分；仍只进行一次线性子序列扫描，
不承诺全局最优对齐。两种语言分别验证完整词、拆散词与长前缀顺序，
并在成对范例展示选择结果。Windows 严格 C、C/XLang 边界与范例门禁，
Linux C ASan/UBSan、线程和双语对照均通过；完整带输出 AOT 阻断不变。

同日 C 测试追加 512 组固定种子候选列表，以逐项公开 `score` 的最高分和
并列首项为独立选择参照，交错覆盖 LF/CRLF、末尾换行、空候选和无匹配。
Windows 包门禁及 Linux C ASan/UBSan、XLang 对照均通过；这验证选择器
与当前评分规则一致，不证明评分本身是全局最优对齐。
