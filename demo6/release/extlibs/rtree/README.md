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

- `release/examples/extlibs/rtree.xl` 与 `rtree_c.c` 可直接 `xl run`，输出一致。
- `release/extlibs/rtree/test/test_rtree.c` 使用暴力矩形扫描校验 2048 项、1000 次随机查询、删除/重插和故障注入；`test/boundaries.xl` 覆盖 600 项完整查询、公开类型和非法输入；`test/allocations.xl` 覆盖创建、分裂插入和查询的 `MemoryError` 与恢复。两份 XLang 测试已登记到包清单的 `coverage.tests`。

在 `demo6` 目录可运行 `tools/smoke_extlib_catalog.ps1 -Package rtree`；Linux C/XL sanitizer 门禁为 `bash release/extlibs/rtree/test/check.sh`。它同时编译运行公开 C 范例、运行中央与包内 XLang 用例，并逐字节比较两种语言的范例输出；`xl run release/examples/extlibs/rtree_c.c` 也可直接运行。

Windows 聚焦快测为 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/rtree/test/check.ps1 -SkipAot`，同次验证 C 查询参照、包外原生方法访问拒绝、XLang 边界及三条范例路径的固定输出。不带 `-SkipAot` 时，默认还构建运行无输出核心和公开范例 AOT；后者必须匹配同一固定输出。构建使用单次独占目录，失败后也清理。当前快测和核心 AOT 通过，完整范例构建仍缺共享 `xl6ValueDataSnapshotV1`，所以默认发布门禁如实失败。

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
