# rss

RSS 2.0 与 Atom 文本订阅格式的解析和生成。XML 语法由 `xml.dom` / Expat 处理；DTD 和外部实体禁用。范围是常用 feed 字段，不承诺 RSS 1.0、扩展命名空间、Atom XHTML 内容或日期规范化。

RSS 根节点必须有 `version="2.0"`。缺少版本号或使用更早的版本时，`detect` 返回空字符串，`parse` / `parseRSS` 报错；不保留旧版本兼容入口。这按 [RSS Advisory Board 的 RSS 2.0 规范](https://www.rssboard.org/rss-specification)收紧格式身份判断，并不意味着本包验证规范中的全部频道字段。

## XLang

```xlang
import rss

var channel = {"title": "News", "link": "https://example.test", "description": "Updates"}
string xml = rss.buildRSS(channel, [{"title": "First", "guid": "1"}])
var feed = rss.parse(xml)
println(feed["items"][0]["title"])
```

`detect` 返回 `rss`、`atom` 或空字符串；格式不匹配或 XML 无效时返回空字符串，分配失败抛 `MemoryError`。`parse` 自动识别，`parseRSS` / `parseAtom` 要求相应根元素。错误输入抛 `ValueError`。生成器要求元信息为对象、条目为数组，字段类型为字符串；错误不生成部分 XML。

生成 RSS 2.0 时，channel 必须有非空的 `title`、`link`、`description`；每个 item 至少有非空 `title` 或 `description`。生成 Atom 时，feed 必须有非空 `id`、`title`、`updated`；每个 entry 必须有非空 `id`、`title`、`updated`，并提供 `content` 或 `link`。作者写为字符串 `author`，entry 可继承 feed 的作者；未提供 feed 作者时，每个 entry 都必须有自己的作者。这里校验的是生成内容的最低结构，未实现日期格式、IRI、媒体类型或全部扩展元素的规范验证。解析器仍宽容读取字段不完整但 XML 正确、根类型正确的 feed。

RSS 保留 `title/link/description/updated` 与 item 的 `title/link/description/pubDate/guid/author/categories`。Atom 保留 `title/subtitle/link/updated/id/author` 与 entry 的 `title/link/id/updated/summary/content/author`；解析 link 优先 `rel="alternate"`，否则取第一个 `href`。解析后的 Atom `entry.author` 是有效作者，依次取 entry 自身、entry 的 source、feed；扁平结果不保留作者来源或完整 source 元信息。没有实现的字段不保证往返。

解析最大输入 8 MiB、10 万 XML 节点、深度 64；生成的 XML 正文最大 16 MiB（不计返回缓冲末尾的 NUL），恰好达到上限仍有效，超一字节时 C 接口报告 `XERR_RANGE`，不返回截断结果。XML 字段完整保留，不静默截断；生成拒绝无效 UTF-8 和不符合 XML 1.0 字符范围的内容，包括 NUL、非法控制字符及 U+FFFE/U+FFFF。
生成器对元素文本中的 CR 和 Atom `link` 属性中的 Tab/LF/CR 使用字符引用，避免 XML 解析器规范化字面空白时改变原始字段值；本包仍不校验链接是否为有效 IRI。

## C

`include/rss_native.h` 提供同一能力。解析函数接收显式字节长度，返回的 `xvalue*` 由调用方 `xrtValueRelease`，失败时返回 `XVALUE_NULL` 并设置 XRT 错误。生成函数返回独立持有的 NUL 结尾 UTF-8 缓冲区，由调用方 `xrtFree`；失败返回 `NULL` 并设置 XRT 错误。分配失败保留 `XERR_MEMORY`，不改写为输入格式错误；`xrss_detect` 在这种情况下仍返回空字符串，但保留 XRT 错误供 C 调用方判断。`xrtValueObjectSetNew` 和 `xrtValueArrayAppendNew` 无论成功失败都消费传入值，不应二次释放。

## 验证

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/rss/test/check.ps1
.\release\xl.exe run test\cases\extlib_rss.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package rss -Source release\extlibs\rss\test\test_rss.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package rss -Source release\examples\extlibs\rss_c.c
.\release\xl.exe run release\examples\extlibs\rss_pair.xl
.\release\xl.exe run release\examples\extlibs\rss.xl
.\release\xl.exe run release\extlibs\rss\test\author_inheritance.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package rss -Source release\extlibs\rss\examples\atom_author_c.c
.\release\xl.exe run release\extlibs\rss\examples\atom_author.xl
.\release\xl.exe build release\extlibs\rss\test\aot_core.xl -o build\test\rss_aot_core.exe
.\build\test\rss_aot_core.exe
```

Linux 上可从 `demo6` 目录运行 `bash release/extlibs/rss/test/check.sh`；它在临时目录编译 XRT、Expat、XML DOM 与 RSS C 测试并以 ASan/UBSan 执行，还运行中央与包内 XLang 用例，对照两组成对范例，不留下发布目录产物。
Windows 的 `test/check.ps1` 是同范围单入口：严格 GCC 语法、C 边界、
中央与包内 XLang 用例、两组 C/XLang 同输出，以及核心与 Atom 范例 AOT。
`-SkipAot` 跳过这两项 AOT，只用于聚焦开发；默认门禁会在下述共享缺符号
处准确失败，不能把核心 AOT 单独通过视为发布签收。

`rss_c.c` 与 `rss_pair.xl` 使用同一份频道和单条目数据，输出可逐行对照；`rss.xl` 是包含多条目及 Atom 往返的扩展范例。包内 `atom_author_c.c` 与 `atom_author.xl` 使用相同的 Atom 输入，均输出 `Editor`、`Source` 两行。不同文件名保证 XL 范例不会将 C 范例作为同名伴随源编译。
成对 RSS 范例还断言解析后的单条目标题，并由包级门禁固定 `type: rss`、
`items: 1` 两行预期，避免两种语言共同输出错误结果时误判通过。

2026-10-14：生成、解析、识别三条路径分别补齐 RSS/Atom 逐点分配失败注入；修复内存失败误报格式错误，并让 XLang `detect` 在内存失败时抛错。Windows C/XLang、Linux ASan/UBSan 与无输出 AOT 核心探针通过。

同日追加 XML 1.0 字符边界：C 负例复现了合法 UTF-8 的 U+FFFE 可写入无效 RSS；现 RSS/Atom 共用的转义入口按 XRT UTF-8 标量校验。C/XLang 两种格式的 U+FFFE/U+FFFF 拒绝、Linux ASan/UBSan、双语范例与无输出核心 AOT 均通过。

完整 XLang 边界用例的 AOT 链接仍受共享 `xl6ValueIndexReadCopyV1` 阻断，见 `dev/extlib_blockers/value_index_link.md`；带 `println` 的范例还受 `xl6ValueDataSnapshotV1` 阻断，见 `dev/extlib_blockers/text_snapshot_link.md`。两者不是 RSS 包内实现的问题。

2026-10-14：原解析器把无版本号或 `version="0.91"` 的根节点误认作 RSS 2.0；现统一由根类型判断拒绝。C/XLang 负例先红后绿，原有有效样本补齐版本属性；Windows 严格 GCC、双语范例、Linux ASan/UBSan 和无索引 AOT 核心通过。完整 AOT 的共享缺符号阻断不变。

2026-10-15：清单显式声明 RSS 自身使用的 XRT Buffer、Unicode 与 value 容器模块，原生源码和头文件的模块宏改为条件定义；XML DOM 仍由依赖包提供。同一无输出 AOT 核心由 3,901,440 字节降至 986,112 字节，构建运行通过；C 测试、XLang 中央用例、双语逐字同输出与 Linux ASan/UBSan 门禁通过。完整带输出 AOT 仍缺 `xl6ValueDataSnapshotV1`，不签收发布级别。

2026-10-15：补生成结果超过 16 MiB 的 C 回归，先复现泛化的值错误，再在写入处统一报告 `XERR_RANGE`；Windows C 测试、XLang 中央用例、双语范例、无索引 AOT 核心与 Linux ASan/UBSan 复验通过。完整 AOT 的共享符号阻断不变。

2026-10-16：生成器原可输出缺少必需字段的 RSS/Atom；现检查 RSS channel/item 与 Atom feed/entry 的最低结构，补 feed 作者读写及 entry 作者继承。C 正反例、XLang 中央用例、多条目范例、Windows C 与 Linux ASan/UBSan 通过；解析器保持宽容。未声明完整 RSS/Atom 规范校验，完整 AOT 仍受上述共享符号阻断。

同日补齐解析侧的有效作者继承：entry 自身优先，其次 source，最后 feed。新增 C/XLang 回归和同输入双语范例，Windows C、Linux ASan/UBSan 与目录 smoke 通过；完整 AOT 的共享符号阻断不变。

2026-10-17：Linux 包级脚本复用一次编译的原生依赖对象，加入中央用例、
作者继承及两组 C/XLang 范例逐字节对照。`atom_author_c.c` 的计数索引改为
`size_t`，两份 C 范例均启用 `-Werror`；C ASan/UBSan 与 XLang 门禁通过。
完整 AOT 共享阻断不变。

2026-10-19：RSS/Atom 共用的生成收尾原把恰好 16 MiB 的 XML 正文误判为超限。C 边界通过真实 RSS 对象构造精确上限与上限加一字节，先红后绿；Windows C/XLang、Linux C ASan/UBSan、XLang 与双语范例门禁均通过。解析输入仍限制 8 MiB，完整 AOT 的共享阻断不变。

同日新增 Windows 包级单入口门禁；PowerShell 7 的完整模式和 Windows
PowerShell 5.1 的 `-SkipAot` 模式均通过，涵盖严格 C 语法、C/XLang
边界、两组成对范例及核心 AOT。Linux ASan/UBSan 门禁沿用既有脚本；
新门禁不把无输出核心 AOT 误报为完整带输出 AOT 验收。

2026-10-20：默认 Windows 门禁现实际构建/执行带输出的 Atom 双语范例
AOT，并与 C 黄金输出对照；当前构建在共享 `xl6ValueDataSnapshotV1`
缺符号处失败。`-SkipAot` 的严格 C、XLang 边界和双语 JIT 仍通过；
失败后本次 AOT 产物会清理。RSS 包实现不变。
