# 顶层 native-backed class 在独立 EXE 退出时失败

2026-10-24 使用当前源码构建的 `public_source.exe` 验证，不使用旧发布版。

从 `demo6` 目录设置 `$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path`，
分别对以下三个文件执行 `run` 和 `exe`，然后运行生成的 EXE：

| 文件 | 内存运行 | 独立 EXE |
| --- | --- | --- |
| `global_native_class_aot_fini.xl`：顶层保存 `blake3.hasher()` | 打印 `ready`，退出 0 | 打印 `ready`，随后抛 `RuntimeError: operation is not valid in the current state`，退出 1 |
| `native_class_import_aot_fini_control.xl`：仅导入 | 退出 0 | 退出 0 |
| `native_class_local_aot_fini_control.xl`：函数局部 hasher | 退出 0 | 退出 0 |

公开 `blake3_basic.xl` 与 `blake3.xl` 同样在独立 EXE 打印完正确结果后失败，
而源码驱动的内存运行通过。BLAKE2 公开范例也出现相同的退出错误，
但尚未证明其最小根因相同。

生成的 `module.c` 在 `xlir_module_fini_body` 中释放顶层的 `xvalue*`
对象槽；报错发生在用户代码执行完毕后的模块退役阶段。需要主线检查
顶层 native-backed class 的所有权、析构调用与模块退役顺序，特别是
清理时是否仍允许调用所属代码及原生析构。修复后应分别复验三份最小
文件，以及 BLAKE2/BLAKE3 公开范例的 JIT、独立 EXE；不能仅把范例
移进 `main` 来掩盖全局对象缺陷。本普通扩展包任务不修改主线。

2026-10-24 当前发布版复验：`release/xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
`xl run dev/extlib_blockers/global_native_class_aot_fini.xl` 打印 `ready`
并退出 0；同文件 `xl build` 的独立 EXE 打印 `ready` 后仍抛上述状态
错误并退出 1。SQLite 包本轮修正了普通 class 的析构期字段写入，
但 Blake3 使用原生 `extern destroy`，不能把两者视为已证实的同一根因。
应在主线排查模块退役时的 native-backed class 析构及错误作用域。

同日 `consistent_hash` 的包级默认 Windows 门禁提供第二个受影响路径：
`release/extlibs/consistent_hash/test/boundaries.xl` 在顶层创建两个
`ConsistentHash` 对象。JIT 检查通过并退出 0，独立 EXE 也先打印
`consistent_hash boundaries: PASS`，随后模块退役时报相同的运行时状态
错误并退出 1；而对象放在函数局部的公开范例 EXE 正常退出 0。
核心/公开范例 AOT 均已越过旧的 `xl6ValueDataSnapshotV1` 缺符号点。
这强化了顶层对象退役边界的风险，但尚不能仅凭相同症状断定与 BLAKE3
完全同根。修复后保留此顶层用例，重跑完整包门禁，不把测试移到局部。

同日 `lru` 默认 `test/check.ps1` 的 C、JIT、双语范例与边界 AOT 构建
通过。`release/extlibs/lru/test/boundaries.xl` 在顶层持有多个原生句柄；
独立 EXE 打印 `lru boundaries: PASS` 后退出阶段仍报
`RuntimeError: operation is not valid in the current state`，退出码 1。
这又增加一个同症状路径，尚未通过底层跟踪证明与 BLAKE3 同根；
主线修复后应复跑 LRU 默认包门禁，并保留顶层对象测试。
