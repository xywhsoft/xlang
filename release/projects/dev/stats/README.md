# stats - 统计计算器

对一组数字计算常用统计指标。

## 用法

```bat
cd /d D:\GIT\x-lang\release

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

- 数字一律按 `float` 收集到 `array<float>`，用 `push` 追加；数组作为参数传给 `sortFloats` 后原地排序。
- `math` 标准库**没有** `sum`/`mean`，所有聚合手写 `while` 循环。
- 中位数需要排序：用原地选择排序 (无需额外数组)。
- 输出整数落点的浮点值显示为整数 (`5.0` → `5`)，由 `fmtNum` 处理；列对齐由 `string.padRight(label, 10)` 处理。
- 命令行参数经 `arg[i]`/`arg.length` 取；文件经 `file.readText` + `string.splitLines` + `string.trim`。

## 验证

`array<float>` 的追加、函数传参与原地修改均使用当前正式语义；命令行输入和文件输入应得到一致结果。
