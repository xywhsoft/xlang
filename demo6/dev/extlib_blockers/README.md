# 普通扩展库的共享阻断索引

本页只汇总 2026-10-27 重建发布版后仍可复现、会阻断普通扩展库发布验收的边界。完整症状、最小源和修复建议以链接的单项记录为准。包内正例通过不等于对应共享边界已经修复；本任务不修改 XLang 主线，也不为这些问题增加包内兼容回退。

| 边界 | 当前最小证据 | 受影响包及解除条件 |
| --- | --- | --- |
| [外部 `var` 容器投影](foreign_var_container_projection.md) | `release/xl.exe run dev/extlib_blockers/foreign_var_array_container.xl` 报 `input is not an array`；语言数组抵达 C 时不是预期的 XRT 数组。 | 至少阻断 `statistics`，另有 BSON、CBOR、TOML 等同形报告。主线须明确外部调用的容器类型、嵌套元素和所有权投影，再逐包复验。 |
| [类型化嵌套数组写入](nested_array_append_runtime.md) | `release/xl.exe run dev/extlib_blockers/csv_nested_array_value.xl` 报 `value has an incompatible type`，复现源不导入 CSV。 | `csv` 的语言门禁。先修复并复验纯语言最小源的 JIT/AOT，再跑 CSV 全包门禁。 |
| [私有原生句柄构造器](blake2_private_adopt_constructor.md) | `release/xl.exe check release/extlibs/blake2/test/private_adopt_constructor_negative.xl` 仍输出 `ok`。只编译负例，**绝不运行伪造句柄**。 | `blake2`、`blake3` 等导出 native-backed class。包外必须被访问控制拒绝，然后复验各包安全负例和完整门禁。 |
| [顶层原生对象 AOT 退役](global_native_class_aot_fini.md) | `blake3/test/aot_global_hasher_repro.xl` 的 JIT 输出摘要并退出 0；独立 EXE 输出同一摘要后报运行时状态错误并退出 1。 | `blake3` 等顶层持有 native-backed class 的包。主线修复顶层析构和错误状态顺序后，先复验 JIT/AOT 一致退出，再跑各包门禁。 |
| [普通 class 析构字段写入](class_destroy_field_write_state.md) | `release/xl.exe run release/extlibs/serial/test/teardown_repro.xl` 报运行时状态错误；无扩展库的 class 对照见单项记录。 | `serial`。对象在析构体执行完前须能访问自身字段；修复后跑纯语言对照及串口包门禁。此问题不能直接等同于上面的顶层 AOT 故障。 |
| [CLI `os.args` 缺符号](cli_os_args_symbol.md) | `release/xl.exe run test/cases/extlib_cli.xl` 报未解析的 `xl6_xrt_os_args`。 | `cli`。主线统一提供 JIT/AOT 实现与注册，再验直接 `os.args()`、仅导入和 `parseOs()`。 |
| [Linux AOT 普通输出缺图提供者](linux_aot_print_graph_provider.md) | 无包 `println("ok")` 的 Linux JIT 通过，独立 EXE 构建报 `xl6DictGraphProviderV1` 未解析；Windows AOT 通过。 | 至少阻断 `dotenv` 的 Linux 完整门禁；先在新构建的 Linux 发布物复验最小源，再查打印运行库的传递依赖。 |
| [Volc 的 HTTP 导入链](volc_phone_import_chain.md) | `release/xl.exe check release/extlibs/volc.phone/src/phone.xl` 停在独立维护的 `xhttp.client`：构造重载、`NetEngine.nativeHandle()` 与 `TlsConfig` 无法解析。 | `volc.core`、`volc.sts`、`volc.phone`。HTTP 维护上下文按当前网络 API 修复并回归导入链；Volc 包不添加旧接口回退。 |

另有 [双 `XL_Context` 并行执行](singleinstance_context_execution.md)、[XML 导出类型可见性](xml_public_class_visibility.md)等单项记录；本次未重跑其完整门禁，不在此声称它们已解除或完成。待共享修复合入后，先跑对应最小复现，再跑受影响包的 C、XLang JIT/AOT 与双语范例，不以单个正例替代发布验收。
