# roaring

2026-10-26 重建 Windows/Linux x64 发布物后，两平台默认包门禁
均退出 0：严格 C、Linux ASan/UBSan、朴素集合差分、桶升级
与分配失败回滚、私有原生入口负例、C/XLang 固定输出，以及
核心与 C/XLang 范例独立 EXE AOT。旧 VFS/共享快照阻断不再复现。
本次仅签收文档化的 `uint32` 内存集合范围，不包含标准 Roaring
二进制格式、其他架构或长时性能承诺。

2026-10-25 Linux 发布物：包级 `test/check.sh` 现同次执行严格 C 与
ASan/UBSan、XLang JIT 边界/分配故障、C/XLang 固定输出，以及核心和
公开范例的独立 EXE AOT 构建运行；AOT 范例逐字节对照 C 输出，本轮
全部通过。Windows 默认包门禁也已通过。下方同日“Linux 独立 EXE
未验收”的文字均是加入此门禁前的历史记录，不再描述当前状态。

2026-10-25 新 Windows 发布版：从 `demo6` 根目录执行默认
`test/check.ps1`，严格 C、并交差和快照边界、XLang JIT/分配故障、
双语输出及独立 EXE AOT 全通过。下方同日“根目录 VFS 头文件阻断”
只描述先前发布物，不再是本次门禁状态。Linux C ASan/UBSan 和
XLang 门禁已过，仍未用新版 Linux 编译器验收独立 EXE。

精确的 `uint32` 非负整数内存集合。按高 16 位分桶，桶内从有序数组按密度升级为位图；支持重复插入、删除、查询、计数、最小/最大值、升序快照、集合并交差及清空。适合整数 ID 去重与成员查询。此实现**没有** Roaring 格式序列化、run container 或磁盘持久化，不与其他 Roaring 实现交换二进制数据。

```xlang
import roaring

roaring.RoaringBitmap ids = roaring.create()
ids.add(65536)
assert ids.contains(65536)
assert ids.count == 1
var ordered = ids.toArray()
assert ordered.length == 1 and ordered[0] == 65536
roaring.RoaringBitmap other = roaring.create()
other.add(42)
roaring.RoaringBitmap both = ids.unionWith(other)
assert both.count == 2 and ids.count == 1
assert ids.remove(65536)
```

取值范围为 `0..4294967295`，越界抛 `ValueError`。`add` 重复值不改变数量；`remove` 返回是否确实删除；空集合的 `min` / `max` 为 `-1`。桶从数组升为位图后，删除至 4096 项时降回有序数组；继续删减时逐级收缩容量，避免稀疏桶长期占用整张位图。创建、插入或删除期间的内存分配失败由原生调用桥接抛 `MemoryError`，失败的操作不改变集合。同一实例的并发访问需调用方同步。

`toArray()` 返回独立的 `var` 数组快照，元素是按数值升序排列的整数；之后修改或销毁原集合不会改变快照。调用成本与元素数量成正比，并需为结果分配内存；内存不足抛 `MemoryError`，原集合保持不变。

`unionWith(other)`、`intersectWith(other)`、`differenceWith(other)` 分别返回并集、交集与差集（当前集合减去 `other`）的新 `RoaringBitmap`。输入保持不变；允许同一实例与自身运算，空结果也是有效集合。归并按已有的升序桶和桶内元素执行，结果仍遵守 4096 项的数组/位图阈值。创建或归并分配失败抛 `MemoryError`，两个输入保持不变。

## C 接口

`include/roaring_native.h` 公开不透明句柄和 `xroaring_*` 操作。`xroaring_add` 成功返回 `true`，含重复插入；`remove` / `contains` 为 `bool`。输入使用 `int64_t` 以便拒绝越界值，结果范围仍限于 `uint32`。通过 `xroaring_destroy` 释放句柄，内存分配复用 XRT。
有结果的 C 调用成功时清除本线程旧的 XRT 错误；有效集合中不存在的值返回
`false` 且无错误。空句柄则报告 `XERR_ARGUMENT`：`count` 返回 `-1`，
`min` / `max` 返回 `-1` 但带错误，`clear` 也设置错误；这与合法空集合
的 `count == 0`、`min/max == -1` 且无错误区分开。
`xroaring_to_array` 返回 XRT `xvalue` 数组，调用方用 `xrtValueRelease` 释放。空集合返回空数组；空句柄返回 XRT null 值并设置 `XERR_ARGUMENT`；分配失败返回 XRT null 值并保留 `XERR_MEMORY`。该 API 需要 `XRT_MODULE_VALUE_CONTAINER`，包清单已声明。
`xroaring_combine_into(output, left, right, operation)` 使用 0=并、1=交、2=差（`left-right`）。三个句柄可互相别名；先完整构造结果，成功后替换 `output`，失败时 `output` 与输入均保持原样。空句柄/无效操作码报告 `XERR_ARGUMENT`，内存不足报告 `XERR_MEMORY`。

## 验证

2026-10-25：新增并、交、差的 C/XLang 接口与双语范例。C 固定边界和独立布尔模型覆盖数组+数组、位图+数组、数组+位图、位图+位图、跨桶、`UINT32_MAX`、输出别名与失败回滚；XLang 测试覆盖返回对象、输入不变及分配故障分类。Windows 严格 GCC、C 原生测试、从 `release` 目录运行的 XLang JIT、双语范例及公开范例独立 EXE 通过；Linux `test/check.sh` 通过 C ASan/UBSan、语言边界与分配用例、双语固定输出。Linux 仍使用现有发布物，未验收新 Linux 独立 EXE。当时的根目录 VFS 头文件阻断仍按下文记录，旧发布物的默认 Windows 包门禁未签收。

2026-10-25：新增升序快照 C/XLang 接口及双语范例。C 测试覆盖空集、跨桶与 `UINT32_MAX`、数组/位图两种桶、修改源集合后的快照独立性、空句柄和逐分配点故障回滚；XLang 边界及内存故障用例同步补齐。Windows 严格 GCC、C 原生测试、从 `release` 目录运行的 XLang JIT、公开范例 AOT 和双语范例输出均通过。Linux `test/check.sh` 也通过 C ASan/UBSan、XLang 边界/故障用例和双语范例对照；仍使用现有 Linux `release/xl`，不据此签收新版 Linux 发布物。先前 Windows 发布版从 `demo6` 根目录运行包级脚本时找不到 `include/xruntime_decl.h`；这是当时的运行时 VFS/路径问题，见 `dev/extlib_blockers/release_root_xruntime_decl.md`，当次未据此宣称默认整包门禁通过。

2026-10-25：在 WSL 中运行现有 `test/check.sh` 全通过：严格 GCC、
独立位图模型和分配故障回滚的 ASan/UBSan、XLang 边界与分配用例，
以及 C 分离编译、C 单文件直跑和 XLang 范例的固定输出对照。
本次使用现有 Linux `release/xl`，未重建 Linux 发布版，也未验收
Linux 独立 EXE。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C、独立位图模型、分配故障回滚、XLang 边界、私有原生入口
拒绝、双语固定输出及核心/公开范例 AOT 全部通过。旧的完整 AOT
共享符号缺失只对应先前发布物；该次未重跑 Linux sanitizer。
验收仍限于本包的 `uint32` 内存集合合同，不包含标准 Roaring
二进制序列化或跨实现互通。

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\roaring\test\check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\roaring\test\check.ps1 -SkipAot
.\release\xl.exe run test\cases\extlib_roaring.xl
.\release\xl.exe run test\cases\extlib_roaring_boundaries.xl
.\release\xl.exe run release\extlibs\roaring\test\allocations.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package roaring -Source release\extlibs\roaring\test\test_roaring.c
.\release\xl.exe build release\extlibs\roaring\test\aot_core.xl -o build\test\roaring_aot_core.exe
.\build\test\roaring_aot_core.exe
.\release\xl.exe run release\examples\extlibs\roaring.xl
.\release\xl.exe run release\extlibs\roaring\examples\roaring.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package roaring -Source release\extlibs\roaring\examples\roaring.c
bash release/extlibs/roaring/test/check.sh
```

专项测试覆盖跨桶、`UINT32_MAX`、重复值、数组转位图、删除、复用，以及新桶/数组扩容/位图升级和降级收缩时的分配失败回滚。C 用例另以固定种子对照朴素位图，交错验证三桶的插入、删除、查询、计数及最小/最大值。Windows 严格 GCC、Linux ASan/UBSan、XLang JIT 与双语固定输出通过。先前发布物的带输出范例 AOT 曾受主线 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`；2026-10-26 两平台默认门禁均验证不再复现。
Windows 默认包门禁要求核心和公开 XLang 范例 AOT 均构建运行，范例输出
须匹配固定预期；`-SkipAot` 只执行 C/JIT 聚焦测试。
2026-10-15 新增满 65536 项单桶回归：逆序插入、重复插入、逐项删空、相邻桶保留及原位置重建均通过 Windows C 和 Linux ASan/UBSan；现有 XLang 边界用例复跑通过。完整 AOT 阻断未变。
2026-10-14 新增失败→成功 C 错误状态及空句柄负例，包内 Linux 脚本可用
严格 GCC + ASan/UBSan 复跑该测试；XLang 完整边界与核心 AOT 保持通过。

2026-10-15 核对原生源码只使用 XRT 基础分配/释放与错误接口，无可裁剪模块依赖，
manifest 因此显式声明空 `xrtModules`。同一无输出 AOT 核心从 3,577,856
缩至 586,752 字节且运行通过；Windows C/XLang 边界与双语范例同输出、
Linux ASan/UBSan 包级门禁均复跑通过。带输出范例 AOT 的共享缺符号阻断未变。

2026-10-16 增加 XLang 创建、新桶插入与数组升级的内存故障注入测试，验证 `MemoryError`、事务状态和清除故障后恢复。C 原有故障测试补 `XERR_MEMORY` 断言。Windows 严格 GCC/C、目录 smoke 2/2、无输出故障注入 AOT 与 Linux ASan/UBSan 通过；编译器已负责错误映射，包本体不增加重复桥接代码。完整带输出 AOT 阻断不变。

2026-10-17：Linux `test/check.sh` 现于同次运行中执行 C ASan/UBSan、
两组中央 XLang 用例、包内分配故障用例和 C/XLang 范例逐字输出对照，
全部通过。只验证文中承诺的内存集合，不据此宣称二进制格式互通；
完整带输出 AOT 的共享主线阻断未变。

2026-10-18：导入方原可直接调用 `addNative` 等原生方法，绕开公开
`add/remove/contains` 的 uint32 范围、对象状态与异常口径。C 入口自身
也校验取值范围，未发现静默截断；本次收紧的是语言层访问边界。
`xl check extlibs/roaring/test/private_native_negative.xl` 先返回 `ok`，
现因访问控制失败；七个 `*Native` 方法只供类内部使用。Windows XLang
边界/分配故障与 Linux C ASan/UBSan、XLang 双语门禁通过，完整带输出
AOT 共享阻断未变。

2026-10-19：C 范例现可直接用 `xl run` 单文件运行；TCC 路径引用包内
实现，GCC 路径仍分离编译。Windows 新增包级 `test/check.ps1`，联合
严格 GCC、C 差分/故障边界、XLang 用例、私有原生入口编译负例和三方
固定黄金输出；Linux `test/check.sh` 同步验证直跑与分离编译的结果一致。
两平台门禁已通过，完整带输出 AOT 的共享阻断未变。

2026-10-22：Windows 默认门禁原在 C/JIT 通过后直接报告 PASS；现先构建
运行无输出核心，再构建运行公开范例 AOT 并检查同一固定输出。
`-SkipAot` 聚焦门禁通过；默认门禁的核心 AOT 通过后，公开范例构建
因共享 `xl6ValueDataSnapshotV1` 缺符号返回失败。本次独占构建目录
成功和失败后均清理；实现/API 未改，不签收整包发布级。
