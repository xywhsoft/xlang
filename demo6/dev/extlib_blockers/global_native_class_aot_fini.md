# 顶层 native-backed class 在独立 EXE 退出时失败

## 2026-10-29 Watch 第22个已实测涉及包

`release/extlibs/watch/test/top_level.xl` 在顶层创建真实Watcher，main内
检查valid/count/空poll并打印PASS，随后自然退出；没有监听文件，不
涉及事件修改或资源等待。固定Windows/Linux发布物均JIT退出0、构建
退出0，独立程序打印PASS后报同一状态RuntimeError并退出1。

另外的最小全局控制只保留valid查询，同样失败；相同操作置于main的
局部控制退出0。两个平台均验证，不因其他native类相似而直接归类。
这些只证明相同可观察退役边界，不证明22包内部全部同根。

从demo6可用普通命令复验，不需要更换运行库：

```text
release/xl run release/extlibs/watch/test/top_level.xl
release/xl build release/extlibs/watch/test/top_level.xl -o build/test/watch-global.exe
build/test/watch-global.exe
```

Windows完整默认门禁49阶段，47正向成功、一项预期私有方法访问拒绝
符合要求，只有最后全局AOT退役失败；十条普通独立程序均过。报告和
全部现场保留在 `build/test/watch_guard/run_df5079c2a2a34c098d9cb6053e67a7ab`
及其旁边的JSON，44项实际依赖指纹变动0。

Linux原生快照 `/var/tmp/xlang-watch-digest-src-EXpcL7` 同样先过C
ASan/UBSan、目录竞态、15个元数据夹具、八个实际生成原型、旧新
JIT/十条普通独立程序与双语黄金输出，最后全局AOT退出1。父子现场
`tmp/xlang-watch-KOsmv2` 与源件41份前后核对均保留；缺编译器还按
真实127退出并保留诊断。最小控制在该快照controls/，Win控制和初始
现场在 `build/test/watch_digest_before_b4a0b973b594441482b7cc74c1e937ba`。

包内目录摘要抵消和关闭覆盖首因已独立修复、逐项验证，不是本问题的
修复。主线应核对全局对象释放、类型/模块/代码和错误上下文退役顺序，
确保最后一个全局引用释放前析构仍可调用，并保持析构仅一次及异常时
后续清理；不要全局放宽FINALIZING或把对象挪入main/提前销毁。
修复后先重跑这个最小正例和局部对照，再跑默认门禁；也要补跨模块
全局、工厂返回值、容器持有对象和初始化失败的退役顺序覆盖。

Watch从G转M，M2已实测涉及21增至22包；主线/XRT/冻结发布物未改。

## 2026-10-29 Dotenv 第21个已实测涉及包

`release/extlibs/dotenv/test/top_level.xl` 在全局创建真实Config，main
内检查valid及空配置计数，然后自然退出。固定发布版Windows/Linux
均为JIT退出0、构建退出0，独立EXE打印 `dotenv top-level: PASS`
后报同一RuntimeError状态错误、退出1；未主动销毁或移动顶层对象。

从demo6运行共享专项即可独立复验，无需重复整包C与算法测试：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_native_globals.ps1 -Package dotenv
```

```sh
bash tools/check_extlib_native_globals.sh dotenv
```

Windows最终完整门禁42项，40个正向阶段和一项预期私有访问拒绝符合
预期，仅最后生命周期子门禁失败；包括新增同对象借用输入/自别名
OOM、八组存活分配和非法释放/边界指标、13个真实生成原型，以及
旧配置/环境/故障边界、新旧C/XLang双语和九条独立AOT。
输入变动0，正式结果及产物为
`build/test/dotenv_guard_97a7c3343c524705951cd7678c14dcfd.json` 及同名
目录，子报告 `build/test/native_globals_302038e3361842eb89de2bfd94a6a90f`。

Linux完整门禁在原生快照
`/var/tmp/xlang-dotenv-ownership-src-7mIqXx` 先过C ASan/UBSan、同样
所有权检查、13原型、新旧JIT、九条独立AOT和黄金对照，最后顶层
AOT失败。父现场 `tmp/xlang-dotenv-3aFwaq`、子现场
`tmp/xlang-native-globals-QqpZ1y` 保留；内外输入前后复核一致。

这次没有确认需要改动的Dotenv本体BUG，只补测试、双语安全重载
范例及带超时/指纹/失败保留的门禁；没有修改主线或冻结XRT。
共享专项现可默认检查九包，当前只新增实测Dotenv。M2涉及20增至
21包、Dotenv由G转M，不从同症状推断21包内部已全部定位为同根。
主线仍需核对全局对象、module fini、代码/类型与错误作用域退役次序；
不能把局部对象通过代替此边界，也不能吞退出错误或放宽失活调用。

## 2026-10-29 Gzip 第20个已实测涉及包

补 `release/extlibs/gzip/test/top_level.xl`：全局保存一个真实压缩器，
main内断言有效并打印 `gzip top-level: PASS`，自然退出时析构。
固定发布版的Windows/Linux均为JIT退出0、构建退出0、独立EXE打印
PASS后报同一状态错误并退出1。没有更新主线/XRT或靠提前销毁绕开。
共享专项增加gzip选择及默认覆盖，恢复时从demo6执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_native_globals.ps1 -Package gzip
```

```sh
bash tools/check_extlib_native_globals.sh gzip
```

Windows完整默认门禁54项，51个正向阶段和两项预期访问拒绝符合预期，
只有最后生命周期子门禁未过；四种C头组合、25个生成C原型、139项
zlib互通、13个内存/70个文件分配故障点、旧C/XLang与双语JIT/AOT
均通过，输入变动0。正式报告及产物保留于
`build/test/gzip_header_8baac82b7e504d25aca76f03806074b8`；生命周期
子报告为 `build/test/native_globals_92745b063d7c46df9e0b4abfe53b0395`。

Linux完整门禁使用原生快照 `/var/tmp/xlang-gzip-header-src-PeO005`。
四头组合、C ASan/UBSan、139项互通、13个内存/48个文件故障点、
七份旧AOT及双语输出均先通过，最后在新顶层AOT退出时报错，整体
退出1。逐阶段生命周期现场在 `tmp/xlang-native-globals-53ulQH`，
父门禁与外层源件/快照复核一致；全部失败现场保留。

同轮的Gzip声明头模块污染/包含顺序问题已独立修正并验证先红后绿，
它不是这一退出期问题的修复。M2已实测涉及19增至20包，Gzip由G
转M；仍只证明同一可观察边界，不证明20包内部已全部同根。主线
修复方向仍为全局对象、模块、代码/类型及错误作用域的有序退役。
文件双故障没有在本轮实测，不从其他压缩包静态相似路径把它计为
Gzip已确认RFS，也不把旧单点OOM通过当成双故障已验收。

## 2026-10-29 七包顶层生命周期专项

继续逐包实测，而不是根据相似析构声明推断：LZ4、BZ2、LZMA、miniz、
Snappy、xxHash、Zstd 各新增 `test/top_level.xl`。六个压缩包在全局
创建一个真实压缩器，xxHash 创建真实 XXH64 哈希器；main 内断言对象
有效并打印本包 PASS，退出时自然退役，不主动销毁、不搬入函数。

| 范围 | JIT | 构建独立 EXE | 独立 EXE 运行 |
| --- | --- | --- | --- |
| Windows 七包 | 全部退出0并打印各自PASS | 全部退出0 | 全部打印PASS后报同一状态错误、退出1 |
| Linux x64 七包 | 全部退出0并打印各自PASS | 全部退出0 | 全部打印PASS后报同一状态错误、退出1 |

原始错误均为
`uncaught xlang error: RuntimeError: operation is not valid in the current state`。
每平台21个实际阶段是7次JIT、7次构建、7次EXE运行；没有把七个退出1
改成“预期成功”。两平台使用本矩阵固定的 d64672903 发布物与 XRT，
未升级主线、运行库或算法，也未为了该补测重跑整组格式/算法认证。

从 demo6 运行完整专项，或指定一个包：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_native_globals.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_native_globals.ps1 -Package lz4
```

```sh
bash tools/check_extlib_native_globals.sh
bash tools/check_extlib_native_globals.sh lz4
```

Windows 正式结果为
`build/test/native_globals_6163e0c3ee4940398f48138d04d0d5cf/results.json`：
14个阶段通过、7个AOT运行失败、输入变动0，全部EXE保留。
Linux 在原生文件系统快照
`/var/tmp/xlang-native-globals-src-kQetJP` 运行，详细逐阶段命令、
stdout/stderr、真实退出码、输入前后指纹保留于
`tmp/xlang-native-globals-xPQdP7`；外层源件/快照复核一致，整体退出1。
随后只更新文档与测试工具的UTF-8控制台输出配置，未改变语言用例、
包实现或发布物。最终工具的Windows单包复验仍为xxHash JIT0/构建0/
AOT1，现场 `build/test/native_globals_21bd7dd0bda8411fb030543dde077a6a`。

为验证默认接线，另外只选代表性包完整执行，不七包重复全量回归：
Windows xxHash 默认门禁53项，50个正向阶段和两项预期访问拒绝符合
预期，25个生成原型、旧JIT/AOT与双语对照均过；只有最后生命周期
子门禁失败。报告
`build/test/xxhash_guard/results_050f6dc1a63346df8272a872053fe2ed.json`
及其同名run目录均保留。Linux Snappy默认完整门禁在原生快照
`/var/tmp/xlang-snappy-lifecycle-src-DQVhNv` 中先过严格C ASan/UBSan、
分配故障、语言JIT、七份旧AOT和双语输出，最后同样在新顶层AOT
失败，整体退出1；内外输入无变动，父/子门禁现场均保留。

各包默认完整门禁已经接入同一专项；Windows快速 `-SkipAot` 只做JIT，
报告明确AOT跳过，不能当作发布验收。LZ4/BZ2仍保留此前更早报红的
独立RFS关闭态回滚用例，不能因为默认门禁先停在RFS而漏记本次M2；
单独跑本专项可以不重跑算法/C/OOM完整门禁地复验该生命周期边界。
失败不删除生命周期EXE/诊断；不把正常析构改成显式提前释放来换绿。

当前M2已实测涉及包数12增至19。五包G转M、LZ4/BZ2由R转M，但
RFS涉及包数仍为4，风险没有因状态优先级变化而消失。只是证明同一
可观察退出边界，不声称19包内部已全部定位为同根。主线建议继续
核对全局对象、module fini、xlUnit、代码/类型描述与错误作用域的
退役顺序；不能无条件允许失活模块调用或直接吞掉清理期错误。

## 2026-10-29 Brotli 扩大已验证覆盖

固定 d64672903 发布版上新增
`release/extlibs/brotli/test/top_level.xl`：顶层保存压缩器和解压器，
main内完成真实往返并校验数据。Windows/Linux JIT均退出0；独立
EXE均打印 `brotli top-level: PASS` 后，在模块退出期报
`RuntimeError: operation is not valid in the current state`，退出1。
更小的 `dev/extlib_blockers/brotli_global_native_aot_fini.xl` 仅在顶层
创建一个压缩器并打印 `brotli global ready`，不更新或完成流；两平台
同样为JIT退出0、独立EXE打印后退出1，排除了必须处理数据才能触发。

Windows默认完整门禁80项中，77正例和两项预期访问拒绝符合预期，
仅最后top-level-aot失败。四种C头组合、26个实际生成C原型、116个
内存故障点、94个文件故障点、旧C/XL双语JIT/AOT及新顶层JIT都过；
输入无变化。完整命令、输出、EXE和报告保留于
`build/test/brotli_header_3dc59ff59b7941c395681901dde995ea`；两份最小
Windows EXE现场为 `build/test/brotli_top_abe9ccb4f434426fb2e515dc4a053e01`。

Linux默认门禁在原生文件系统快照
`/var/tmp/xlang-brotli-header-src-qaTe8g` 同样仅在最后顶层AOT失败，
退出1并保留现场。此前C ASan/UBSan、四种头组合、116个内存故障点、
76个文件故障点、六份旧语言EXE、C文件EXE和双语输出均过；独立
单对象探针亦复现，内外输入指纹复核一致。没有靠挂载盘重试或
提高超时换绿，也没有修改主线、XRT、算法或原生析构函数。

这些结果把已实测的M2涉及包由11增加为12，Brotli从G转M；只是
同一退出边界的可观察证据，不声称12包的内部因果已全部证明。
建议主线先核对生成入口中全局槽释放、module fini、xlUnit和代码/
类型描述退役的次序，以及清理期原生析构的调用/错误作用域权限；
不能吞掉异常、无条件放宽失活模块调用，或把全局对象搬进main。

恢复时保留单对象最小源和真实双对象用例，分别跑JIT与独立EXE，
再跑两平台Brotli默认完整门禁。C声明头的模块污染/包含顺序问题已
独立修复，不能把该包内修复当作退出期主线问题已经解决。

2026-10-26 `consistent_hash` 的 Windows 默认门禁通过严格 C、线程、
语言 JIT、核心及公开 C/XLang 范例 AOT，随后顶层环对象边界 EXE
打印 `consistent_hash boundaries: PASS`，在退出期报
`RuntimeError: operation is not valid in the current state`，退出 1。
Linux 包级 `test/check.sh` 现也构建核心、C/XLang 范例及顶层边界
独立 EXE：C ASan/UBSan、语言 JIT 与前三条 AOT 均通过，最后一条
同样打印 PASS 后报该错误，默认门禁退出 1。对应的 `boundaries.xl`
在两平台 JIT 均退出 0。此症状与其他包同形，但仍须由主线定位真正
根因；本任务不在包侧更改析构顺序或搬移顶层对象。

2026-10-26 `daemon` 追加独立最小路径：包内
`release/extlibs/daemon/test/top_level.xl` 在顶层持有
`daemon.create("top-level")`，Windows/Linux JIT 均打印
`daemon top-level: PASS` 后退出 0；两平台独立 EXE 均打印同一行，
随后报 `RuntimeError: operation is not valid in the current state` 并
退出 1。默认包门禁先通过严格 C、FFI/中央边界及 C/XLang 双语
AOT，再在该退出边界报红。该类只在包内命名，由公开工厂返回；
不能因包外不能命名类型就忽略顶层对象持有。主线应检查模块退役期
原生析构和错误作用域；修复后重跑两平台默认门禁，不移动顶层用例。

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
