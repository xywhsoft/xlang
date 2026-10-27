# cbor

2026-10-27 重建 Windows 发布版后，独立 `Buffer` AOT 探针已构建并运行，
旧 `xl6NativeHandleTraceLeaf` 缺符号不再是当前首阻断。默认包门禁的
严格 C 编译、原生边界/分配故障及 C 范例通过，随后 XLang
`test/boundaries.xl` 在语言字典传入 `encode(var)` 时报告
`NotImplementedError: CBOR cannot encode this dynamic value type`。
既有 `dev/extlib_blockers/cbor_var_projection.xl` 用整数正例和数组
负例固定同一投影边界；本次字典失败由 `test/boundaries.xl` 复现。
等待主线统一修复 foreign `var` 容器投影后再跑完整双语/AOT 门禁。

2026-10-25 使用当日重建的 Windows 发布版执行默认 `test/check.ps1`：
manifest 严格 C、C 边界/分配故障与 C 范例均通过；首个 XLang
`test/boundaries.xl` 在语言数组进入 `encode(var)` 时抛
`NotImplementedError: CBOR cannot encode this dynamic value type`，尚未进入 AOT。
`dev/extlib_blockers/cbor_var_projection.xl` 用整数正例与单元素数组在
JIT/独立 EXE 均复现，见共享 foreign `var` 容器投影记录。本包不解释
编译器私有句柄，当前不能签收完整语言和发布门禁；下文旧版缺符号
及 C/JIT 通过的记录仅对应各自当时的发布物。

成对 C/XLang 范例覆盖六个公开入口：值图往返包含 `UINT64_MAX`，
二进制往返使用 `00ff41`，固定编码为 CBOR `4300ff41`。
Windows/Linux 包门禁均将两种语言的输出与 `examples/basic.expected.txt`
比较，避免两侧同时输出错误结果仍误判通过。

CBOR 编解码以 XRT `xvalue` / `xbuffer` 为 C 数据模型，XLang 使用 `var` / `Buffer`。支持 `null`、布尔、有符号 int64 与无符号 uint64 范围整数、float64、UTF-8 字符串、字节串、数组、文本键字典与 RFC 3339 时间标签 0、epoch 时间标签 1。解码接受 CBOR 的半精度、单精度、双精度浮点和不定长字符串、数组、映射；编码输出定长形式。字节串可用 `encodeBytes`、`decodeBytes` 直接处理。

`var` 无法无损表示任意 CBOR 数据模型：超出 int64 的负整数、非文本映射键、未知语义标签及未建模的简单值均明确报错。正整数可表示至 `UINT64_MAX`；解码不超过 `INT64_MAX` 的值沿用 `int`，超过时用 XRT `uint64` 保存。线格式不保留较小正整数原本的有符号类型。重复映射键、无效 UTF-8、截断/尾随数据也报错，不会静默覆盖或截断。编码没有承诺 RFC 8949 的确定性/规范形式，不能直接用输出字节做跨实现签名。CBOR 数据限制为 64 MiB，HEX 文本最多可达 128 MiB；嵌套深度限制为 64。时间值的 tag 0 载荷再占一层深度，编码器不会生成解码器因深度超限而拒绝的时间值。C 入口将缺失输入报为 `XERR_ARGUMENT`，资源超限报为 `XERR_RANGE`；XLang 中畸形数据抛 `ValueError`，深度超限可捕获 `RangeError`，分配故障抛 `MemoryError`。

CBOR 文本键和字符串值都按显式字节长度处理，合法 UTF-8 中的内嵌 NUL 可以完整往返；这与 `fromHex` 的 NUL 结尾文本参数不同。包内 C 测试用固定 9 字节 CBOR 向量、XLang 测试用字典往返固定该边界。
不定长 bytes/text 的分块先拼入 XRT Buffer，完整遇到 break 后由 `xvalue` 接管内存，避免再复制整份结果；空分块结果及接管时的分配失败也进入 C 故障扫描。定长字符串仍从借用的输入线格式复制，不能接管调用方的内存。
`decodeBytes` 是字节专用入口：定长载荷直接复制到最终 `Buffer`，
不定长载荷复用同一分块读取器并直接交出拼接缓冲；不再先建立
`xvalue` 字节串再复制一份。非字节类型、尾随数据和损坏线格式仍报错。
未知语义标签在解码其内层值之前拒绝，避免对明确不支持的数据做无用的递归解析。整份文档的数组元素与字典条目合计最多 100 万项，编解码均执行这一预算；解码不定长字符串时，分块数也计入总预算。超过上限报告范围错误。
定长头部编码的 `2^64-1` 不是不定长标记：字节串、文本、数组、映射在该长度下均报范围错误；只有附加信息 `31` 才进入不定长解析。

C 接口见 `cbor_native.h`：编码和 `decodeBytes` 返回调用方拥有的 `xbuffer*`，以 `xrtBufferDestroy` 释放；`decode` 成功返回拥有型 `xvalue*`，合法 CBOR `null` 为 `XVALUE_NULL`，失败返回 C 的 `NULL`；`to_hex` 返回调用方拥有的字符串，以 `xrtFree` 释放。失败时用 `xcbor_error_message()` 查错。供 XLang 使用的 `_abi` 入口只做指针/空值哨兵适配，不另行实现编解码；XLang `decode` 用线程错误状态区分合法 `null` 与失败。`fromHex` 的语言入口拒绝内嵌 NUL，不能只解析有效前缀。
原生 `xcbor_from_hex` 在读取 C 文本终止符时最多扫描 128 MiB 加一个字节；
超限返回 `XERR_RANGE`，不会先对不可信长度调用无界 `strlen`。

范例：`release/extlibs/cbor/examples/cbor.xl` 与 `cbor_c.c`，以及 `release/examples/extlibs/cbor.xl`。在 `demo6` 目录执行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/cbor/test/check.ps1 -SkipAot` 跑包级 C/JIT 门禁；去掉 `-SkipAot` 加测 AOT。
在 `demo6/release` 下可直接运行 C 范例：`./xl run extlibs/cbor/examples/cbor_c.c`。
该单文件范例包含 `cbor_native.c`；正式扩展库仍通过包清单独立编译
原生源码，不要在同一范例构建命令中重复加入。
Linux 可执行 `wsl bash release/extlibs/cbor/test/check.sh`；脚本以严格 GCC、ASan/UBSan
运行 C smoke、边界用例和 C 范例，同时运行 XLang 边界和范例并逐字节核对双语输出，不依赖发布目录中的临时文件。
当前正式 `xl.exe` 的完整门禁已复跑：包级 C/JIT 通过，但首个核心 AOT 构建缺少
`xl6NativeHandleTraceLeaf`；独立 `Buffer` 最小复现同样失败，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`。此前边界 AOT 还曾缺
`xl6ValueIndexReadCopyV1`，需在首个阻断修复后重新验证；本包不添加绕过。
包 manifest 与原生源码的七项 XRT 模块声明现已一致；同一
`test/aot_core.xl` 从 3,624,960 缩至 661,504 字节，运行结果不变。
完整 AOT 的共享缺符号阻断仍需另行解决。
可选的 [cbor2](https://pypi.org/project/cbor2/) 6.1.5 双向互操作检查：C/xlang 范例输出同字节、外部库可读取；外部库写出普通容器、`bytes`、RFC 3339 tag 0 与带小数秒的 epoch tag 1，C/xlang 均可读取，其中 C 侧核对到微秒。`cbor2` 只安装在 `build/test` 的隔离环境，不进入发布包。从 `demo6` 目录运行：

```powershell
python -m venv build\test\cbor\venv
.\build\test\cbor\venv\Scripts\python.exe -m pip install cbor2==6.1.5
.\release\xl.exe run release\extlibs\cbor\examples\cbor.xl -- build\test\cbor_example_xl.bin
.\release\xl.exe build release\extlibs\cbor\examples\cbor_c.c --inc-path lib\xrt -o build\test\cbor_example.exe
.\build\test\cbor_example.exe build\test\cbor_example_c.bin
.\build\test\cbor\venv\Scripts\python.exe release\extlibs\cbor\test\check_external.py build\test\cbor_example_c.bin build\test\cbor_example_xl.bin build\test\cbor_python.bin
.\release\xl.exe run release\extlibs\cbor\test\external_python.xl -- build\test\cbor_python.bin
.\release\xl.exe build release\extlibs\cbor\test\external_python_c.c release\extlibs\cbor\cbor_native.c --inc-path lib\xrt -o build\test\cbor_external_c.exe
.\build\test\cbor_external_c.exe build\test\cbor_python.bin
```

范例的路径参数只用于保留测试产物；不传路径时输出不变。单个简单范例字节相同不代表承诺 CBOR 确定性编码。

2026-10-14 追加 C 时间值往返回归：从 epoch 小数秒解码后重新编码，以及零点两侧和现代日期的非整秒微秒值，均可无损读回；Windows C/XL 包级门禁通过，未改动编解码实现。
2026-10-15 补未知标签包裹超限容器的红绿回归：旧实现先解析容器并返回限额错误，新实现立即返回不支持标签；Windows C/XL 门禁及 Linux GCC ASan/UBSan 门禁通过。
2026-10-15 补编码超限的红绿回归：超过 100 万项的数组原返回失败却不设置 XRT 错误；现在数组、字典均明确报告 `XERR_RANGE`。Windows C/XLang、双语范例与 Linux ASan/UBSan 包级门禁通过；完整 AOT 主线阻断不变。
2026-10-15 修复定长最大值与不定长哨兵碰撞：四类头部接 break 原会误解析为空值，现统一报 `XERR_RANGE`。C 负例先红后绿，Windows C/XLang 与 Linux ASan/UBSan 包级门禁通过；完整 AOT 当前首个阻断为上述 native-handle 缺符号。
2026-10-15 修复编码和解码的累计项数预算不一致：两个各含 50 万项的数组原可编码、不可解码。恰好 100 万项成功往返，再多两项明确报 `XERR_RANGE`；Windows C/XLang 包级门禁、双语范例及 Linux ASan/UBSan 通过。完整 AOT 首个阻断仍是 `xl6NativeHandleTraceLeaf`。
2026-10-16 新增 C 与 XLang 六个公开入口的逐点分配故障扫描。C 侧编码、嵌套容器解码、字节串编码/解码和 HEX 往返，在每个触发的分配点均返回空结果与 `XERR_MEMORY`，清除故障后可重试；XLang 同一路径均捕获 `MemoryError`，不误报 `ValueError`。Windows `-SkipAot` 与 Linux ASan/UBSan 包级门禁通过；完整 AOT 的共享缺符号阻断不变。
2026-10-21：XLang 故障扫描从每入口固定前 24 点改为逐入口扫描到首次
无故障成功，要求每个入口都出现触发故障与正常完成，并在每个故障点
解除注入后立即重试。Windows 与 Linux 正式运行器上的六入口用例均通过；
原生实现未改，完整 AOT 的共享阻断未变。

2026-10-18：`xcbor_from_hex` 的无界 `strlen` 被 128 MiB 加一字节且无
终止符的 C 输入在 Linux ASan 下复现堆越界读取；现限界扫描并把已取得的
长度直接交给 XRT HEX 解码。红绿 C 回归、Windows 严格 GCC/C/XLang
及双语范例、Linux ASan/UBSan 与 XLang 对照门禁通过。完整 AOT 仍受
上述共享主线缺符号阻断，未作发布签收。

同日增加嵌套不定长数组与分块字节串的逐长度截断矩阵：在外层 break
到达前，每个严格前缀都必须失败并留下诊断；完整输入须还原数组、原始
字节串和 null。Windows `-SkipAot` 全包门禁及 Linux ASan/UBSan
联合门禁通过；这不代替任意 CBOR 输入 fuzz 或完整 AOT 验收。

同日增加固定种子的短输入扫描：遍历 256 种首字节和 8 种输入长度，
分别调用通用解码与字节串解码，要求成功时错误槽为空、失败时留下诊断。
Windows `-SkipAot` 严格 C/XLang 门禁与 Linux ASan/UBSan 联合门禁通过；
这是有限的确定性回归，不宣称覆盖任意长度或替代持续 fuzz。
当前完整 Windows 门禁在首个核心 AOT 构建仍缺
`xl6NativeHandleTraceLeaf`，见 `dev/extlib_blockers/native_handle_trace_aot.md`。

2026-10-19：新增 C 固定 9 字节向量和 XLang 字典往返，验证 UTF-8 文本键
与字符串值中的 NUL 都按 3 字节保留。Windows `-SkipAot` 严格 C/XLang、
双语范例与 Linux C ASan/UBSan、XLang 联合门禁通过；编解码实现无需
修改。完整 AOT 的共享主线阻断不变，未签收发布级。

2026-10-19：C 范例补齐单文件 `xl run` 路径；Windows 严格 GCC、TCC、
C/XLang 同输出及 Linux C ASan/UBSan、TCC/XLang 同输出门禁通过。
正式包仍独立编译原生实现，完整 AOT 的
共享主线阻断不变。

同日修正四个公开 C 输入路径的超限错误分类：`decode`、
`decodeBytes`、`encodeBytes`、`toHex` 超过 64 MiB 时报告 `XERR_RANGE`，
不再与缺失输入的 `XERR_ARGUMENT` 混同。新增一次分配的四入口红绿
回归；Windows `-SkipAot` 与 Linux ASan/UBSan 包级门禁通过。
完整 AOT 仍受共享主线缺符号阻断。

2026-10-21：新增 C 标准线缆字节头回归，固定字节串与文本在长度
23/24、255/256、65535/65536 的编码切换，并解码核对完整载荷；
XLang 边界同步覆盖 23/24、255/256 文本往返。Windows `-SkipAot`
严格 C/XLang、双语门禁及 Linux C ASan/UBSan、XLang 联合门禁通过。
本轮未修改编解码实现；完整 AOT 的共享主线阻断不变。

2026-10-21：XRT 已有 `XVALUE_UINT`，本包却仍拒绝 CBOR 主类型 0 中超过 `INT64_MAX` 的合法值。C/XLang 红测后补全无符号编解码，固定 `0x1b` 最大值字节、返回类型与分配失败恢复。成对 C/XLang 范例及 `cbor2` 6.1.5 双向互通均覆盖 `UINT64_MAX`；Windows `-SkipAot`、Linux C ASan/UBSan 与 XLang 双语门禁通过。完整 AOT 的共享主线缺符号未变，不签收整包发布级。

2026-10-22：C 边界新增一份有效的嵌套 map/不定长 array/分块 bytes
向量，逐个截断并逐位翻转 16 字节线缆，检查解码结果与失败诊断一致。
Windows `-SkipAot` 严格 C、XLang 与双语门禁，Linux C ASan/UBSan、
XLang 门禁通过。这只覆盖确定的短复合输入，不替代持续 fuzz；完整 AOT
仍受共享主线缺符号阻断。

2026-10-22：六个公开编解码/HEX 入口的已有逐点分配故障扫描增加
XRT 活动分配数和字节数基线检查；`UINT64_MAX` 解码的故障与恢复也
核对释放后基线。Windows 严格 C/XLang 与双语范例、Linux C ASan/UBSan
及 XLang 门禁通过；未发现包内泄漏，线格式和公开 API 未改。
完整 AOT 本轮未复验，不据此签收整包发布级。

2026-10-23：`decodeBytes` 改为直接产出最终 `Buffer`，通用解码与专用
解码共用不定长分块读取器。C 边界新增非字节、未知标签与尾随字节的
错误分类，分块字节入口纳入逐点分配失败和内存平衡扫描。Windows
`-SkipAot` 严格 C/XLang 与双语门禁、Linux C ASan/UBSan 联合门禁通过；
完整 AOT 的共享主线缺符号阻断未解除，不据此签收整包。
