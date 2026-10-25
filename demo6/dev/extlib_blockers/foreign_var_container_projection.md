# 当前源码的 var 容器传入原生扩展失败

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
