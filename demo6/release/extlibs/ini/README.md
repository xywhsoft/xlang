# ini

当前发布状态为 **R：本体验证通过，工具链风险未闭环**。本轮最后的
Windows 默认门禁与 Linux 新快照默认门禁均完整通过，但中途曾在 GCC
16.2.0 编译未修改的 XRT 翻译单元时发生一次 `Illegal instruction` 内部
错误；同参数单独复编与整套复跑均成功，原因仍未知。另一次 WSL
非正常重启使临时输入零字节化，该失效运行未计为包测试结果。详见
[原始证据与关闭条件](../../../dev/extlib_blockers/ini_gcc_ice_20261028.md)。
不修改主线、替换 XRT、换优化级别或借用旧对象来取得绿灯。

2026-10-28 插值引用分类补测：`${s.}`、`${.}` 和带前后缀的空键引用原
误报 MemoryError，现与 `${}`、未闭合引用统一报 INTERPOLATION / ConfigError。
仅拒绝空键，不把空节、空值或带点键一并禁用：`${.key}` 读取全局键；
按首个点分隔节名，`${s.a.b}` 的键为 `a.b`，`${s..}` 的键为 `.`。
缺键仍报 NOT_FOUND，循环引用仍报 INTERPOLATION，真实 OOM 仍报 MEMORY。
关闭插值时原文不解释。C 的 9 个合法、9 个错误对照及逐点 OOM/活动分配
恢复、语言消费正反例、新旧双语范例 JIT/AOT、严格 GCC 和 Linux ASan/UBSan
均通过默认门禁；此前工具链 R 风险未因此关闭。

## 2026-10-28 覆盖赋值与无损写回

修复三个 C/XLang 共用的包内错误，公开 ABI 不变：

- `strictDuplicates=false` 时覆盖较早的键，缩进续行曾错误追加到表尾的
  另一个键。内部插入函数现在直接返回实际插入/更新的条目，不增加二次
  查找、排序或值复制。覆盖保留原键的顺序和拼写；默认严格模式照常拒绝重复。
- 首个全局键以 U+FEFF 开头时，写回再读取曾把这个字符误当文件 BOM 删除。
  现在只在文件起始存在这一歧义时写一个空行，保留键的真实数据；这个字节
  也纳入输出限额。已有前置键或具名节的普通输出不受影响。
- 空键名、去除首尾空白后为空的节名统一是 `XRT_INI_ERROR_FORMAT`，
  不再误报资源超限；真正的名称/值长度超限仍是 `XRT_INI_ERROR_LIMIT`。

`test/test_updates.c` 覆盖 54 种覆盖组合：条目表扩容、首/中/末键、
大小写敏感/不敏感、全局/具名/重开节，以及 LF/CRLF/CR。每组还验证
默认严格拒绝、规范文本回读、键数预算、顺序和其他键不被污染。另有
8 种 BOM 键布局、精确输出预算、格式/真实大小超限及逐点分配失败与
活动分配恢复检查。`test/updates.xl` 纳入包清单，保持真实语言消费路径。

本轮 Windows 默认门禁和 Linux 原生文件系统同输入快照默认门禁均退出 0，
包含严格 GCC、Linux ASan/UBSan、内嵌 TCC、Python `configparser` 双向
互通及覆盖/续行对照、私有访问负例、新旧用例与范例的双语 JIT/AOT。
测试共用一次编译的 XRT 对象，检查实际输入和发布物指纹；失败保留原始
命令、状态及输出，成功清理独占产物目录。Linux 验证范围不关闭已登记的
WSL 挂载盘构建耗时风险；未重建主线或替换 XRT，基线仍见
[验收矩阵](../../../docs/普通扩展库发布验收矩阵_2026-10-28.md)。

### 部署配置范例

[`examples/profiles.xl`](examples/profiles.xl) 与
[`examples/profiles_c.c`](examples/profiles_c.c) 展示允许覆盖的配置、
多行欢迎文本、读取时插值，以及写回后按默认严格模式重新读取。
两份都检查实际结果，不只比较输出；无需网络、外部服务或数据文件。
范例的协议名从 `${.scheme}` 读取全局配置，服务内的 host/port 按本节读取。
在 `demo6` 目录运行：

```text
./release/xl.exe run release/extlibs/ini/examples/profiles.xl
./release/xl.exe run release/extlibs/ini/examples/profiles_c.c
```

Linux 使用 `./release/xl`。两份的预期输出完全一致：

```text
http://localhost:9090
hello
world
unchanged
```

## 所有权与使用说明

`maxOutput` / `tLimits.iMaxOutputBytes` 统一限制拥有型 `get` 结果和序列化
文本的 **UTF-8 字节数**，0 表示不限；等于预算成功，多一个字节报 LIMIT
（XLang 为 ConfigError）。类型化 getInt/getFloat/getBool 的内部取值同样
使用这一预算。只读借用视图 `xrtIniGetRaw` 不复制结果，不受输出预算截断。
超限读取不修改保存的数据，后续成功读取会清除旧错误。

插值展开时，前缀、每个引用的正文和后缀共用累计预算，在追加/扩容前检查；
不再先构造完整的超限字符串才拒绝。没有 `${` 的普通值即使启用插值，也
直接检查长度并复制一次，不分配引用栈和插值缓冲。这里限制的是结果长度，
不是进程总内存上限，固定解析状态、缓冲容量和错误对象仍有自身开销。
`test/test_output_limits.c` / `test/output_limits.xl` 单独覆盖准确预算、
累计引用、跨节引用、UTF-8、0 不限、恢复，以及必要分配失败时的 MemoryError；
超限和 OOM 分开验证，不依赖固定第几次分配来制造限额错误。
该资源边界的独立红例先复现了普通 get 绕过限额、插值超限仍已扩容，
修复后严格 GCC/TCC、Linux ASan/UBSan、语言 JIT/AOT 都通过。
大引用的检查还核对没有额外的缓冲 realloc；这只证明该用例在扩容前拒绝，
不等于所有程序路径的分配上界或整个库零开销。

2026-10-27 所有权收口：错误文本、节名、键名、值和序列化文本的
C 出口均明确返回 XRT 拥有的 `char*`；XLang native-backed class 使用
`@owned string?` 直接接管。生成 C 门禁逐项检查直接保存原生指针，
不在交接处复制正文；仍需一次 `strlen` 获取长度。错误与空值分支
保留原有合同，错误文本自身分配失败明确抛 MemoryError。

2026-10-26 当前发布物：Windows 默认包门禁、Linux C ASan/UBSan
与独立 Python `configparser` 双向互通均通过。两平台中央用例、
边界及公开范例 JIT/AOT 通过；Linux 浮点核心/边界/中央/公开
范例/C 范例五份独立 EXE 均运行成功，C/GCC/XLang 范例输出一致。
Windows 从 `demo6` 根目录直接运行公开范例也成功，旧工作目录
敏感 VFS 故障不再复现。未据此宣称 32 位、TSan 或长时压力完成。

2026-10-26 文本所有权复核：`Config.get` 和 `Config.stringify` 返回
普通 `string`，`takeCstr` 直接移交各自的 XRT 文本分配。先前
`cstring` 转成 `string` 时反而复制，现已修正；`getOr` 仍返回
带默认值的 `string`。Windows 包门禁核对生成 C 的直接移交、
C/XLang 范例及完整 AOT。节名/键名装入 `array<string>` 时，
共享容器 lowering 仍创建字符串值副本；包内不另造私有绕行。
Linux C ASan/UBSan、独立 C/Python `configparser` 双向互通、XLang JIT、
浮点核心/边界/公开范例及 C/XLang 对照此前也已通过；本轮扩展
中央与 C 范例 AOT 门禁。
2026-10-25 在 WSL Ubuntu 运行 `test/check.sh`：C ASan/UBSan
主测试、独立 Python `configparser` 双向互通、Linux `xl` 边界与
C/XLang 范例输出对照均通过。使用的是现有 Linux `release/xl`，
不是新构建的 Linux 发布物；当时独立 Linux AOT 尚未验收，
已由上方 2026-10-26 的发布版门禁补齐。
上述 Windows 门禁在 `release` 工作目录运行 XLang 阶段；旧发布版从
`demo6` 根目录运行时的共享 TCC VFS 头文件故障见
`dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`，当前已不复现。
`test/interop.c` 与 `test/interop.py` 另外用 Python 标准库
`configparser` 做双向格式互通：C 写出后由 Python 读取，Python
写出后由 C 读取。范围限于双方共有的节、键、UTF-8 值和普通赋值语法。

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

`stringify()` 通过 `xini_stringify_text` 取走 XRT 文本缓冲的底层分配，
XLang 以普通 `string` 直接接管，避免再经 `Buffer.text()` 复制；补齐终止字节时缓冲容量不足
仍可能扩容。C 侧使用 `xini_stringify_text` 须以 `xrtFree` 释放返回值；
原有 `xini_stringify` 仍返回 `xbuffer*`。

`ini.create`、`ini.parse`、`ini.load` 返回 `Config`。格式错误、重复键、
缺少文件或键、类型转换失败、写入失败和资源限制均抛出 `ConfigError`；
底层分配失败抛出 `MemoryError`，不混作普通配置错误；
缺失键可用 `getOr` 提供默认值。`errorCode`、`errorLine`、`errorColumn`、
`errorOffset` 保存最近一次错误信息。对象销毁时释放原生句柄。
非法 UTF-8 的位置指向首个错误字节：`errorOffset` 是含可选 BOM 的原始
字节偏移，`errorColumn` 从 BOM 后的正文起算；单独 CR 和 CRLF 都只计一行。
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
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/ini/test/check.ps1
bash release/extlibs/ini/test/check.sh
```

2026-10-24 使用更新后的 Windows 发布版复验，默认门禁通过严格 GCC、
C/XLang 边界、双语同输出范例、边界 AOT 和公开文件范例 AOT；Linux
C ASan/UBSan 与 XLang 双语门禁也通过。旧发布版的共享缺符号阻断已解除。
语言限额负例不再依赖固定的第 14 次分配位置；超限错误与逐点分配
故障分别验证，避免编译器调用路径变化使测试误报。
`release/examples/extlibs/ini.xl` 和 `ini_c.c` 还在独占临时目录里成对展示
`create`、`save`、`load` 的文件往返，并验证加载后的值；GCC、TCC 和
XLang 输出一致，范例结束时删除该目录。
门禁每次使用独占构建目录，成功清理本次产物，失败保留现场；不会复用
旧 `build/test/ini_guard` 中的可执行文件。
Linux 门禁使用 ASan/UBSan 验证 C 解析及文件路径边界，另以内嵌 TCC
执行 C 用例，运行中央/包内 XLang 边界并逐字节核对双语范例。
这些是 ini 包级证据，不等于整个 xlang 发布版签收。
包内 `test/boundaries.xl`、`test/updates.xl`、`test/output_limits.xl`、`test/interpolation_refs.xl` 已登记到 `coverage.tests`，统一扩展库 smoke 可发现；
现已从 `demo6` 工作目录与默认包门禁一并复跑。

以下为历史验证记录，其中缺符号描述均指当时的旧发布二进制。

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

2026-10-22：C 边界新增两份有效配置的逐前缀截断与单字节畸变矩阵，
覆盖 NUL、非法 UTF-8、换行和结构字符。成功解析且能写出的输入必须
逐节逐键保留原始值，写出文本再次解析后必须规范化稳定；不能无损写出
时必须明确报 `FORMAT`。Windows 严格 C/XLang 与双语范例、Linux C
ASan/UBSan 与 XLang 对照门禁通过；
完整带输出 AOT 仍受共享主线缺符号阻断，不据此签收整包发布级。

2026-10-23：包内边界用例的两处单名 `catch MemoryError` / `catch ConfigError`
原是无类型捕获变量，不能证明解析分配失败与缺失文件分别按预期分类。
现改为 `catch MemoryError error` / `catch ConfigError error`，与同文件其余
分类捕获一致。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh`
的 C ASan/UBSan、XLang 和双语门禁通过；包实现与公开 API 未改，完整
AOT 的共享阻断仍待主线修复。

2026-10-24 当前 Windows 发布版复测：默认 `test/check.ps1` 完整通过，
包括严格 GCC C 范例、C/XLang 边界、私有入口负例、双语输出对照，
以及浮点核心、完整边界和教学范例的 AOT 构建与运行。先前的
`xl6NativeHandleTraceLeaf` 缺符号在本次范围内不再复现。
Linux 语言门禁此次未以当前源码重建，旧 Linux 发布物及重建阻塞见
`demo6/dev/extlib_blockers/linux_release_x509_sources.md`；本条不作为
Linux AOT 或跨平台发布级签收。
