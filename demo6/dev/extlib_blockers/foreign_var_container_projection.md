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
