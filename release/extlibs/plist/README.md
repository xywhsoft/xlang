# plist

## 当前验收（2026-10-30）

本轮修复一组资源/错误分类边界，不增加 API 或改变 ABI：五个基础
桥接入口在首次线程状态初始化失败时不再清错继续操作；binary 节点、
字节复制、UTF-16 转换和容器构造 OOM 不再误报 FORMAT，条目超限
明确为 LIMIT；XML 字典条目上限改为比较实际数量，避免 uint64
大上限乘二回绕。已有结构化首因、PlistError/MemoryError 分工及
零表示不限的 C 限额合同保持。

Windows 完整门禁 587 项符合预期，C 冷启动 356 场景、公开语言
JIT/AOT 各 87 预算通过；Linux 原生 C 冷启动 348 场景，带
ASan/UBSan，其余同等路径通过。两平台最终同一夹具只换旧三文件，
均有 50 个冷启动失败场景、64 条 binary/限额断言，修复后归零。
这不是 114 个互不相关的 BUG，也不是完整格式认证。

新增 C 回归覆盖 12 类完整值、142 个实际 OOM 点、48 组限额及
36 个不支持标记/旧错误对照；Python plistlib 独立核对 12 份 C
生成 binary 文件。四线程/64 诊断在 GCC、TCC JIT、独立 AOT 通过；
41 条实际生成 C 原型和故意错改 owned 返回值负例通过；每平台
13 份正常 AOT、新旧双语输出和原 XML/binary plistlib 互通均通过。
新增[内存状态双语范例](examples/README.md)，以及
[冷启动/错误分类完整记录](test/COLD_BINARY_AUDIT_2026-10-30.md)。

门禁现有界执行并保留失败现场与原始退出码，真实静默编译器 37、
Linux 缺失编译器 127 的控制已验证。生产仅改包内三文件，不改
编译器、XRT、XML 依赖或 Expat；不新增分配、正文复制或全扫描，
未测吞吐，不称完整零开销。下方旧预算是预热固定夹具，不是本轮
冷启动总分配数；不能混用两者。

2026-10-28 完整返回链复查：原生接管后，XML 结果的可空参数辅助函数
与诊断消息的可空局部返回仍复制正文。两者改用直接调用临时值 `??`，
删除重复的结果辅助函数；仅在失败时进入原有 PlistError/MemoryError
分类。保留 `@owned string?` 与独立诊断结构，没有把 NULL 改成通用
非空错误，也没有改变 C ABI、XML/binary 算法或原生实现。

预热后的固定空字符串序列化，源值构造在预算外：等价 C 完整构树/
写出需 7 次 XRT 分配，旧 XL 需 19 次，现为 18 次。新回归限制成功
预算上限为 18，预算 0 仍是内存错误，并验证不支持的 XML 值、输出
限额仍为有消息的 PlistError（码 4/5）、随后恢复及持有旧结果。
控制 cgen 夹具由 7 个复制代码点减到 5 个，去掉结果和诊断的两个
返回复制点；构树/输入转换、语言对象与错误作用域仍有成本。这不是
任意输入单次分配、没有任何复制或整条调用零开销的承诺。

Windows/Linux 默认完整门禁通过严格 C、Linux ASan/UBSan、原有故障
扫描、私有类型负例、中央/边界/新预算与双语 JIT/AOT；Python plistlib
独立读取 C/JIT/AOT 生成的 XML/binary 产物。补中央 AOT、输入及 XML
依赖指纹，失败保留命令/现场；主线/XRT 未改，也未替换依赖源码。

2026-10-26：当前 Windows/Linux 发布物的默认包门禁均通过 C 边界、
Linux ASan/UBSan、语言边界/分配故障 JIT 与 AOT、私有原生类型负例、
公开 C/XLang 范例独立 EXE。两平台分别生成 XML 与 binary plist，
由 Python 标准库 `plistlib` 独立读回并核对 C、JIT、AOT 的内容。
旧日期的共享 AOT 缺符号和序列策略记录仅描述当时发布物。
此验收限于本包明确支持的 XML 1.0 / `bplist00` 值类型、资源上限
与时间精度，不外推为所有 Apple 私有扩展或任意大型文件兼容性。

2026-10-24 当前 Windows 发布版默认 `test/check.ps1` 通过：C 本体与
桥接测试、XLang 边界/分配故障、包外私有类型负例、双语范例同输出，
以及最小导入和公开范例 AOT。门禁现让 C/JIT/AOT 分别写出 XML 与
binary plist，由 Python 标准库 `plistlib` 独立读回并核对内容；
`-SkipAot` 模式同样执行 C/JIT 的独立格式检查。下文的 AOT 缺符号与
序列策略阻断是旧发布物记录，不是这次 Windows 门禁结果；本轮未
重建 Linux 发布版，不能把 Windows AOT 结果外推到 Linux。

Apple property list 的 XML 1.0 与 `bplist00` 读写。XLang 入口使用 `import plist`；C 入口是 `include/xrt_plist.h` 中的 `xrtPlist*` API。实现复用 XRT 的 Buffer、文件、时间和资源限额，以及 `xml.dom` 的 XML 解析能力。

## XLang API

| 函数 | 结果 |
| --- | --- |
| `loads(textOrBuffer, format = "auto", ...)` | 解析文本或 Buffer，返回普通 XLang 值 |
| `load(path, format = "auto", ...)` | 从 UTF-8 路径读取 |
| `dumps(value, format = "xml", ...)` | 返回拥有自身数据的 Buffer |
| `stringify(value, ...)` | 返回 XML 字符串 |
| `dump(path, value, format = "xml", ...)` | 写入 UTF-8 路径 |
| `uid(value)`, `isUid(value)`, `uidValue(value)` | 二进制 plist 的 UID 值 |
| `errorCode/Line/Column/Offset()` | 最近一次原生解析/写入错误，按线程隔离 |

`stringify` 仅在 XML 模式通过 `xplist_stringify_text` 接管原生缓冲的
底层 XRT 分配，避免 `dumps(...).text()` 再复制整份文本；补齐终止字节
时缓冲容量不足仍可能扩容。C 侧调用文本桥接后用 `xrtFree` 释放。
二进制输出继续使用 `dumps(..., "binary")` 和 `xbuffer*`，不经过 C 字符串。
XLang 声明将文本和错误消息均标为 `@owned string?`，通过调用临时值的
`??` 直接移交成功结果，不再经 `point`、`takeCstr` 或借用参数返回辅助
函数。原生 NULL 仍走独立错误结构，保留 PlistError 分类及诊断内容。

`UID` 是导出类型，可写 `plist.UID id = plist.uid(42)`；内部原生文档句柄不向导入方导出。
`NativeDocument` 仅供包内桥接使用，导入方无法声明或构造；
`test/private_native_document_negative.xl` 将此作为编译负例纳入 Windows/Linux 门禁，
防止绕过公开的解析、写出与资源限额接口。

`loads` 自动识别 `bplist00`；二进制内容应传 `Buffer`，不要经文本编码转换。XML 支持布尔、整数、实数、字符串、数据、日期、数组、字典；`null` 和 UID 只支持二进制。二进制字符串与字典键可包含 NUL，XML 不允许 NUL 码点。`Buffer`、数组和字典在转换后由调用方正常持有，不借用已销毁的文档节点。
`loads(string)` 将字符串及显式 UTF-8 字节长度直接传给同步原生入口，不额外创建一份 XLang `Buffer`；`loads(Buffer)` 仍按二进制缓冲传入。解析器在调用期间读取输入并独立构造结果，返回值不借用调用方的字符串或 Buffer。
XML 写出器将字符串和字典键中的 CR 写成 `&#xD;`，避免解析器把字面 CR 规范化为 LF；C 与 XLang 均验证键和值往返。XML 文本中的 LF/Tab 保持原值，不改变二进制 plist 的编码。
XML `<real>` 使用区域无关的点号十进制；即使进程设置了逗号小数点，写出仍为 `1.5`，读取也只接受标准点号写法，不接受 `1,5`。
Apple XML plist 的日期文本按整秒 UTC 写出；带微秒的 XRT `time` 在 XML 模式报 `PlistError`，不会静默截断。需要保存微秒时使用 binary plist；它按 2001 年起的 IEEE 754 double 秒数存储，只有写出后能精确还原为原 XRT 微秒值才成功，否则也报 `PlistError`，不承诺任意极值时间都无损。XML 读取器仍可解析 RFC 3339 日期文本；写出遵循 [Apple 的整秒格式](https://github.com/apple-oss-distributions/CF/blob/main/CFPropertyList.c)。
`load`/`dump` 的文件路径通过 C 字符串传递，含嵌入 NUL 时抛出
`ValueError`，不会访问或覆盖 NUL 前缀指向的文件；此限制不影响值中的
二进制数据及二进制 plist 字符串/键。
`dump`/C `xrtPlistSave` 先完成序列化，再通过 XRT 原子替换目标文件；
序列化或写入失败时不先截断已有文件。C 测试覆盖失败后旧内容保持与成功读回。
C `xrtplistoptions.tLimits` 中的 0 表示对应项目不限制，XML 解析也遵守这个语义；XLang 接口则要求显式传入正限额。C 端传入选项结构时须先调用 `xrtPlistOptionsInit`；外层或内嵌资源限额的版本、长度无效，或保留字段、未支持的资源策略标志非零，都会报 `ARGUMENT`，不会静默回退到默认限额。传入 `NULL` 才使用默认选项。读取、分配、限额、XML 格式错误分别保留对应的 plist 错误码，不把文件不存在、内存耗尽或超限统一报告为格式错误。

内存不足抛 `MemoryError`；其他读取和写入失败抛 `PlistError`。无效格式名、非正资源限额和非法缩进抛 `ValueError`，不支持的值类型抛 `TypeError`。`maxInput/maxOutput/maxItem/maxEntries/maxNodes` 必须为正数，`maxDepth` 为 1..512，`indent` 为 0..16。XRT C API 可直接配置 `xrtplistoptions`，并通过 `xrtplisterror` 获取错误详情；文档和输出缓冲分别使用 `xrtPlistDestroy`、`xrtBufferDestroy` 释放。
C `xrtPlistParse` / `xrtPlistWrite` 的 `bTakeOwnership=true` 在所有退出
路径关闭流；正文成功但关闭失败时返回 `NULL` / `false`，错误码为 `IO`。
正文已失败且关闭也失败时保留原错误；`bTakeOwnership=false` 的流仍由
调用方关闭，不被库提前释放。
XLang 的 `dumps`/`dump` 在构造原生文档时即检查节点总数、每个数组/字典的条目数，以及字符串、字典键和 Buffer 的 UTF-8/原始字节长度；超限抛 `PlistError`，不会等整棵原生树建好才拒绝。`maxOutput` 仍由写出阶段检查。

## 示例与验证

- XLang：[plist.xl](../../examples/extlibs/plist.xl)；C：[plist_c.c](../../examples/extlibs/plist_c.c)。两者先输出相同的 XML，再加入只适用于 binary 的 UID，核对读回值与二进制大小。
- `test/test_plist.c`：XML/binary 往返、外部整数/日期字节向量、DTD 与循环引用拒绝。
- XML 整数/实数直接使用 XRT 严格数值解析和空白标志，不复制数值文本；Base64 数据直接使用 XRT 忽略空白的严格解码，删除私有清洗与填充检查。整数正负边界、溢出/尾随垃圾、空/非法 Base64，以及整数、字符串、实数、日期和数据各前 128 个分配点的失败分类由 C 回归覆盖。分配失败返回内存错误，不会崩溃或伪装成格式错误。
- `test/test_native.c`：桥接层 NUL 键值与错误码。
- `test/boundaries.xl`：XLang 二进制/XML、UID、数据与路径 NUL、非法输入和限额。
- `test/owned_results.xl`：完整 XML 预算、结果持有、PlistError/限额及恢复；`test/owned_probe.xl` 为有必要输入转换的控制 cgen 夹具，不要求整个文件零复制。
- `test/test_cold_state.c`、`cold_start.xl`：首次进程/线程错误状态、11 条入口分支、保存保护和立即恢复；每预算独立进程，不预热错误槽。
- `test/test_binary_boundaries.c`、`binary_errors.xl`：全部值类型的 binary OOM、UTF-16 与容器限额分类；`check_binary_fixtures.py` 另用 Python 验完整值。
- Windows：`powershell -File release/extlibs/plist/test/check.ps1`，包含 C/XL 边界、双语范例同输出、`plistlib` 独立格式验证、最小导入与公开范例 AOT；`-SkipAot` 仅用于 C/JIT 快测，不代表发布验收。
- Linux：`sh release/extlibs/plist/test/check.sh`，以 ASan/UBSan 检查 C 本体与桥接层，另用内嵌 TCC 执行同一份 C 日期测试、运行包内 XLang 边界/分配测试，核对双语范例输出，并由 Python 标准库读取四份范例产物。

两平台默认还运行中央及新预算用例的独立 EXE，测试按产品模块构建，
独占目录在成功时清理，失败时保留命令、输入指纹与产物。

桥接层的 C `uint64_t`（XRT `uint64`）在 XLang extern 中统一声明为 `uint64`，包括输入/输出限额、数据长度及 UID；不使用在 Linux LP64 下不同类型的 `c_ulonglong`。Linux 包级脚本同时运行 XLang 主用例，防止声明再次漂移。

Python 标准库 `plistlib` 可独立读取双语范例产生的 XML/binary plist，不需要额外安装依赖。两份 XML 解码值及字节一致；两份 binary 解码值一致，且比 XML 多一个 `UID(42)`。binary plist 的字节布局不承诺一致。从仓库根目录运行：

```powershell
.\release\xl.exe run release\examples\extlibs\plist.xl -- build\test\plist_example_xl.xml build\test\plist_example_xl.bplist
.\release\xl.exe run release\examples\extlibs\plist_c.c -- build\test\plist_example_c.xml build\test\plist_example_c.bplist
python release\extlibs\plist\test\check_external.py build\test\plist_example_c.xml build\test\plist_example_xl.xml build\test\plist_example_c.bplist build\test\plist_example_xl.bplist
```

输出路径为可选参数，仅用于保留验证产物；不传路径时两份范例仍输出相同的 XML、二进制大小与 UID 读回值。

二进制短整数遵循 plistlib/Apple 互操作约定：1/2/4 字节为无符号正数，负数使用 8 字节二补数。当前包的 C 与 XLang JIT、Windows 严格 GCC、Linux ASan/UBSan 已通过；完整 AOT 在共享 `xl6_sequence.h:79` 失败，见 `demo6/dev/extlib_blockers/sequence_policy_aot.md`，不因包级通过而标为已发布。
2026-10-21：C 分配故障先复现 `xrtPlistSave` 将原子写入内存失败误报 IO；
XLang `dump()` 全链扫描又发现 XML 字典排序缓冲区分配失败误报 TYPE。
现保留保存、流写入/关闭及 XML 写出中的内存错误，清除成功序列化留下的
陈旧 XRT 错误；XLang 的构树与原生读写按错误码抛 `MemoryError`。
C 标量/字典保存逐点故障、内存 writer 与真实关闭 IO、XLang `dump()`
故障后恢复均通过；Windows 包门禁及 Linux ASan/UBSan、`plistlib`
四格式互通门禁通过。完整 AOT 的共享阻断未变。
2026-10-15 补 XML 日期微秒丢失及 binary 极值时间精度丢失的红绿回归：XML 明确拒绝非整秒，binary 对无法精确往返的值拒绝，普通微秒时间照常往返；Python `plistlib` 的 1 微秒固定向量读写一致。Windows C/XL 包门禁与 Linux ASan/UBSan 通过。
2026-10-15 将私有头中的 `XRT_MODULE_ALL` 改为直接依赖的模块清单，并同步到包清单。Linux sanitizer 测试运行库采用产品与 `xml.dom`/`xml.sax` 的模块并集；仅因测试源码使用调试分配入口额外启用 `MEMORY_DEBUG`。缩小运行库后的 C 本体、桥接、范例及 Windows C/XL 门禁均通过。最小导入 AOT 仍缺主线 `xl6ValueDataSnapshotV1`，未完成独立产物运行/体积验收。
2026-10-15 新增语言层前 64 个分配点故障与恢复测试，并接入 Windows 包级门禁；将 C `xrtPlistSave` 的最终写盘改为 XRT 原子写入，新增旧文件保持与成功读回 C 回归。Windows C/XLang、Linux ASan/UBSan 门禁通过；完整 AOT 阻断不变。
2026-10-15 修复 XML 实数读写受 `LC_NUMERIC` 影响：逗号小数点下原写出 `<real>1,5</real>`，并拒绝标准 `<real>1.5</real>`；C 负例先红后绿，现复用 XRT 数字解析/写出模块。Windows C/XLang 包门禁及 Linux ASan/UBSan 通过；AOT 的既有主线阻断不变。
2026-10-15 修复读取错误码丢失：未知长度流超限、分配失败、缺失文件和随后成功恢复都有 C 回归；XLang 增加超限及缺失文件的错误码断言。另修正 C 侧 0 输入限额被当作 0 字节上限，以及 XML 子解析器误拒绝 0 限额的问题；XML 深度超限现返回限额错误。Windows C/XLang 和 Linux ASan/UBSan 包门禁通过；完整 AOT 仍待共享主线修复。
2026-10-16：XLang 桥接构树时提前执行 `maxNodes/maxEntries/maxItem`，补阈值正反例及 `dump` 超限保留旧文件测试；路径负例改用独立临时目录并在 `finally` 清理。两条包内用例登记为 `coverage.tests`。当前 Windows C/XLang 与双语范例包级门禁通过；Linux sanitizer 和完整 AOT 本轮未重跑，主线 `var as string` 缺陷由包内安全转换路径避开。
2026-10-17：Linux 内嵌 TCC 编译日期换算时，原 `long double` 中间计算将 `time(1)` 误判为不可精确保存；独立 GCC 测试和 Windows 路径未暴露此问题。包内改用整数秒/微秒拆分，写出前通过同一读取规则核对无损往返，并以安全整数边界检查替代浮点极值比较。原 C 日期向量与 XLang 边界从失败转为通过；Windows 包门禁、Linux GCC ASan/UBSan 和新增 TCC C/XL 双语联合门禁均通过。AOT 主线阻断不变。
2026-10-17：XML 整数原在去空白分配失败后将空指针交给 CRT `strtoll`；整数/实数现直接调用 XRT 长度感知解析并允许首尾空白，省去临时分配。继续对 XML 标量做故障扫描，修正字符串、实数、日期与数据节点的内存错误分类；Base64 改用 XRT 严格解码，去掉私有清洗分配与填充代码。C 端五种标量各 128 点故障扫描、极值及非法文本、XLang 正负例，Windows 和 Linux ASan/UBSan 联合门禁通过。完整 AOT 阻断不变。
2026-10-18：拥有型 Reader/Writer 原在正文成功后忽略关闭失败；
自定义失败流 C 用例先红后绿，现报告 `IO`，借用流不被提前关闭，
正文已有格式或输出限额错误时保持首因。Windows C/XLang 包级门禁与
Linux ASan/UBSan、XLang/双语对照通过；当前边界 AOT 仍缺主线
`xl6ValueDataSnapshotV1`，本包未签收完整发布验收。
2026-10-18：新增包外访问 `NativeDocument` 的编译负例，并接入 Windows/Linux
包级门禁。两端均确认导入器拒绝未导出类型；Windows C/XLang 门禁及 Linux
ASan/UBSan、XLang 与 `plistlib` 互操作门禁通过。AOT 阻断状态不变。
2026-10-19：C Reader 输入限额按当前读取位置到结尾的剩余字节数计算，
不按 Reader 总长度计算。带前缀 Seek 的合法输入先红后绿；上限少一字节
仍报 `LIMIT`，只有 `Size` 而没有 `Tell` 的 Reader 由读取阶段执行限额。
Windows C/XLang 和 Linux ASan/UBSan、`plistlib` 互操作门禁通过；完整
AOT 的共享主线阻断不变。
2026-10-19：C 配置的 `tLimits.iReserved` 必须为 0。读/写两条负例先红后绿，共用选项解析器拒绝非零保留字段；Windows 包级 C/XLang 与 Linux C ASan/UBSan、XLang、Python `plistlib` 四格式互通门禁通过。完整 AOT 共享阻断不变。
同日：`tLimits.iFlags` 对 plist 没有定义的行为，原来非零值被静默忽略；解析与写出负例先红后绿，现统一返回 `ARGUMENT`。Windows 包级与 Linux C ASan/UBSan、XLang、`plistlib` 互通门禁复跑通过；完整 AOT 共享阻断不变。
2026-10-22：复查 XML DOM 拥有型节点引用后，新增活动分配数回归，覆盖嵌套容器成功解析、容器内非法文本、多根值、缺少字典键，以及嵌套解析的 256 个分配故障点。所有路径在销毁结果后均回到解析前的活动分配数，未发现 RSS 同类引用泄漏；无需修改 plist 解析器。Windows 包级 C/XLang 门禁和 Linux C ASan/UBSan、XLang、双语范例及 `plistlib` 互通门禁通过。本轮未重跑完整 AOT，不据此签收整包发布级。

2026-10-23：Windows 包级门禁默认增加最小导入及公开范例 AOT 构建/执行，范例结果必须与 C/JIT 逐行一致；只在显式 `-SkipAot` 时跳过。当前快测通过，默认门禁在最小导入构建处因共享 `xl6ValueDataSnapshotV1` 未解析而失败，独占目录已清理。此前只测 C/JIT 的 PASS 不再作为发布签收。
