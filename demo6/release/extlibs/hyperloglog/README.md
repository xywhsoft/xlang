# hyperloglog

`hyperloglog` 以约 `2^precision` 字节寄存器近似估计不同数据项的数量，不储存原始数据。它不是精确集合，`count()` 的结果可能偏高或偏低；`added` 记录本实例的 `add` 调用数及每次合并带来的源计数，包含重复项。使用 XRT 固定种子的 64 位哈希，寄存器秩只计算扣除索引后的剩余位。

## XLang API

- `create(precision = 14)`：精度 4..18，对应 16..262144 个寄存器。
- `hll.add(text)`：按 UTF-8 **字节序列**计项，传入实际字节长度，嵌入 NUL 不会被截断。
- `hll.count()`：返回近似去重数；空状态为 0。
- `hll.added` / `hll.precision`：查看写入调用次数及精度。
- `hll.merge(other)`：只接受同精度草图，合并寄存器；自合并是无操作。
- `hll.clear()`：清空寄存器与 `added`。

`HyperLogLog` 是导出类型，需要显式声明时可写
`hyperloglog.HyperLogLog sketch = hyperloglog.create(8)`；`auto` 仍可使用。

寄存器合并是幂等的，重复合并相同来源不会改变 `count()` 的估计；`added` 没有来源去重元数据，因此每次重复合并仍会再次累加源的计数。它既不是精确去重数，也不能用来判断某条数据是否已经出现。

估计使用 HLL 谐波平均和小样本线性计数修正。经典 HLL 的典型相对标准误差约为 `1.04/sqrt(m)`，`m=2^precision`；这是统计性质，不是单次结果的硬保证。公式来源为[Flajolet 等人的原始论文](https://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf)。本包未实现 HLL++ 偏差表或持久化格式，勿把当前寄存器布局当作跨版本文件协议。

## C 接口与并发

公开声明在 `include/hyperloglog_native.h`。`xhll_create` 返回的句柄由调用方用 `xhll_destroy` 释放；`xhll_add` 的 `length=-1` 表示 NUL 结尾文本，显式非负长度可加入二进制数据。C 函数通过返回值和 XRT 错误对象报告无效句柄、精度或长度。单个实例的并发读写**没有内部锁**；不同实例可在不同线程独立使用。`merge` 前应由调用方同步参与的两个实例。
除析构外，每个公开 C 入口开始时清除本线程旧的 XRT 错误；失败后可直接
调用其它操作或重试，成功不会留下前一次调用的错误。此合同覆盖热路径 `add`
及创建、计数、合并、清空和属性读取。

## 验证与范例

- XLang 范例：`release/examples/extlibs/hyperloglog.xl`
- 同输出 C 范例：`release/extlibs/hyperloglog/examples/hyperloglog_c.c`
- 边界：包内 `test/test_hyperloglog.c`、`test/test_threads.c`、`test/boundaries.xl`、`test/allocations.xl`，以及 `demo6/test/cases/extlib_hyperloglog.xl`
- `xl check extlibs/hyperloglog/test/private_add_negative.xl` 和 `private_merge_negative.xl` 应被访问控制拒绝；两条负例只编译，不执行。

在 `demo6/release` 运行 `xl run extlibs/hyperloglog/test/boundaries.xl`；在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package hyperloglog -Source release/extlibs/hyperloglog/test/test_hyperloglog.c`。C 测试对固定的 100000 项、20000 项数据集做统计余量断言，而非要求近似值等于真实值。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界和线程测试、同输出范例及 `test/cases/extlib_hyperloglog.xl` 无输出核心 AOT。`xl build extlibs/hyperloglog/test/boundaries.xl -o ../build/test/hyperloglog_xl_test.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/hyperloglog/test/check.sh`，以严格 GCC、ASan/UBSan 复验 C 边界、独立实例线程测试及 C 范例；门禁也运行中央和包内 XLang 用例，逐字节比较 C/XLang 范例输出。2026-10-14 增加“分片合并与直接写入的估计相等”对照、重复合并的 `added` 口径，以及创建句柄两次分配失败后的恢复测试；估计算法未改。
Linux 门禁同时检查按 manifest 传入 XRT 哈希模块宏时的严格编译，避免原生源码重复定义该宏。
`count` 与 `added` 的 C 返回类型为 `int64_t`，与 XLang `int` 的 LP64 ABI 一致；`add` 的长度参数仍是明确的 C `long long` / XLang `c_longlong`。
同日增加七个公开 C 入口的失败→成功错误槽回归；10 万项既有测试、
C/XLang 范例、Windows 严格 GCC 和 Linux ASan/UBSan 复跑通过。

2026-10-16：XLang 边界用例补精度下限 4、上限 18 的创建与计数，
并登记到包的 `coverage.tests`，可由统一扩展库 smoke 发现。包级及中央
XLang 用例从 `demo6` 工作目录运行通过；完整 AOT 阻断仍未解除。

2026-10-16：Linux XLang 导入原在 `count` 的 `long long` 声明处与生成的 `int64_t` 原型冲突；两个结果统一为 `int64_t`，补 ABI 契约测试与 C 范例可移植输出。完整 AOT 阻断不变。

同日修复 C 范例忽略三次 `daily` 写入结果的问题：任一次失败都不会继续打印 `PASS`。Linux C/XL 包级门禁整套复跑通过；完整 AOT 的共享缺符号阻断仍未解除。

2026-10-18：导入方原可直接调用 `addNative(text,length)` 伪造长度，
或向 `mergeNative(point)` 传任意句柄。两条编译负例先确认可达，六个
`*Native` 方法现仅供类内部使用，公开 XLang 方法和 C ABI 不变。
Windows XLang 边界以及 Linux 严格 GCC、C ASan/UBSan、大样本、
线程与 C/XLang 双语对照通过；完整 AOT 共享阻断未变。

同日补导出 `HyperLogLog` 类型：原先仅 `auto` 可接收 `create()` 结果，
包外显式类型声明报未导出。跨包命名类型测试先红后绿，现纳入清单和 Linux 门禁。

2026-10-18：C/XLang 边界增加部分重叠的 128 项数据集，分别在精度 4 和 14 下
比较“两个草图合并”与“全部数据直接写入”的估计，并检查重复合并不改变估计、
`added` 按调用次数累加。Windows 定向 C/XLang 用例及 Linux C ASan/UBSan、
线程、XLang、双语范例联合门禁通过。此测试验证寄存器并集语义，
不将近似估计宣称为精确基数；完整 AOT 共享阻断不变。

2026-10-19：修复 `xl run release/extlibs/hyperloglog/examples/hyperloglog_c.c` 缺少原生符号的问题；仅 TCC 单文件入口包含包实现，正式包/GCC 仍分离编译。新增 Windows `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/hyperloglog/test/check.ps1`，与 Linux `test/check.sh` 均比较包运行器 C、单文件 C、XLang 范例，并对当前固定哈希算法的样本估计值作黄金输出检查；该固定值是回归基线，不是 HLL 精确性保证。两平台门禁通过，Linux 含 ASan/UBSan；完整带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。

2026-10-22：XLang 创建入口增加前 16 个分配位置的故障扫描，要求故障触发与
`MemoryError` 一致、成功返回有效空草图，并验证清除故障后可继续写入。
此用例已登记包清单并进入 Windows/Linux 包门禁；Linux C ASan/UBSan、
线程与双语对照同次通过。C 已有两次底层分配失败回归，原生实现未改；
完整带输出 AOT 的共享缺符号阻断仍未解除。
