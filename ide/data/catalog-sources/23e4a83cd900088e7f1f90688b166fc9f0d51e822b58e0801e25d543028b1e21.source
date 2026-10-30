# linalg

2026-10-26 完整重建 Windows/Linux 发布物后，本包在两平台 x64 的
包级范围签收。默认门禁通过严格 C、Linux ASan/UBSan、数值边界、
XLang 边界与分配故障、十三项公开函数的双语固定输出，并在两平台
构建运行核心、XLang 范例和 C 单文件范例的独立 EXE。旧
`xl6ValueIndexReadCopyV1` 缺符号记录属于历史发布物；本结论不代替
全产品回归，也不外推 32 位/ARM 或长时性能。

`linalg` 是小型二维/三维向量和 2×2 矩阵工具，不是任意维 BLAS。
XLang 使用普通 `float` 与多返回值，C 使用输出参数；两者计算路径相同。
标量 `clamp`、角度/弧度转换已由 XRT `math` 标准库提供，包内不重复导出。

```xlang
import linalg

float nx = 0.0
float ny = 0.0
nx, ny = linalg.normalize2(3.0, 4.0)

float ia = 0.0
float ib = 0.0
float ic = 0.0
float id = 0.0
ia, ib, ic, id = linalg.inverse2(1.0, 2.0, 3.0, 4.0)
```

向量入口：`dot2/dot3`、`mag2/mag3`、`normalize2`、`cross3`、
`angle2`（弧度）、`dist2/dist3`。矩阵入口：`det2`、`inverse2`、
`multiply2`；矩阵参数按行序 `(a,b,c,d)` 表示
`[[a,b],[c,d]]`，`multiply2(left,right)` 返回 `left × right`。
`lerp(a,b,t)` 是标量线性插值。向量长度与距离调用 XRT 的
`xrtMathHypot`；归一化、角度与求逆在计算前缩放输入，避免大/小尺度
中间值不必要地溢出或下溢。不可归一化向量、数值不稳定或不可表示的
逆矩阵在 XLang 中抛 `ValueError`；C `_into` 入口返回 0，且失败时
不覆盖调用方输出参数。
异号极大端点的 `lerp` 不先计算 `b-a`；轻微外推的有限结果也不会因
中间差值溢出而误变成无穷大。
`angle2` 在缩放后以 `atan2(|cross|, dot)` 计算 0 至 π 的无向夹角，
保留近乎平行或反向向量约 `1e-8` 弧度的差异。
求逆边界测试还覆盖元素为 `1e308`、原行列式超出 `double` 范围但
逆矩阵有限的非对角矩阵；不能仅凭直接计算的行列式是否溢出判断可逆性。
2026-10-25 历史验证：这一边界的 C、XLang JIT 与独立 EXE 在 Windows 通过，
Linux 包级 C ASan/UBSan、XLang 和双语门禁通过。Windows 根目录的
整包门禁仍被共享 VFS 头文件查找问题阻断；从 `release` 目录运行的
成功结果不替代该项发布验收。

C 头文件为 `include/linalg_native.h`。XLang 多返回值所需的 `xvalue`
桥接由包内实现；C 调用方直接使用 `_into`，避免额外装箱。函数均不保留
输入或结果指针，适合多线程并发调用。`dot`、`det` 等纯算术入口遵循
IEEE 浮点传播规则；成功的标量调用会清除当前线程的旧 XRT 错误，
不会让前一次失败污染本次结果。它们不替代数值分析库的条件数评估。
四个 `_into` 入口的输出指针必须非空且两两不同；缺失或重复时返回 0、
设置 XRT 错误，且不会写入任何输出。XLang 的多返回值包装不需要调用方
提供输出指针，因此不受此约束。

聚焦验证：

- `xl run release/examples/extlibs/linalg.xl` 与
  `tools/run_extlib_c.ps1 -Package linalg -Source release/examples/extlibs/linalg_c.c`
  输出一致；C 范例也可直接 `xl run release/examples/extlibs/linalg_c.c`，
  仅在 TCC 单文件路径包含包原生实现，正式包仍由清单分离编译。
- `release/extlibs/linalg/test/check.ps1`：Windows 严格 GCC、C/XLang 边界及
  包运行器 C、单文件 C、XLang 三条范例路径的固定数值输出；默认还需
  构建运行核心、XLang 和 C 范例 AOT，后两者须匹配同一固定输出。`-SkipAot`
  仅运行 C/JIT 聚焦门禁，不是发布验收。
- `xl run extlibs/linalg/test/boundaries.xl`
- `tools/smoke_extlib_catalog.ps1 -Package linalg`：包清单登记的边界、
  分配故障用例和中央现行接口用例均纳入目录 smoke。迁移到 `math` 的
  标量函数由 `test/cases/stdlib_math.xl` 单独验证。
- `xl run extlibs/linalg/test/allocations.xl`：`normalize2`、`cross3`、`inverse2`、`multiply2` 各自逐点注入多返回值分配失败，核对 `MemoryError` 与解除故障后的再次调用。
- `tools/run_extlib_c.ps1 -Package linalg -Source release/extlibs/linalg/test/test_linalg.c`
- `bash release/extlibs/linalg/test/check.sh`：Linux 严格 GCC 与 ASan/UBSan C 测试、中央和包内 XLang 用例，以及 C/XLang 范例逐字节对照；默认还构建运行核心、XLang 和 C 范例独立 EXE。
- 边界覆盖极小/极大向量、相差 600 个数量级的矩阵行、奇异/近奇异矩阵、
  非有限输入、近乎平行/反向角度、失败时输出参数保持不变、XRT `xvalue` 多返回桥及逐点分配失败。
  异号极值端点的插值与轻微外推也有 C/XLang 回归；C 回归另覆盖失败后八个标量入口的错误状态复位，XLang 回归覆盖捕获异常后的继续调用。
  Windows GCC `-Wall -Wextra -Werror`、Linux ASan/UBSan 已通过。

2026-10-18：C 边界从 1024 组固定种子的 2×2 矩阵中筛除近奇异输入，
实际核对 965 组，并设置至少 900 组的测试下限；
用不同量级的行缩放核对 `A × inverse(A)` 近似单位矩阵；
残差按乘积量级判断，避免把浮点消去误差误判为实现错误。
Windows C 定向用例和 Linux 严格 GCC/ASan/UBSan、XLang 与双语范例
联合门禁通过。求逆实现及公开语义未改，完整 AOT 阻断不变。

同轮扩展 `release/examples/extlibs/linalg.xl` 与 `linalg_c.c`：
13 个公开函数均有成对调用，包含多返回值的向量叉积、矩阵求逆与乘法。
Windows 两份范例和 Linux 包级逐字节输出对照通过；示例不改变 API。

2026-10-18 当时 `release/xl.exe` 的 AOT 构建缺少共享符号
`xl6ValueDataSnapshotV1`，见 `dev/extlib_blockers/text_snapshot_link.md`；
该阻断在 2026-10-26 发布物复验中已不再出现。
2026-10-18：中央用例删除三个不再由本包导出的旧标量断言；原失败
`[import] import package has no such exported symbol` 先复现，随后
Windows 目录 smoke 3/3 和 Linux 包级 C ASan/UBSan、语言与双语门禁通过。
没有恢复旧别名，也没有改变 `linalg` 计算实现。
清单只声明 Math 与 Value Container 两个原生直接依赖；Linux C 测试
宿主按这两个模块编译，另仅为故障注入启用 Memory Debug。2026-10-18
最小 `test/aot_core.xl` 曾缺 `xl6ValueIndexReadCopyV1`；当前发布物
已可独立构建运行。AOT 体积收益未单独测量。

2026-10-19：新增 Windows `test/check.ps1` 与覆盖十三项公开函数的固定数值输出；Linux `test/check.sh` 同步核对预期。两平台 C 边界、XLang 边界和三条 C/XLang 范例路径通过，Linux 包含 ASan/UBSan。数值算法未改，完整 AOT 的共享缺符号阻断仍在，不签收发布级。

2026-10-21：C `_into` 四入口原接受别名输出指针并返回成功，较早结果会被
后续写入覆盖。C 负例先红后绿，现先检查指针互异，再进行计算和写入；
失败时调用方输出保持不变。Windows 严格 C、XLang 边界与双语范例，
Linux C ASan/UBSan、XLang 与双语门禁通过。完整 AOT 仍受共享缺符号阻断。

2026-10-22：Windows 默认门禁原只检查 C/JIT 就报告 PASS；现默认还要求
核心与公开范例 AOT 构建运行，范例输出必须匹配固定预期。`-SkipAot`
聚焦门禁通过；默认门禁在核心构建的共享
`xl6ValueIndexReadCopyV1` 缺符号处正确失败，尚未执行到公开范例 AOT。
成功和失败均不留下本次独占构建目录。未修改算法或 API，不签收整包发布级。

2026-10-24：C 边界新增 256 组固定种子、小整数向量，核对叉积与两输入
正交及交换输入后结果反号；XLang 边界增加非单位向量的三返回值与
点积对照。Windows `-SkipAot` 与 Linux C ASan/UBSan、XLang、双语门禁
通过。此有限样本不代替浮点极值分析，完整 AOT 共享阻断未解除。
