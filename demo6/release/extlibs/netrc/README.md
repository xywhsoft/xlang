# netrc

2026-10-26 当前发布物复验：Windows 默认包门禁的严格 C、TLS、
双语范例、UTF-8 文件保存与 Python `.netrc` 互通，以及语言边界
均通过；具名类型用例在直接用语言字典构造 `Netrc` 时报告
`ConfigError: netrc document must be a dict`。既有最小复现于当前
Windows/Linux 发布物均失败，见
`../../../dev/extlib_blockers/foreign_var_container_projection.md`。
后续分配与 AOT 门禁未执行，整包未签收；不在包侧识别私有句柄。
下方旧共享链接缺符号仅为历史状态。

2026-10-24 当前 Windows 发布版复验：严格 C/原生边界、C/XLang 范例、
UTF-8 文件往返与 Python `.netrc` 互通均通过；默认门禁在具名类型
用例的语言字典 `stringify()` 处失败。JIT 与独立 EXE 的最小复现见
`../../../dev/extlib_blockers/netrc_constructor_literal.xl`，原因范围见
`../../../dev/extlib_blockers/foreign_var_container_projection.md`。
本轮完整门禁和 AOT 未签收，下列历史结果不能替代当前发布物验收。

读取、修改和保存 UTF-8 `netrc` 凭据文件。XLang 使用 `import netrc`，
C 使用 `include/xrt_netrc.h`。示例见 `release/examples/extlibs/netrc.xl`
与 `examples/basic.c`。范例同时展示具名主机、`default` 回退和 `macdef`
宏，通过 `hostNames/macroNames` 核对条目，并以 `stringify/parse` 验证内存往返；
再以未闭合引号展示 `errorCode/errorLine/errorColumn/errorOffset` 的精确诊断及
成功调用后的错误清除。只显示用户名、条目数量和语法错误位置，不显示密码或
序列化后的凭据文本。
两份范例可选传入一个目标文件路径，此时用包级 `save/load` 保存并读回
用户名和密码，验证往返但仍不打印密码；不传路径时仅在内存中解析。
不要将真实凭据写进命令行参数或日志。

## 数据与所有权

`netrc.parse(text)` / `netrc.load(path)` 返回独立的 `Netrc` 对象；
`Netrc` 已导出，可写 `netrc.Netrc config = netrc.parse(text)`，并在函数
参数中显式标注类型。直接调用 `netrc.Netrc(value)` 时，`value` 必须是
含 `hosts`、`macros` 两个字典的文档；形状不符立即抛 `ConfigError`。
保存前仍由 C 写出器校验条目和值的完整格式。
`hosts`、`macros` 为可访问的字典，`authenticators(host)` 优先返回精确主机，
不存在时返回 `default`，再不存在返回 `null`。
`hostNames()`、`macroNames()` 返回名称数组，`stringify()` 返回文本，
`save(path)` 保存到文件。C 版 `xrtNetrcParseMemory` / `xrtNetrcLoad`
返回调用方拥有的 `xvalue*`，须 `xrtValueRelease`；
`xrtNetrcStringify` 返回调用方拥有的 `xbuffer*`，须 `xrtBufferDestroy`。
XLang 的 `stringify()` 使用 `xnetrc_stringify_text` 直接接管该文本缓冲的
底层分配，不再通过 `Buffer.text()` 复制；C 侧调用文本桥接后须用
`xrtFree` 释放。补齐终止字节时，容量不足的缓冲仍可能扩容。
解析 `login`、`account`、`password` 时，词法缓冲直接移交给认证字段的
`xvalue`，不再先复制 token、入字段时再复制一次。空引号字段与长 UTF-8
字段保持原有长度语义；`xrtValueStringTake` 可能因调整容量而重新分配，
不保证底层绝对零搬移。主机名等仍由文档持有，不能移交借用指针。
从文档字典读取的子值是借用的，文档释放后不可继续使用。
C 流入口 `xrtNetrcParse` / `xrtNetrcWrite` 的 `bTakeOwnership=true` 表示
无论正文成功或失败都关闭并释放流；若正文成功而关闭失败，返回
`NULL` / `false`，关闭时内存不足报 `MEMORY`，其他关闭失败报 `IO`。
`bTakeOwnership=false` 时流仍由调用方持有，
不会被库提前关闭。
正文已失败且关闭也失败时，保留正文的首个错误。

顶层只接受 `machine`、`default`、`macdef`。凭据字段为 `login`、
`account`、`password`，兼容字段名 `user`、`passwd` 并归一到前述名字。
重复项默认拒绝，C 侧可通过 `xrtnetrcoptions.bStrictDuplicates=false`
使用后者覆盖。关键字作为普通值时需加引号；序列化器自动处理。
序列化器始终先写具名 `machine`，最后写 `default`，即使输入文档先定义了
`default`；这样生成的凭据文件也符合外部 `.netrc` 读取器的匹配顺序要求。
保留宏行原文，空行结束宏。NUL（包括注释内）、token 中的换行、`macdef` 名称后
的意外内容均拒绝，不静默丢弃。支持 UTF-8 BOM 与 CRLF；单独 CR 也结束注释行，
不会吞掉后一行的凭据声明。
宏正文允许 CRLF 行尾，但拒绝行内独立 CR；解析成功的宏必须能够由
`stringify` 写出并再次读回。C 边界测试从合法凭据与宏派生截断和字符替换
矩阵，固定检查拒绝错误类别及成功结果的二次规范化往返。
从 C 构造的文档写出时也检查 UTF-8；不会保存出本库随后无法读取的凭据文本。
文档顶层只允许 `hosts`、`macros`，每个主机凭据只允许
`login`、`account`、`password`；写出时遇到额外字段会报类型错误，
不会成功保存却悄悄丢失数据。
手工构造的文档中，主机名和宏名也必须非空；`stringify/save` 会拒绝
空名称，避免写出随后无法解析的凭据文件。

解析及输出受 `xrtresourcelimits` 控制；XLang `parse/load/stringify/save`
均提供对应的限额可选参数。XLang 失败抛 `ConfigError`，可读
`errorCode()`、`errorLine()`、`errorColumn()`、`errorOffset()`。
XLang 限额参数必须非负；显式传入 `0` 与 C 侧一致，表示该项不限，
省略参数则使用公开函数签名中的默认上限。文件路径不能包含内嵌 NUL，否则在交给
`const char*` 原生入口前抛 `ConfigError`，不会误读或覆盖截断后的路径。
`parse(string)` 将 UTF-8 数据和字节长度直接交给原生解析器，不再先建立 XLang
Buffer 副本；原生层在读取前检查 `maxInput`，并完整验证文本中的内嵌 NUL。
模块级错误位置函数记录最近一次原生操作；XLang 参数预检抛错不会覆盖它们。
词法器已经接受的单独 `\r` token 分隔符现在计作新行，`\r\n` 仍只计一行；
宏正文中的行内独立 `\r` 依旧按语法错误拒绝，不借此扩大宏语法。
原生读取器的输入超限、分配失败、实际读取失败分别报告 `LIMIT`、
`MEMORY`、`IO`；内存读取器创建失败与文件打开失败也分别归入
`MEMORY`、`IO`，不混同于调用者传入空读取器的 `ARGUMENT`。
C 的 Reader 输入限额只计算**当前游标之后**的字节：若 `Size` 与 `Tell`
都可用，预检按剩余长度判断；只支持其中一个或都不支持时，仍由限额
读取器按实际读到的字节裁决，不按文件总大小误拒绝已 seek 的流。
自定义 Writer 的写入或刷新回调失败时，若本次 XRT 错误为内存不足，
`xrtNetrcWrite` 报告 `MEMORY`；其他写入或刷新失败报告 `IO`。

`save` 使用同目录 0600 临时文件写满、刷新、关闭后替换目标，失败删除临时文件；
POSIX 下新文件与替换后的文件均只允许所有者访问。Windows 下沿用
XRT 文件 API 和目录 ACL；调用方仍需保证目录访问策略适当。此库不会
自动发现用户主目录的 `.netrc`，也不会自动把凭据送入网络请求。
`xrtNetrcWrite` 的 writer 以及 `xrtNetrcSave` 的目标路径不能为空；
此类调用在序列化或创建临时文件之前返回 `ARGUMENT`。有效路径上的
原子保存与所有权规则不变。
传入 C 选项结构时须先调用 `xrtNetrcOptionsInit`；结构本身或内嵌的
`xrtresourcelimits` 版本、长度无效会报告 `ARGUMENT`，不会静默回退到
默认限额。传入 `NULL` 才表示使用默认选项。
`netrc` 不使用通用限额的深度、压缩比或资源标志；初始化函数将前两项
置零，调用方设置它们或任何包级/内嵌保留字段都会得到 `ARGUMENT`，
不会被静默忽略。输入/输出字节、单项、条目和节点限额仍按原合同生效。

## 定向验证

在 `demo6` 目录运行 `release/extlibs/netrc/test/check.ps1` 和
`bash release/extlibs/netrc/test/check.sh`。Windows 门禁覆盖
GCC 严格编译、C 边界、XLang 边界、具名类型与直接构造结构检查、
双语范例结果及 AOT；开发时可用
`-SkipAot` 跳过当前主线仍有缺符号的 AOT 发布链路。
Windows 门禁每次使用独立构建目录，并在该目录运行会写文件的 C 边界用例；退出时仅清理本次产物，避免并行运行互相覆盖。
包内 `test/boundaries.xl` 已登记到 `coverage.tests`；在 `demo6` 目录运行
`tools/smoke_extlib_catalog.ps1 -Package netrc` 可连同原主用例一起复验。
Linux ASan/UBSan 门禁覆盖 C 边界与范例，Ubuntu 用例包含
覆盖旧宽权限文件后检查新文件权限为 0600；该用例在 Linux 原生临时目录
运行，不把 WSL `/mnt` 的 DrvFS 权限映射当作 POSIX 验收。
同一脚本需要 `release/xl`，运行中央与包内 XLang 边界，并将双语范例
与黄金输出逐字比较；不运行完整 AOT。
Linux 脚本还在独立临时目录运行双语文件范例，核对保存内容相同且两份
文件模式均为 0600。Windows `check.ps1 -SkipAot` 在本次测试专用路径下
运行 C 文件范例和 XLang 中文路径文件范例，核对输出与文件哈希，随后
删除两份临时凭据文件。
两平台门禁在安装 Python 3 时还用其标准库 `netrc` 独立解析双语生成的
固定测试文件，检查主机、用户名、密码和 `deploy` 宏；没有 Python 时会提示跳过，
不能将跳过记作跨实现格式验收。测试不会读取用户主目录的凭据文件，
也不会输出测试密码。
同一门禁还验证保存到符号链接路径时不会改写链接指向的外部文件，
替换后的凭据文件为私有普通文件。
读取器边界测试还覆盖未知长度输入的限额、读取失败、成功后的错误槽清理，
以及逐点分配故障时的错误分类与释放。长 token 迫使词法缓冲扩容；
在 C 选项中 `iMaxInputBytes = 0` 表示不限输入长度；非空 Reader 不会被误判超限，超过平台 `size_t` 的限额也不会截断为较小值。
注入该分配失败时报告 `MEMORY` 和 token 位置，不误报为缺少字段值。
序列化故障注入另以 600 字节凭据覆盖输出缓冲扩容，要求每次失败
返回 `MEMORY`、不交出半成品，并能在故障清除后正常写出。
扫描错误使用首个错误的类别、文本和位置；未闭合引号位于主机名、宏名或
字段值时，诊断仍指向该引号的实际列号，不被外层“缺少名称/值”覆盖。
此行为已由 C 三分支和 XLang 字段值负例固定。
非法 UTF-8 也报告首个错误字节的行、字节列和偏移；位置以去掉可选 BOM
后的文本为起点，CRLF 计一行。C 边界用例固定第二行错误字节的位置。
2026-10-15 补空 writer、空/缺失目标路径的 C 参数边界，先红后绿；
Windows C/XLang 包级门禁和 Linux ASan/UBSan 通过。完整 AOT 仍待
主线共享快照符号修复。
2026-10-15 补保存阶段逐点分配故障：原先父目录/临时文件准备的内存不足
被误报为 `IO`，现按 XRT 实际错误归为 `MEMORY`。C 回归要求命中序列化
之后的分配失败、保留原凭据文件，并在注入结束后成功保存。Windows 包级
C/XLang 与 Linux ASan/UBSan、0600 权限门禁通过；完整 AOT 阻断不变。
2026-10-15 补 0 输入限额的非空 Reader 红绿回归，修正 C 限额传递给 XRT Reader 时的 0 字节上限误判；Windows C/XLang 与 Linux ASan/UBSan 包级门禁通过，AOT 主线阻断不变。
2026-10-16 补手工构造文档中的空主机名与空宏名负例：写出器原先成功
生成解析器会拒绝的文本，现统一在遍历入口报 `TYPE`。C 两分支先红后绿，
XLang 空主机名负例、Windows C/XLang 与 Linux ASan/UBSan 门禁通过；完整 AOT
共享阻断未变。

2026-10-18：拥有型 Reader/Writer 原在正文成功后忽略关闭失败，可能
误报解析或写入成功。自定义失败流的 C 用例先红后绿；现关闭失败报告
`IO`，正文失败时保留原错误，借用流不被提前关闭。Windows 严格 GCC、
C/XLang 边界与双语范例，以及 Linux ASan/UBSan 和 XLang 对照门禁通过；
当前 `check.ps1` 不跳过 AOT 时仍在 `xl6ValueDataSnapshotV1` 缺符号处停止。

2026-10-22：拥有型 Reader/Writer 在正文成功、关闭回调报告 `XERR_MEMORY`
时原误报 `IO`。自定义失败流的 C 用例先红后绿；现按关闭故障实际类别
报告 `MEMORY` / `IO`，正文已有错误仍优先保留。Windows 聚焦门禁的
C/XLang、双语范例与 Python 文件互通，以及 Linux C ASan/UBSan 和语言门禁
通过；完整 AOT 本轮未复验，发布签收状态不变。
同日补齐双语 `save/load` 教学范例：C 范例在读取前检查解析结果，失败
统一释放对象；显式传路径时两侧核对用户名和密码但只输出用户名。
Windows C/XLang 文件往返（XLang 中文路径）、Linux 双语同内容与 0600
权限门禁通过。完整 AOT 共享阻断不变。
2026-10-16 补 C 选项版本和长度边界：无效选项原静默变为默认配置，可能
绕过调用方设置的输入/输出限额；解析与写出共用校验并报 `ARGUMENT`。
有效选项的限额行为和嵌套资源结构无效分支也纳入 C 回归。
2026-10-16 Linux XL 导入复现 LP64 ABI 声明冲突：包内 `c_ulonglong` 与
XRT 的 `uint64` 并非同一 C 类型。所有原生入口及调用转换统一为 `uint64`，
Linux 门禁加入中央 XL 导入测试；Windows C/XL 与 Linux ASan/UBSan 复跑通过。
同日 Linux 门禁加入包内边界及 C/XLang 范例逐字对照，完整 C sanitizer
与语言侧门禁通过；完整 AOT 的主线缺符号阻断不变。

2026-10-17 修正 XLang 桥接把显式 `0` 限额误变成默认上限的问题：
重复凭据条目超过默认 100000 项的红绿用例通过。内部头的 XRT 模块宏
改为条件定义；Windows/Linux 门禁加入清单宏严格编译，避免构建时重定义。
Windows C/XL、Linux ASan/UBSan 与双语范例复验通过；完整 AOT 阻断不变。

2026-10-17 修复注释内 NUL 被词法器跳过的问题：长度感知的 C 输入
现在对整行注释和行尾注释中的 NUL 均报 `SYNTAX`，位置指向该字节，
不再接受带隐藏尾部的凭据文本。Windows 严格 GCC/C/XL 与 Linux
ASan/UBSan、双语范例门禁通过；完整 AOT 的共享阻断不变。

2026-10-17 补齐 `macdef` 标题后行尾注释的同类边界：此前该分支仍会
跳过 NUL 并接受隐藏后缀。C 负例先复现后修复，XLang 负例也固定该行为；
Windows 包级 C/XLang、Linux ASan/UBSan 与双语范例通过。完整 AOT
共享阻断未变。

2026-10-18：定点变异矩阵复现宏正文行内 CR 被解析器接受、写出器拒绝，
使解析成功的文档无法保存。解析器现只将行尾 CRLF 视为换行，行内 CR
报告 `SYNTAX` 和准确位置；C/XLang 同时验证合法 CRLF 保持可用。
Windows `-SkipAot` 与 Linux C ASan/UBSan、XLang、双语文件往返门禁通过。
完整 AOT 的共享阻断未变。

2026-10-19：C 选项原接受却忽略包级保留字节、内嵌资源保留字段及
资源标志；通用限额的深度和压缩比也带非零默认值却不适用于 netrc。
五类 C 负例先红后绿，现初始化不适用项为零并统一拒绝显式设置。
Windows `-SkipAot` 严格 C/XLang/双语文件门禁、Linux C ASan/UBSan、
0600 权限与 XLang 对照均通过。完整 AOT 共享缺符号阻断不变。

2026-10-20：Windows 包级脚本中的中文路径字面量原为无 BOM UTF-8，
PowerShell 5.1 在 GB2312 系统代码页下会误读成乱码，导致“中文路径”
用例没有验证原定名称。脚本现带 UTF-8 BOM；同一包级 `-SkipAot`
门禁通过 C/XLang 文件内容对照、中文路径及全部聚焦边界。库实现未改，
完整 AOT 的共享阻断不变。

2026-10-21：`test/test_netrc_boundaries.c` 原在调用方当前目录的固定
`build/test/netrc_*` 路径写入并删除凭据夹具；正式门禁有外层隔离，
直接运行 C 测试仍可能覆盖已有文件。测试入口现自行创建独占系统
临时目录，进入后运行文件与链接负例，恢复工作目录后只清理本次目录。
Windows `-SkipAot`、Linux C ASan/UBSan 与 XLang/Python 联合门禁，
以及仓库根目录直接 C 包运行器均通过；固定路径与系统临时目录无本次
残留。netrc 实现未改，完整 AOT 的共享阻断不变。

2026-10-22：C/XLang 成对范例增加固定语法错误，逐项核对错误码 2、
第 1 行第 17 列、字节偏移 16；后续合法解析必须清除最近错误。
共同黄金输出只记录位置，不包含密码。Windows `test/check.ps1 -SkipAot`
的严格 C、双语、文件/中文路径、Python 互通与故障测试，以及 Linux
`test/check.sh` 的 C ASan/UBSan、XLang 和文件互通门禁通过。
解析实现/API 未改；完整 AOT 仍受共享主线缺符号阻断，不签收整包发布级。

同日补流式 writer 错误分类：自定义 Writer 的 `Write` 或 `Flush`
回调报告 `XERR_MEMORY` 时，`xrtNetrcWrite` 原统一返回 `IO`，使调用方
无法区分资源耗尽和设备故障。C 回归先红后绿，现对应返回
`XRT_NETRC_ERROR_MEMORY`；普通 `XERR_IO` 和关闭失败仍报告 `IO`。
字符串化成功后会先清理旧线程错误，分类只依据本次流操作。
Windows 严格 C/ABI、C/XLang 边界、Python 文件互通与 Linux C
ASan/UBSan、XLang 对照门禁通过；本次跳过完整 AOT，整包仍未发布签收。

2026-10-22：新增原生绑定错误槽的双线程交错测试。线程 A 解析失败后，
线程 B 成功解析并清理自己的错误；线程 A 的错误类别与行号仍保持不变，
主线程也没有继承两者的错误。Windows `-SkipAot` 与 Linux C ASan/UBSan、
XLang/Python 文件互通门禁通过。仅补测试，未改公开 API 或解析器；
完整带输出 AOT 的共享阻断仍未签收。

同日补充 XLang 公开 `parse` / `stringify` 的前 80 个分配位置故障扫描：
触发失败时必须抛 `MemoryError`，成功时凭据内容不变；清除故障后可
继续解析和序列化。固定假凭据只在内存中处理，不写入测试文件或输出。
用例已登记包清单并进入 Windows/Linux 门禁；Linux C ASan/UBSan、
双语范例与 Python 文件互通同次通过。原生实现未改，完整 AOT 仍未签收。
