# MessagePack 扩展库

`msgpack` 使用 `Buffer` 承载二进制数据，`var` 承载动态值。`nil` 是合法值，`decode` 返回 XLang `null`，不会误报为解码失败。

```xlang
import msgpack

Buffer packed = msgpack.encode({"name": "xlang", "count": 7})
var document = msgpack.decode(packed)
println(document["name"])
```

公开接口：`encode`、`decode`、`encodeBytes`、`decodeBytes`、`toHex`、`fromHex`。后两者面向调试及固定向量；正常数据交换直接使用 `Buffer`。
成对 C/XLang 范例现展示全部六个入口：值图往返含 `uint64`/`time`，二进制往返含 NUL 和非 UTF-8 字节。后者固定 wire 为 `c40300ff41`，还原为 `00ff41`；两侧输出须匹配 `examples/basic.expected.txt`。

| XLang `var` | MessagePack |
| --- | --- |
| `null`、`bool`、`int`、`uint64`、`float` | nil、bool、整数、float64 |
| `string`、字节值 | str、bin |
| `time` | 标准 timestamp 扩展类型 `-1`（32/64/96 位） |
| `array`、`dict` | array、map（只接受 UTF-8 字符串键） |

解码接受 float32 并转换成 XLang 浮点数；时间以 XRT 微秒精度表示，编码选最短标准布局。外部时间戳的纳秒部分若不是整微秒，或时间超出 XRT `time` 范围，会明确报错，不做静默舍入。整数可覆盖标准线格式的有符号 int64 和无符号 uint64；解码值不超过 `INT64_MAX` 时沿用 `int`，超过时用 XRT `uint64` 保存。线格式无法保留较小正整数在输入端的有符号类型区别。其他未建模的 ext 类型仍明确拒绝。编码不支持的 `var` 类型也明确失败。文本键和值都按显式长度处理，允许内嵌 NUL，不会在第一个 NUL 处截断；HEX 文本入口则拒绝内嵌 NUL。二进制输入/输出上限 64 MiB；调试用 HEX 文本最多 128 MiB。C 入口将缺失输入报为 `XERR_ARGUMENT`，超限报为 `XERR_RANGE`；这些输入错误在 XLang 层抛 `ValueError`，分配故障另抛 `MemoryError`。整个文档的数组项和字典项合计最多 100 万，嵌套深度最多 64；编解码均采用相同预算。重复 map 键、非法 UTF-8、截断与尾随字节都拒绝。范围遵循 [MessagePack 规范](https://github.com/msgpack/msgpack/blob/master/spec.md)允许应用层约束 map 键和扩展类型的设计。

C 侧包含 `msgpack_native.h`，返回的 `xbuffer*` 用 `xrtBufferDestroy`，返回的 `xvalue*` 用 `xrtValueRelease`，`xmsgpack_to_hex` 返回的字符串用 `xrtFree`。`xmsgpack_encode(NULL)` 是缺少参数，返回 `NULL` 并设置 `XERR_ARGUMENT`；传入 `xrtValueNull()` 则合法编码为 MessagePack `nil`。C 的 `xmsgpack_decode` 失败返回 `NULL`，合法 `nil` 返回拥有型 `XVALUE_NULL`；XLang `_abi` 将失败值适配为空值哨兵，用线程错误槽区分。HEX 文本入口拒绝内嵌 NUL，不会只解析前缀。
原生 `xmsgpack_from_hex` 查找 C 文本终止符时最多读取 128 MiB 加一个字节；
超限返回 `XERR_RANGE`，不会对不可信长度调用无界 `strlen`。
公开 C 入口的新调用不会继承上次的错误状态；失败后成功调用 `toHex`，
`xmsgpack_has_error()` 不会继续报告旧错误。

在 `demo6` 目录运行：

```powershell
./release/xl.exe run release/extlibs/msgpack/examples/msgpack_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/msgpack/test/check.ps1 -SkipAot
```

C 教学范例包含包内原生实现，可由 `xl run` 单文件执行；正式包仍由
manifest 单独编译该实现。范例在参数错误、解码失败或文件写入失败时
统一释放已持有的值和缓冲，不留下半成品。Windows/Linux 包门禁将
直跑与严格 C 编译输出逐字比较；Linux ASan 还执行两条失败出口。

Linux 包级门禁：`sh release/extlibs/msgpack/test/check.sh`，对 C 核心和边界用例
启用 ASan/UBSan 与泄漏检查，并运行 XLang 边界、双语范例逐字节对照。
2026-10-13 复现并修复 map 键分配失败被误报为
类型错误，以及截断的 uint64 被误报为范围错误；故障注入、严格 GCC、
C/XLang 边界和 Linux sanitizer 均通过。

`-SkipAot` 只运行严格 C、XLang JIT 和双语对照；不带该开关时依次验证核心、边界及带输出范例 AOT。当前发布构建在核心 AOT 即缺少 `xl6NativeHandleTraceLeaf`，见 `dev/extlib_blockers/native_handle_trace_aot.md`，因此完整包级门禁不能通过。C/XLang 对照范例在 `examples/`。
与 [MessagePack 官方 Python 实现](https://github.com/msgpack/msgpack-python) 的双向互操作可选验证：C/xlang 保存的文件字节一致且 Python 可解码；Python 写出的 UTF-8、64 位整数、嵌套值及 `bin` 原始字节，C/xlang 均可读取。Python 包只安装在 `build/test` 的隔离环境，不进入发布包。从 `demo6` 目录运行：

```powershell
python -m venv build\test\msgpack\venv
.\build\test\msgpack\venv\Scripts\python.exe -m pip install msgpack==1.2.3
.\release\xl.exe run release\extlibs\msgpack\examples\msgpack.xl -- build\test\msgpack_example_xl.bin
.\release\xl.exe build release\extlibs\msgpack\examples\msgpack_c.c release\extlibs\msgpack\msgpack_native.c --inc-path lib\xrt -o build\test\msgpack_example.exe
.\build\test\msgpack_example.exe build\test\msgpack_example_c.bin
.\build\test\msgpack\venv\Scripts\python.exe release\extlibs\msgpack\test\check_external.py build\test\msgpack_example_c.bin build\test\msgpack_example_xl.bin build\test\msgpack_python.bin
.\release\xl.exe run release\extlibs\msgpack\test\external_python.xl -- build\test\msgpack_python.bin
.\release\xl.exe build release\extlibs\msgpack\test\external_python_c.c release\extlibs\msgpack\msgpack_native.c --inc-path lib\xrt -o build\test\msgpack_external_c.exe
.\build\test\msgpack_external_c.exe build\test\msgpack_python.bin
```

范例的文件路径参数只用于保留互操作产物；不传路径时输出示例字段。双语范例写出微秒时间，Python 独立实现读取；Python 写出的负时间戳又由 C/XLang 读回。此项只验收上述公开子集，不意味着支持其他 MessagePack ext。

本轮固定种子随机输入发现超限 `str32` 以及过深嵌套曾返回失败却未设置错误；
现已给这两条路径设置诊断并加入最小回归。当前 C/JIT 门禁通过；完整门禁
在核心 AOT 缺少 `xl6NativeHandleTraceLeaf` 处停止，未签收全部发布形态。
包 manifest 现与原生源码的六项 XRT 模块声明一致；同一
`test/aot_core.xl` 从 3,639,296 缩至 663,552 字节并执行通过。
边界 AOT 的历史结果与 C/XLang 对照通过；当前完整 AOT 仍受上述共享链接阻断。
2026-10-15 追加标准 timestamp 32/64/96 编解码：Python `msgpack` 1.2.3 生成的固定向量覆盖零点、负时间、64 位秒数及 XRT 时间上下界；外部 Python 双向互操作、Windows C/XL 门禁与 Linux ASan/UBSan 通过。完整带输出 AOT 仍等待共享缺符号修复。
2026-10-15 补编码超限的红绿回归：超过 100 万项的数组原返回失败却无 XRT 错误；现在数组、字典均明确报告 `XERR_RANGE`。Windows C/XLang、核心及边界 AOT、双语范例与 Linux ASan/UBSan 通过；带输出完整 AOT 阻断不变。
2026-10-15 追加 4096 个固定种子时间点与格式边界矩阵，32/64/96 位时间戳均精确往返；Windows 严格 GCC、C/XLang JIT 与 Linux ASan/UBSan 通过。包内边界用例已登记到统一 smoke。此前 AOT 结果是旧构建的历史记录；当前 `release/xl.exe` 的核心 AOT 停在上述 native handle trace 缺符号，完整 AOT 仍未签收。
2026-10-15 修复编码与解码的累计项数预算不一致：两个各含 50 万项的数组原先编码成功、解码拒绝；编码现与解码统一按整个文档合计计数，超限报告 `XERR_RANGE`。红绿回归、Windows C/XLang 包门禁、双语范例和 Linux ASan/UBSan 通过；完整 AOT 阻断不变。
2026-10-16 六个公开编解码/HEX 入口新增 C 与 XLang 逐点分配故障扫描。C 嵌套容器解码覆盖数组、字典和字节串释放，触发失败时结果为空且保留 `XERR_MEMORY`，清除故障后重试成功；XLang 同类故障均捕获 `MemoryError`，不误报 `ValueError`。Windows `-SkipAot` 与 Linux ASan/UBSan 包级门禁通过；完整 AOT 的共享缺符号阻断不变。
2026-10-21：XLang 六入口故障测试不再固定只扫前 24 个分配点；
每入口扫描到首次无故障成功，要求故障与成功均出现，且每次触发故障
后立即解除注入并重试。Windows/Linux 正式运行器上的用例通过；
包实现与线格式未改，完整 AOT 的共享阻断未变。
2026-10-17 复合 map/array/bin 样本新增 13 个截断长度与 96 个单比特扰动的 C 边界扫描，验证解码结果与错误槽一致；Windows C 边界及 Linux ASan/UBSan、XLang 和双语范例门禁通过。该固定样本扫描不代表完整 fuzz，完整 AOT 阻断不变。

同日追加全部 256 个线格式首字节各 18 种短输入长度的 C 解析矩阵，
要求成功无错误、失败有诊断。Windows 严格 GCC、Linux ASan/UBSan 与
包内 XLang、双语范例门禁通过；矩阵未发现新的包内缺陷，不代替完整
fuzz 或带输出 AOT 验收。

2026-10-18：原生 `fromHex` 在检查 128 MiB 文本上限之前调用无界
`strlen`。没有终止符的限额边界输入在 Linux ASan 下先复现堆越界读取；
改为限界扫描后，同一 C 用例、Windows 严格 GCC/C/XLang/双语及 Linux
ASan/UBSan/XLang 包级门禁均通过。完整 AOT 的共享缺符号阻断不变。

2026-10-19：新增带 NUL 的 map 文本键和值的固定线格式 C 向量和 XLang
往返用例，核对三个字节均保留。Windows `-SkipAot`、Linux C ASan/UBSan
及 XLang 门禁通过；实现符合显式长度合同，无需改动。完整 AOT 仍未签收。

2026-10-21：XRT 已支持 `XVALUE_UINT`，本包原先仍拒绝编码无符号值，并拒绝解码超过 `INT64_MAX` 的标准 `0xcf` 整数。C/XLang 边界先红后绿；C 固定字节向量核对 `UINT64_MAX` 与 `INT64_MAX+1` 的类型和值。成对范例和 Python 1.2.3 双向互通增加最大无符号整数，Windows `-SkipAot`、Linux C ASan/UBSan 与双语门禁均通过。完整 AOT 的共享缺符号仍未解除。

同日 C 范例补单文件直跑并统一失败出口清理；Windows `-SkipAot` 的
严格 C、XLang 边界和双语对照通过，Linux C ASan/UBSan、失败出口及
直跑同输出门禁通过。包实现未改，完整 AOT 主线阻断不变。

同日修正公开 C 入口的超限错误分类：`decode`、`decodeBytes`、
`encodeBytes`、`toHex` 输入超过 64 MiB 时均报 `XERR_RANGE`，不再与
缺失输入的 `XERR_ARGUMENT` 混同。新增一次分配的四入口边界回归；
Windows `-SkipAot` 和 Linux ASan/UBSan 包门禁通过。HEX 文本的
128 MiB 上限已在合同中单独说明；完整 AOT 共享阻断不变。

2026-10-21：C 边界测试逐字节核对 str 的 31/32、255/256、65535/65536
字节及 bin 的 255/256、65535/65536 字节格式头，并验证解码后内容完整；
XLang 侧补短字符串跨界往返。Windows `-SkipAot` 严格 C/XLang、双语范例
和 Linux C ASan/UBSan、XLang 门禁通过。默认 Windows 门禁仍在核心 AOT
遇到共享 `xl6NativeHandleTraceLeaf` 缺符号，未签收发布级。
