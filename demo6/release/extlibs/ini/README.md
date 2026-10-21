# ini

INI 配置解析与写入库。C 端公开 `xrtIni*` API；XLang 端提供 `Config` 类。
支持节、注释、大小写选项、重复键检查、`${key}` / `${section.key}` 插值、
类型化读取、修改、文件读写及输入输出限制。文本和路径使用 UTF-8。
解析器拒绝内嵌 NUL 字节，避免 C 字符串模型静默截断；XLang 的节名、键名、
值和路径在进入原生 API 前也会拒绝内嵌 NUL。资源限额不接受负数，
`maxDepth` 须落在 `uint32` 范围内。
`parse(string)` 直接把 UTF-8 数据及其字节长度传给原生解析器，不额外创建
XLang Buffer 副本；原生读取器在复制正文前检查 `maxInput`，并继续拒绝内嵌 NUL。
向新节写入失败会撤销本次创建的节，保留原配置与错误信息。
`Config.save` / `xrtIniSave` 先写目标目录中的临时文件，再由 XRT 替换目标；
写入中途失败不会截断已有配置文件。
`set()` 可以在内存中保留任意合法 UTF-8 文本；`stringify()` / `save()`
只写当前 INI 语法能无损表示的内容。节名、键名及值含换行或首尾空白等
歧义时，明确报 `XRT_INI_ERROR_FORMAT`（XLang 抛 `ConfigError`），
不会写出一份再次读取后改变数据的文件。多行值可用 `\n` 分隔非空续行，
续行不能以 `#` 或 `;` 开头；全局节始终先于具名节写出。

## XLang 用法

```xlang
import ini

auto config = ini.parse("[server]\nport=8080\n")
int port = config.getInt("server", "port")
config.set("server", "port", "9090")
string text = config.stringify()
```

`ini.create`、`ini.parse`、`ini.load` 返回 `Config`。格式错误、重复键、
缺少文件或键、类型转换失败、写入失败和资源限制均抛出 `ConfigError`；
底层分配失败抛出 `MemoryError`，不混作普通配置错误；
缺失键可用 `getOr` 提供默认值。`errorCode`、`errorLine`、`errorColumn`、
`errorOffset` 保存最近一次错误信息。对象销毁时释放原生句柄。
`getFloat` 固定按点号十进制解析，不受进程 `LC_NUMERIC` 影响；逗号小数
不是合法浮点文本。一次转换失败后，下一次成功操作会清除旧错误状态。
`getInt` 使用 XRT 的严格十进制整数解析，必须消耗整个值；前导空白
不会被 CRT `strtoll` 悄悄接受。需要宽容输入时先明确清理文本。
`Config` 的 `*Native` 成员仅供类内部使用，包外应走上述公开方法，由包装层检查文本并接管原生结果。`test/private_native.xl` 是预期编译失败的访问权限负例。
`Config(string,size,...)` 的原生构造入口所在类不导出；
`test/private_config_constructor.xl` 是只编译、不执行的包外负例，
防止调用方绕过工厂伪造字符串长度。Windows/Linux 门禁均检查类型未导出。

## C 用法

包含 `include/xrt_ini.h`，并与 XRT 一起编译 `xrt_ini_model.c`、
`xrt_ini_parser.c`、`xrt_ini_writer.c`。先用 `xrtIniOptionsInit` 设置默认选项，
再通过 `xrtIniError` 检查错误。`xrtIniGet` 返回归调用方所有的字符串，
须用 `xrtFree` 释放；`xrtIniGetRaw` 返回借用视图。配置用 `xrtIniDestroy` 释放。
`xrtIniParse` 接受未知总长度的 Reader；读取超限、分配失败和实际读取失败
分别记为 `XRT_INI_ERROR_LIMIT`、`XRT_INI_ERROR_MEMORY`、`XRT_INI_ERROR_IO`。
`xrtIniLoad` 的空路径是 `XRT_INI_ERROR_ARGUMENT`；有效路径打不开时是
`XRT_INI_ERROR_IO`，打开或读取过程中的内存失败是 `XRT_INI_ERROR_MEMORY`。
`xrtIniParse` / `xrtIniWrite` 的 `bTakeOwnership=true` 会在退出时关闭流；
正文成功但关闭失败时报告 `XRT_INI_ERROR_IO`，已有格式、限额或写入错误
保持首因。`bTakeOwnership=false` 时流由调用方关闭，不被库提前释放。
传入 C 选项时必须先调用 `xrtIniOptionsInit`；顶层选项及内嵌的
`xrtresourcelimits` 各自校验版本和结构长度，顶层及限额保留字段必须为零。无效结构报告
`XRT_INI_ERROR_ARGUMENT`，不会退回默认限额后继续解析。
`iMaxEntries` 限制整个配置中所有节的键总数，不含节本身；删除键后释放额度，
覆盖已有键不新增计数。`iMaxCompressionRatio` 与 `iFlags` 不适用于 INI：
OptionsInit 将倍率置零，显式设置这些字段会返回 `XRT_INI_ERROR_ARGUMENT`。
`xrtinioptions.tLimits.iMaxInputBytes = 0` 表示不限制输入长度；非空输入仍须正常读取。大于平台 `size_t` 范围的 C 限额不会截断为较小值。
仅 `Size` 探测不受支持而后续读取成功，不会留下 XRT 错误。
直接值复制、插值输出扩容及最终结果复制的分配失败均报告
`XRT_INI_ERROR_MEMORY`；`NULL` 结果不再保留成功错误码，也不误报输出限额。

在 `demo6` 目录运行定向验证：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/ini/test/check.ps1 -SkipAot
bash release/extlibs/ini/test/check.sh
```

Windows `-SkipAot` 门禁覆盖严格 GCC、C/XLang 边界与双语同输出范例。
门禁每次使用独占构建目录，失败和成功退出时只清理本次产物；不会复用
旧 `build/test/ini_guard` 中的可执行文件。
Linux 门禁使用 ASan/UBSan 验证 C 解析及文件路径边界，另以内嵌 TCC
执行 C 用例，运行中央/包内 XLang 边界并逐字节核对双语范例。带输出的 XLang
当前无输出 AOT 核心先缺 `xl6NativeHandleTraceLeaf`，独立 `Buffer`
最小复现同错；带输出范例还可能受 `xl6ValueDataSnapshotV1` 阻断，
不能据此宣称完整发布验收。
包内 `test/boundaries.xl` 已登记到 `coverage.tests`，统一扩展库 smoke 可发现；
2026-10-16 从 `demo6` 工作目录单独运行通过，不代表本轮重跑了 C/Linux/AOT 门禁。

2026-10-17：修复 Linux LP64 导入 ABI：XRT `uint64` 参数使用 XLang
`uint64`，原生 `uint32` 深度参数使用明确的 `c_uint`；此前 `c_ulonglong`
与 XRT `uint64` 在 Linux 下形成冲突原型。包内 XLang 边界追加中文
路径的 `save`/`load` 往返及临时文件清理。Windows 包门禁和 Linux
GCC/内嵌 TCC、中央与包内 XLang 测试及双语范例对照均通过。当前
无输出 AOT 核心与独立 `Buffer` 最小复现均缺 `xl6NativeHandleTraceLeaf`；
本包不修改编译器主线，待修复后重跑完整门禁。

2026-10-15：保存文件改用 XRT 原子写入；Linux 限制文件大小的故障用例
先复现旧文件被截断，再验证修复后旧内容保持不变。Windows 严格 GCC、
C/XLang 边界与双语范例、Linux ASan/UBSan 和裁剪 AOT 核心通过。

2026-10-14：新增 C 逐点分配故障扫描，覆盖上述三条取值路径；Windows 严格 GCC、C/XLang 包门禁及 Linux ASan/UBSan 通过。完整 AOT 阻断不变。

2026-10-15：浮点转换从 CRT `strtod` 改为 XRT 严格解析；Windows 德语
小数区域设置下 `1.25` 失败、`1,25` 误接受的 C 用例先红后绿。
同一回归还发现成功调用残留上次 XRT 错误，现由配置操作的统一清错入口处理。
Windows C/XLang 包级、Linux ASan/UBSan 与 `test/aot_float_core.xl`
裁剪 AOT（798,208 字节）通过；完整带输出 AOT 的共享阻断不变。
2026-10-15：修复 C 输入限额为 0 时非空 Reader 被当作超限；未知长度 Reader 的 C 负例先红后绿。Windows 严格 GCC/C/XLang 包门禁和 Linux ASan/UBSan 通过；完整带输出 AOT 阻断不变。

2026-10-17：C 负例复现 `getInt(" 42")` 被 `strtoll` 接受，而浮点与布尔
读取要求完整值。改用 XRT 严格整数解析，补 C/XLang 的拒绝与修正后恢复
用例，并声明整数模块依赖。Windows `-SkipAot` 严格 C/XLang、Linux
ASan/UBSan/XLang 与双语范例门禁通过；默认 AOT 再次实测仍缺
`xl6NativeHandleTraceLeaf`，未签收发布级别。

2026-10-18：拥有型 Reader/Writer 原在正文成功后忽略关闭失败。
自定义失败流的 C 用例先红后绿；现报告 `IO`，借用流保持调用方所有权，
正文失败且关闭也失败时保留原始格式或限额错误。Windows 严格 GCC、
C/XLang 边界和双语范例，以及 Linux ASan/UBSan、内嵌 TCC 与 XLang
对照门禁通过。当前完整 `check.ps1` 仍先缺 `xl6NativeHandleTraceLeaf`。

同日补 C 选项 ABI 校验：原先只检查顶层版本/大小，内嵌资源限额的
版本或结构长度失配仍被接受。`create`、`parse` 两条负例先红后绿，
合法限额仍正常生效；Windows 严格 GCC/C/XLang/双语及 Linux
ASan/UBSan、内嵌 C 和 XLang 门禁通过。AOT 主线阻断不变。

2026-10-18：补 INI 写出无损边界。C 用例先复现尾随换行静默丢失；
现在对无法无损表示的节名、键名、值在写出时报告 `FORMAT`，保留
`set()` 的内存行为；后创建的全局节也会先于具名节输出。
Windows 严格 C、XLang 边界和双语范例及 Linux ASan/UBSan、内嵌 C、
XLang 对照门禁均通过。完整 AOT 仍待共享主线缺符号修复。

同日补成对范例：C/XLang 均验证并输出修改 `port` 后重新插值得到的 URL，
避免仅凭相同的主机端口输出掩盖语言侧插值未复验。Linux 包级门禁改用
本次运行专属的临时目录，退出时清理，不与并行检查争用固定构建文件。

2026-10-19：C Reader 输入限额按当前读取位置到结尾的剩余字节数计算，
不按 Reader 总长度计算。带前缀 Seek 的合法输入先红后绿；上限少一字节
仍报 `LIMIT`，只有 `Size` 而没有 `Tell` 的 Reader 由读取阶段执行限额。
Windows 严格 GCC/C/XLang 和 Linux ASan/UBSan、双语范例门禁通过；
完整 AOT 的共享主线阻断不变。

2026-10-19：内嵌资源限额的非零保留字段原被 `create`、`parse` 接受；
C 负例先红后绿，现由共用选项复制入口拒绝，不更改结构大小策略。
Windows 严格 C/XLang 与双语范例、Linux C ASan/UBSan 和 XLang 联合门禁通过；
完整 AOT 阻断未变。

2026-10-19：C 选项边界新增总键数限制，以及对无用倍率、未知标志和顶层保留字段的
明确拒绝。Windows 严格 C、C/XLang 边界、共享用例和双语范例通过；Linux C
ASan/UBSan 与 XLang 对照门禁通过。当前 Windows 核心 AOT 通过，边界目标构建
仍缺共享主线符号 `xl6NativeHandleTraceLeaf`，不签收完整 AOT。

2026-10-21：首个键前带注释的默认节新增逐点分配故障扫描。原先注释缓冲和注释复制
失败可能被吞掉，内存 Reader 创建失败还会误报参数错误；现在均报告内存错误，不把
失败结果当成空注释继续解析。扫描确认无故障时键值和注释均保留。Windows 严格 C、
C/XLang 边界及双语范例、Linux C ASan/UBSan 与 XLang 对照门禁通过；完整 AOT
的既有主线阻断未变。

同日补齐跨层错误类型：配置对象的 `XRT_INI_ERROR_MEMORY` 原会把 XRT 线程错误槽
写成 `XERR_STATE`。现保持为 `XERR_MEMORY`，C 逐点故障测试同时检查对象错误码
与线程错误类型，XLang 用例逐点确认 `ini.parse` 失败抛 `MemoryError` 且故障清除后
可恢复。Windows/Linux 包级 C、XLang 与双语门禁通过；不改变其他配置错误映射。

同日收紧文件入口：不存在的文件原被 `xrtIniLoad` 当作无效 Reader，误报参数错误；
现在区分空路径、打开 IO 失败和内存失败。C 用例覆盖三类错误、UTF-8 路径往返与
逐点加载故障，XLang 用例覆盖缺文件异常。C 文件往返、格式拒绝保留旧文件和原子写
失败三组测试改用各自独占系统临时目录，只清理本次文件，不再写删固定 `build/tmp`
路径。Windows 严格 C、
C/XLang 边界与双语范例，以及 Linux C ASan/UBSan、XLang 对照门禁均通过；完整
AOT 的共享主线阻断未变。

2026-10-21：原子保存和流写入曾把底层 `XERR_MEMORY` 统一映射为 INI
`IO` 错误，XLang `Config.save` 因而把分配失败报成 `ConfigError`。C
保存逐点故障回归先红后绿；现于成功序列化后清理旧线程错误，再按本次
文件/流操作的原因区分 `MEMORY` 与 `IO`。C 流写入故障及旧错误污染
负例、XLang `save` 逐点故障与恢复均通过；现有真实 IO、原子保留旧文件、
双语范例和 Linux ASan/UBSan 门禁也通过。完整 AOT 的共享主线阻断不变。
