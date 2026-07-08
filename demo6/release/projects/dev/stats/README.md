# stats - 统计计算器

对一组数字计算常用统计指标。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release

:: 命令行直接给数字
xl run projects\dev\stats\stats.xl -- 1 2 3 4 5 6 7 8 9 10
xl run projects\dev\stats\stats.xl -- 3.14 2.72 1.41 1.73

:: 从文件读数字 (每行一个，空行自动跳过)
xl run projects\dev\stats\stats.xl -- -f projects\dev\stats\data.txt

:: 编译为 exe
xl build projects\dev\stats\stats.xl -o projects\dev\stats\stats.exe
projects\dev\stats\stats.exe 1 2 3 4 5 6 7 8 9 10
```

## 输出

```
Count:    10
Sum:      55
Mean:     5.5
Median:   5.5
Min:      1
Max:      10
Range:    9
Variance: 8.25
StdDev:   2.872281323269014
SumSq:    385
```

字段说明：

| 字段 | 公式 |
|---|---|
| Count | n |
| Sum | Σ xi |
| Mean | Sum / n |
| Median | 排序后取中位 (n 偶数取中间两项均值) |
| Min / Max | 排序后首尾 |
| Range | Max - Min |
| Variance | Σ(xi - Mean)² / n (总体方差) |
| StdDev | sqrt(Variance) |
| SumSq | Σ xi² |

## 实现要点

- 数字一律按 `float` 收集。容器收集用 `list<float>` + 索引赋值扩容，绕过 **B2** (array 无 append)。
- `math` 标准库**没有** `sum`/`mean`，所有聚合手写 `while` 循环。
- 中位数需要排序：用原地选择排序 (无需额外数组)。
- 输出整数落点的浮点值显示为整数 (`5.0` → `5`)，由 `fmtNum` 处理；列对齐由 `string.padRight(label, 10)` 处理。
- 命令行参数经 `arg[i]`/`arg.length` 取；文件经 `file.readText` + `string.splitLines` + `string.trim`。

## 已知限制 / 触发 BUG

- **BUG-11** (高)：把 `array`/`list` 作为参数传给用户自定义函数时，函数内容器 `length=0` 且下标读崩溃。本工具因此把排序、聚合全部内联进 `main`，仅保留 `string` 形参的辅助函数。详见 `../BUGS.md`。
- **BUG-12** (中)：`var`/无类型形参无法接收带类型容器实参。
- 其余遵循 `../README.md` 已确认 BUG (B5: built exe 仅支持 ASCII argv，故文件路径用 ASCII)。
