# genetic - 通用遗传算法求解器

用一套通用 GA 引擎求解三类经典问题，验证 xlang 的泛型 class、一等公民函数/闭包、
var 动态容器与 typed array 语义。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\genetic\genetic.xl onemax 64
xl run projects\dev\genetic\genetic.xl tsp    projects\dev\genetic\cities.json 500 100
xl run projects\dev\genetic\genetic.xl func   5 300
xl run projects\dev\genetic\genetic.xl test
```

| 命令 | 说明 |
|---|---|
| `onemax [N=100]` | 最大化二进制串中 1 的个数（验证算法正确性） |
| `tsp <cities.json> [gens=500] [pop=100]` | 旅行商问题，找最短回路 |
| `func [D=5] [gens=300]` | 最大化 f(x) = sum(xi·sin(xi))，xi∈[0,10] |
| `test` | OneMax/TSP/FuncOpt 自检（PASS/FAIL） |

## 设计

- `Individual`：`var chromosome`（承载 `array<int>` 或 `array<float>`）+ `float fitness`。
- `gaRun(...)` 通用引擎：初始化 → 评估 → 选择（轮盘赌/锦标赛）→ 交叉 → 变异 →
  精英保留 → 迭代。引擎对染色体类型透明，仅搬运 `Individual`。
- 每个问题模块提供 5 个策略闭包：`initFn / evalFn / crossFn / mutateFn / cloneFn`，
  闭包内部知道染色体精确类型并**总是构造全新数组返回**（引擎从不就地改写共享染色体）。
- 引擎统一“最大化” fitness：TSP 用 `1/(1+路程)` 把最小化转成最大化。
- 选择：锦标赛（`tournamentSize`）与轮盘赌（自动平移负适应度）。
- 交叉：OneMax 单点、FuncOpt 算术混合、TSP 顺序交叉 OX。
- 变异：OneMax 位翻转、FuncOpt 高斯扰动（Box-Muller）、TSP 段反转（2-opt 风格）。
- 随机种子固定，`xl run` 与 built exe 输出一致。

## 语义注意（见 dev/README.md 限制表）

- typed array 赋值为值拷贝：`fitness/evaluated` 一律用 `pop[i].field = ...` 直接索引
  就地写入；染色体永远重建新数组，避免别名问题。
- `test` 中 TSP 与暴力最优（n=6）比对，并校验回路为合法排列。
