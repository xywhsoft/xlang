# bench - 微型基准测试工具

用 `time.timer()` 前后取差，测量 xlang 各项基础操作的吞吐。

## 用法

```
xl run bench.xl -- loop   [N]   空循环开销      (默认 1000000)
xl run bench.xl -- math   [N]   数学运算        (默认 1000000)
xl run bench.xl -- string [N]   字符串拼接      (默认 100000)
xl run bench.xl -- array  [N]   数组 append     (默认 100000)
xl run bench.xl -- json   [N]   JSON parse+stringify (默认 10000)
xl run bench.xl -- all          依次运行全部
```

构建独立 exe：

```
xl build bench.xl -o bench.exe
bench.exe all
```

## 输出

每行输出：迭代数 N、总耗时(ms)、单次操作耗时(us/op)、每秒操作数(ops/sec)。

```
xlang micro benchmark
================================================================
  loop    n=1,000,000  time=6.103 ms  0.0061 us/op  163,859,214 ops/sec
  math    n=1,000,000  time=61.259 ms  0.0613 us/op  16,323,998 ops/sec
  string  n=100,000  time=2165.453 ms  21.6545 us/op  46,179 ops/sec
  array   n=100,000  time=27.619 ms  0.2762 us/op  3,620,748 ops/sec
  json    n=10,000  time=5360.030 ms  536.0030 us/op  1,865 ops/sec
================================================================
checksum 1950014.786 (防优化累计值)
```

## 设计说明

- 计时用 `time.timer()`（单调秒，float），差值换算 ms / us / ops。
- 每个 bench 函数只做"计时 + 循环"，结果通过全局 `float g_sink` 累计，程序末尾打印 checksum，避免死代码消除影响测量。
- `string` 拼接为 O(n²) 复制（预期行为，反映 xlang 字符串值语义），故默认 N 较小。
- 所有循环用 `while` + 计数器，保证 `loop` 项循环体可空（仅 `i += 1`）。

## 限制 / 注意

- `arg` 仅在 `func main(...)` 内可用（见 dev `BUGS.md` NOTE-22/23）。在非 main 函数里访问 `arg` 报 `[checked] class field receiver is not a class value`（诊断反直觉），故 N 解析内联在 main 中。
- 微基准受机器负载影响，单次运行数值会波动；关注数量级与相对关系即可。
