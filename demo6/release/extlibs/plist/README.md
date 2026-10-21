# plist

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
- Windows：`powershell -File release/extlibs/plist/test/check.ps1`，包含 C/XL 边界和双语范例同输出检查。
- Linux：`sh release/extlibs/plist/test/check.sh`，以 ASan/UBSan 检查 C 本体与桥接层，另用内嵌 TCC 执行同一份 C 日期测试、运行包内 XLang 边界/分配测试，核对双语范例输出，并由 Python 标准库读取四份范例产物。

桥接层的 C `uint64_t`（XRT `uint64`）在 XLang extern 中统一声明为 `uint64`，包括输入/输出限额、数据长度及 UID；不使用在 Linux LP64 下不同类型的 `c_ulonglong`。Linux 包级脚本同时运行 XLang 主用例，防止声明再次漂移。

Python 标准库 `plistlib` 可独立读取双语范例产生的 XML/binary plist，不需要额外安装依赖。两份 XML 解码值及字节一致；两份 binary 解码值一致，且比 XML 多一个 `UID(42)`。binary plist 的字节布局不承诺一致。从 `demo6` 目录运行：

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
