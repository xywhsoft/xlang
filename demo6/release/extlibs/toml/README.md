# toml

TOML 解析、写出、文件读写及日期/时间值。底层 C API 在 `include/xrt_toml.h`，
XLang API 由 `import toml` 导入；包内共用一套 XRT `xvalue` 实现。
解析语法以 TOML 1.0 为目标：内联表不可跨行或带尾逗号，时间必须包含秒，
Unicode 键须加引号，字符串不接受 `\xNN` 或 `\e` 转义。私有解析器快照
原有的超出 1.0 的宽松写法不进入本包合同。

```xlang
import toml

var config = toml.parse("title = \"demo\"\n")
config["released"] = toml.localDate(2026, 7, 16)
string text = toml.stringify(config)
assert toml.dateTimeKind(toml.parse(text)["released"]) == 1
```

## 公开能力

- `parse(text)`、`load(path)`：解析字符串或 UTF-8 路径文件，返回 `var` 字典。
- `stringify(value)`、`save(path, value)`：生成 TOML 文本或写入文件。
- `localDate`、`localTime`、`localDateTime`、`offsetDateTime`：创建四种 TOML 日期时间值。中央 C/XLang 对照范例分别构造四种值，检查微秒和时区偏移；保存后的文件由 Python `tomllib` 独立解析。
- `dateTimeKind` 返回 0（非日期时间）、1（日期）、2（时间）、3（本地日期时间）或 4（带偏移日期时间）；`isLocalDate` 等函数提供布尔判断。
- `dateTimePart(value, part)`：part 0..7 依次为年、月、日、时、分、秒、微秒、时区偏移分钟；`dateTimeText` 返回 TOML 文本表示。
- 日期时间仍以可访问字段的 XRT 对象表示，但由 XRT `TypeId` 区分；请用本包构造函数创建，不要靠手写 `__toml_type` 字段冒充。普通 TOML 表即使与日期对象的字段完全同形，也保持表语义并可写回；深拷贝日期对象保留类型身份。
- 日期时间支持到微秒（6 位秒小数）。按 [TOML 规范](https://toml.io/en/v1.0.0)，输入超过实现精度的小数位会截断，不会四舍五入；再次写出仅保留前 6 位。需要纳秒无损往返时不要使用本包的日期时间值。
- C 侧原始 `xrttomldatetime`、可变日期对象和文本格式化共用合法性检查；日期不能暗含时间字段，时间不能暗含日期字段，本地日期时间不能暗含时区偏移，保留字段必须为零。无效记录不会被格式化为看似有效的 TOML 日期。
- `errorCode/errorLine/errorColumn/errorOffset`：查询当前线程最近的 TOML 错误位置。语法/参数错误抛 `ValueError`，资源限额抛 `RangeError`，文件及流失败抛 `IOError`，内存失败抛 `MemoryError`；包装层未得到原生错误时抛 `TomlError`。

私有 tomlc17 适配层使用 XRT 分配器；解析器自身的分配失败也报告为
`MemoryError`，不会误判为 TOML 格式错误。包内 C 测试逐点注入解析分配失败，
并核对释放路径；私有解析器对上游快照的语法收紧见 `_tomlc17/VENDOR.md`。
浮点数读写均使用 XRT 的 locale 无关数字接口，固定输出 TOML 要求的 ASCII
小数点。即使宿主进程切换 `LC_NUMERIC`，`1.5`、`1.0`、指数、负零、无穷与 NaN
仍可解析或往返；包不会修改进程的 locale。Windows C 测试会实际切换到可用的
非 C 数字区域设置；没有相应 locale 的平台仍执行默认区域设置下的语法检查。
有限浮点字面量若超出本包的 `float64` 范围则报格式错误，不静默变为无穷大；
显式 `inf`/`nan` 仍可用。这是本包的严格数值合同，不声称 TOML 1.0 要求
所有实现对溢出都作相同选择。

解析/写出 API 均可配置输入、输出、单项、条目、节点和深度上限；默认值见
`src/toml.xl` 的函数签名。C API 的结果由调用者 `xrtValueRelease` 或
`xrtBufferDestroy`，包内不持有输入值。
传入 C 选项结构时须先调用 `xrtTomlOptionsInit`；选项本身或内嵌的
`xrtresourcelimits` 版本、长度无效会报 `ARGUMENT`，不会静默使用默认限额。
内嵌限额的保留字段也必须为零；解析和写出遵循同一检查。
压缩倍率与资源标志不适用于 TOML；OptionsInit 将倍率置零，显式设置
`iMaxCompressionRatio` 或 `iFlags` 会报 `ARGUMENT`，不静默忽略。
传入 `NULL` 才表示使用默认选项。
XLang 包装函数的限额参数传 `0` 表示使用 XRT 的对应默认值，不表示取消限制；
负数限额会在语言层
报 `ValueError`，深度超过 32 位无符号范围也会拒绝，避免 C ABI 窄化回绕。
`parse(string)` 直接向原生入口传递 UTF-8 数据和字节长度，不再创建 XLang
Buffer 副本。Reader 同时支持总长度和当前位置查询时，会先按剩余字节数
检查输入限额；其他 Reader 仍由限额读取约束。正文或注释中的内嵌 NUL
都按格式错误拒绝，不接受合法前缀。
日期时间构造参数在转成 C `int` 前检查范围，具体日期合法性仍由 XRT 判断；
`dateTimePart` 的 `part` 只接受 0..7。`load`/`save` 的路径包含内嵌 NUL 时
报 `ValueError`，不会访问 NUL 前缀指向的另一文件。
写出器拒绝无效 UTF-8 的键和值；`save` 使用 XRT 原子写接口，序列化失败时
不会替换已有文件。
写出器与解析器一致，对原始 UTF-8 键/字符串执行 `maxItem` 字节限制，
并对每个数组、表（包括内联表和数组表）执行 `maxEntries` 条目限制；
到达上限仍允许写出，超过时返回限额错误。`save` 在限额失败时保留旧文件。
数组表的数组节点及其额外一层深度也计入 `maxNodes`/`maxDepth`，与解析侧一致。
C 流接口的 `bTakeOwnership=true` 会在操作结束时关闭传入的 reader/writer；
即使正文读写成功，关闭失败也会返回失败并报告 I/O 或内存错误。解析成功后
若所拥有的 reader 关闭失败，已构造的结果会被释放；`false` 则由调用方
负责关闭流，包不会提前关闭。若正文先失败，关闭阶段不覆盖原始诊断。

## 定点验证

在 `demo6/release` 目录运行：

```powershell
.\xl.exe run extlibs/toml/test/test_toml.c
.\xl.exe run extlibs/toml/test/boundaries.xl
.\xl.exe run examples/extlibs/toml.xl
.\xl.exe run examples/extlibs/toml_c.c
.\xl.exe build extlibs/toml/test/aot_core.xl -o ..\build\test\toml_aot_core.exe
..\build\test\toml_aot_core.exe
```

也可从 `demo6` 目录运行包级 Windows 门禁；默认模式依次验证核心和
带输出范例 AOT，后者还与 C 范例输出对照。`-SkipAot` 跳过这两项
AOT，不跳过 C、XLang、ABI、双语范例及文件格式检查。Windows 文件
格式门禁需要 Python 3.11+ 的标准库 `tomllib`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/toml/test/check.ps1 -SkipAot
```

Linux 包级门禁同时运行 C ASan/UBSan、中央与包内 XLang 用例、双语逐字
同输出，并用 Python 3.11+ 标准库 `tomllib` 独立解析两份同字节范例文件：

```sh
bash release/extlibs/toml/test/check.sh
```

仅运行 XLang/C 与格式对照时可用 `--skip-aot`；不加该参数会继续检查
无输出核心 AOT，已知当前主线缺符号会使其失败：

```sh
bash release/extlibs/toml/test/check_xlang.sh --skip-aot
```

可选的 [toml-test](https://github.com/toml-lang/toml-test) 1.0 语法及预期值审计不进入日常门禁；
在 `demo6` 目录获取独立测试集后运行：

```sh
git clone --depth 1 https://github.com/toml-lang/toml-test.git build/test/toml_conformance_src
bash release/extlibs/toml/test/check_conformance.sh build/test/toml_conformance_src
```

在上游提交 `ff49d109861c1ad25af53f687f2aef19ab650600` 上，208 个合法、
501 个非法文件的**语法接受/拒绝**全部匹配。208 个合法文件的官方预期
JSON 与 Python `tomllib` 解析原文、解析本包写回结果后的类型和值均相等；
布尔、整数、浮点、字符串和本地/带时区日期时间按类型比较。
这验证写回后的可观察值，不直接检查内部 `xvalue` 的每个中间节点；
包内 C/XLang 用例继续覆盖 API 边界。语料版本变化时需重新运行该可选门禁。

`test/boundaries.xl` 已登记到包清单的 `coverage.tests`；从 `demo6` 目录运行
`tools/smoke_extlib_catalog.ps1 -Package toml` 可连同中央用例复验。

最后两个范例输出应完全相同。上述 Windows JIT/严格 GCC（`-O0`、`-O2`）与 Linux
ASan/UBSan C 门禁已通过；`test/abi_contract.c` 在 Windows/LP64 Linux
核对生成的 C 原型与包桥接一致。`toml` 和私有 `_tomlc17` 都声明了实际使用的
XRT 模块；历史构建中无输出核心 AOT 曾从 3,727,872 字节降到 824,320
字节并运行通过。**当前发布物的 AOT 未通过**：Windows 包级门禁和 Linux
门禁的无输出核心构建均缺 `xl6NativeHandleTraceLeaf`；完整边界另有
`xl6ValueDataSnapshotV1` 等共享符号阻断，须待首个阻断解除后重验。
这些问题已交由编译器主线线程处理，本包不修改主线。
当前 Linux XLang 边界与双语 JIT 已通过；
32 位与长期负载尚未验收。

2026-10-17：Linux C sanitizer 与 XLang 门禁接为同一条可复跑脚本；新增
中央主用例和 Python `tomllib` 对 C/XLang 写出文件的独立解析及字节对照。
包级脚本通过；默认 AOT 模式再次复现 `xl6NativeHandleTraceLeaf` 缺符号，
完整发布验收仍未签收。

可选的独立格式验证使用 Python 标准库 `tomllib`，不需要安装第三方包。从 `demo6` 目录运行以下命令，保留 C/xlang 范例生成的 TOML 文件；解析后核对根级日期、带时区日期时间和嵌套表，并检查两份文件字节一致：

```powershell
.\release\xl.exe run release\examples\extlibs\toml.xl -- build\test\toml_example_xl.toml
.\release\xl.exe run release\examples\extlibs\toml_c.c -- build\test\toml_example_c.toml
python release\extlibs\toml\test\check_external.py build\test\toml_example_c.toml build\test\toml_example_xl.toml
```

范例的输出路径是可选参数；不传路径时原有控制台输出不变。
传入路径时，C/XLang 范例均调用本包 `save` 和 `load` 写入并读回，
核对标题字段；不再绕过 TOML 文件 API 直接写序列化文本。
保存或读回失败时 C 范例返回非零，不打印成功提示。

2026-10-18：双语范例的可选文件模式改为包级 `save/load` 往返；
Windows 严格 C 与双语默认输出门禁、Linux C/XLang 文件读回和 Python
`tomllib` 独立解析通过。C 保存失败负例返回非零且不输出 `PASS`。
此项未修改解析器；完整 AOT 仍受共享运行时缺符号阻断。

2026-10-15：中央 `test/cases/extlib_toml.xl` 的非法语法负例按当前
合同捕获 `ValueError`，而非仅在无原生错误时使用的 `TomlError`；
完整中央 JIT 用例与错误位置断言通过。中央 AOT 构建仍缺
`xl6ValueIndexReadCopyV1`，包内带输出边界 AOT 另缺
`xl6ValueDataSnapshotV1`；本次只修测试，不改包实现或主线。

2026-10-15：修复 C 流接口忽略所拥有 reader/writer 的 `Close` 失败。
读写成功但关闭失败的测试先红后绿；借用流仍由调用方关闭。Windows C
聚焦测试、Linux ASan/UBSan、Linux XLang/C 对照和核心 AOT 通过；
完整带输出 AOT 的共享缺符号阻断未变。


2026-10-16：C/XLang 新增重复键、标量再声明为表和重复表的拒绝回归，
均返回格式错误且保留位置；包解析器无需修改。Windows C/XL 与 Linux
ASan/UBSan 通过。当前 Linux `check_xlang.sh` 的边界和双语 JIT 通过，
随后精简 AOT 构建报 `xl6NativeHandleTraceLeaf` 未解析；同命令可独立复现，
见 `demo6/dev/extlib_blockers/native_handle_trace_aot.md`。此轮 Linux AOT
不再宣称通过，待主线产物修复后重跑。

同日复核有限浮点上界：最大有限 `float64` 可解析，`1e999` 返回格式错误，
显式 `inf`/`nan` 继续可用。C/XLang 定向回归固定原有行为，并将语言边界
纳入统一目录 smoke；解析器无需修改，完整 AOT 阻断不变。

2026-10-16：写出器原未检查 `maxItem`/`maxEntries`，C 六类超限用例先红后绿；
补字符串与键、普通数组与表、内联表及数组表，并验证恰好到限额可写出。
XLang `stringify` 抛 `RangeError`，`save` 限额失败不覆盖旧文件。
Windows 严格 GCC、C/XL 边界及双语范例与 Linux ASan/UBSan 通过；
完整 AOT 的共享链接阻断仍在，尚不签收发布级。
同日补数组表资源账本：原写出路径少计数组节点和一层深度；
解析/写出双侧同阈值红绿回归，2 拒绝、3 成功。

2026-10-18：C/XLang 补表定义冲突边界：内联表外扩、点号键形成的表
重新声明、静态数组改数组表、普通表与数组表相互改写；八种非法输入均
报格式错误及非零行号。先声明子表、后声明尚未显式定义的父表作为合法
对照保留。Windows `-SkipAot` 包级门禁与 Linux C ASan/UBSan、XLang JIT、
Python `tomllib` 对照门禁通过；解析器无需修改，完整 AOT 主线阻断不变。

同日发现私有 `tomlc17` 快照接受六类超出 TOML 1.0 的宽松写法：内联表尾逗号/
换行、时间省略秒、Unicode 裸键、`\xNN`/`\e` 转义。其中 Unicode 裸键连
官方 1.1 也不允许。七条 C/XLang 拒绝
用例先红后绿；带引号的中文键可写出并读回。私有解析器只删除这些扩展。
独立 `toml-test` 1.0 语法集的 709 个文件无接受/拒绝差异，
其中 208 个合法文件的写回值与 `tomllib` 解析原文一致；Windows
`-SkipAot` 与 Linux C ASan/UBSan、XLang JIT、Python `tomllib` 写出对照
通过。此后可选门禁又核对了官方预期 JSON，208 个合法、501 个非法
文件的语法、参考值和写回值差异均为零；这仍不是对内部每个 `xvalue`
节点的直接检查，完整 AOT 主线阻断不变。

2026-10-19：C 配置中的非零限额保留字段原被解析和写出入口接受；
两条负例先红后绿，现由共用选项解析器拒绝，结构长度策略不变。
Windows 严格 C/XLang、双语范例与 Linux ASan/UBSan、XLang、Python
`tomllib` 格式门禁通过；完整 AOT 仍待共享主线修复。

2026-10-19：C 定向红测确认无用压缩倍率沿用通用默认值，未知资源标志和显式
倍率均被解析/写出入口接受。现初始化时清零倍率，并由共用选项入口拒绝
显式设置。Windows 严格 C、C/XLang 边界与双语对照，以及 Linux C
ASan/UBSan、XLang 与 Python `tomllib` 格式门禁通过；当前核心 AOT 仍缺
共享符号 `xl6NativeHandleTraceLeaf`，整包发布验收未签收。

2026-10-20：普通表与日期记录字段完全同形时，原写出器把它误判为日期，
导致表结构丢失。C 最小用例先红后绿；日期构造结果现绑定 XRT `TypeId`，
识别入口要求该身份，普通表不受保留字段名影响。C 深拷贝、XLang 写回
边界、Windows 严格 C/双语门禁、Linux ASan/UBSan 与 XLang 联合门禁通过。
官方 TOML 1.0 语料 208 个合法、501 个非法文件的语法、预期值及写回值
仍零差异；完整 AOT 的共享主线阻断不变。

2026-10-20：默认 Windows 包门禁增加带输出范例 AOT 构建、执行和 C
输出对照，接在核心 AOT 后。当前复测仍首先在核心构建遇到共享
`xl6NativeHandleTraceLeaf` 缺符号，新增分支尚不能执行；`-SkipAot`
的严格 C、XLang 边界和双语 JIT 通过，失败后独占构建目录已清理。
聚焦模式不作为完整发布验收。

2026-10-20：包内 NUL 路径边界用例原直接写删固定
`build/test/toml_nul_boundary.toml`，可能覆盖已有文件；现使用 XRT 独占
临时目录、检查夹具写入，并在 `finally` 清理。Windows `-SkipAot`
严格 C、XLang 边界与双语范例，以及 Linux C ASan/UBSan、XLang、
`tomllib` 格式对照门禁均通过。解析/写出实现未改；完整 AOT 仍受
共享主线缺符号阻断，不据此签收整包发布级。

2026-10-21：原子保存文件遇到 XRT 分配失败时，底层 `XERR_MEMORY`
曾被写出器统一映射为 `XRT_TOML_ERROR_IO`，使 XLang `save` 抛出
`IOError`。C 与 XLang 最小复现先红后绿；现在流写入、关闭和原子保存
共用底层错误分类，保留内存失败与真正 IO 失败的区别。新增
`test/allocations.xl`，逐点扫描 `parse/stringify/load/save` 的前 32 个
分配点并验证清除故障后文件往返；用例已接入 manifest 和两平台门禁。
Windows 严格 C、XLang 与双语输出，Linux C ASan/UBSan、XLang 与
`tomllib` 格式对照均通过。完整 AOT 的主线缺符号阻断仍未解除。

2026-10-22：日期时间对象构造中的两个字段辅助函数原使用 `SetTake`，
字段写入分配失败时未释放尚未被接管的临时值。C 活动分配红例在第 3 个
故障点确认泄漏；现改用成功或失败均消费临时值的 XRT `SetNew`。
80 点分配故障扫描验证结果、内存错误分类、释放后活动分配数与字节数；
Windows 严格 C/XLang、双语范例、Python `tomllib` 文件互通，以及 Linux
C ASan/UBSan、XLang/格式互通门禁通过。完整 AOT 本轮未复验，
不据此签收整包发布级。
