# 当前源码的 var 容器传入原生扩展失败

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
