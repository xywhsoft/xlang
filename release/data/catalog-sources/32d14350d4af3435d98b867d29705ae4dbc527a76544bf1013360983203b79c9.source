# gettext

## 当前验收：2026-10-30

PO普通正文从逐字节追加改为CRT memchr定位的连续片段追加，转义仍走
原有唯一分支；不改变语法、API/ABI或所有权。新增156组字节/续行/
独立寿命用例，新旧结果一致；四组实际OOM点102→98，均零失败。
共享教学PO增加续行，原C/XL六行golden、GNU MO及Python独立对照不变。

Windows默认59阶段/56指纹全部通过，Linux完整ASan/UBSan/JIT/AOT/
GNU专项通过；两平台各14条普通独立程序路径。新严格argc测试还发现
并修正本包Windows Run把null参数表变成一个空串实参的问题，PS5/PS7
实际Run的三种空参数语义均验证，未改公共工具或主线。

同一C入口固定长ASCII负载旧/新中位耗时比为2.45–3.41；但Linux TCC
两个转义样本本轮约慢5%，不是所有负载都提速。native对象text增加
32B/64B，不是整个程序体积变化；仍有扫描、缓冲和目录值复制。
完整数字与边界见[本轮审查](PO_SPAN_AUDIT_2026-10-30.md)；
使用命令、续行示例与借用/拥有区别见[双语范例说明](examples/README.md)。
八冻结文件不变，未同步开发中的XRT或修改编译器。

## 当前验收：2026-10-29

修复 PO 字段与条目边界的两类错误：`msgid/msgid_plural/msgctxt` 原先
只接受字段名后的固定空格，拒绝合法制表符或紧贴引号的拼写；完成一条译文后，
若没有空行就进入下一条 `msgctxt`，也会误报格式错误。字段名现在按 token
边界统一识别，完整译文在新上下文开始时提交；缺译文、重复上下文或形似
关键字的名称仍拒绝，没有添加历史语法回退或第二套解析器。

新的 `test/po_layout.po` 被 GNU `msgfmt 0.23.2 --check` 接受，修复前
的 C 回归为 21 项、12 次失败；修复后同一矩阵为零失败，并扫描 16 个
分配预算至成功，检查目录释放后的存活分配及非法释放/上下溢指标。语言
回归还保留全局目录的自然退役，不将它移入局部作用域来缩小验证范围。

Windows PowerShell 5.1 默认 50 阶段通过，输入变动 0；Linux 原生快照
通过 C ASan/UBSan、旧新边界与分配故障、七个实际生成原型、十二条普通
独立程序路径和双语黄金输出。Linux 另运行 GNU 生成 MO 的既有专项，并让
新的 C/XLang 范例按 PO/MO 两种模式读取同一目录，输出一致；Python
标准库 `GNUTranslations` 也独立核对了新目录的六行结果。
公开 API、ABI、借用/拥有结果规则、复数表达式和冻结 XRT 均未改变。

四个 C 查询入口 `xgt_t`、`xgt_tn`、`xgt_tc`、`xgt_tnc` 返回独立的
`char*`，调用方用 `xrtFree` 释放；`*_borrowed` 返回目录或输入中的
借用文本，不能释放。XLang 包装层从借用文本构造独立 `string`，
这是保证目录销毁或输入退役后结果仍有效所需的一次复制，
不把它误称为可消除的桥接复制。

2026-10-28：修复 C 上下文查询在 `context` 或 `msgid` 为 NULL 时绕过
目录类型、分隔符和复数规则验证的问题。缺参路径复用既有统一查询，不再
另写原文返回逻辑；正常借用/拥有合同与有效的原文返回保持不变。
独立 C 测试覆盖五种非目录值、三种缺参组合、借用/拥有两组接口共 60 个
类型负例，以及空上下文区别、非法分隔符、计数错误和错误后的恢复。
Windows 默认完整门禁及 Linux 原生文件系统同源码门禁通过，含
ASan/UBSan、逐分配故障、八条普通 AOT、双语同输出；Linux 另以 GNU
`msgfmt` 生成 MO，sanitizer C、XLang JIT/AOT 读取均通过。
门禁现在成功才清理，失败保留现场；Windows 受控退出 37、Linux 无输出
退出 37 与缺编译器退出 127 均验证了原始诊断。Linux 复用一次编译的共同
XRT 对象，未改变生产依赖或宏裁剪口径。

2026-10-26 当前 Windows/Linux 包门禁均退出 0：严格 C、Linux
ASan/UBSan、中央/重复键/边界/分配故障/输入限额用例 JIT 与独立
EXE AOT、双语成对范例和公开范例 AOT 全通过。C 单文件范例也由
`xl build` 生成 EXE，必须从仓库根目录运行以读取同一 MO 夹具；
其输出与 GCC、TCC 和 XLang 成对范例一致。Linux 有 `msgfmt` 时，
默认门禁还用 GNU 工具从 `test/external.po` 生成真实 MO，由
sanitizer C、XLang JIT/AOT 共同读取；无工具时明确输出 SKIP。
旧 Linux `cbytes` 和 AOT 缺符号记录仅描述当时发布物。
本轮验收只覆盖下述 UTF-8 PO/MO 子集，不代表全部 GNU gettext 特性
或整个 xlang 发布版已签收；更新后的主线发布物仍需重建复验。

2026-10-25 新 Linux 发布物：此前 `cbytes` 声明处的导入错误已不再复现。
包级 `test/check.sh` 现完整通过严格 C、ASan/UBSan、PO/MO 边界与
大输入、XLang JIT/分配故障、双语范例，并新增边界与成对范例两份
独立 EXE 的构建运行；范例 AOT 输出逐字节对照 C 结果。下文关于旧
Linux `release/xl` 阻断的文字是历史记录，不再描述当前门禁状态。

从 GNU PO 文本或 MO 二进制目录查询译文。目录本身是 XRT object；C 可直接调用 `gettext_native.h` 声明的函数，原有查询结果由调用方使用 `xrtFree` 释放，解析结果使用 `xrtValueRelease` 释放。
`xgt_*_borrowed` 只在同步调用期间借用目录译文或传入的原文，不得释放返回指针；保留结果时必须保持目录及原文存活或自行复制。XLang 包装使用借用入口，直接形成语言字符串，省去原先 C 侧分配及第一次正文复制；C 的拥有型查询合同不变。
PO/MO 解析时，译文片段直接从借用的解析视图构造独立 XRT 字符串值，
不再先复制进临时 C 字符串；目录和输入的所有权合同不变。

| XLang | 作用 |
| --- | --- |
| `gettext.parsePo(text)` | 解析 PO 文本，格式无效时抛 `ValueError` |
| `gettext.parseMo(bytes)` | 解析大小端 MO 字节，格式无效时抛 `ValueError` |
| `gettext.loadMo(path)` | 从文件读取 MO |
| `gettext.t(catalog, msgid)` | 单数查询；未命中返回原文 |
| `gettext.tn(catalog, msgid, plural, n)` | 按目录 `Plural-Forms` 选复数译文；未命中返回原文 |
| `gettext.tc(catalog, context, msgid)` | 带 `msgctxt` 的单数查询 |
| `gettext.tnc(catalog, context, msgid, plural, n)` | 带 `msgctxt` 的复数查询 |

没有 `Plural-Forms` 头时默认按英语 `n != 1` 选择形式。复数数量必须非负。PO 的转义、多行条目、上下文和按顺序编号的复数项均可解析；MO 支持大端、小端。查询结果彼此独立，不会覆盖前一次调用结果。PO/MO 格式错误不作静默降级。目录是数据对象而不是共享全局状态，可同时保留多个语种。
PO 字段名后的空格/制表符与紧贴引号写法均可识别，例如 `msgid"Open"`；
相邻完整条目不强制插入空行，上下文切换也遵循同一提交规则。这与
[GNU PO 条目中的可选空白](https://www.gnu.org/software/gettext/manual/html_node/PO-File-Entries.html)
一致。字段及其首个引号片段仍按本包的单行格式书写，后续引号行用于续接；
不因此宣称支持 GNU PO 的全部排版或词法扩展。
`t` / `tc` 是**无计数**查询：命中复数条目时取第 0 个形式，不保证它对应
`n=1`。需要按计数选译文时使用 `tn` / `tnc`，即使计数恰好是 1。
这沿用 [GNU libintl 的无计数查询语义](https://raw.githubusercontent.com/autotools-mirror/gettext/master/gettext-runtime/intl/dcigettext.c)，
不同于 [Python GNUTranslations 在复数条目上计算 plural(1)](https://github.com/python/cpython/blob/3.14/Lib/gettext.py#L416)。
C/XLang 的六形式边界与成对范例均固定此区别，不将 API 差异误作 BUG 改动。
四种查询的 `catalog` 必须是解析得到的 XRT object；传入 `null`、数字或其他类型时，C 返回 `NULL` 并设置 `XERR_TYPE`，XLang 抛 `TypeError`。只有合法目录中的消息未命中时才返回原文。
C 的 `tc` / `tnc` 若没有 `context` 或 `msgid`，不查找译文键，按原文规则
返回结果；这不免除目录类型、其余非空字符串和复数规则的检查。`NULL`
上下文与 `""` 不同：前者没有查询键，后者可以命中 `msgctxt ""` 条目。
借用接口仍直接返回目录或输入中的指针，拥有接口仍返回由 `xrtFree` 释放的
独立字符串；不将这项 C 可空指针规则扩成新的 XLang 语言语法。
复数公式的总递归深度上限为 48，括号、条件表达式和连续一元运算符共同计数；超限目录报格式错误，不绕过解析器的栈深限制。
复数头部的 `nplurals` 取值为 1..64，公式最长 256 字节；这是本包实现
限制，超限报格式错误，不是 GNU 格式本身的限制。
导入时会验证公式语法并抽样常见计数，但不能证明任意计数都不会除零；若某个计数触发公式错误，该次查询报 `ValueError`，其他计数的查询仍可继续。未选中的条件分支不执行除法。
PO 源文本与 MO 中的原文、译文必须是有效 UTF-8；无效字节及 PO 转义产生的无效字节均报格式错误。包不按目录头的 `charset` 转码；非 UTF-8 目录须在调用前转换为 UTF-8。
PO 字符串支持 C 风格的 `\a`、`\b`、`\f`、`\n`、`\r`、`\t`、`\v`、`\?`、`\"`、`\\`、1–3 位八进制及 `\x` 后连续十六进制数字；不支持 `\u` / `\U`。数字转义须产生 1–255 的字节值，`\0`、空 `\x` 和超范围值均报格式错误；连续字节最终还必须组成有效 UTF-8。PO 语法依据 [GNU gettext 对字符串的说明](https://www.gnu.org/software/gettext/manual/html_node/More-Details.html)。
MO 的原文和译文描述符长度不包含末尾 NUL；解析时会检查该终止字节，
缺失或落在文件末尾之外均按格式错误拒绝，而不是接受损坏的目录。
本包的 MO 查询模型只承载一个单数原文和一个复数原文；原文描述符内
出现第二个 NUL 分隔符时报告不支持的输入，不会只取第一段并丢弃其余原文。
单数 MO 译文的长度范围内不允许 NUL，避免查询结果作为 C 字符串返回时被截断；复数译文的 NUL 分隔符仍按格式使用。
PO 中 `#, fuzzy` 标记的普通译文不会进入目录；空 `msgstr` 及空复数形式查询时回退原文。空 `msgid` 的目录头即使带 fuzzy 标记也保留，以供读取复数规则。MO 中的空译文同样回退原文；这里遵循 [GNU gettext 对待审核与未翻译条目的口径](https://www.gnu.org/software/gettext/manual/html_node/Translated-Entries.html)。
同一目录内实际进入目录的重复消息键（含相同上下文）报格式错误，不再由后一个译文静默覆盖前一个；被跳过的 fuzzy 或空单数译文不占键。不同 `msgctxt` 的同名消息仍可共存。PO 与大小端 MO 遵循同一规则。
PO 文本、MO 文件路径、消息 ID、复数 ID 与上下文进入 C 字符串接口前
拒绝内嵌 NUL，避免静默截断；`parseMo` 接受的是二进制 MO 数据，其中
NUL 是格式的一部分，不受这一文本入口规则限制。
`0x04` 专用于上下文键分隔：PO 的 `msgctxt`、`msgid`、`msgid_plural`
及查询参数均不能自行包含它。MO 原文可含一个上下文分隔符，第二个及
复数原文中的分隔符会被拒绝；不支持把该控制字符当普通消息 ID 使用。
C 侧公开解析、加载和查询函数在新调用开始时清除本线程旧的 XRT 错误；
有效上下文参数下若查找键分配失败，返回 `NULL` 并保留内存错误，
不会伪装成未命中译文的原文回退。C 拥有型查询结果非空时由调用方 `xrtFree`；`*_borrowed` 的结果不得释放。
`loadMo` 的 XLang 入口复用标准库 `file.readBytes`，默认最多读取 64 MiB：
文件不存在、无权限或读取失败保留 `FileError`，MO 格式错误抛 `ValueError`。
`parsePo(text)` 与 `parseMo(bytes)` 也最多接受 64 MiB 输入；超限在解析
UTF-8 或 MO 头部前拒绝。C 三条入口均报告 `XERR_RANGE`，XLang 映射为
`RangeError`；超限后可继续解析正常目录。大输入回归单独放在包级门禁，
不加入日常目录 smoke。
原生 `parsePo` 只扫描至第一个 NUL 或 64 MiB 上限外一字节，不先对超限
输入执行无界 `strlen`；传入无终止字节的上限加一缓冲也会先返回范围错误。
包内 `test/input_limits.xl` 固定语言层 `RangeError` 与拒绝后的恢复；
`test/test_gettext.c` 固定恰好 64 MiB 的 PO 正边界，以及 PO/MO 超出
一字节时的 `XERR_RANGE`。两项随 Windows/Linux 包级门禁运行。
C 的 `xgt_load_mo` 使用 XRT 限额读取，同样最多接受 64 MiB；超限返回
`XERR_RANGE`，其他文件错误保留其原始类别；
格式错误仍为 `XERR_VALUE`，不把文件系统失败伪装成坏目录。

XLang 示例：`release/examples/extlibs/gettext.xl`。对应 C 示例：`release/extlibs/gettext/examples/gettext_c.c`。在仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/gettext/test/check.ps1` 可执行完整包级 C、XLang、双语输出及 AOT 验证。

新 `examples/layout.xl` / `examples/layout_c.c` 从路径加载目录，区分普通、
菜单、工具栏和空上下文，并按计数选复数。两种语言共用
`examples/layout.expected.txt`，C 借用结果只在目录存活期间使用；解析用的
文本读入后即可释放，不为打印结果额外创建拥有型副本。
在仓库根目录运行：

```powershell
./release/xl.exe run release/extlibs/gettext/examples/layout.xl -- release/extlibs/gettext/test/po_layout.po
./release/xl.exe run release/extlibs/gettext/examples/layout_c.c -- release/extlibs/gettext/test/po_layout.po
```

将 PO 用 GNU `msgfmt -o catalog.mo ...` 编译后，在末尾追加 `mo` 参数
即可用同一范例读取 MO；独立程序的参数也相同。Linux 默认门禁在存在
`msgfmt` 时自动执行这条链，没有工具时仍显式报告 SKIP，不增加用户运行依赖。
Windows 默认门禁现在在 `build/test/gettext_guard_*.json` 中保留原始命令、
输出和指纹，成功才清理对应构建目录；Linux 成功/失败都检查输入指纹，
失败保留现场及真实退出码。缺少 C 编译器的定向诊断复验仍为退出 127。

2026-10-25：边界测试新增目录及原文离开局部作用域后的译文/未命中回退保留验证，
确保借用的 C 返回指针已转换成独立的 XLang 字符串。Windows 完整门禁通过；
Linux C ASan/UBSan 通过；当次语言门禁受下述旧 Linux `xl` 的 `cbytes` 缺口阻断。

2026-10-24 使用更新后的 Windows 发布版复验：默认包门禁完整通过，含
严格 C、PO/MO 边界、分配故障、双语范例和公开范例 AOT。旧 Windows
发布版的共享符号阻断已解除。Linux `check.sh` 先完成 C ASan/UBSan，
随后现有 10 月 6 日的 `release/xl` 在 `src/gettext.xl` 的 `cbytes`
声明处报“未知类型”；当时需重建 Linux 发布版后重跑完整语言门禁。
这不构成 `gettext` 包源码的语法回退要求。

以下为历史验证记录；共享缺符号的“当前”表述均指记录当时的旧发布版。
2026-10-22：包内 C/XLang 成对范例新增 `tnc` 上下文复数、`parseMo` 固定二进制夹具和 `loadMo` 文件入口，现覆盖七个公开语言入口；C 范例还展示拥有型 MO 文件字节与目录值的释放。两侧各自核对译文，再比较完整输出。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh`（含 C ASan/UBSan）通过；库实现/API 未改，完整 AOT 仍受下述共享缺符号阻断。
Windows 门禁每次使用独立的 `build/test/gettext_guard_*` 目录编译 XRT、C 测试和范例，结束时清理本次产物；并行门禁不会复用旧对象或互相覆盖。
当前门禁只在成功后清理，失败保留目录供排查；运行前后核对包、夹具、
XRT/声明头及实际 CLI/运行库指纹。PATH 中有多个 GCC 时只选第一个应用路径，
不把多个命令结果拼接成无法执行的文件名。`-SkipAot` 仍不能替代完整验收。
当前默认门禁的 C/JIT 阶段通过，首个边界 AOT 构建仍缺共享 `xl6NativeHandleTraceLeaf`；脚本按退出码报告此失败并清理本次目录，不能据此签收完整 AOT。
对应的 C 示例可单文件运行：`release/xl.exe run release/extlibs/gettext/examples/gettext_c.c`。示例包含目录和复数表达式两个实现文件；正式扩展包仍按 manifest 分别编译，并由 C 测试验证。
Linux C sanitizer 与 XLang 对照门禁可用 `bash release/extlibs/gettext/test/check.sh` 复跑；
它从仓库根目录运行依赖固定 MO 夹具的包内边界用例，另跑重复键负例，并逐字比较
C/XLang 范例输出。当前 Windows 严格 GCC、C/XL 边界、双语输出、边界 AOT、
Linux XLang 边界与 Linux ASan/UBSan C 门禁通过；带输出
范例 AOT 在 `xl6ValueDataSnapshotV1` 缺符号处阻断，见
`dev/extlib_blockers/text_snapshot_link.md`。完整 AOT 尚未验收。
`test/boundaries.xl` 与 `test/duplicates.xl` 均已登记到 `coverage.tests`；
边界用例的夹具路径以 仓库根目录为基准，可直接由目录级 smoke 运行。
`test/allocations.xl` 也已登记：PO/MO 解析与四种查询分别扫描至首个
无故障成功（最多 128 点），每个入口均须触发分配失败并报告
`MemoryError`，每次清除故障后立即重试。2026-10-21 的 Windows
`-SkipAot` 与 Linux 包级门禁均通过此项；不据此签收完整 AOT。
2026-10-16 移除包 C 源文件对编译器私有 `xl_runtime.h` 的依赖，改用公开 `xrt.h`；
Linux 门禁现在同时运行中央 XLang 导入用例，Windows C/XLang 输出对照与 Linux ASan/UBSan 均复跑通过。
包内负例还覆盖缺失文件、坏 MO 和有效 MO 的错误分类及失败后恢复；
新增 C 逐点分配故障扫描分别覆盖含上下文/复数的 PO 与二进制 MO 解析，
要求失败归类为 `MEMORY`、不返回半成品目录，故障清除后可正常解析；
Windows 包级与 Linux ASan/UBSan 门禁通过。
2026-10-16 增加重复消息键的 PO、大小端 MO C 负例和 XLang PO/MO 负例，先复现静默覆盖，再改为 `ValueError` / `XERR_VALUE`；Windows C/XLang 双语门禁、Linux ASan/UBSan、目录 smoke 2/2 通过。无文件路径依赖的 `test/duplicates.xl` 已登记目录 smoke；该新用例的 AOT 构建缺共享 `xl6NativeHandleTraceLeaf`，带输出范例仍缺 `xl6ValueDataSnapshotV1`，均未签收。
MO 终止字节的大小端 C 负例、无效 UTF-8 的 PO/MO C 负例，以及 XLang `parseMo`/`parsePo` 异常负例也已进入包级门禁。
另用六种复数形式的固定目录，在 C 与 XLang 两端核对嵌套条件、取模和
0、1、2、3–10、11–99、100 等分组；Windows 包级、Linux ASan/UBSan
及从 `release` 启动的无输出边界 AOT 均通过。
边界 AOT 需要从 `release` 启动，以保持夹具相对路径一致。
包 manifest 只声明原生实现实际用到的 XRT 值容器、整文件读取与 Unicode 模块；
同一边界 AOT 由 3,928,576 缩至 1,007,616 字节，运行结果不变。
这项裁剪不解除上述带输出示例的共享链接阻断。

可选的外部格式互操作门禁用 `polib==1.2.0` 独立生成 GNU MO，先由 Python
标准库 `gettext.GNUTranslations` 校验，再让 XLang 与 C 分别读取同一文件，
核对上下文、三种复数形式和 fuzzy 回退。先在独立环境安装 `polib`，然后在
仓库根目录运行：

```powershell
python -m venv build/test/gettext_external_venv
./build/test/gettext_external_venv/Scripts/python.exe -m pip install polib==1.2.0
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/gettext/test/check_external.ps1 -Python build/test/gettext_external_venv/Scripts/python.exe
```

此项只验证调用和 GNU MO 互操作；外部 Python 依赖不进入扩展包或发布物。

2026-10-16：连续一元运算符原未计入复数公式的递归上限；47 层可解析、48 层拒绝的 C/XLang 红绿回归已加入包内边界用例。Windows 严格 GCC/C/XL 双语门禁与 Linux ASan/UBSan 通过，带输出 AOT 的共享阻断不变。

2026-10-17：C 测试增加固定 GNU MO 样本的 57 个截断长度和 416 个单比特扰动输入，覆盖表偏移、长度及正文边界；Linux ASan/UBSan、包内 XLang 用例和双语范例门禁通过，未发现非法内存访问。该确定性扫描不代替完整模糊测试；带输出 AOT 的共享缺符号阻断不变。

2026-10-17：PO 文本解析也加入从有效复数目录出发的逐前缀截断和逐字节单比特扰动，覆盖目录头、上下文、原文、复数原文和译文。Windows 严格 GCC 包级门禁、Linux C ASan/UBSan、XLang 边界用例及 C/XLang 范例输出对照通过；扫描未发现包内缺陷，不替代完整模糊测试或上述 AOT 验收。

2026-10-18：小端/大端 MO 负例复现原文描述符中第三段被静默忽略；
现将超出单数/复数双段模型的目录明确拒绝。Windows 严格 C/XLang、
双语范例及 Linux ASan/UBSan/XLang 对照门禁通过；完整 AOT 阻断不变。

2026-10-20：原生 PO 入口的 64 MiB 限额检查改为有界逐字节扫描，
新增上限外无 NUL 缓冲的 C/ASan 回归。Windows 与 Linux 包级 C/XLang
门禁均通过。C 文件用例现接收单次独占工作目录，Windows 不再复用
固定对象或测试文件，Linux 也不再创建仓库内固定目录；失败后清理本次
产物。默认 Windows 门禁仍在首个边界 AOT 构建处缺共享
`xl6NativeHandleTraceLeaf`，尚未完成整包发布验收。

2026-10-23：查询原将非对象 `catalog` 当成未命中，静默返回原文。现由共用查询出口检查类型；C 对 `t/tn/tc/tnc` 及空指针、XLang 对四种公开查询分别固定 `XERR_TYPE` / `TypeError`，合法目录的原文回退不变。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh`（C ASan/UBSan、XLang 双语对照）通过；完整 AOT 仍未签收。
