# langdetect

2026-10-29：拉丁评分由每个常见词重新扫描全文，改成每个模型扫描
完整 ASCII 词段：69 次词扫描降为七次模型扫描，UTF-8 统计仍单独
执行。没有新增缓存、分配或共享状态；六条 C ABI、十条语言 API、
词表、有限脚本边界、分数与错误合同不变。删去未用的内部枚举及
标量计数，不据源码减少宣称可执行文件必然同比缩小。

新增 44,713 项逐词/重复词表与 Unicode 边界参照，以及 2,071 组
固定输入的五种公开结果/错误状态语料。默认检查固定语料摘要；
`test/corpus_c.c --records` 可逐项输出供独立版本比较，摘要不是密码学
承诺。分配故障检查确认正常评分不分配。C 返回的语言名/代码/脚本
是静态借用文字，语言仍须复制一次获得独立字符串；错误文字另借用
本线程错误槽。专用生成 C 测试检查四条借用结果各复制一次、包装
返回没有第二次正文复制；不误标为 `@owned`，不宣称全链零开销。

默认两平台门禁补严格 manifest 模块编译、六条实际生成原型、参照/
语料、语言边界、C/XLang 黄金输出与十二条独立 AOT。子进程有超时，
输入前后有指纹，失败保留现场，成功只清本次目录，报告留目录外；
Linux 原生 C 另经 ASan/UBSan。测试运行库只启用 Unicode 和内存诊断。
性能测量在 `test/benchmark_c.c` 单独进行，不把机器计时阈值放进
功能门禁，也不将一个本地场景的倍数当作通用保证。

轻量 UTF-8 文本语言**启发式**判断，不是训练过的统计语言模型。日文依靠假名、韩文依靠谚文、中文依靠占主导的汉字；英语、法语、德语、西班牙语、葡萄牙语、意大利语、荷兰语使用有限常见词评分。西里尔/阿拉伯文字可识别脚本，但仅凭脚本无法确认具体语言，此时 `detect` 返回 `unknown` / `code` 返回 `und`。无足够证据或评分并列也返回 `unknown`。

```xlang
import langdetect

assert langdetect.detect("the garden is nice and the sky is blue") == "english"
assert langdetect.detect("こんにちは") == "japanese"
assert langdetect.script("Привет") == "Cyrillic"
```

`confidence` 为内部启发式分数 `0..95`，**不是概率或统计校准值**。在本包已识别的文字范围内，混合文字须由某个脚本严格占半数以上；两种脚本平分时返回 `unknown`，拉丁常见词也不能打破脚本平票。未收录的文字不参与多数统计，因此这不是完整的 Unicode 脚本检测。不做分句检测、地域变体、繁简中文区分。分析输入上限 1 MiB；超限或畸形 UTF-8 的 C 输入返回 `unknown`，详细错误可从 `lastError` 获取。XLang 字符串字面量本身必须是有效 UTF-8，坏字节路径由 C 测试覆盖。内嵌 NUL 按真实字节长度处理，不会被截断。`isValidUtf8` 只验证编码，不进行语言判断。

`isValidUtf8` 对非法 UTF-8 返回 `false`，这是普通布尔结果，不设置错误；每次调用都会清除前一次检测留下的错误。C 接口收到空指针时返回 `false` 并设置参数错误。`lastError` 只报告最近一次原生调用的错误，不应将旧诊断当作当前结果。

拉丁字母分类不把 Latin-1 范围中的乘号 `×` 和除号 `÷` 计为字母；它们可以分隔 ASCII 常见词，例如 `the×` 仍可命中独立的 `the`。这仍是有限词表和脚本范围的启发式规则，不是完整 Unicode 分词。
汉字统计覆盖 Unicode 17 已分配的扩展 A-J 及兼容汉字补充区，包括补充平面的 U+20000、U+2EBF0 和 U+323B0；相邻未分配码位不计为文字证据。这里仍沿用“汉字严格占多数则推断中文”的有限启发式规则，并非任何汉字文本都必然是中文。区间依据 [Unicode 17 CJK 码位表](https://www.unicode.org/irg/docs/n2856-CJKUIv17.pdf)。
平假名和片假名区间中的中点、双连字符、浊音附标与未分配码位不计为假名字母；`・・` 不足以判为日语，`カ・ナ` 中的两个假名字母仍能提供日语证据。组合附标不单独增加假名数量。
阿拉伯文区块中的标点、数字和组合附标也不计为字母；`،،`、`٣٤` 单独出现时脚本为 `Unknown`，`مر٣٤` 中的字母仍可提供 Arabic 脚本证据。西里尔区块的符号及兼容汉字、韩文音节区块中的未分配码位同样不提供脚本证据。这些是本包覆盖范围内的静态字母子区间，不等于完整 Unicode 属性数据库。

## C 接口

`include/langdetect_native.h` 的每个文本入口都接收指针与真实字节长度。`confidence` 返回 `int64_t`，与 XLang `int` 的 C ABI 一致，包括 LP64 平台。`detect` / `code` / `script` / `last_error` 返回借用的静态或 XRT 错误文字，不释放；没有共享可写结果环。文本最长 1 MiB，调用方可先用 `xlangdetect_valid_utf8` 检查原始输入。脚本识别仅覆盖本包列出的 Unicode 范围，不宣称完整 Unicode Script 数据库。

`detect` / `code` / `script` 的静态文字在对应代码模块存活期间有效；
`last_error` 依赖本线程错误对象，后续调用清除/改变错误即可能失效。
需要跨调用保存错误时由 C 自行复制；不要释放借用指针。语言绑定
已经取得自己的副本，不承担该借用生命周期。分别调用 `code` 与
`confidence` 会分别分析输入，本轮不新增缓存或合并分析 API。

## 人工复核队列

`examples/review_queue.xl` 与 `examples/review_queue_c.c` 展示同一项
应用策略：已识别且分数不低于 60 的消息进入语言队列，其余进入
`manual`。阈值由应用选择，不代表统计置信概率；同样覆盖证据不足、
仅能识别脚本和内嵌 NUL。C 最后一条输入使用真实长度而非 `strlen`，
静态返回值不释放。两程序的十行结果固定在 `examples/review_queue.expected.txt`。

在 `release` 下可直接运行：

```text
xl run extlibs/langdetect/examples/review_queue.xl
xl run extlibs/langdetect/examples/review_queue_c.c
```

已有 `examples/langdetect.c` 与 `release/examples/extlibs/langdetect.xl`
仍覆盖全部十条语言入口；队列例是组合用法，不替代逐 API 范例。

## 性能复核

`test/benchmark_c.c` 测同一个普通 C `detect` 入口，含短句、长词段、
密集常见词、分隔符、UTF-8 边界、汉字与接近 1 MiB 的输入；输出
用 `clock()` 计时及结果校验和。比较版本须使用相同编译器/优化选项/
XRT/输入/循环次数，交错运行并比较多次中位数，先核对结果相等。
未插桩的性能程序与 sanitizer 功能测试分开，不比较不同运行库的结果。
测量不代表 XLang 整程序或不同架构的吞吐；CJK 不走拉丁评分路径。

## 验证

2026-10-26：当前 Windows/Linux 发布物的默认包门禁均退出 0，覆盖严格 C、
Linux ASan/UBSan、语言中央与边界 JIT/AOT、核心独立 EXE，以及公开
C/XLang 范例的 JIT/AOT 同输出。旧日期的共享 AOT 缺符号仅描述当时发布物，
不再是当前阻断。验收仅限本包列出的脚本范围和词表启发式；
`confidence` 不是概率，也不代表广泛自然语言识别准确率。

2026-10-24：更新后的 Windows 发布版 `xl.exe` 跑通默认
`test/check.ps1`：严格 C 语法、C/XLang 边界、公开双语范例、核心与
完整范例 AOT 均通过。此前带输出 AOT 的共享缺符号阻断仅是旧发布物
历史状态；这次没有重跑 Linux 门禁或扩大启发式识别范围。

2026-10-22：Windows 包级脚本增加完整 AOT 发布门禁：默认构建运行无输出核心，再构建运行公开双语范例的 XLang AOT，并与 C 范例逐行比较；`-SkipAot` 仅做 C/JIT 聚焦快测。当前快测通过，默认门禁核心 AOT 通过、完整范例在共享 `xl6ValueDataSnapshotV1` 未解析处准确失败。每次构建使用独占目录，失败后也清理本次产物；不签收整包发布级。

2026-10-22：C 红例复现补充平面的已分配汉字被漏计；按 Unicode 17 已分配区间补齐分类，保留区间间的未分配空洞。C 边界覆盖 B/C/E/I/G/H/J 与兼容区的端点及相邻空洞，XLang 边界覆盖补充汉字、未分配码位和 Latin 词边界；C/XLang 成对范例新增同一补充汉字样本。Windows 严格 C/XLang 双语门禁和 Linux C ASan/UBSan、XLang/双语门禁通过；完整 AOT 未复验，不签收整包发布级。

2026-10-22：C 红例复现阿拉伯文标点/数字、西里尔符号、韩文与兼容汉字未分配码位被误计为字母；分类器改为只统计各覆盖区间的实际字母，组合附标单独出现也返回 `Unknown`，正常阿拉伯文字及带数字文本保持可识别。Windows 严格 C/XLang 双语门禁和 Linux C ASan/UBSan、XLang/双语门禁通过；完整带输出 AOT 未在本轮复验，不签收整包发布级。

2026-10-22：C/XLang 红例复现 `・・`、`゛゜` 和单个假名加组合附标被误计为足够的日语证据；假名字母范围收紧后，这些输入返回 `unknown`，`カ・ナ` 仍判为日语。Windows 严格 C/XLang 双语包门禁和 Linux C ASan/UBSan、XLang/双语门禁通过；完整带输出 AOT 未在本轮复验，不签收整包发布级。

2026-10-22：C 红例复现 `the×`、`×the`、`the÷` 被漏判为独立英文词，`×÷` 被误记为 Latin 脚本；排除 U+00D7/U+00F7 后，C/XLang 边界与原有重音字母、emoji 用例均通过。Windows 严格 C/XLang 双语包门禁及 Linux C ASan/UBSan、XLang/双语门禁通过；完整带输出 AOT 未在本轮复验，不签收整包发布级。

从仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\langdetect\test\check.ps1 -SkipAot
.\release\xl.exe run test\cases\extlib_langdetect.xl
.\release\xl.exe run test\cases\extlib_langdetect_boundaries.xl
.\release\xl.exe run release\extlibs\langdetect\test\analysis_boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package langdetect -Source release\extlibs\langdetect\test\test_langdetect.c
.\release\xl.exe build release\extlibs\langdetect\test\aot_core.xl -o build\test\langdetect_aot_core.exe
.\build\test\langdetect_aot_core.exe
.\release\xl.exe run release\examples\extlibs\langdetect.xl
.\release\xl.exe run release\extlibs\langdetect\examples\langdetect.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package langdetect -Source release\extlibs\langdetect\examples\langdetect.c
```

Linux/WSL 从仓库根目录运行 `bash release/extlibs/langdetect/test/check.sh`，
用严格 GCC 与 ASan/UBSan 复验 C 边界和范例，并通过 `release/xl` 运行
XLang 包内边界、双语范例逐字对照及独立 C/XLang EXE。

边界测试覆盖 UTF-8、内嵌 NUL、混合脚本、无证据返回值和 1 MiB 限额；Windows 严格 GCC、Linux ASan/UBSan、C/XL 范例及独立 EXE 均通过。不加 `-SkipAot` 会执行完整 Windows 发布门禁；旧发布物的 `xl6ValueDataSnapshotV1` 缺符号记录见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。

2026-10-15：补 `isValidUtf8` 成功/失败后清除旧错误的 C 红绿回归、空指针诊断和 XLang 捕获后恢复用例；C/XLang 范例调整为同输入同输出并逐行核对。显式声明 Unicode 模块后，同一核心 AOT 从 3,556,352 降至 565,248 字节，构建运行通过；Windows C/XLang 与包内 Linux sanitizer 门禁通过。完整 AOT 阻断不变。

2026-10-16：修正 `confidence` 在 Linux LP64 下的 C/XLang ABI 类型不一致；新增严格头文件契约编译检查，并将 NUL/上限/恢复的 XLang 测试登记为包内套件。

同日修复 C 范例用 `%lld` 打印 `int64_t` 置信度而无法通过 LP64 严格
GCC 的问题，改用 `PRId64`；Linux 门禁纳入 C/XLang 范例逐字对照。
Windows 严格 GCC 语法、Linux C ASan/UBSan + XLang 门禁通过，完整 AOT
共享阻断不变。

2026-10-18：常见词原仅检查 ASCII 相邻字节，`theé` / `éthe` 被误计为
独立英语词。现只在词文本匹配后，用 XRT UTF-8 解码相邻标量，并复用本包
已有脚本字母分类；上述两例返回 `unknown`，`the😀` 仍按非字母边界
识别英语。C 负例先红后绿，XLang 边界、Windows 严格 GCC 语法和
Linux C ASan/UBSan、XLang、双语对照门禁通过。词表及非概率分数合同
未扩张，完整 AOT 的共享缺符号阻断不变。

2026-10-19：C 范例可直接用 `xl run` 单文件运行，并在打印成功标记前
核对固定的英、法、德、中、日文及无证据样本。Windows 新增包级
`test/check.ps1`；Linux `test/check.sh` 增加两份中央 XLang 用例和
单文件 C 输出对照。两平台严格 GCC/C、语言边界、双语范例及 Linux
ASan/UBSan 门禁均通过。有限词表/非概率分数合同不变，完整带输出 AOT
的共享主线阻断也未解除。

2026-10-20：C/XLang 负例先复现汉字与韩文平票被误判为韩语，以及汉字与
拉丁常见词平票仍判成英语。脚本与拉丁词表入口现统一要求严格多数；
日文/韩文、汉文/西里尔文、西里尔文/阿拉伯文平票也返回 `unknown`，
三比二的韩文多数保持识别。Windows 严格 GCC、C/XLang 边界和双语范例、
Linux C ASan/UBSan 与 XLang 对照、无输出 AOT 核心均通过。完整带输出
AOT 的共享缺符号阻断不变，不据此签收整包发布级。
