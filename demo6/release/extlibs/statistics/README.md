# statistics

`statistics` 对 XRT 数组中的整数、浮点数计算描述性统计。XLang 的 `var`
数组可直接传入，C 侧使用 `xvalue` 数组。非数值或非有限元素会拒绝。

```xlang
import statistics

var values = [2, 4, 4, 4, 5, 5, 7, 9]
assert statistics.mean(values) == 5
assert statistics.median(values) == 4.5
```

## API

- 聚合：`count`、`sum`、`mean`、`median`、`mode`、`min`、`max`、`range`。
- `mode` 在最高频次并列时返回数值最小者；全部元素只出现一次时也
  返回最小值。该规则与输入排列顺序无关，C 与 XLang 一致。
- 离散程度：`variance`、`varianceSample`、`stdev`、`stdevSample`、`mad`、`iqr`。
- 分位数：`percentile(values, p)`、`quartiles(values)`；`p` 必须是有限的 0–100，使用线性插值。
- 平均数：`geomean` 要求正值；`harmmean` 要求非零且调和和不为零。
- 双变量：`covariance`、`covarianceSample`、`correlation`、`linearFit`。
- 分箱：`histogram(values, bins)`，`bins` 范围为 1–100000。

`examples/basic.xl` 展示常用汇总，`examples/trend.xl` 展示分位数、拟合与分箱，
`examples/distribution.xl` 展示众数、总体/样本离散度、几何/调和平均数及协方差；
每份均有同输出的 C 范例和固定预期输出，Windows/Linux 包级门禁会逐份对照。

`histogram` 在复制和遍历输入前检查 `bins`；非法箱数立即报参数错误，
不因输入规模或输入复制时的分配失败改变错误类别。

`variance`/`stdev` 使用总体定义（除以 `n`）；`Sample` 版本除以 `n-1`。
空数组、无效类型或不满足运算要求时抛出 XLang 异常。`count` 则返回数值元素个数，
非数组输入返回 0。XLang 侧除 `count` 外返回 `float`，`quartiles` 和 `linearFit`
返回多个 `float`，`histogram` 返回计数数组。

有限输入若产生无法用有限 `float64` 表示的统计结果（如两个最大浮点数的和），
也会报错。均值、求和和分位数避免先计算可能溢出的中间和/差；
当直接累加不会溢出时，它们也避免逐项缩放导致次正规数提前下溢。
均值在接近 `float64` 上限时与求和共用缩放补偿累加，但在恢复尺度前
除以元素数，并把舍入结果限制在输入范围内；三个 `DBL_MAX` 的均值
仍为 `DBL_MAX`，大数抵消后的次正规余量也不会被逐项更新抹掉。
两个最小正浮点值的求和、均值与分位数已在 C/XLang 边界测试中逐值校验。
求和在直接累加与极值缩放两条路径均使用补偿算法，保留如
`[1e16, 1, -1e16]` 和 `[DBL_MAX, 1, -DBL_MAX]` 抵消后的小项。
极值路径另将缩放后会进入次正规区的原值单独累加，使两个最小
次正规值也能在极值抵消后保留；仍不承诺任意输入的正确舍入。
`stdev` 和 `stdevSample` 直接按归一化偏差计算，不要求方差先可表示；
例如 `[-1e308, 1e308]` 的方差不可表示，但总体与样本标准差都可表示。
`variance`、`varianceSample` 和 `mad` 同样先缩放有限偏差，避免中间平方和或绝对值总和溢出；
最终结果确实超出 `float64` 范围时仍报错。
`mad` 对绝对偏差复用稳健均值计算，两个最小次正规偏差的平均值不会因逐项先除以样本数而归零。
`harmmean` 先按最小绝对输入缩放倒数项，使 `1e-320` 这样的有限非零输入不因直接取倒数溢出；
缩放后的倒数使用补偿求和，保留 `[1e-16, 1, -1e-16]` 抵消后的有限余项；
倒数和确实为零时仍属于未定义结果。
直方图在极值跨度溢出时按等比例缩放后定位分箱。
协方差、相关系数和线性拟合共用归一化偏差计算，避免有限相关性或斜率
被中间平方和溢出误判。线性拟合的截距在 `slope * mean(x)` 中间乘积
溢出而最终结果有限时仍会返回有限结果；斜率或截距确实超出 `float64`
范围则报错。
`min`、`max`、`range` 只扫描输入，不复制整份 `double` 数组；除返回值本身外使用 O(1) 额外内存。三者仍逐项验证数值类型与有限性，极值差不可表示时 `range` 报错。

对应 C API 见 `statistics_native.h`。除 `xstat_count` 外，C 函数返回
`xvalue*`；结果需用 `xrtValueRelease` 释放。失败时返回 XRT null 值并设置
XRT 错误，调用方可用 `xrtTakeError` 取走；参数/定义域错误为 `XERR_VALUE`，
内存分配失败为 `XERR_MEMORY`，XLang 对应抛出 `ValueError`/`MemoryError`。
不要尝试把失败结果当浮点读取。输入缓冲和输出数组的逐点分配失败已
验证错误类型、清理和下一次调用恢复。
成功调用会清除本线程上一次保留的 XRT 错误；这也适用于独立的 `count`
以及双数组统计函数，失败后可直接重试，无须调用方先手动清错。

## 验证

在 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/statistics/test/check.ps1 -SkipAot
bash release/extlibs/statistics/test/check.sh
```

去掉 `-SkipAot` 会增加独立可执行文件验证。脚本只跑本包 C/XLang 测试，
并对照 `examples/basic.xl` 与 `examples/statistics_c.c`、
`examples/trend.xl` 与 `examples/trend_c.c`、
`examples/distribution.xl` 与 `examples/distribution_c.c` 三组成对范例的
共同黄金输出。三份 C 范例均自包含统计实现，可在 `demo6` 目录直接运行：

```powershell
release/xl.exe run release/extlibs/statistics/examples/statistics_c.c
release/xl.exe run release/extlibs/statistics/examples/trend_c.c
release/xl.exe run release/extlibs/statistics/examples/distribution_c.c
```

包门禁同时对照 GCC 编译运行、`xl run` 和 XLang 范例的输出。
趋势范例展示分位数、多返回值线性拟合、相关系数与直方图；相关系数
在展示时四舍五入到整数，避免两种语言默认浮点打印精度不同。
Linux 脚本使用严格 GCC + ASan/UBSan 跑独立 C 边界测试与 C 范例，
同时需要 `release/xl` 运行包内 XLang 边界、分配失败测试和范例；
C/XLang 范例与黄金输出逐字比较。Windows 和 Linux 门禁均检查
manifest 传入 XRT 模块宏时的严格 GCC 编译。
公开 C 接口的 `count` 结果与 `histogram` 箱数使用 `int64_t`，和 XLang `int` 在 LP64 平台保持一致。
包清单现只声明直接使用的 `XRT_MODULE_VALUE_CONTAINER`；C 故障注入宿主
额外启用 `XRT_MODULE_MEMORY_DEBUG`。Windows GCC 下该宿主的 XRT 对象
从完整模块的 3,362,091 字节降到 266,005 字节，C/XL 定向门禁和 Linux
sanitizer 已复跑；这不是最终 AOT 产物大小。
当前完整 AOT 在 `xl6ValueIndexReadCopyV1` 缺符号处阻断，详见
`dev/extlib_blockers/value_index_link.md`；新建的 `test/aot_stdev.xl` 也可复现此阻断，
本包的 C/JIT 门禁不代表完整发布验收。
2026-10-15：`min/max/range` 改为共用无临时数组的极值扫描；单个浮点结果分配失败由共用入口返回 XRT null 值并保留内存错误，不再把空指针交给 C 调用者。C 无效元素、结果分配故障/恢复、XLang `MemoryError` 与双语新增极值黄金输出通过；Windows 包门禁和 Linux ASan/UBSan 通过。完整 AOT 阻断不变。

2026-10-16：三个 `DBL_MAX` 的均值原会在逐项除法后相加时舍入为无穷大，
并使依赖均值的常量数组方差、标准差和 MAD 失败；C 用例先红后绿，
XLang 极值边界同步通过。Windows C/XL 包门禁、双语黄金输出及 Linux
ASan/UBSan 通过；语言边界和分配故障用例已登记到目录 smoke。
完整 AOT 仍受共享 `xl6ValueIndexReadCopyV1` 阻断。

同日补直方图错误优先级：非法箱数原先先分配整份输入，故障注入时
误报内存错误。C/XL 用例现在验证箱数先检查、解除故障后可正常分箱；
C 边界用例以 `DBL_MIN * DBL_EPSILON` 表示最小次正规值，兼容 TCC。
Windows 严格 GCC/C/XL、TCC C 边界与 Linux ASan/UBSan 通过，完整 AOT
共享阻断未变。

2026-10-16：Linux XLang 导入曾因 `count` 的 `long long` 与生成的 `int64_t` 原型冲突而失败；两个整数跨界签名统一为 `int64_t`，新增严格 ABI 头文件检查。完整 AOT 阻断不变。

同日修复 C 范例在 LP64 上用 `%lld` 打印 `int64_t` 的严格编译失败，
改用标准 `PRId64`；Linux 门禁纳入 C 范例、XLang 两组边界及
双语同输出。Windows `-SkipAot` 与 Linux ASan/UBSan + XLang 包级门禁均通过，
完整 AOT 的共享缺符号阻断不变。

2026-10-17：补齐 `mode` 并列众数和全部唯一值的 C/XLang 对照断言，
确认均返回数值最小者；包内 README 与已有官网详情页统一口径。
Windows 包级 C/XL、双语黄金输出与 Linux ASan/UBSan 门禁通过。
完整 AOT 共享阻断不变。

同日修复极值求和的小残差丢失：C/XLang 负例先复现
`[DBL_MAX, 1, -DBL_MAX]` 与夹在极值间的次正规值返回零；
缩放项改用补偿求和，缩放后成为次正规值的输入保留原值另行累加。
Windows 包级 C/XLang 与双语黄金输出、Linux ASan/UBSan 联合门禁通过；
完整 AOT 共享缺符号阻断不变。

同日补数组收集时的分配长度检查：`int64` 元素数乘 `sizeof(double)`
先确认不超过 `SIZE_MAX`，避免 32 位平台回绕后少分配。
包内定向 C 测试无需构造巨大数组即可验证正常长度和越界错误；
Windows 严格 GCC/C/XLang、Linux ASan/UBSan 与双语黄金输出通过。
本机未运行 32 位目标，完整 AOT 共享阻断不变。

2026-10-18：调和平均数的缩放倒数原先直接累加，`[1e-16, 1, -1e-16]`
会把有限结果 3 误判为未定义。C/XLang 红色复现后改用已有的补偿累加；
Windows C/JIT/双语黄金与 Linux ASan/UBSan/XL 联合门禁通过。完整 AOT
仍受共享索引链接缺符号阻断。

同日补 `geomean` 原缺失的聚焦回归：C/XLang 均检查 `[2,8]`、两个最小正次正规数、两个最大有限值及含零输入的 `ValueError`/`XERR_VALUE`。Windows 严格 C/JIT/双语黄金与 Linux C ASan/UBSan、XLang 联合门禁通过；现有实现无需修改，完整 AOT 阻断不变。

同日新增趋势分析双语范例，覆盖 `quartiles`、`linearFit`、`correlation`、
`histogram` 的实际组合调用；Windows 与 Linux 包级门禁均对比同一黄金输出。

同日修复几何平均数极值舍入：100 个相同 `DBL_MAX` 的输入原在
`sum(log(x)) / n` 后因浮点舍入使 `exp` 溢出，尽管真实结果仍为有限值。
返回值现限定在正输入的最小值与最大值之间；普通计算路径保留，
`geomean([2, 8]) == 4` 的现有精确断言不受影响。C/XLang 极值负例
先复现后修复，Windows 严格 C/JIT 和 Linux C ASan/UBSan、XLang
联合门禁通过；完整 AOT 的共享缺符号阻断未变。

2026-10-19：C 边界新增 512 组固定种子、有符号短整数数组的独立模型
对照。同一组输入逐项核对计数、和、均值、中位数、并列众数、极值、
总体/样本方差、25%/75% 分位数和七箱直方图；参考值由测试侧整数
累计、插入排序和整数分箱计算，不调用待测统计函数。Windows 严格
C/XLang 与双语黄金门禁、Linux C ASan/UBSan 和 XLang 联合门禁通过。
实现无需修改；本轮未重试完整 AOT，既有共享链接阻断不据此关闭。

2026-10-20：极值均值的逐项更新会丢失 `[DBL_MAX, 3 * DBL_TRUE_MIN,
-DBL_MAX]` 的次正规余量。C 回归先红后绿；均值现与求和共用缩放补偿累加，
在恢复尺度前除以样本数，避免中间总和溢出。C/XLang 均覆盖余量在极值
中间与末尾的两种排列；Windows 严格 C/双语黄金与 Linux C ASan/UBSan、
XLang 门禁通过。完整 AOT 的共享缺符号阻断未重验，不据此签收发布级。

同日 Windows 包门禁改为每次在独占子目录重新编译 XRT 和测试目标，
成功或失败退出均只清理本次产物。`-SkipAot` 全通过；默认门禁在边界
AOT 构建处仍缺共享 `xl6ValueIndexReadCopyV1`，失败后无本次目录残留。

2026-10-22：`histogram` 原在遍历每个样本时重建一个 `xvalue` 箱计数，
分配次数随样本数增长。现先用原生 `int64` 数组计数，仅在返回时为每个箱
创建一次值；分箱公式、API 与输出不变。4096 样本/4 箱的定向故障注入
先复现第 64 次分配失败，改后通过；另扫描 20 个分配故障点并确认恢复。
既有 512 组整数参考对照、Windows 严格 C/XLang/双语黄金和 Linux C
ASan/UBSan、XLang 对照门禁均通过。完整 AOT 仍受共享主线阻断，
本轮不签收整包发布级。
