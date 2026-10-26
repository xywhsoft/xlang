# 顶层 native-backed class 在独立 EXE 退出时失败

2026-10-26 `skiplist` 加入同症状回归：Windows 当前发布版
`release/extlibs/skiplist/test/check.ps1` 先通过 C/ABI、JIT、双语输出、
核心及 C/XLang 公开范例 AOT，再在顶层分配故障用例 AOT 退出时报
`RuntimeError: operation is not valid in the current state`，门禁返回 1。
Linux `test/check.sh` 的 C ASan/UBSan、语言 JIT、核心/中央用例及
双语公开范例 AOT 也先通过，随后在同一顶层用例退出时报同错。
`dev/extlib_blockers/skiplist_aot_top_level.xl` 只导入包、在顶层
创建一个 `SkipList` 并打印 `created`；Windows `xl run` 退出 0，
独立 EXE 打印该行后退出 1。该包的 `extern destroy` 与其他受影响
包相似，但仅凭症状不能断言完全同根；主线应核查模块退役顺序、
原生析构和错误作用域，修复后重跑双平台默认门禁。不得把顶层对象
搬入 `main` 以换取绿色结果。

2026-10-26 当前发布物复验 `tabulate`：Windows 默认包门禁的严格 C、
线程、XLang JIT、私有入口负例及无输出核心 AOT 通过，新增的
`release/extlibs/tabulate/test/boundaries.xl` 独立 EXE 在退出阶段报
`RuntimeError: operation is not valid in the current state`，门禁退出 1。
Linux 默认 `test/check.sh` 也在 C ASan/UBSan、线程与 XLang JIT 通过后，
于同一边界 EXE 打印 PASS 后退出 1。最小源
`dev/extlib_blockers/tabulate_aot_top_level.xl` 在 Windows/Linux
均先打印 `created`，JIT 退出 0，独立 EXE 则在模块清理时报同错并退出 1。
同一表对象操作放在 `main` 局部时，Windows 独立 EXE 可正常退出；
症状指向顶层 native-backed class 退役顺序，但未证明与其他包同根。
主线应检查模块退役时原生析构及错误作用域，修复后重跑两平台默认
包门禁；不能把顶层边界用例挪进函数以换取绿色结果。

2026-10-26 `hyperloglog` 新门禁复验：Windows 当前发布版的严格 C、
线程、XLang JIT、私有入口负例、双语黄金输出，以及中央/公开范例
AOT 已通过；新增 `release/extlibs/hyperloglog/test/boundaries.xl` AOT
构建成功，运行打印 `hyperloglog boundaries: PASS`，随后在退出期报
`RuntimeError: operation is not valid in the current state`，退出 1。
从 `demo6` 运行 `release/xl.exe run release/extlibs/hyperloglog/test/boundaries.xl`
正常退出；`release/xl.exe build` 同文件为独立 EXE 后则复现。
Linux 发布版独立构建、运行同一文件也在打印 PASS 后报同错并退出 1；
更新后的默认 `hyperloglog/test/check.sh` 完整门禁亦在此阶段退出 1，
此前的 C ASan/UBSan、线程和 XLang JIT 阶段均通过。
现行 Windows 发布版的本文件最小用例再次验证 JIT 退出 0、独立 EXE
退出 1。症状与已有多包顶层对象案例一致，但未证明原生根因完全相同。
主线修复后须重跑双平台 `hyperloglog/test/check.*` 默认门禁；不可把
顶层对象挪到函数局部以掩盖退出清理边界。本普通扩展包任务不改主线。

2026-10-25 发布版复验：`release/xlc.dll` SHA-256 为
`70A40D58A996B558B91A030799B82EC0C3EB7A88D258717A34FF1A6010B69BBA`。
从 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File
release/extlibs/lru/test/check.ps1`，严格 C、线程、XLang JIT、双语
范例及边界 AOT 构建均通过；边界 EXE 打印
`lru boundaries: PASS` 后在退出阶段抛
`RuntimeError: operation is not valid in the current state`，退出 1。
包级门禁仍未签收，不应通过搬移顶层对象掩盖问题。

2026-10-25 当前 Windows 发布版复验：
`release/xl.exe run dev/extlib_blockers/global_native_class_aot_fini.xl`
打印 `ready` 并退出 0；同一文件以 `release/xl.exe build` 生成独立 EXE，
运行时先打印 `ready`，随后报
`RuntimeError: operation is not valid in the current state`，退出 1。
此阻断尚未解除，不能以包内局部对象范例通过替代顶层对象生命周期验收。

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

同日从当前源码重建 Windows 发布版后，`floatEnv` 默认包门禁的
严格 C、语言 JIT 边界/分配、四项包外访问负例及双语范例通过，
但独立 EXE 运行 `test/cases/stdlib_crt_float_env.xl` 时，顶层
`floatEnv.Snapshot` 的操作完成后于退出期报相同状态错误并退出 1。
同文件 JIT 正常退出。此用例还涵盖跨线程恢复拒绝，不能把快照
移到局部来掩盖退出期故障；修复主线后重跑默认包门禁。此处只确认
可观察症状，尚未证明与哈希器、缓存类的内部根因完全相同。

2026-10-26 `rtree` 最小复现见 `rtree_global_native_aot_fini.xl`：
顶层仅创建一个 `rtree.RTree` 并打印 `rtree ready`。当前 Windows 和
Linux 发布版 `xl run` 均打印后正常退出；同文件 `xl build` 的独立
EXE 均先打印相同内容，再抛
`RuntimeError: operation is not valid in the current state` 并退出 1。
Windows 默认 `rtree/test/check.ps1` 已越过严格 C、查询 oracle、
边界 AOT，在 `allocations.xl` 独立 EXE 的退役阶段失败；Linux 默认
`test/check.sh` 通过 native/narrow 两档 C oracle 与分裂回归后亦
在新增语言 AOT 阶段失败。旧的文本快照缺符号不再是本轮阻断。
这些只是同症状证据，不能在未跟踪析构调用前断言与其他包同根。
主线应检查顶层 native-backed class 对象的所有权、模块退役顺序与
错误作用域，并以这个最小文件和两平台默认包门禁作回归；不要把
测试对象挪入函数局部来掩盖问题。本扩展包任务不修改主线。
