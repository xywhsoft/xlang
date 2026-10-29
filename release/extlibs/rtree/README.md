# rtree

二维矩形空间索引，使用每节点最多 8 项的 R-tree、二次分裂和包围盒剪枝。坐标必须是有限数值且满足 `minx <= maxx`、`miny <= maxy`；相交判断采用闭区间，所以接触边界也算命中。点和零宽矩形有效。

```xlang
import rtree
rtree.RTree tree = rtree.create()
tree.insert(10, 0.0, 0.0, 2.0, 2.0)
tree.insert(20, 5.0, 5.0, 8.0, 8.0)
var ids = tree.search(1.0, 1.0, 6.0, 6.0)  // [10, 20]，顺序未定义
println(ids.length)
```

`create()` 在原生句柄分配失败时抛 `MemoryError`，不返回不可用的空句柄对象。`insert(id, minx, miny, maxx, maxy)` 和 `search(...)` 的分配失败也抛 `MemoryError`，非法矩形则抛 `ValueError`；分裂所需节点在改变树前分配，分配失败不会留下半更新状态。`search(...)` 返回完整整数数组，不设 256 条上限；`searchCount(...)` 无须创建结果数组。`remove(id)` 删除一个匹配项并返回布尔值，允许重复 ID；`clear()` 保留树对象供再次插入。搜索结果顺序未定义。

查询通过包围盒剪枝，插入采用最小面积增量和二次分裂。由于公开删除接口只提供 ID 而不提供矩形，`remove(id)` 需要扫描树，复杂度为 O(n)；大量频繁删除的场景应先评估此成本。删除时收缩空节点和单子节点根，但不重排非空低占用节点。不同树没有共享可变状态；同一树不支持无锁并发写入。

C API 见 `include/rtree_native.h`。句柄由 `xrtree_destroy` 释放，`xrtree_search` 返回的 `xvalue` 数组由调用方用 `xrtValueRelease` 释放。空命中返回空数组，非法查询返回 XRT null 值并设置错误。公开操作在调用开始时清除旧的 XRT 错误；`xrtree_destroy` 不清错，以免清理资源时丢失失败诊断。XLang 原生调用桥接会把 XRT 内存错误转换为 `MemoryError`，无需额外错误分类入口。
整数 ID 与计数结果在公开 C ABI 上均为 `int64_t`，与 XLang `int` 在 Linux LP64 上保持一致。

## 范例与验证

### 2026-10-30 多层分裂与失败现场

本轮没有确认新的索引算法错误，也未修改 `rtree_native.c`、语言绑定或
公开头。补强的是原测试仅覆盖首次根分裂、门禁失败后删除现场的缺口。

- `test/cascade_rollback.c` 在完全重叠、按坐标递增两种布局下，分别捕获
  需要 2/3/4 个预分配节点的分裂各两次，共 12 次事件、36 个失败点。
  每次失败检查原树逐字节未变、全部 ID 完整、存活分配/字节及非法释放
  计数不变；同一插入随后用恰好足够的预算立即恢复。另独立重算父包围盒、
  叶层深度与叶条目数；测试在 853/1719 项时完成，不是只校验根计数。
- 补 `INT64_MIN/MAX`、超过 32 位的 ID、重复 ID 的完整查询/逐项删除；
  持有的结果在清空/销毁树后仍有效，删除/计数/清空/销毁在零分配预算
  下完成。整份夹具每次执行 2,488,071 条断言，主要是重复结构巡检，
  **不把断言数称为同等数量的独立输入**。三个隔离破坏副本（改总数、
  改叶 ID、遗漏预分配回收）均被对应检查拒绝；正式实现没有测试钩子。
- C 独立夹具只启用 Math/Value Container/Memory Debug，避免原 ALL
  引入与本包无关的网络链接依赖。正式包的模块声明与 ABI 未变。八个
  真实 cgen 原型均用正式 C 头检查，故意错配返回类型的副本确实编译失败。
  初始 ABI 夹具漏启用 Value Container、初始黄金文本比较未统一 CRLF，
  都是测试建设问题，已纠正，不计作产品 BUG。
- 新的 `examples/scene.xl` 与 `scene_c.c` 覆盖视野查询、边缘相交、点、
  大 ID、移动、缺失删除、结果快照、重复 ID 与复用，六行固定输出一致。
  `remove` 后 `insert` 是两次独立修改，不承诺失败原子的移动事务；
  `search` 顺序未定义，例子不依赖树的遍历次序。两份例子都只建立局部
  场景对象，原有顶层故障测试没有移入函数。
- Windows 默认 42 阶段前 41 符合预期，Linux 默认 48 阶段前 47 符合
  预期，均含一项真实私有访问拒绝；八原型、旧查询 oracle/分裂回归、
  新旧双语固定输出、七条普通独立程序均通过。Linux 同时覆盖 native/
  narrow long double 的 ASan/UBSan/泄漏检查。最后未改的
  `test/allocations.xl` 独立程序仍报运行时状态错误并退出 1，**M2 未关闭，
  不是整包门禁通过**。42/40 项门禁输入前后未变、编译警告均为零。

Windows 每个子进程限时 120 秒，报告保存在 `build/test/rtree_guard/run_*.json`；
失败保留该目录内生成物、原始状态与输出，成功才删除临时目录。Linux 同样
逐阶段保存命令、stdout/stderr、原始退出码、前后指纹及 `*.report.txt`。
真实失效探针验证 Windows 静默编译器内部 37/脚本外层 1、Linux 缺工具
原始 127 都保留现场，不能因为没有输出或找不到工具而被算成通过。
`-SkipAot` 仅用于开发快测，不用于发布签收。

在仓库根目录运行新范例：

```powershell
release/xl.exe run release/extlibs/rtree/examples/scene.xl
release/xl.exe run release/extlibs/rtree/examples/scene_c.c
```

Linux 将 `release/xl.exe` 换成 `release/xl`，从相同根目录运行。预期输出见
`examples/scene.expected.txt`，无网络、模型或外部服务前提；索引和结果均在
作用域结束时回收，C 例子显式释放结果和句柄。完整门禁入口仍为
`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/rtree/test/check.ps1`
和 `bash release/extlibs/rtree/test/check.sh`。本轮完整 Linux 复验在原生
`/var/tmp/xlang-rtree-final-src-dkHOgx` 快照执行，76 个复制源与原件前后一致，
失败现场为 `tmp/xlang-rtree-vhJCWB`；Windows 对应报告为
`build/test/rtree_guard/run_d897ee9e86ef45b9bc09554464b56c20.json`。
全部证据索引在 `build/test/rtree_audit_676f28c4c6a34d36b8b0c4e9fb0f5f29`。

2026-10-26：当前发布版已越过旧的 `xl6ValueDataSnapshotV1` 缺符号点。
Windows 默认门禁的严格 C、查询 oracle、语言边界 AOT 通过，
但 `allocations.xl` 独立 EXE 在退出清理阶段抛运行时状态错误；
Linux native/narrow 两档 C oracle 与分裂回归通过，默认门禁
也在新增语言 AOT 阶段失败。最小 `rtree_global_native_aot_fini.xl`
证实顶层原生对象在双平台 `xl run` 正常、独立 EXE 打印后失败，
详见 `dev/extlib_blockers/global_native_class_aot_fini.md`。
本包仍未达到发布签收条件；保留顶层对象用例，等待主线修复。

2026-10-25：同一新发布版从 `release` 工作目录运行中央 XLang
用例通过，但从仓库根目录执行默认 Windows 包门禁时，C 查询
oracle 与分裂回归通过，中央 XLang 用例先报共享 VFS 头文件
`include/xruntime_decl.h` 不存在。因此本轮未进入 AOT，也不覆盖
2026-10-24 的历史通过结果；见
`dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C/ABI、2048 项暴力查询参照、极端矩形分裂选种、XLang 边界
和故障注入、私有原生入口拒绝、三条范例路径固定输出，以及核心/
公开范例 AOT 全部通过。旧的带输出 AOT 共享缺符号属于先前
发布物；本次没有重跑 Linux native/narrow `long double` sanitizer。

- `release/examples/extlibs/rtree.xl` 与 `rtree_c.c` 可直接 `xl run`，输出一致。
- `release/extlibs/rtree/test/test_rtree.c` 使用暴力矩形扫描校验 2048 项、1000 次随机查询、删除/重插和故障注入；另在固定种子的 600 步交错修改中，反复删除和重新插入同一批 ID，每步检查局部查询，定期检查全量查询。2048 项完整查询另扫描前 64 个分配故障点，确认失败诊断和树状态不变。`test/boundaries.xl` 覆盖 600 项完整查询、公开类型和非法输入；`test/allocations.xl` 覆盖创建、分裂插入及 64 项查询前 64 点的明确 `MemoryError` 类型与恢复。两份 XLang 测试已登记到包清单的 `coverage.tests`。

在仓库根目录可运行 `tools/smoke_extlib_catalog.ps1 -Package rtree`；Linux C/XL sanitizer 门禁为 `bash release/extlibs/rtree/test/check.sh`。它同时编译运行公开 C 范例、运行中央与包内 XLang 用例，并逐字节比较两种语言的范例输出；`xl run release/examples/extlibs/rtree_c.c` 也可直接运行。

Windows 聚焦快测为 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/rtree/test/check.ps1 -SkipAot`，同次验证 C 查询参照、包外原生方法访问拒绝、XLang 边界及双语范例固定输出。不带 `-SkipAot` 时，默认还构建运行核心、边界、分配故障、公开 XLang/C 范例的独立 EXE，并核对范例固定输出。构建使用单次独占目录，失败后保留报告和现场。当前默认门禁在分配故障 AOT 的顶层对象退出清理处失败，不可据快测签收。

2026-10-12：Windows 严格 GCC、Linux ASan/UBSan、C/XLang 边界、双语同输出及无输出边界 AOT 通过。2026-10-13 补原生创建的首、次分配故障与失败后恢复测试，复跑 C 查询 oracle、XLang 边界和双语范例。2026-10-14 修复合法极值坐标在窄 `long double` 目标上使面积溢出、分裂复制错误 ID 的问题；面积启发式限幅到有限 `DBL_MAX`，不限制可插入坐标。C 回归逐一核对全部 ID，`test/check.sh` 在支持时额外用 `-mlong-double-64` 运行严格 GCC/ASan/UBSan。带输出范例 AOT 仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断；发布验收未完成，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。

2026-10-15：包清单声明原生代码直接使用的 Math、Value Container 模块，源码宏改为条件定义。中央无输出 AOT 从 3,666,432 降至 716,288 字节，构建及运行通过；XLang 边界、C/XLang 范例逐行同输出和 Linux native/narrow ASan/UBSan 复跑通过。完整带输出 AOT 仍缺 `xl6ValueDataSnapshotV1`，不计作发布完成。

2026-10-16：修复 C 公开操作成功后遗留旧 XRT 错误；新增创建、插入和查询的 XLang 分配故障用例，确认现有原生调用桥接直接抛 `MemoryError`，无须包内重复映射。C 红绿回归覆盖错误槽和查询/节点分配故障；Windows C 可执行测试、统一目录 smoke、无输出故障注入 AOT 与 Linux native/narrow ASan/UBSan 通过。完整带输出 AOT 的共享阻断不变。

同日追加 LP64 ABI 修复：Linux XLang 原在 `insert` 的 `long long` ID 与生成的 `int64_t` 原型处停止；四个整数边界入口统一为 `int64_t`，补头文件契约门禁。完整 AOT 阻断不变。

2026-10-16：公开 C 范例原用 `%lld` 打印 `int64_t` 计数，在 Linux LP64 的严格 GCC 下失败；改用 `PRId64`，并将该范例和 C/XLang 同输出纳入 Linux 包级门禁。native/narrow 两档 ASan/UBSan、XL 用例与直接 `xl run` 均通过；完整 AOT 共享阻断不变。

2026-10-18：导入方原可直接调用 `searchNative` 等原生成员，绕过
XLang 对对象状态、结果与异常口径的检查；C 层矩形和句柄校验并未
在本次改动。编译负例先红后绿，六个 `*Native` 方法现只供类内部使用。
`xl check extlibs/rtree/test/private_native_negative.xl` 应报告访问错误。
本轮 XLang 边界、分配故障和 C/XLang 范例同输出通过；C 大规模
oracle/sanitizer 未因纯可见性修改重复执行，完整 AOT 共享阻断未变。

2026-10-19：新增 Windows 单入口 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/rtree/test/check.ps1`，合并严格 GCC/ABI、2048 项 C 暴力查询参照、中央与包内 XLang 边界/分配故障，并将包运行器 C、直接 `xl run` C、XLang 范例各自对照固定输出。Linux `test/check.sh` 补跑此前遗漏的 `allocations.xl`、C 直跑和固定输出；Windows 与 Linux native/narrow 两档 ASan/UBSan 门禁均通过。索引算法未改，完整带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。

2026-10-22：二次分裂选种的固定 `-1e300` 初值在合法极端矩形下高于所有面积浪费值，原实现错误地回退到第 0、1 项。包内 C 用例用九个大矩形先复现，再验证应选第 2、3 项；初值改为平台 `long double` 下界。Windows 严格 GCC/C/XLang/双语门禁及 Linux native/narrow 两档 ASan/UBSan、XLang 门禁通过。公开 API 与查询语义不变；完整带输出 AOT 的共享缺符号阻断未解除，不签收整包发布级。
