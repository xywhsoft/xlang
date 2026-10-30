# 当前源码的 var 容器传入原生扩展失败

2026-10-30 根目录布局复验 `rss`：注册链接关系修复后的默认 Windows
门禁前55项符合预期（含一个ABI类型冲突负例），第56原中央
`rss.buildRSS(channel, [])` 仍报 ValueError；Linux sanitizer、JIT/AOT
和双语例完成相同前置后同样失败。独立复现从仓库根执行
`release/xl.exe run dev/extlib_blockers/rss_build_var_object_projection.xl`
（Linux 用 `release/xl`），两平台JIT和AOT均先打印 native roundtrip:
PASS，再在同形语言字典/数组处失败、退出1。包不识别编译器私有句柄，
不以原生 parse 对象替代原生成验收。修复仍应在共享 foreign 参数投影
中保留 XRT 对象/数组类型、嵌套元素和所有权，覆盖失败回滚及重复调用；
之后重跑该最小源、原中央用例和默认双平台门禁。

2026-10-30 根目录布局复验 `srt`：三个文本返回改为 @owned 直接接管后，
Windows/Linux 默认门禁分别完成 39/41 项可运行前置验证（含 C/XL 各
22 预算、六条生成原型、新旧双语及七条独立程序），最后原语言 cue
数组仍报 `ValueError: cues must be an array`。原 core_aot 也使用字面量
数组，定向执行同样报红，不能改为解析原生值来签收。从仓库根执行
`release/xl.exe run dev/extlib_blockers/srt_build_var_array_projection.xl`
（Linux 用 `release/xl`），JIT 与独立程序仍先打印 native roundtrip:
PASS，再在同形语言数组处退出 1；check/build 成功。修复仍应位于共享
foreign 容器投影，不在 SRT 里识别编译器私有句柄。报告见验收矩阵及
任务台账 2026-10-30 SRT 条目，完整原语言边界待主线修复后复跑。

2026-10-30 根目录布局复验 `graph`：Windows/Linux 包门禁各完成 38 项
可运行前置检查（含全部九条实际生成原型、三种独立程序、两种 DLL/SO
同上下文各三轮正常 JSON 范例），最后原中央语言边数组仍报
`ValueError: graph edges must be an array`。从仓库根执行
`release/xl.exe run dev/extlib_blockers/graph_var_array_projection.xl`
（Linux 用 `release/xl`）仍退出 1；同源 check/build 成功，但独立程序
运行同样退出 1。标准 json.parse 返回的原生配置可用于九项图 API，不代表
语言字面量投影已经修复；没有新增工厂、包侧私有句柄识别或序列化绕行。
修复仍应在主线共享 foreign 参数投影层完成，覆盖嵌套行数组、类型、所有权、
失败回滚及重复调用，再复跑最小源和完整包级门禁。证据见验收矩阵与台账
2026-10-30 Graph 条目；普通扩展库任务不修改主线。

2026-10-27 从当前工作树执行 `demo6/build.bat` 成功后复验：
`release/xl.exe run dev/extlib_blockers/ical_build_var_object_projection.xl`
先输出 `native roundtrip: PASS`，随后语言字典调用 `ical.build` 仍抛
`ValueError: invalid calendar fields or output exceeds 8 MiB`。这确认问题
并非只存在于 2026-10-26 的旧发布物；本轮未修改主线或包侧 ABI。

## 已定位的数组物理表示边界（2026-10-26）

从 `demo6/release` 执行
`./xl.exe cgen ../dev/extlib_blockers/foreign_var_array_container.xl --backend xlir`
可见语言字面量 `[2, 4]` 首先由 `xl6SequenceReferenceCreateV1` 建立，
再经过 `xl6ArrayProjectionTakeV1` 和 `xl6ArrayValueAppendTakeV1`；
`statistics.sum(values)` 最终把保留的同一个 `xvalue*` 直接交给
`xstat_sum(v12)`，调用前没有转换为 XRT 原生数组的步骤。
`src/runtime/xl6_sequence.c` 的 `xl6SequenceWrapAsV1` 使用
`xrtValueHandleTake` 包装这一序列，因此实参的物理类型是
`XVALUE_HANDLE`，而 `xstat_sum` 要求 `XVALUE_ARRAY`。语言侧
`type(values) == array` 与 C 侧 `xrtValueType(values) == XVALUE_ARRAY`
并不是同一个判断。这条链路已解释 `statistics` 的最小复现，
不能据此断言下文所有字典或嵌套容器故障都已证明完全同根。

主线应在**外部调用 ABI 边界**按期望的 XRT 容器形态投影元素与所有权，
而非只把 `XVALUE_HANDLE` 的指针交给 C，或让各扩展库认识编译器私有
序列句柄。投影须覆盖嵌套容器、失败回滚和重复调用；修复后用本文件
列出的最小复现及包级 JIT/AOT 门禁验证。普通扩展库任务不修改该边界。

2026-10-26 `com.invoke` 新增确定性夹具最小复现：
`dev/extlib_blockers/com_foreign_array_projection.xl` 显式同编
`release/extlibs/com/test/boundaries.c`，创建测试 `IDispatch` 后调用
`com.invoke(handle, "LongText", array[])`；Windows JIT 与独立 EXE
均报 `ValueError: arguments must be an array`。同一发布物的 COM
原生 C 边界通过，包级门禁在此之前还通过隐式清理的 JIT/AOT。
诊断来自 `x_com_artifact.c` 对传入 `xvalue*` 的 `XVALUE_ARRAY` 检查；
与其他 foreign 容器问题同形，但尚未证明底层完全同根。主线应检查
外部 C 调用参数中语言 `array`/`var` 到 XRT 值的类型、元素和所有权
投影；修复后先跑该最小源，再跑 COM 完整边界与 C/XLang 范例，
不得在 COM 包内解读编译器私有数组句柄。

从 `demo6/release` 执行：

```powershell
./xl.exe run ../dev/extlib_blockers/com_foreign_array_projection.xl extlibs/com/test/boundaries.c
./xl.exe build ../dev/extlib_blockers/com_foreign_array_projection.xl extlibs/com/test/boundaries.c -o ../build/test/com_foreign_array_repro.exe
../build/test/com_foreign_array_repro.exe
```

2026-10-26 `clipboard.set_files` 提供相邻的 foreign 数组 ABI 证据：
隔离 window station 中，原生 C 用真实 XRT `xvalue` 字符串数组写入
成功；`release/extlibs/clipboard/test/file_list_projection.xl` 创建
同形语言 `array`，`type(paths) == array` 成立，调用公开入口却返回
false 和 `clipboard: invalid input or data`。Windows 默认门禁先通过
C、图形快照和只读语言测试，再在这条正例报红。包内外部声明
`array` 改成 `var` 的受控试验同样失败，现已恢复原签名；无证据
支持包侧重新编码/私有句柄探测。主线应核对 `array`/`var` 实参
到 `xvalue*` 的物理投影、元素类型和所有权，修复后重跑隔离
正例及完整门禁。该症状与其他 foreign 容器问题同形，尚未证明
底层完全同根。

2026-10-26 当前发布物的 `bson` Windows 默认门禁先通过严格 C、
原生边界和 C 范例，随后首个 XLang 字典编码报
`ValueError: bson: encode requires a table (var dict)`。
`dev/extlib_blockers/bson_encode_var_dict_projection.xl` 仅构造语言
字典，先确认 `type(document) == dict`，再调用 `bson.encode`；
Windows/Linux `xl run` 均退出 1 并报同错。修复应在共享 foreign
`var`/`xvalue*` 投影中保留字典类型、嵌套值和所有权，不在 BSON
包内识别编译器私有句柄；修复后重跑两平台默认门禁和独立 EXE。

2026-10-26 当前发布物复验 `srt`：Windows 默认
`release/extlibs/srt/test/check.ps1` 的严格 C、C 边界及 C 范例通过，
中央 `test/cases/extlib_srt.xl` 在 `srt.build(var made)` 报
`ValueError: cues must be an array`，后续边界和 AOT 未运行。
最小源 `dev/extlib_blockers/srt_build_var_array_projection.xl`
在 Windows/Linux 都先完成原生 `parse` 数组的 `build` 往返，再由
语言数组字面量触发同一错误。修复应在共享 foreign `var` 容器
投影边界保留 `XVALUE_ARRAY` 及嵌套字典/数组，不在 SRT 包内
识别私有句柄；修复后复验最小源和两平台默认包门禁。

2026-10-30 根目录布局复验 `toml`：Windows/Linux 默认包门禁分别完成
58/59项可运行前置验证（包括新日历双语、全部十六条实际原型和六个独立
程序），最后仍在原中央字典写出报上述 TypeError。原最小源从仓库根执行
`release/xl.exe run release/extlibs/toml/test/root_argument_repro.xl`；
Linux 使用 `release/xl`。两平台 JIT 均退出1；同源 build 成功，但独立
程序也在相同字典处退出1。不能把 parse 配置的写回、C 成功或构建成功
当作语言字典投影闭环。详情与报告见当前验收矩阵 TOML 行及任务台账。

2026-10-26 重建发布版后复验 `toml`：Windows 默认
`release/extlibs/toml/test/check.ps1` 的严格 C 和原生边界先通过，
随后 `boundaries.xl` 在 `stringify(var literalDict)` 报
`TypeError: TOML document root must be a dict`。现有最小源
`release/extlibs/toml/test/root_argument_repro.xl` 在 Windows/Linux
均退出 1；同一源中，`toml.parse` 返回的原生字典可正常写出，
语言字典字面量传到相同入口却失败。修复应位于语言容器到
foreign `var`/`xvalue*` 的共享投影边界，保留类型和所有权，
不能在 TOML 包内识别编译器私有句柄。修复后先跑该最小源，
再跑两平台默认包门禁、双语输出和独立 EXE AOT；当前未执行后续阶段。

2026-10-26 新发布物复验 `netrc`：Windows 默认包门禁的严格 C、
原生边界、TLS、C/XLang 公开范例、UTF-8 文件往返与 Python
`.netrc` 互通，以及中央/包内语言边界均通过；具名类型用例
`netrc.Netrc({"hosts": {}, "macros": {}})` 报
`ConfigError: netrc document must be a dict`，后续分配与 AOT 未运行。
现有最小源 `dev/extlib_blockers/netrc_constructor_literal.xl` 在当前
Windows/Linux 发布物均先通过 `netrc.parse("")` 对照，再于语言
字典直接构造处失败。修复应让语言字典传入 `var` 形参时保留完整
字典类型与嵌套内容；不能在 netrc 包内识别编译器私有句柄。
主线修复后须重跑最小源和双平台默认门禁，包括独立 EXE。

2026-10-26 新发布物复验 `shellwords`：Windows 默认包门禁的严格
C、`CreateProcessW` 参数往返、C 范例单文件直跑与公开双语范例均
通过；中央 `test/cases/extlib_shellwords.xl` 在
`shellwords.join(var words)` 报
`ValueError: shellwords join requires an array`，后续语言边界与 AOT
未运行。最小源 `dev/extlib_blockers/shellwords_join_var_array_projection.xl`
仅构造两个元素的语言数组并调用 `join`，在当前 Windows/Linux
发布物上均可复现相同错误；C 原生
范例的数组输入及 `join` 通过。修复应让语言 `var` 数组以完整
`XVALUE_ARRAY` 投影到外部 `xvalue*`，而非在本包按私有句柄识别。
修复后重跑最小源与双平台默认门禁，不能只检查 `quote`/`split`。

2026-10-26 新发布物复验 `sitemap`：默认包门禁的 C 边界通过，
`test/cases/extlib_sitemap.xl` 在构建语言数组时报告
`ValueError: sitemap entries must be an array`。现有最小源
`dev/extlib_blockers/sitemap_build_var_array_projection.xl` 再次先输出
`native roundtrip: PASS`，然后同样失败，证明原生解析结果可往返，
而语言数组到 foreign `var` 的投影仍缺失。修复边界保持下文建议，
包内不识别私有数组句柄；修复后须重跑最小源和完整双平台门禁。

2026-10-26 重建发布版后复验 CBOR：`xlc.dll` SHA-256
`C2163D747976C11394C7DDB563FCA1DF0076937CAE263CF2077501177A32CA78`。
`release/xl.exe run dev/extlib_blockers/cbor_var_projection.xl` 仍退出 1，
报告 `NotImplementedError: CBOR cannot encode this dynamic value type`。
`release/extlibs/cbor/test/check.ps1` 的严格 C、边界、分配故障与 C 范例
全过，首个 XLang 边界用例遇到同一错误，后续 AOT 未运行。
新构建解除共享符号阻断，但没有解除 foreign `var` 容器投影问题；
修复后须完整重跑包级 JIT/AOT，不在 CBOR 内识别私有句柄。

2026-10-25 当前 Windows 发布版再次复验 `statistics`：默认
`release/extlibs/statistics/test/check.ps1` 的严格 C、ABI、尺寸保护及
三组 C 范例先通过，第一个 XLang 中央断言 `count([2,4,4,4,5,5,7,9]) == 8`
却收到错误结果，门禁退出 1。更小的既有复现
`release/xl.exe run dev/extlib_blockers/foreign_var_array_container.xl`
退出 1，报 `ValueError: input is not an array`。原生 `xstat_count` 在
非 `XVALUE_ARRAY` 时静默返回 0，`xstat_sum` 则显式报错；两者是同一
投影问题的不同症状。主线修复后须重跑完整 Windows/Linux JIT/AOT 门禁，
不能只复验原生 C 或 `count` 的单条断言。

2026-10-25 CBOR 新发布版的独立最小复现：
`release/xl.exe run dev/extlib_blockers/cbor_var_projection.xl` 与同源
`xl build` 独立 EXE 均在 `cbor.encode(var values)` 抛
`NotImplementedError: CBOR cannot encode this dynamic value type`；此前
`cbor.encode(1)` 的固定字节对照通过。原生编码器已有
`XVALUE_ARRAY` 分支，语言数组到原生 `var` 的表示未满足它。
默认包门禁的严格 C、边界/分配和 C 范例通过，首个 XLang 用例失败，
后续 AOT 未执行；不能沿用旧发布物的“C/JIT 通过、AOT 缺符号”状态。

2026-10-25 `fix` 发布版复验：从 `demo6` 执行
`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/fix/test/check.ps1`，
严格 C 语法、XRT 编译、C 测试与 C 范例均通过；首个
`test/cases/extlib_fix.xl` 在 `fix.build("FIX.4.4", fields)` 抛出
`ValueError: FIX build needs a BeginString and an array of field objects`，
门禁退出 1，未进入后续 XLang 边界与 AOT。相同发布版执行
`release/xl.exe run dev/extlib_blockers/foreign_var_array_container.xl`
仍报 `ValueError: input is not an array`。`fix` 的 C 接口要求
`XVALUE_ARRAY`，故继续按本文件的共享 foreign `var` 投影问题处理；
不能把 C 测试通过或旧版本 `-SkipAot` 通过当作整包发布验收。

2026-10-25 发布版复验：`release/xlc.dll` SHA-256 为
`70A40D58A996B558B91A030799B82EC0C3EB7A88D258717A34FF1A6010B69BBA`。
从 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File
release/extlibs/bson/test/check.ps1`，ABI、严格 C、BSON 边界及 C 范例
先通过；第一个 XLang 边界用例随即报
`ValueError: bson: encode requires a table (var dict)`，退出 1，
后续语言和 AOT 门禁没有执行。仍应在共享 foreign `var` 投影边界修复，
不能把包内 C 通过视为 BSON 发布验收。

从 `demo6` 目录先构建当前源码驱动，再设置包搜索路径：

```powershell
pwsh -NoProfile -File tools/build_xlir_public_source_driver.ps1 `
    -Directory build/c-owned/extlib-foreign-var
$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path
build/c-owned/extlib-foreign-var/public_source.exe `
    dev/extlib_blockers/foreign_var_dict_container.xl run `
    build/c-owned/extlib-foreign-var/dict
build/c-owned/extlib-foreign-var/public_source.exe `
    dev/extlib_blockers/foreign_var_array_container.xl run `
    build/c-owned/extlib-foreign-var/array
```

2026-10-24 的源码驱动分别报：

- `bson: encode requires a table (var dict)`；
- `statistics: input is not an array`。同一数组的 `count()` 还会静默
  返回 `0`，实际应为 `2`。

相同源码编出的独立 EXE 也失败。完整公开范例还在 `cbor`、`msgpack`、
`fix` 的容器输入，以及 `json.patch` 的字典路径失败；这些是同类
现象，尚未逐个证明根因完全相同。当前发布目录的旧 `xl.exe` 运行
`statistics/test/check.ps1 -SkipAot` 时，严格 C、XLang 和双语黄金
输出全部通过；因此不能用旧二进制的通过替代当前源码验收。

2026-10-25 更新发布版后，`msgpack` 的最小复现
`./release/xl.exe run dev/extlib_blockers/msgpack_var_dict_projection.xl`
退出 1，报 `NotImplementedError: MessagePack cannot encode this dynamic value type`。
输入仅是 `var document = {"x": 1}`；原生编码器支持 `XVALUE_OBJECT`，
此诊断来自不受支持的动态 tag 分支。默认包门禁的严格 C、边界、
分配故障及 C 范例全部通过，首个 XLang 边界用例失败，未进入 AOT。
应由主线统一 foreign `var` 容器投影，修复后复跑最小源及完整包门禁；
不能在 MessagePack 包内解释编译器私有句柄。

2026-10-27 以当前 Windows/Linux 发布物再次运行同一 MessagePack 最小源，
两平台均退出 1 并报告上述动态类型错误。Windows 默认包门禁先通过
严格 C、分配故障、C 范例和 C 单文件直跑，随后停在语言边界；
本轮未执行 AOT，旧版缺符号记录不代表当前第一失败点。

生成 C 对 `var document = {"x": 1}` 调用 `xl6DictCreateTakeV1`，
对 `var values = [2, 4]` 使用 `xl6SequenceReferenceCreateV1` 与数组
投影。前者在运行时由 `xrtValueHandleTake` 包装为 `XVALUE_HANDLE`；
`bson_native.c` 的 `xvoIsTable` 则要求 `XVALUE_OBJECT`，
`statistics_native.c` 也按原生 XRT 数组检查。传参指针 ABI 没有
丢失，但值的容器表示不满足原生 `var` 约定。

建议在主线的 foreign `var` 参数边界按实际动态类型统一投影为
XRT 可用的数组/对象快照，明确所有权、循环引用、深度限额和
修改可见性。若本来要求原生 C 通过公开 XRT 类型化容器 API 直接
访问句柄，也须正式定义这一 ABI 并统一更新标准库与扩展库，
不能让每个包自行识别编译器私有的 handle 布局。修复后同时复验
这两个最小文件，以及 BSON、CBOR、MessagePack、FIX、statistics、
JSON Patch 的 JIT/独立 EXE 公开范例。本普通包任务不改主线。

2026-10-24 补充当前 Windows 发布物的 `clipboard` 复现：包级隔离运行器
在 `test/mutate.xl` 的 `clipboard.set_files(files)` 断言处失败，C 入口
`x_clipboard_set_files(xvalue*)` 要求 `xrtValueType(paths) == XVALUE_ARRAY`。
生成 C 对 `array files = [...]` 先调用 `xl6SequenceReferenceCreateV1`，
再调用 `xl6ArrayProjectionTakeV1`，并将该结果直接传入原生函数；
`last_error()` 为 `clipboard: invalid input or data`。未通过修改扩展库
接受编译器内部 handle 布局来绕过，待主线统一 foreign 容器 ABI 后复验。

同日 `com` 的当前 Windows 发布版也复现此边界：在 `demo6/release` 运行
`./xl.exe run extlibs/com/test/boundaries.xl`，C 的 COM 测试对象创建成功，
首次 `com.invoke(handle, "Sub", array[9, 4])` 却抛出
`ValueError: arguments must be an array`。原生 `x_com_invoke_array` 收到的
参数类型不是它声明要求的 `XVALUE_ARRAY`；独立 C 门禁通过。
包内另有不传数组的 `implicit_object_cleanup.xl`，JIT/EXE 均通过，
故该 COM 调用阻断不能与对象析构问题混作一个故障。修复共享边界后应
重跑 `release/extlibs/com/test/check.ps1` 的完整 JIT/AOT 和双语对照。

同日 `shellwords` 的当前 Windows 发布版复现数组侧：
`./release/xl.exe run dev/extlib_blockers/shellwords_var_array_projection.xl`
退出 1，报 `ValueError: shellwords join requires an array`。最小源只调用
`join(var words)`，词法拆分和引号解析均不参与。默认包门禁先通过严格
C、真实 Windows `argv` 往返、两种语言的公开范例，在中央 XLang 用例
调用 `join` 时失败。原生 `xsw_join` 要求 XRT 数组 tag；不在包里读取
编译器私有句柄，修复共享 `var` 投影后复跑本最小文件和完整包门禁。

同日 `netrc` 的当前 Windows 发布版复现字典侧：
`./release/xl.exe run dev/extlib_blockers/netrc_constructor_literal.xl`
在合法的 `netrc.Netrc({"hosts": {}, "macros": {}}).stringify()` 报
`ConfigError: netrc document must be a dict`；独立 EXE 也以相同诊断
退出 1。对照组 `netrc.parse("").stringify()` 正常，语言层构造及
`hostNames()` 亦正常。默认包门禁先通过严格 C、原生边界、C/XLang
双语和 UTF-8 文件范例、Python `.netrc` 互通及 XLang 主用例，
在具名类型用例的此写出路径失败。`xnetrc_stringify_text(xvalue*)`
最终要求 XRT 字典 tag；不要改写 Netrc 的合法构造语义来隐藏
缺失的 foreign `var` 投影。主线修复后复跑最小 JIT/EXE 和完整包门禁。

同日 `tar` 的当前 Windows 发布版复现数组侧：
`./release/xl.exe run dev/extlib_blockers/tar_selected_names_projection.xl`
在 `TarReader.extractSelected(output, ["one.txt"])` 报
`TypeError: TAR extraction names must be an array<string>`；独立 EXE
同样失败。最小源只创建一个普通条目并选择提取它，失败出口清理
独占临时目录。原生 `xtar_reader_extract_selected` 检查
`xrtValueType(pNames) == XVALUE_ARRAY`，语言声明却是
`array<string> names`。包级严格 C/TAR 主用例与多份 C/XLang
范例先通过，中央选择性提取范例首次走此路径时失败；完整 AOT
未通过包门禁。此问题与 TAR 覆盖旧文件的非原子风险分别处理，
主线投影修复后复跑最小 JIT/EXE 与完整包门禁。

同日 `graph` 的当前 Windows 发布版复现数组侧：
`./release/xl.exe run dev/extlib_blockers/graph_var_array_projection.xl`
用一条合法有向边调用 `graph.isDag(var edges)` 即报
`ValueError: graph edges must be an array`；独立 EXE 也失败。
`graph_native.c` 在解析任何行或执行算法前先要求
`xrtValueType(input) == XVALUE_ARRAY`。默认专项门禁的严格 C、
分配故障及线程测试均通过，首个 XLang 主用例即停止，后续
语言、DLL 宿主复用、生成原型和 AOT 路径本轮均未验收。
修复共享容器投影后须复跑本最小 JIT/EXE 及完整 `graph` 门禁。

同日 `vcard` 的当前 Windows 发布版复现对象侧：
`./release/xl.exe run dev/extlib_blockers/vcard_var_object_projection.xl`
先输出 `parsed object: PASS`，随后在语言字典 `{"fn": "Ada"}` 调用
`vcard.build(created)` 时抛出 `ValueError: invalid vCard field or output exceeds 4 MiB`；
独立 EXE 也在同一位置失败。作为正对照，`vcard.parse(...)` 返回的
XRT 对象可成功 `build()`。包级严格 C 测试通过，默认门禁首个中央
XLang 用例在构造字典写出时终止；完整语言和 AOT 门禁未签收。
原生 `xv_build(xvalue*)` 在写出字段前要求 `XVALUE_OBJECT`，
不能通过在 vCard 包内识别编译器私有 handle 来回避共享 ABI 问题。
主线修复后应重跑此 JIT/EXE 最小复现与完整包级门禁。

2026-10-24 补充 `rss`：`./release/xl.exe run
release/extlibs/rss/test/aot_core.xl` 在合法字典与空数组调用
`rss.buildRSS` 时退出 1，报告
`ValueError: buildRSS expects a channel object and items array`。
默认 `release/extlibs/rss/test/check.ps1` 的严格 C、C 边界及两组
C 范例先通过，首个 XLang 边界同样失败。原生 `xrss_build_rss`
入口要求 `XVALUE_OBJECT` 和 `XVALUE_ARRAY`，但语言声明为两个
`var`；这是同一 foreign 容器合同的可观察失败，尚未单独确认
内部物理根因。修复共享边界后需重跑 RSS 的 JIT、AOT 和双语门禁。

同日 `ical` 复现对象侧：`./release/xl.exe run
dev/extlib_blockers/ical_build_var_object_projection.xl` 先打印
`native roundtrip: PASS`，证明 `ical.parse` 返回的 XRT 对象可被
`ical.build` 正常处理；随后将只有合法 `prodid` 的语言字典交给
`build`，报 `ValueError: invalid calendar fields or output exceeds 8 MiB`。
`ical_build.c::xi_build` 的第一项检查要求 `XVALUE_OBJECT`，而
`src/ical.xl` 的公开参数是 `var`。这与已登记的 foreign 对象投影
故障同形；尚未证明内部根因完全相同。普通包任务不加入私有 handle
识别或手工重建 XRT 对象；主线修复后复跑最小源、公开日历范例和
完整包级 JIT/AOT 门禁。

同日 `sitemap` 复现数组侧：`./release/xl.exe run
dev/extlib_blockers/sitemap_build_var_array_projection.xl` 先打印
`native roundtrip: PASS`，证明 `sitemap.parse` 返回的 XRT 数组可
由 `build` 重新生成 XML；相同条目的语言数组传入 `build` 却报
`ValueError: sitemap entries must be an array`。`xl check` 接受该源，
当前发布版的独立 C 边界测试通过。`sitemap_xml.c::xsm_build_xml`
要求 `XVALUE_ARRAY`，公开语言签名则为 `var`。仍需主线统一 foreign
数组投影，不能在本包识别编译器内部 handle；修复后复跑此最小源、
公开双语范例与默认包级 JIT/AOT 门禁。

同日 `terraform_state` 复现对象侧：从 `demo6/release` 运行
`./xl.exe run extlibs/terraform_state/test/literal_state.xl`，先确认
同一 JSON 经 `parse()` 后可被 `version(var)` 查询，再把等价的
XLang 字典传入该函数，报 `TypeError: Terraform state must be a
JSON object`。原生 `xtf_state` 要求 XRT object；公开签名只声明
`var`。`xl check` 通过，JIT 与独立 EXE 均在运行时失败。旧包级门禁
的 C、JIT、AOT 对解析产物均通过，但未覆盖直接构造。新用例已登记
包清单与默认门禁，使整包保持红色直至主线统一对象投影。修复后
复跑最小用例及完整 `release/extlibs/terraform_state/test/check.ps1`；
不要在包侧识别编译器私有句柄。

同日 `epub` 复现对象和数组输入边界：从 `demo6` 运行
`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/epub/test/check.ps1`，
严格 C 测试通过，随后 `test/cases/extlib_epub.xl` 的合法
`epub.build(path, meta, chapters)` 报
`ValueError: invalid EPUB metadata or chapter`。`meta` 为语言字典，
`chapters` 为语言数组；原生 `xep_build` 在解析字段前要求
`XVALUE_OBJECT` 和 `XVALUE_ARRAY`。这是与 foreign 容器投影同形的
可观察失败，尚未逐个证明两个参数的内部表示。当前发布版默认门禁
尚未进入完整范例和 AOT；共享边界修复后复跑该门禁。

## 2026-10-30 CLI 被 os.args 掩盖的独立投影

CLI 是 M1 第21实测包，同时仍有 M7。默认导入会先在编译/链接时缺
`xl6_xrt_os_args`，此前并没有运行到原生解析器，不能据此证明parse正常。
新增[最小源](cli_args_projection.xl)及同名C伴随源，只引入真实的CLI
解析器，不导入含parseOs的语言模块：

```powershell
./release/xl.exe check dev/extlib_blockers/cli_args_projection.xl
./release/xl.exe run dev/extlib_blockers/cli_args_projection.xl
./release/xl.exe build dev/extlib_blockers/cli_args_projection.xl -o build/test/cli_args_projection.exe
./build/test/cli_args_projection.exe
```

最小源先在同一执行环境通过C构造完全相同的原生数组/对象，打印
`cli native control: PASS`，随后才调用语言字面量路径。Windows/Linux均
check与build成功，JIT/EXE均返回1并报
`ValueError: parse requires an args array and a spec object`。xcli_parse第一项
要求XVALUE_ARRAY/XVALUE_OBJECT，实际生成声明为
`extern xvalue* xcli_parse(xvalue*, xvalue*)`，与正式C头相符；独立C相同
声明/参数成功。证明当前合法语言字面量尚未满足此foreign入口的物理要求，
不据错误文本断言两个参数内部所有表示或全部包的根因已经逐项证明。

默认CLI门禁保留该正例并要求0，不能用识别私有handle、将输入改成json.parse、
删除parseOs或C工厂当作本包修复。主线统一数组/字典的foreign var投影、
引用与OOM回滚，连同M7后再复跑中央用例、两份双语范例和默认JIT/AOT。
新包优化仅将已验证为字符串的默认值从图复制改为标量复制并移除不再需要的
VALUE_GRAPH依赖，没有改物理ABI、接受错误容器或更改声明范围。

Windows报告 `build/test/cli_guard/run_46637af6c9454ef99d872bd788fe54f9/results.json`；
Linux原生快照 `/var/tmp/xlang-cli-final-src-UjUpgL`，现场
`tmp/xlang-cli-yI8zVT`。两端39阶段29符合要求，C/882故障/真实原型先通过，
四份含os.args源的八条路径与本最小源两条路径分别失败，整体仍返回1。
