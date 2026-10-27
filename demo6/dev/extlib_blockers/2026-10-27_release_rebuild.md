# 2026-10-27 发布版重建后的扩展库共享阻断

从当前工作树在 `demo6` 目录执行 `cmd /c build.bat` 成功，生成
`xl.exe`、`xl.dll` 与 `xlc.dll`。`release/xl.exe` 的 SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
构建没有修改跟踪中的生成源码；以下均在这个新发布物上复现。

| 共享边界 | 最小命令（在 `demo6` 目录） | 当前结果 | 主线修复方向 |
| --- | --- | --- | --- |
| 语言 `var` 字典传给原生 XRT 对象参数 | `release/xl.exe run dev/extlib_blockers/ical_build_var_object_projection.xl` | 原生往返通过；语言字典调用 `ical.build` 抛 `ValueError`，退出 1 | 外部调用 ABI 应按 XRT 对象/数组合同投影类型、元素和所有权；见 [容器投影记录](foreign_var_container_projection.md) |
| `cli.parseOs` 的运行时符号 | `release/xl.exe run test/cases/extlib_cli.xl` | TCC 报 `unresolved reference to 'xl6_xrt_os_args'`，退出 1 | 补齐主线运行时导出、注册和 AOT 链接，再跑 CLI 全门禁 |
| 导出类的私有原生构造器 | `release/xl.exe check release/extlibs/zip/test/private_memory_constructor_negative.xl` | 错误输出 `ok`，退出 0；**只能编译检查，不能运行负例** | 语义/重载解析必须拒绝包外调用 `private extern create`，再跑 ZIP 和同类负例 |
| 顶层 native-backed class 的 AOT 退出清理 | `release/xl.exe build dev/extlib_blockers/global_native_class_aot_fini.xl -o build/test/native_fini_probe_20261027.exe`，再运行该 EXE | 构建成功，先输出 `ready`，随后抛 `RuntimeError: operation is not valid in the current state`，退出 1 | 修复生成代码与 XRT 对象终结状态的清理约定；见 [顶层对象记录](global_native_class_aot_fini.md)，复验受影响包的顶层对象 AOT |

这些问题不在扩展库内添加私有句柄探测、重复包装或兼容回退。
修复共享边界后，先运行对应最小复现，再运行受影响包的 Windows/Linux
C、XLang JIT/AOT、双语范例与分配故障门禁；不能把某一条复现转绿
外推为整组包已发布签收。
