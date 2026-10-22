# skiplist

有序字符串键值表。`set` 插入或覆盖，`get` 支持缺省值，`contains`/`remove` 查询和删除，`keys()` 返回全部键，`range(start, finish)` 返回两端包含的 `{key, value}` 条目数组。`count` 与 `clear()` 管理总数和复用。逆序区间返回空数组，不静默截断结果。

```xlang
import skiplist

skiplist.SkipList table = skiplist.create()
table.set("a", "first")
table.set("b", "second")
var rows = table.range("a", "b")
println(rows[0]["value"])
```

键的排序使用完整 UTF-8 原始字节的字典序，不做 Unicode 归一化或大小写折叠；键和值都可包含 NUL。缺失键由 `get` 的 `default` 参数区分，已存储的空字符串不会被当作缺失。覆盖时先分配新值，成功后才释放旧值；新节点完全构造后才接入跳表。每个实例用 XRT 系统随机源独立播种 XRT RNG 层级序列，避免公开的固定序列被外部输入预先利用；随机源失败时创建失败并保留 XRT 错误，不回退到可预测种子。失败的插入不推进 RNG 状态。平均查找、插入、删除复杂度为 O(log n)，最坏情况仍可达 O(n)；有序全量结果需要 O(n) 额外内存。同一实例的并发读写需要调用方同步。

`skiplist.create()` 在底层分配失败时由原生调用桥接抛出 `MemoryError`，系统随机源失败时保留 XRT 原错误；不会返回带空原生句柄的实例。`set/get/keys/range` 的分配失败同样抛 `MemoryError`；失败的插入和覆盖不改变原有键值。

`setNative` 等原生入口仅供类内部使用。包外必须通过公开方法传入键和值，由包装层计算真实字节长度；`test/private_native.xl` 是预期编译失败的可见性负例，不执行其中的长度不匹配调用。

## C 接口

`include/skiplist_native.h` 的键和值均传入指针及明确字节长度。`xskiplist_create` 的句柄由 `xskiplist_destroy` 释放；`xskiplist_get`、`xskiplist_keys`、`xskiplist_range` 返回的 `xvalue*` 由调用方用 `xrtValueRelease` 释放。缺失键返回 `XVALUE_NULL`，错误返回空指针并设置 XRT 错误。输入缓冲可在调用返回后释放。

公开操作进入时清除同一线程的旧 XRT 错误；合法未命中不携带之前的错误。`clear(NULL)` 和 `count(NULL)` 报参数错误，`destroy(NULL)` 安全。

`xskiplist_count` 返回 `int64_t`，与 XLang `int` 的 C ABI 一致；C 输出可使用 `<inttypes.h>` 的 `PRId64`。

## 验证

Windows 聚焦快测：`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/skiplist/test/check.ps1 -SkipAot`。它同次检查 C 独立模型、包外 `setNative` 访问拒绝、XLang 边界与 C/XLang 固定范例输出。去掉 `-SkipAot` 后，默认还构建运行核心和公开范例 AOT，后者必须匹配同一黄金输出；每次使用并清理独占构建目录。当前快测和核心 AOT 通过，完整范例仍被共享 `xl6ValueDataSnapshotV1` 缺符号阻断，不签收整包发布级。

从 `demo6` 目录运行：

```powershell
.\release\xl.exe run test\cases\extlib_skiplist.xl
.\release\xl.exe run release\extlibs\skiplist\test\allocations.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package skiplist -Source release\extlibs\skiplist\test\test_skiplist.c
wsl bash release/extlibs/skiplist/test/check.sh
.\release\xl.exe build release\extlibs\skiplist\test\aot_core.xl -o build\test\skiplist_aot_core.exe
.\build\test\skiplist_aot_core.exe
.\release\xl.exe run release\examples\extlibs\skiplist.xl
.\release\xl.exe run release\examples\extlibs\skiplist_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package skiplist -Source release\examples\extlibs\skiplist_c.c
```

C 测试覆盖 1000 键排序与闭区间、5 KiB 值、NUL、空值与缺失、覆盖及新增的故障注入；创建的首次和第二次分配失败均返回空句柄，清除故障后可正常创建。另用独立有序键值表对照 400 步插入、覆盖、删除和清空，逐步核对二进制键值、排序及区间结果；包内 Linux ASan/UBSan 门禁运行同一测试。XLang 常规用例、双语范例及无输出 AOT 核心用例通过。完整边界和带输出范例的 AOT 构建仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。

2026-10-15：新增失败后成功的连续 C 调用测试，修复合法未命中和成功操作携带旧错误；空句柄 `clear/count` 明确报参数错误。包清单声明 Value Container 依赖，核心 AOT 从 3,587,584 降至 596,480 字节并运行通过。C 范例可直接 `xl run`，也可用包级脚本执行；C/XL 输出一致，Windows C、Linux ASan/UBSan 与 XL JIT 复跑通过。完整带输出 AOT 主线阻断不变，未签收发布级。

2026-10-16：C 创建、首次插入和覆盖的分配故障测试增加 `XERR_MEMORY` 断言；新增 XLang 逐点故障测试覆盖创建、插入、覆盖、读取、`keys`、`range` 的异常类型、原值保持与恢复，并登记统一目录 smoke。Windows 严格 GCC/C、目录 smoke 2/2、Linux ASan/UBSan 通过。新增完整故障用例的 AOT 构建仍缺 `xl6ValueDataSnapshotV1`，包侧不做编译器绕过。

2026-10-16：修正 `count` 在 Linux LP64 下的 `long long`/`int64_t` 原型冲突，增加公开头文件 ABI 门禁；Windows/Linux XLang 调用、C 测试及双语范例复验。完整 AOT 的共享阻断不变。

2026-10-17：Linux `test/check.sh` 现在同次执行 C ASan/UBSan、中央和包内
XLang 用例，以及 C/XLang 范例逐字输出对照，全部通过。C 范例为支持
单文件 `xl run` 已包含 `skiplist_native.c`，门禁只编译范例与 XRT 运行时，
不重复链接实现源。完整边界 AOT 的共享主线缺符号阻断未变。

2026-10-19：新增 Windows 单入口 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/skiplist/test/check.ps1`，整合严格 GCC/ABI、800 步 C 独立模型及故障边界、中央与包内 XLang 用例，并将包运行器 C、单文件 C、XLang 范例各自对照固定输出。Linux `test/check.sh` 同步加入固定输出核对；Windows 与 Linux ASan/UBSan 包门禁通过。跳表实现未改，完整带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。

2026-10-23：固定种子会使每个实例公开同一层级序列，外部输入可预先安排插入顺序，将搜索路径推向线性。创建时改为直接使用 XRT 系统随机源播种 XRT RNG；随机源失败即释放半成品并保留其错误，不提供可预测回退。插入失败时不提交 RNG 状态。包内 `test/entropy_failure.c` 模拟系统随机源失败，检查原错误、创建时分配回收和解除故障后的重建。Windows `-SkipAot` 严格 C/XLang 与双语门禁、Linux C ASan/UBSan 和 XLang 门禁通过；默认 Windows 门禁的核心 AOT 构建运行通过，公开范例仍缺共享 `xl6ValueDataSnapshotV1`，因此整包尚未发布签收。

同日补大范围查询故障矩阵：C 在 1000 项闭区间上扫描前 64 个分配点，XLang 在 64 项区间上扫描前 64 点；失败必须是 `XERR_MEMORY`/明确的 `MemoryError`，成功结果必须完整，原列表保持可用，解除故障后仍能查询全部项。已有创建、插入、覆盖、读取与键查询用例同步收紧为类型捕获。Windows `test/check.ps1 -SkipAot` 和 Linux C ASan/UBSan、XLang/双语门禁通过；跳表实现/API 未改，完整 AOT 主线阻断不变。
