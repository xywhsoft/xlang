# cuckoo

16 位指纹、每桶 4 项的内存 Cuckoo Filter。支持 `add`、`contains`、`remove`、`clear` 与 `count`，适合大量短键的近似成员查询。该结构**可能产生假阳性**；在未执行删除时，成功插入的键不会产生假阴性。但按指纹删除可能删除一个与查询键碰撞的项，因此不应把它当成精确集合或安全授权表。

```xlang
import cuckoo

cuckoo.CuckooFilter seen = cuckoo.create(10000)
seen.add("session:abc123")
assert seen.contains("session:abc123")
seen.remove("session:abc123")
```

`create(capacity)` 的容量是**可插入项数上限**，而非字节数；桶数取能使目标装载率不超过约 75% 的 2 的幂。重复插入会计作多项。容量耗尽或踢出无法完成时 `add` 抛错，并且原有指纹不变。键按原始字节处理，支持 UTF-8 与内嵌 NUL，不做大小写折叠。`contains` / `remove` 在指纹未命中时返回 `false`；不存在的键若与已存指纹碰撞，也可能返回 `true`。只应删除已知成功插入的键，否则可能误删其他键的指纹。同一实例的并发读写由调用方同步；实例不用时由 XLang 生命周期或 C 的 `xcuckoo_destroy` 回收。

## C 接口

`include/cuckoo_native.h` 公开原生句柄与操作。每个键同时传入指针和真实字节长度；不存在 `len=-1` 的隐式 `strlen` 模式。C 函数 `xcuckoo_add` / `contains` / `remove` 返回 `bool`，与 XLang extern ABI 一致。内存由 XRT 分配和释放，哈希复用 `xrtHash64`。失败踢出用栈上回滚日志逆序恢复被改写的桶位，不留下半次插入。
构造容量使用 `int64_t`，对应 XLang `int`，Linux LP64 下不以 `c_longlong` 代替。
有结果的 C 调用成功时清除本线程旧的 XRT 错误；有效键未命中时
`contains` / `remove` 返回 `false` 且不设置错误。空句柄不是空过滤器：
`count` / `buckets` 返回 `-1` 并设置 `XERR_ARGUMENT`，`clear` 同样报告
参数错误。这样可以在 C 中区分正常未命中与失败，并在失败后直接重试。

## 验证

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\cuckoo\test\check.ps1
.\release\xl.exe run test\cases\extlib_cuckoo.xl
.\release\xl.exe run test\cases\extlib_cuckoo_boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package cuckoo -Source release\extlibs\cuckoo\test\test_cuckoo.c
.\release\xl.exe build release\extlibs\cuckoo\test\aot_core.xl -o build\test\cuckoo_aot_core.exe
.\build\test\cuckoo_aot_core.exe
.\release\xl.exe run release\examples\extlibs\cuckoo.xl
.\release\xl.exe run release\extlibs\cuckoo\examples\cuckoo.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package cuckoo -Source release\extlibs\cuckoo\examples\cuckoo.c
bash release/extlibs/cuckoo/test/check.sh
```

C 测试构造落在同一对桶的 9 个不同指纹，验证第 9 次失败后原 8 项仍可查询，移除一项后仍能插入。另覆盖容量、空键及内嵌 NUL。Windows 严格 GCC、Linux ASan/UBSan、XLang JIT 和无输出 AOT 核心已通过；完整带输出范例 AOT 仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。默认 Windows 门禁现在也检查范例 AOT，不再只凭核心通过返回成功；`-SkipAot` 仅用于聚焦 C/XLang 验证。发布级验收暂不签收。
包内 `test/high_load.xl` 已登记到 `coverage.tests`。C 与 XLang 都用
容量 3000 的确定性多桶序列验证高装载插入：首次失败或容量耗尽后计数
不变，全部先前成功插入的键仍可查询。这是无删除条件下的无假阴性回归，
不承诺未插入键一定返回 false。
包清单只声明原生层实际使用的 `XRT_MODULE_HASH64`；同一无输出核心 AOT
在当前工具链下由 3,574,784 缩至 583,168 字节，构建和执行均通过。

2026-10-23：修复 16 位指纹计算出的备用桶偏移为零时两个候选桶重合、
导致未满容量却拒绝插入的问题。仅零偏移改用相邻桶，其余桶映射不变。
C 回归测试搜索五个不同指纹、同一主桶且原备用桶重合的键；旧实现第
五次插入失败，修复后五次插入和查询均通过。Windows 聚焦门禁及 Linux
C ASan/UBSan、XLang 和双语范例对照通过；完整带输出 AOT 的共享缺符号
阻断仍未解除，不据此签收整包发布级。
2026-10-14 新增失败→成功 C 错误状态回归与空句柄计数/清空负例；
包内 Linux 脚本还检查 manifest 哈希模块宏下的严格 GCC 编译，再以 ASan/UBSan 复验。C 范例检查删除结果，
并与 XLang 范例输出相同的成功标记。
2026-10-16 高装载性质测试通过 Windows C、Linux ASan/UBSan 与 XLang
JIT；目录 smoke 2/2 通过。完整带输出 AOT 阻断未变。

2026-10-18：导入方原可直接调用 `addNative(text,length)` 伪造键的
字节长度；编译负例先确认可达，六个 `*Native` 方法现仅供类内部使用。
`xl check extlibs/cuckoo/test/private_native_negative.xl` 应因访问控制失败，
该负例只编译、不执行。Linux 门禁新增中央 XLang 边界、高装载用例和
C/XLang 范例逐字节对照；严格 GCC、C ASan/UBSan 及上述用例均通过。
公开 XLang/C API 与哈希算法不变，完整带输出 AOT 共享阻断未变。

同日新增构造分配故障回归：C 测试逐点覆盖过滤器与寄存器数组的两次分配，
核对 `XERR_MEMORY`、失败后恢复与资源清理；包内 XLang 用例扫描前 32 个
分配位置，确认故障确实触发时只捕获 `MemoryError`、无故障时成功，且清除
故障后可写入并查询。用例登记于包覆盖清单和 Linux 门禁。
这项验证无需修改工厂实现；完整带输出 AOT 的主线阻断不变。
Windows 包级入口同时执行严格 C 测试、中央与包内 XLang 边界、上述分配故障、
私有原生入口编译负例、C/XLang 范例同输出、无输出核心 AOT 和带输出范例 AOT。
AOT 每次使用独立测试文件，退出时只清理本次产物。

2026-10-22：旧默认脚本返回 PASS，但公开 XLang 范例单独 AOT 构建缺
`xl6ValueDataSnapshotV1`。现在把范例构建、运行及 C 输出对照加入默认门禁；
`-SkipAot` 的 C/语言聚焦路径通过，默认门禁如实报告共享缺符号。

2026-10-19：C 范例可直接用 `xl run` 单文件运行；TCC 路径在范例内引用包内
原生实现，GCC 路径仍独立编译原生实现。Windows/Linux 包级门禁同时对照
直接运行、独立 C 构建和 XLang 范例的输出，均已通过。完整带输出 AOT 的
共享主线阻断未变，不据此签收整包发布级完成。
