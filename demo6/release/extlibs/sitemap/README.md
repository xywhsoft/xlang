# sitemap

`sitemap` 生成和解析 Sitemap XML 0.9 的 URL 集合、索引文件，并解析常用 `robots.txt` 规则。XML 使用现有 `xml.sax` / Expat 扩展库作结构解析；本包只负责 sitemap 字段，避免自写 XML 词法器。

## XLang API

- `sitemap.build(urls)`：由对象数组生成 `<urlset>`。每项必须有少于 2,048 个 UTF-8 字符的非空字符串 `loc`，可带字符串 `lastmod`、`changefreq`、`priority`。
- `sitemap.buildIndex(entries)`：生成 `<sitemapindex>`；每项需 `loc`，可带 `lastmod`。
- `sitemap.parse(xml)`、`sitemap.parseIndex(xml)`：严格解析对应根元素，返回对象数组。支持 sitemap 默认命名空间、XML 实体和 UTF-8；拒绝畸形 XML、异名空间字段、DTD、外部实体、内嵌 NUL 及缺失 `loc`。
- `sitemap.robotsParse(text)`：返回 `{groups: [{agents, allow, disallow}], sitemaps}`。空行和注释不结束规则组；同组可连续声明多个 `User-agent`。已有规则后遇到新的 `User-agent` 才开始下一组。
- `sitemap.robotsAllows(robots, productToken, path)`：传入爬虫的 product token（如 `ExampleBot`，不是完整 HTTP User-Agent）；代理组按整个 token 不区分大小写匹配，不把 `Bot` 当作 `NotBot`。相同 token 的规则组合并；没有专属组时回退到 `*`。最长字面路径规则生效，平局由 `Allow` 胜出。支持路径规则中的 `*` 和末尾 `$`；其他位置的 `$` 是普通字符。没有匹配规则则允许。`robots` 应来自 `robotsParse`；手工构造时，每组必须有非空字符串数组 `agents` 及字符串数组 `allow`、`disallow`，非法组或参与决策的非法规则报 `ValueError`，不会当作空规则放行。

robots 匹配以传入路径的 UTF-8 字节为准，不做 URL 解析、百分号编码规范化或爬虫厂商专有扩展解释。适用于站点常规规则决策，**不承诺与所有搜索引擎的完整 robots 实现逐项一致**。XML 只公开上述字段；生成时 URL 条目仅允许 `loc`、`lastmod`、`changefreq`、`priority`，索引条目仅允许 `loc`、`lastmod`，未知键会报错而不会被静默丢弃。图片/视频扩展元素等额外命名空间目前不进入这个包的结构模型，也不校验日期/优先级字段的业务格式或自动拆分超大站点地图。生成与解析均按 [Sitemaps 协议](https://www.sitemaps.org/protocol.html)限制 `loc` 少于 2,048 个字符，但不作完整 URI/IRI 校验。构建时要求字段为有效 UTF-8 且符合 XML 1.0 字符范围，U+FFFE/U+FFFF 会被拒绝。输入格式错误抛 `ValueError`，不再返回空字符串掩盖生成失败。
robots 长规则按 `*` 分段向前匹配；长字面片段用 KMP，单条规则的匹配耗时随规则和路径字节数线性增长。匹配所需内存分配失败时，C 入口返回 `-1` 并保留 `XERR_MEMORY`，不会误报为“允许访问”。
生成 XML 时对字段中的 CR 使用字符引用，避免 XML 解析器将其规范化为 LF；这保证已接受的字段可原值往返，不表示 `loc` 或日期文本经过协议语义校验。
按 [Sitemaps 协议](https://www.sitemaps.org/en_GB/protocol.html)，URL 集合和索引文件均限 50,000 条、未压缩 XML 限 52,428,800 字节（50 MiB）。生成与解析都在包边界检查；转义后才超限的生成结果也拒绝，C 侧报告 `XERR_RANGE`，XLang 抛 `RangeError`，不会返回截断内容。本包不自动拆分超限站点地图。
C 的零结尾 `xsm_parse` / `xsm_parse_index` 最多探测到 50 MiB 上限外一个字节；超限时先返回 `XERR_RANGE`，不对整个未知长度输入执行无界 `strlen`。非零结尾缓冲区应调用 `_n` 入口并提供真实长度。
robots 文本、代理名和待匹配路径必须是有效 UTF-8；文件开头的 UTF-8 BOM 会被识别，首条 `User-agent` 不会因此被忽略。公开 C 入口在每次调用开始时清除上一次的 XRT 错误，失败后可直接重试。
`robotsParse` 接受至多 1 MiB 的 UTF-8 文本，超限返回 `RangeError`，不截断规则。C 的显式长度入口在触碰输入前检查上限；零结尾入口最多扫描到上限外的第一个字节。这个上限高于 [RFC 9309 的 500 KiB 最低解析量](https://www.rfc-editor.org/rfc/rfc9309.html#section-2.5)，是本包的资源保护合同，不表示实现了完整 RFC 9309 匹配语义。

## C API 与所有权

公开声明在 `include/sitemap_native.h`。`xsm_build`、`xsm_build_index` 返回 XRT 分配的独立 NUL 结尾缓冲，由调用方 `xrtFree`；解析函数返回 `xvalue*`，由调用方 `xrtValueRelease`。格式错误返回 `XVALUE_NULL` 并设置 XRT 错误；分配失败保留 `XERR_MEMORY`，不伪装成输入格式错误。`_n` 后缀版本使用显式字节长度，供 XLang 桥和非 NUL 结尾的 C 缓冲使用。无全局返回区或静态 robots 匹配槽。依赖包：`xml.sax`（及其私有 `_expat`）。

## 范例与验证

- XLang 综合范例：`release/examples/extlibs/sitemap.xl`（运行时不写文件）
- C/XLang 同输出范例：`release/extlibs/sitemap/examples/sitemap_c.c`、`release/examples/extlibs/sitemap_pair.xl`；成对展示 URL 集合与索引的生成/解析往返，以及 robots 决策，覆盖六个公开入口。独立 XML 解析器同时核对两份生成结果。
- C 边界：`release/extlibs/sitemap/test/test_sitemap.c`
- XLang 边界：`release/extlibs/sitemap/test/boundaries.xl`、`demo6/test/cases/extlib_sitemap.xl`
- 协议规模边界：`release/extlibs/sitemap/test/protocol_limits.xl` 与 C 边界用例

在 `demo6` 执行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/sitemap/test/check.ps1`，以严格 GCC、C/XLang 边界和协议规模用例、双语同输出范例及 Python 标准库 XML 解析器完成 Windows 包级验证。该脚本将两端输出归一为 LF/UTF-8 后比较，避免控制台换行差异影响 XML 语义校验。Linux 上执行 `bash release/extlibs/sitemap/test/check.sh`，额外使用 ASan/UBSan 检查 C 边界及范例，并对原始 C/XLang 输出作字节比较。两个脚本都需要 Python；C 范例也可单独执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package sitemap -Source release/extlibs/sitemap/examples/sitemap_c.c`。边界测试覆盖超过旧 64 KiB 缓冲的输出、XML 结构/实体/DTD、规则组、通配符、锚点、内嵌 NUL、独立结果和分配失败错误分类。

2026-10-13：修复 robots 注释/空行误清除当前组导致 `Disallow` 被忽略，补 C/XL 边界与双语范例。2026-10-14：补 XML 构建和 SAX 解析的分配失败注入，修复解析失败误报 `XERR_VALUE`，新增 `test/check.sh` 的 Linux ASan/UBSan 门禁。同日修复重复 `loc` 或可选字段被后值静默覆盖：SAX 解析现在拒绝同一条目内的重复字段，C/XLang 红绿用例、Windows 严格 GCC、Linux ASan/UBSan 与双语范例通过。完整 AOT 仍受共享 `xl6ValueIndexReadCopyV1` / `xl6ValueDataSnapshotV1` 缺失阻断；包级通过不代表发布验收。

同日追加 XML 字符边界：C 负例复现了合法 UTF-8 的 U+FFFE 可被写入无效 XML；现 URL 集合和索引构建共用 XRT UTF-8 标量校验。C/XLang U+FFFE/U+FFFF 拒绝、Linux ASan/UBSan、双语范例及无输出核心 AOT 通过；完整 AOT 阻断未变。

2026-10-15：包清单声明自身使用的 XRT Buffer、String、Unicode、Value Container 模块，`xml.sax` 的依赖模块仍由其包清单提供；源码和公开头的模块宏条件化。同一核心 AOT 从 3,895,296 降至 1,007,616 字节并运行通过；XLang 边界、C/XLang 对照范例和 Linux ASan/UBSan 复跑通过。完整范例 AOT 仍缺 `xl6ValueDataSnapshotV1`，未完成发布验收。

同日补强 robots 入口：C 红绿测试复现了成功调用保留旧错误及无效 UTF-8 被接受；现在解析与判定统一清理旧错误并拒绝无效 UTF-8。失败后成功的 C/XLang 连续调用、Windows 边界与 Linux ASan/UBSan 均通过；完整 AOT 阻断不变。

2026-10-15：补协议规模限制。50,000 条生成/解析正例及 50,001 条的 C/XLang 拒绝用例先红后绿；单字段使生成结果超过 50 MiB、输入本身超过 50 MiB 的 C 边界均报告 `XERR_RANGE`。Windows C、XLang 协议测试和原边界、C/XLang 同输出范例、无输出核心 AOT 与 Linux ASan/UBSan 通过；完整 AOT 共享缺符号阻断不变。

2026-10-15：`robotsParse` 原无限制解析超大文本。新增 1 MiB 上限，C `_n` 与零结尾入口在 1 MiB 正例、超 1 字节负例先红后绿；XLang 超限抛 `RangeError`。Windows C/XLang、Linux ASan/UBSan 与双语对照范例通过；包内边界测试已登记统一 smoke，完整 AOT 仍待共享链接修复。

2026-10-16：`loc` 原只校验非空，可生成或解析超过协议上限的 URL；现在按 UTF-8 字符数限制为少于 2,048。C 与 XLang 的 2,047/2,048 正反例覆盖 URL 集合与索引生成、集合解析；50 MiB 总量测试改由可选字段触发，继续验证独立的文件大小门禁。完整 AOT 的共享主线阻断不变。

2026-10-16：robots 文本开头带 BOM 时，原解析器会静默跳过首条 `User-agent`，随后忽略其 `Disallow`。C/XLang 负例先红后绿；现在只跳过开头三字节 BOM，原有大小/NUL/UTF-8 校验保持不变。Windows C/XLang 边界与 Linux ASan/UBSan 通过；完整 AOT 阻断不变。

2026-10-17：Linux 包级门禁复用编译对象，同时运行 C ASan/UBSan、中央及包内 XLang 边界、协议规模用例和双语同输出范例；新增独立 XML 0.9 命名空间、实体解码及 robots 判定检查。C 范例补齐失败路径的值所有权回收。整套门禁通过；完整 AOT 仍受共享主线符号缺失阻断，未签收发布级。

2026-10-18：`robotsAllows` 原把代理名作不区分大小写的子串匹配，导致 `User-agent: Bot` 错误命中 `NotBot`、`BotPlus`。C/XLang 反例先红后绿；现按完整 product token 匹配并明确 API 参数含义。路径匹配仍是文档所述常用子集，不宣称完整 RFC 9309 实现。

2026-10-19：`robotsParse` 增加 C/XLang 逐点分配故障与恢复回归，覆盖代理组、Allow/Disallow 及 Sitemap 声明的构造。故障时保持 `XERR_MEMORY` / `MemoryError`，清除故障后同一规则可正常解析和决策。Windows 包级门禁及 Linux C ASan/UBSan、XLang 和独立 XML 对照均通过；原生实现无需修改，完整 AOT 仍待共享主线缺符号修复。

同日补多段 `*`、空跨度、`$` 末尾锚点、较长 `Allow` 规则及同长度规则平局的 C/XLang 对照边界。又以 `Disallow: /a$b` 对 `Allow: /a*b` 的红色用例复现内部 `$` 被评分器错当锚点、错误选中 `Allow`；现评分与匹配统一，只排除末尾锚点。Windows C/XLang 与 Linux C ASan/UBSan、XLang、XML 独立检查均通过；完整 AOT 的共享阻断仍在。

同日追加独立递归模型的 robots 通配符矩阵：对 `/` 后最多三个 `a`、`b`、`*` 规则字符，分别带或不带末尾 `$`，逐一比对 `/` 后最多三个 `a`、`b` 路径字符的允许结果。Windows C 边界与 Linux ASan/UBSan、XLang、双语和独立 XML 门禁通过；该有限矩阵不等于完整 RFC 9309 认证，公开语义未变。

同日收紧 XML 生成字段边界：`build` / `buildIndex` 原会忽略未知对象键并返回丢字段的 XML；C/XLang 负例先红后绿，现按各自公开字段表拒绝未知键并报 `ValueError`。Windows C/XLang 和完整 Linux C ASan/UBSan、XLang、双语、独立 XML 门禁通过；完整 AOT 的共享阻断未变。

2026-10-19：新增 Windows `test/check.ps1`，把严格 GCC 宏检查、C/XLang 边界、50,000 条协议规模、双语范例及独立 XML 解析收成单条可复跑门禁；本机通过。算法未改，Linux ASan/UBSan 仍由原有 `test/check.sh` 覆盖；完整 AOT 共享缺符号阻断未解除，未签收发布级。

2026-10-20：Windows 门禁增加 `-SkipAot` 聚焦模式；默认在独占构建目录
构建/运行无输出核心 AOT，再构建/运行完整双语范例 AOT，并用同一份独立
Python XML/robots 检查与 C 输出对照。当前 `-SkipAot` 的 C/XLang、
协议规模与独立解析全部通过；默认门禁核心 AOT 通过，完整范例构建因
共享 `xl6ValueDataSnapshotV1` 缺符号失败。新增构建目录在失败后清理，
不能将核心通过视为整包 AOT 签收。

2026-10-21：C/XLang 边界固定 `&`、`<`、`>`、引号、撇号和回车的 XML 写出字节及解析往返，避免仅由同一生成器和解析器互相证明正确。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh` 的 C ASan/UBSan、语言边界及独立 XML 检查通过。实现/API 未改；完整 AOT 共享缺符号阻断仍在，不签收整包发布级。

2026-10-22：robots 通配符匹配由反复回退改为字面片段顺序查找，长片段使用 KMP，避免长重复路径上的平方级扫描；末尾 `$`、前缀规则、连续 `*` 的公开语义不变。新增 8 KiB 规则/16 KiB 路径的 C 边界及匹配时分配故障、恢复测试；旧实现先红，修复后 Windows `test/check.ps1 -SkipAot` 和 Linux `test/check.sh` 的 C ASan/UBSan、XLang、独立 XML、双语对照均通过。完整 AOT 的共享缺符号阻断未变，不签收整包发布级。
