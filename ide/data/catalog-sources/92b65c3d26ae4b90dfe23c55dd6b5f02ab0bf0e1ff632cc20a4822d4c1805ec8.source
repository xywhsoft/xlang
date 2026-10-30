# money

2026-10-27：`xm_format` 明确返回 XRT 分配的 `char*`，XLang 声明为
`@owned string`，由生成代码直接接管指针，不再经过 `point` 和
`takeCstr` 包装，也不通过可空局部变量再复制。新增独立的 JIT/AOT
与生成代码检查；接管时仍需
计算字符串长度，不宣称整个格式化调用零开销。完整门禁仍受下述
`money.split` 数组相等性主线问题阻断。

2026-10-26 重建 Windows/Linux 发布版后，原生 C、语言主用例与
错误分类继续通过；最小复现 `dev/extlib_blockers/money_split_array_equality.xl`
在两平台均显示普通/类型化数组同内容相等，而 `money.split` 返回的
原生数组内容正确却与同内容字面量比较为 `false`。分配故障用例
保留该断言，默认包门禁须在此报错，不得以逐元素检查替代相等性合同。
已补两平台公开 C 范例独立 EXE AOT 检查，但完整包门禁在主线修复前
不会执行到后段；当前不签收发布级，见下方历史记录与共享阻断文件。

2026-10-24 历史记录：更新后的 Windows 发布版默认包门禁通过 C 算术边界、
中央语言用例与错误类型用例，但在 `test/allocations.xl` 的故障后
恢复断言处失败：`money.split(101, 3)` 内容确为 `[34,34,33]`，
与同内容语言数组字面量的 `==` 却为 `false`。普通 `var` 数组和
`array<int>` 的同形比较均为 `true`，JIT/独立 EXE 可复现；详见
`dev/extlib_blockers/foreign_var_array_equality.md`。包内测试未以
逐项比较规避，完整语言与 AOT 门禁目前不能签收。

以 `int64` 最小货币单位表示金额，适合账单、税率和分摊等不允许整数回绕的场景。金额本身**不含货币码**，不同币种之间的混算需要调用方隔离；本库不提供实时汇率、银行规则或完整 ISO 4217 数据。

```xlang
import money

int subtotal = money.add(7999, 3499)
int discounted = money.scale(subtotal, 9, 10)
int taxed = money.scale(discounted, 107, 100)
println(money.format(taxed, "USD"))  // $110.72
```

## 接口

| XLang | 说明 |
| --- | --- |
| `add(a, b)` / `sub(a, b)` / `mul(a, n)` | 精确 `int64` 运算，溢出抛 `RangeError` |
| `scale(amount, numerator, denominator)` | 精确计算整数比例，绝对值四舍五入；分母为零抛 `ValueError`，溢出抛 `RangeError` |
| `rate(amount, rate)` | 浮点比例便捷入口；非有限值抛 `ValueError`，越界抛 `RangeError`；**不保证十进制金额精确** |
| `split(amount, n)` | 均分为 `n` 份，前几份分得余数；负数向前几份减一，所有份额之和守恒；`1 <= n <= 1000000` |
| `format(amount, currency)` | 千分位与货币符号；未知合法代码以大写 `XYZ ` 前缀展示 |
| `symbol(currency)` / `exponent(currency)` | 已知符号/小数位；未知合法代码分别返回空字符串/2 |

货币码必须恰好是 3 个 ASCII 字母，大小写不敏感；不接受内嵌 NUL、长字符串或任意 UTF-8 文本。内置 23 种常见代码；格式化符号及指数是展示约定，并非可更新的汇率表。`format` 的入参是最小单位，JPY/KRW/VND 指数为 0，其余内置代码指数为 2。未知合法代码按指数 2 处理。`format` 和 `split` 的合法参数若遇分配失败，抛 `MemoryError`，不会误报为参数错误。`rate` 使用二进制浮点，财务计算优先选择整数 `scale`。
`RUB` 使用卢布符号 `₽`；`symbol` 和 `format` 均有 C/XLang 回归用例。

## C 接口

`include/money_native.h` 提供 `xm_*_checked(..., int64_t *out)`；成功写入 `out`，失败保留原值并设置 XRT 错误。`xm_format` 返回独立分配的指针，用 `xrtFree` 释放；`xm_symbol` 返回借用指针，不释放。两个字符串入口都要求显式传入字节长度。`xm_add` 等 `xvalue*` 接口是 XLang extern 桥，调用方用 `xrtValueRelease` 释放返回值。`xm_split` 返回数组或 null `xvalue`，同样由调用方释放。格式化没有共享静态缓冲区。

包清单显式声明 `XRT_MODULE_VALUE_CONTAINER` 和 `XRT_MODULE_STRING`。同一无输出核心 AOT 在 Windows 上由 3,569,664 字节降至 578,560 字节，执行通过；显式混编额外 C 文件仍可能触发编译器的完整 XRT 路径。

每次公开 C 调用开始时会清除本线程上次遗留的 XRT 错误；成功结果不会与旧错误并存，失败后可用 `xrtGetError()` 读取本次原因。
XLang 外部调用会先将 XRT 的 `XERR_RANGE`、`XERR_VALUE` 和 `XERR_MEMORY` 分别抛为 `RangeError`、`ValueError` 和 `MemoryError`；包装层的空结果检查只用于诊断无原生错误的合同破坏。

## 验证

2026-10-22：C/XLang 订单范例补齐 `sub`、`mul`、`rate`、`exponent`，
现与原有 `add`、`scale`、`split`、`format`、`symbol` 一起展示全部公开入口。
浮点 `rate` 仅作为估算例子，正式财务比例仍用整数 `scale`。
Windows `-SkipAot` 的 C/语言/固定输出门禁与 Linux C ASan/UBSan、XLang
门禁通过；默认 Windows 核心 AOT 通过，完整范例 AOT 仍缺共享
`xl6ValueDataSnapshotV1`，不签收整包发布级。

在 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\money\test\check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\money\test\check.ps1 -SkipAot
.\release\xl.exe run test\cases\extlib_money.xl
.\release\xl.exe run test\cases\extlib_money_boundaries.xl
.\release\xl.exe run release\extlibs\money\test\error_kinds.xl
.\release\xl.exe run release\extlibs\money\test\allocations.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package money -Source release\extlibs\money\test\test_money.c
.\release\xl.exe build release\extlibs\money\test\aot_core.xl -o build\test\money_aot_core.exe
.\build\test\money_aot_core.exe
.\release\xl.exe run release\examples\extlibs\money.xl
.\release\xl.exe run release\extlibs\money\examples\money.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package money -Source release\extlibs\money\examples\money.c
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\money\test\check_examples.ps1
```

C 边界测试含 1 万组 GCC `__int128` 参考值对照；包内对照脚本将 C/XLang 的订单结算、分摊和六币种展示逐行与同一份固定预期输出比较。Windows 严格 GCC、Linux ASan/UBSan 与无输出 AOT 核心已通过。带 `println` 的完整 XLang 范例 AOT 仍受共享主线缺符号 `xl6ValueDataSnapshotV1` 阻断，见 `dev/extlib_blockers/text_snapshot_link.md`，包级验收暂不签收。

Windows `test/check.ps1 -SkipAot` 用于 C/JIT 日常门禁；默认模式另在单次
独占目录构建并运行无输出核心 AOT，然后构建、运行完整范例 AOT 并核对
同一固定输出。当前核心 AOT 通过，完整范例在共享缺符号处使默认门禁
返回非零，不能把 `-SkipAot` 的通过记为发布签收。两种模式在成功或失败
退出时都只清理本次生成的测试目录。

2026-10-17：`scale` 的 C 参考值测试增加正负分母和 `INT64_MIN/MAX` 交叉极值；XLang 边界用例固定负分母及最小整数结果。Windows C/XLang 定向测试与 Linux ASan/UBSan 包级门禁通过；完整 AOT 阻断不变。

在 Linux 的 `demo6` 目录可运行 `bash release/extlibs/money/test/check.sh`。该门禁以严格 GCC、ASan/UBSan 复验 C 算术参考值和故障注入，运行中央及包内 XLang 边界用例，并将 C/XLang 范例输出与固定预期逐字节比较；临时产物不写入发布目录。C 范例显式包含包内实现，可直接使用 `xl run`；`run_extlib_c.ps1` 会识别该包含，不会重复编译实现源码。

2026-10-14：`format`/`split` 增加 C 分配故障与失败后恢复测试，XLang 包装层区分非法输入和内存不足；金额算法未变。

2026-10-14：`rate` 不再通过给乘积加减 `0.5` 取整；这在 `int64` 极值附近会把 `rate(maximum, 1.0)` 误判为溢出。现在 `1.0` 保持原整数，其余倍率用 `roundl` 实现半值远离零取整。C、XLang JIT、无输出 AOT 核心与 Linux ASan/UBSan 均覆盖极值；带输出完整 AOT 仍受上述共享缺符号阻断。

2026-10-16：通过包内 `test/error_kinds.xl` 核实原生错误先于包装层空值分支抛出，修正算术越界的公开口径为 `RangeError`，并清理误导性的 `OverflowError` 后备分支；这不改变 C 数值算法。
该用例已登记到目录 smoke；当前 `-Package money` 的包内与中央用例 2/2 通过。

2026-10-19：新增 Windows 单入口 `test/check.ps1`，依次运行 1 万组整数参考值的 C 边界测试、中央与包内 XLang 用例、严格 GCC 宏检查和 C/XLang 固定输出对照，并单独核对 `xl run` 直跑 C 范例。当前 Windows 门禁通过；金额算法未改，Linux ASan/UBSan 本轮未重跑。完整带输出 AOT 的共享缺符号阻断不变，不据此签收整包发布级。

2026-10-19 后续：`split` 新增 9 个极值/小整数及 128 个固定种子金额、
各按 1..16 份分摊的 C 整数参照矩阵；逐份检查余数分配、元素数和总和。
XLang 对 `INT64_MIN/MAX` 同样逐份数检查守恒及首尾差值。Windows 联合门禁
和 Linux 严格 GCC、C ASan/UBSan、XLang 边界与双语固定输出均通过。
金额实现未改；带输出完整 AOT 的共享缺符号阻断仍在。

2026-10-19：`format` 与 `split` 新增 XLang 逐点分配失败和故障解除后的恢复测试，与已有 C `XERR_MEMORY` 检查形成双语边界。Windows 包级及 Linux C ASan/UBSan、XLang、固定双语范例门禁通过；金额实现未改，完整带输出 AOT 仍待共享主线缺符号修复。
