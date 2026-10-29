# epub

2026-10-26：Windows/Linux 默认门禁现将公开 C 范例的独立 EXE
构建、运行和 Python `zipfile`/XML 检查放在 XLang 用例之前。
两平台的严格 C 边界、C 直跑与 C AOT 均通过；生成 EPUB
逐字节一致，外部检查覆盖 mimetype、目录、元数据、spine、
导航及章节。随后首个 XLang 用例仍因语言创建的 `var` 字典和
章节数组无法投影到原生 `xvalue` 而失败。此改动只补 C 发布路径
的验收，不解除完整双语/AOT 阻断。

**当前发布门禁未通过。** 2026-10-24 从当前源码重建 Windows 发布版后，
默认 `test/check.ps1` 的严格 C 边界通过，但首个 XLang 用例
`test/cases/extlib_epub.xl` 在把合法的语言字典和章节数组传给
`epub.build` 时报告 `ValueError: invalid EPUB metadata or chapter`。
原生 `xep_build` 要求 XRT object/array，公开签名却接收两个 `var`；
这与已登记的 foreign 容器投影故障同形，尚未证明内部根因完全相同。
完整 XLang/AOT 门禁未签收；修复共享边界后须重跑默认门禁。

`epub` 把简单 UTF-8 文本章节生成 EPUB 3 电子书。面向小型手册与纯文本出版物，不是通用 HTML/资源打包器。包使用专用 stored ZIP 容器，无额外压缩库；`mimetype` 位于首条目且没有 ZIP extra 字段。元数据、目录、spine 和 XHTML 章节由同一次构建生成。

```xlang
import epub

var meta = {"title": "xlang 电子书", "language": "zh-CN",
    "identifier": "urn:xlang:example:epub",
    "modified": "2026-08-01T00:00:00Z"}
var chapters = [
    {"title": "第一章", "text": "# 开始\n\n你好，xlang。"},
    {"title": "第二章", "text": "# 下一步\n\n使用 C 与 xlang。"}
]
int bytes = epub.build("book.epub", meta, chapters)
```

`meta` 必须有非空 `title`、`language`、`identifier`；`author` 可选。`identifier` 应由作者为每部出版物提供稳定且唯一的值，不再使用固定默认 ID。`modified` 可选，默认当前 UTC 时间；显式指定时必须是有效的 `YYYY-MM-DDThh:mm:ssZ`。章节数组为 1 到 100 项，每项必须有非空 `title`，`text` 可为空。行首 `# ` / `## ` 转成一、二级标题，空行分段；其他文本按纯文本处理并进行 XML 转义，不解析完整 Markdown。

`modified` 会按真实日历校验：闰年的 `2024-02-29T00:00:00Z` 有效，
`2024-02-30T00:00:00Z` 无效；非法日期不会覆盖已有目标文件。

未指定 `modified` 且系统时钟读取失败时，构建会失败并保留 XRT 的 I/O 错误，不会把失败时的零时间戳写成 1970 年，也不会创建或替换目标文件。显式提供 `modified` 不依赖系统时钟。

`build` 返回生成文件字节数，失败抛异常；文件经 XRT 原子写入，失败不会先截断已有目标。输入字符串要求有效 UTF-8、无内嵌 NUL，生成的 ZIP 最大 8 MiB；单段内容或累积 ZIP 超限时，C 接口报告 `XERR_RANGE`，XLang 抛 `RangeError`，已有目标保持不变。不静默截断长行。XLang 接口在传入 C 前检查 `build` 的路径，以及 `readEntry` 的路径和条目名，拒绝内嵌 NUL，避免意外访问截断后的前缀路径或条目。`readEntry(path, name)` 仅用于读取本库生成的 **stored 文本条目**，返回独立字符串，找不到返回空串；损坏归档或不支持的压缩方式报错，不把它当成任意 EPUB 的通用读取器。C 接口见 `include/epub_native.h`，读取结果由调用方 `xrtFree`。

C 构建结果使用 `int64_t`，与 XLang `int` 的 ABI 一致；公开头可单独包含，不要求调用方预定义 XRT 值容器模块宏。

读取器同样限制归档为 8 MiB：复用 XRT 限额整文件读取，超过上限或读取期间增长越界均报范围错误，不会伪装成损坏 ZIP，也不会先将超限文件读入内存。

当前不支持封面、图片、CSS、脚本、多媒体、现成 XHTML、嵌入字体或 EPUB 编辑；也不提供通用 ZIP 解压。`language`、`identifier` 的业务有效性由调用方负责。实现依据 [W3C EPUB 3.3](https://www.w3.org/TR/epub-33/) 的基本容器及必要元数据要求。C 和 xlang 范例生成的电子书已经通过 EPUBCheck 5.4.0（EPUB 3.4 规则，零错误、零警告）；这只验证了本库支持的纯文本生成路径，不代表覆盖 EPUB 3 全部出版能力。

元数据和章节文本除有效 UTF-8 外，还须满足 XML 1.0 字符范围；合法 UTF-8 的 U+FFFE、U+FFFF 也会被拒绝。

## 验证

从 `demo6` 目录运行：

```powershell
.\release\extlibs\epub\test\check.ps1 -SkipAot
.\release\xl.exe run release\extlibs\epub\examples\basic.c
.\release\xl.exe run test\cases\extlib_epub.xl
.\release\xl.exe run test\cases\extlib_epub_boundaries.xl
.\release\xl.exe run release\extlibs\epub\test\ffi_boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package epub -Source release\extlibs\epub\test\test_epub.c
bash release/extlibs/epub/test/check.sh
.\release\xl.exe build release\extlibs\epub\test\aot_core.xl -o build\test\epub_aot_core.exe
.\build\test\epub_aot_core.exe
.\release\xl.exe run release\examples\extlibs\epub.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package epub -Source release\extlibs\epub\examples\basic.c
```

C 教学范例在 TCC 单文件运行时包含包内两份实现；正式包清单和 GCC
构建仍分别编译这些源文件。Linux 包级门禁将两种 C 构建生成的 EPUB
逐字节比较，并与 XLang 范例的输出和生成物对照。

历史包级验证还包括 Python 标准库的独立 ZIP CRC/XML 解析、100/101 章边界、4 KiB 长行、损坏中央目录、分配失败与旧文件保持、Windows 严格 GCC 及 Linux ASan/UBSan。此前 C/xlang 范例输出一致。旧发布物的完整边界 AOT 曾因主线 `xl6ValueDataSnapshotV1` 缺符号无法链接，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`；当前默认门禁先在上述容器输入处失败，不能据旧结论签收。

2026-10-14 追加 XML 1.0 字符边界：C 负例先确认 U+FFFE 原会生成非法 XML，随后包内统一校验元数据和章节文本；C/XLang 对 U+FFFE/U+FFFF 的拒绝、原文件保持、严格 GCC 与正常 EPUB 的独立 ZIP/XML 解析通过。此轮未重跑 Linux sanitizer 或 EPUBCheck，完整 AOT 阻断未变。

2026-10-15：包清单声明源码实际使用的 XRT Buffer、整文件、时间文本、Unicode 和 value 容器模块。相同无输出 AOT 核心从 3,608,576 降至 711,680 字节并运行通过；C 边界、XLang 边界与双语范例同输出复跑通过。完整边界 AOT 的共享文本快照缺符号仍未解决，本包仍待发布验收。

2026-10-15：读取器现在拒绝 EOCD 的非零磁盘号、目录起始磁盘号与不一致的两份条目计数，也拒绝中央目录条目指向其他磁盘或本地条目数据跨入中央目录。畸形归档 C 回归先失败后通过；Windows C、XLang 边界与 FFI，以及新增的包内 Linux 严格 GCC + ASan/UBSan 门禁通过。C 测试使用调用方指定目录放置并删除产物；完整 AOT 的共享缺符号阻断未变。

2026-10-15：补读取超限归档的稀疏文件负例，先复现原实现把容量错误误报为损坏 ZIP；读取路径改用 XRT 限额 API，修复后返回范围错误，并覆盖读取期间增长。另补读取首次分配失败保持内存错误、故障清除后恢复的 C 回归。Windows C、XLang 边界与 FFI、Linux ASan/UBSan 及 C/XLang 同输出范例通过；完整 AOT 阻断未变。

2026-10-16：C 范例的早退路径现在释放尚未交给数组的 `xvalue`，`AppendNew` 交接后立即清空本地引用；仅在本轮成功生成默认输出文件时删除该文件。C/XLang 对照输出相同，C 范例通过严格 GCC 语法检查。库本体与 AOT 阻断未变。

同日 C 边界测试无参数运行时改由 XRT 创建独立系统临时目录，避免
`build/test/epub_guard_test.epub` 固定路径与并行门禁冲突；显式传入路径
仍由调用方管理。测试运行库仅为此补 `XRT_MODULE_DIR_TEMP`，不影响
发布包裁剪清单。Windows TCC C 与 Linux 严格 GCC、ASan/UBSan 通过；
完整 AOT 阻断未变。

同日 XLang `test/ffi_boundaries.xl` 也改用独立系统临时目录与 `finally`
清理，已登记到 `coverage.tests`；从 `demo6` 工作目录运行通过，
不再写入固定的 `build/test/epub_ffi_boundary.epub`。

同日中央 XLang 测试和 C/XLang 范例的默认输出也改为独立系统临时目录，
结束时清理，避免并行运行覆盖固定 EPUB 文件；范例显式指定输出路径时
仍保留文件。两种语言的范例与中央测试均在 `demo6` 工作目录通过。

2026-10-17：Linux `test/check.sh` 增加中央与 FFI XLang 用例、C/XLang
双语范例及生成 EPUB 的逐字节对照；Python 标准库独立检查 ZIP CRC、
首条目 `mimetype`、container、manifest、spine 和两章 XHTML 内容。
C ASan/UBSan 与上述检查同次通过。此固定纯文本范例检查不等于完整
EPUBCheck 认证；完整 AOT 仍受主线共享缺符号阻断。

独立格式验证可使用 [EPUBCheck 5.4.0](https://github.com/w3c/epubcheck/releases/tag/v5.4.0)。从 `demo6` 目录运行以下命令，将 `<epubcheck.jar>` 换为本机解压后的路径，并保持发行包的 `lib` 目录与 jar 同级：

```powershell
.\release\xl.exe build release\extlibs\epub\examples\basic.c release\extlibs\epub\epub_native.c release\extlibs\epub\epub_zip.c --inc-path lib\xrt --inc-path release\extlibs\epub\include -o build\test\epub_basic.exe
.\build\test\epub_basic.exe build\test\epub_c.epub
.\release\xl.exe build release\extlibs\epub\test\test_epub.c release\extlibs\epub\epub_native.c release\extlibs\epub\epub_zip.c --inc-path lib\xrt --inc-path release\extlibs\epub\include -o build\test\epub_test.exe
.\build\test\epub_test.exe build\test\epub_100.epub
.\release\xl.exe run release\examples\extlibs\epub.xl -- build\test\epub_xl.epub
java -jar <epubcheck.jar> build\test\epub_c.epub
java -jar <epubcheck.jar> build\test\epub_100.epub
java -jar <epubcheck.jar> build\test\epub_xl.epub
```

范例在提供输出路径时保留文件；不传路径时仍删除临时文件。验证产物放在 `build/test`，不随包发布。

2026-10-17：修复内部头在清单预定义 XRT 模块宏时被严格 GCC 判为重复定义；五项宏现在均为条件定义，Linux 门禁新增该编译路径。C ASan/UBSan、XLang 边界和 ZIP/XML 独立校验通过；完整 AOT 的共享缺符号阻断不变。

2026-10-18：Linux 一次性 `clock_gettime` 故障注入先复现缺省 `modified` 时仍成功生成 EPUB；现于时间格式化前检查 XRT 时钟错误，保留 `XERR_IO` 并禁止写入。C 测试还验证故障后的正常构建恢复。Windows C/XLang 边界与 Linux ASan/UBSan、XLang、双语同输出及独立 ZIP/XML 门禁通过；完整 AOT 阻断未变。

2026-10-19：C 教学范例原不能直接 `xl run`，因为头文件只由额外编译参数提供。
现仅在 TCC 单文件路径包含包内实现，正式分离编译路径不变。Windows
直接运行与包运行器通过；Linux 包级门禁新增直接运行，并确认其输出及
EPUB 文件与 GCC 分离编译逐字节相同。完整 AOT 共享阻断仍未解除。

同日读取器增加 13 个定向损坏样本，分别改动 `mimetype` 的本地头标志、压缩方法、CRC、长度、名称及正文，以及中央目录的标志、方法、CRC 和长度；每个样本都必须报错且不能返回损坏文本。Windows C 定向测试和 Linux C ASan/UBSan、XLang、独立 ZIP/XML 联合门禁通过；此矩阵不是通用 ZIP fuzz，完整 AOT 仍待主线共享符号修复。

2026-10-20：XML 1.0 允许的 CR 原以字面写入元数据和章节标题，解析时会
归一化为 LF；语言属性中的 Tab/LF/CR 也会丢失原值。C/XLang 负例先红后绿，
共用写出器现使用字符引用保留三种空白；章节正文原有分段规则不变。
Windows 包级 C/XLang 与独立 ZIP/XML 门禁、Linux C ASan/UBSan 和双语
门禁均通过。Windows 门禁的中文固定预期改为显式 UTF-8 文件读取，避免
PowerShell 5.1 在解析脚本前因系统代码页误读中文。完整 AOT 阻断不变。

2026-10-19：新增 Windows `test/check.ps1`，把严格 GCC 清单宏/ABI、C 边界、中央及包内 XLang 边界、C/XLang 同输出与同字节 EPUB，以及 Python 独立 ZIP CRC/XML 检查收成一条可复跑门禁。临时电子书只存于本次系统临时目录，完成后清理；Windows 与 Linux ASan/UBSan 联合门禁通过。构建算法未改，完整 AOT 共享缺符号阻断仍在，不签收发布级。

2026-10-20：Windows 门禁新增显式 `-SkipAot` 快速模式；默认模式继续构建并运行核心 AOT，再构建双语范例 AOT、核对输出与 EPUB 文件字节。AOT 核心电子书使用系统临时目录的随机文件名并在 `finally` 删除，门禁构建产物使用本次独占目录，成功或失败均清理。Windows 快速模式与核心 AOT 通过；默认门禁在范例 AOT 链接处准确报告共享 `xl6ValueDataSnapshotV1` 缺符号，并以非零状态退出，不误报整包通过。完整发布验收仍待主线修复。

2026-10-21：生成超出 8 MiB 原错误归类为参数错误。C/XLang 回归先复现，现单段输出与累积 ZIP 超限均保留范围错误，并核对失败不覆盖已有 EPUB。Windows `test/check.ps1 -SkipAot` 和 Linux `test/check.sh` 的 C ASan/UBSan、XLang、独立 ZIP/XML 门禁通过；默认 Windows 门禁的核心 AOT 通过，范例 AOT 仍缺共享符号 `xl6ValueDataSnapshotV1`。

2026-10-24：读取器增加固定种子的 512 组 ZIP 单位翻转样本。每组只改变一个归档字节，要求读取 `mimetype` 时明确报错、保持原文，或在条目名被破坏时返回空结果，不得静默返回损坏正文。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh`（含 C ASan/UBSan、XLang 和独立 ZIP/XML 校验）通过。这是可复跑的有限扰动回归，不代替通用模糊测试；完整 AOT 的共享符号阻断未变。
