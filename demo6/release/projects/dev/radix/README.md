# radix - 进制转换器

在各种进制（2 / 8 / 10 / 16）之间转换数字，附带 ASCII 码查询与批量转换。
用于验证 xlang 整数位运算、int64 大数算术与 `string.formatInt` 字符串格式化。

## 用法

```
radix <value> <from_base> <to_base>      单值转换
radix ascii <char>                       字符 -> ASCII 码
radix chr <code>                         ASCII 码 -> 字符
radix batch <from> <to> <v1> <v2> ...    批量转换
radix probe                              探测 string.formatInt 行为
radix -h | --help                        帮助
```

支持的进制：`2`、`8`、`10`、`16`。hex 输入大小写均可（`FF` / `ff` 等价）；hex 输出统一大写。

## 示例

```
xl run projects\dev\radix\radix.xl -- 255 10 16        -> FF
xl run projects\dev\radix\radix.xl -- FF 16 10         -> 255
xl run projects\dev\radix\radix.xl -- ff 16 10         -> 255
xl run projects\dev\radix\radix.xl -- 10101010 2 16    -> AA
xl run projects\dev\radix\radix.xl -- 255 10 2         -> 11111111
xl run projects\dev\radix\radix.xl -- 255 10 8         -> 377
xl run projects\dev\radix\radix.xl -- ascii A          -> char='A'  decimal=65  hex=41  binary=1000001
xl run projects\dev\radix\radix.xl -- chr 65           -> A
xl run projects\dev\radix\radix.xl -- batch 10 16 1 2 3 4 5   -> 1,2,3,4,5
```

## 设计要点

- `parseRadix(s, base)` 手写源进制解析：`string.byteAt` 逐字节取字符，`'0'-'9'`/`'A'-'F'`/`'a'-'f'` 映射为数码值，`int64` 累乘累加（验证 `*`、`+` 与 int64 大数）。前导 `+`/`-`、hex 大小写均支持；非法字符或数码 ≥ base 抛 `throw`。
- `formatRadix(value, base)` 手写目标进制格式化：利用 C 截断除法语义（`x % base` 与 `x` 同号、`x / base` 向 0 靠拢），**对负数（含 INT64_MIN）无需取绝对值**即可正确产出带符号结果，避免 `-INT64_MIN` 溢出。
- `formatBinaryBitwise(value)` 用 `mag & 1`、`mag >> 1` 逐位剥离做二进制格式化（验证位运算 `&` `>>`）；INT64_MIN 走硬编码串兜底。
- **交叉对照**：每次转换都同时调用 `string.formatInt(value, format)`（format ∈ `""`/`"o"`/`"b"`/`"X"`），手写结果与 formatInt 不一致时打印 `[note]` 行——这是发现 BUG-18 的关键。
- `chr` 命令查 `ASCII_TABLE`（码点 32..126 的可打印 ASCII 字符串）实现，因 `string.formatInt(code, "c")` **不是**字符格式（见 BUG-18 备注）。

## BUG / 限制

- **BUG-18**（本工具发现）：`string.formatInt` 对负数使用非十进制 format（`x`/`X`/`o`/`b`/`B`）时**丢符号**——`formatInt(-255, "X")` 返回 `FF` 而非 `-FF`。本工具的手写 `formatRadix` 不受影响，并在转换负数时打印 `[note]` 提示差异。详见 `../BUGS.md`。
- **BUG-19**（本工具发现）：当前环境 `xl build` 对所有程序（含 `println("hello")` 最小用例、已知良好的 primes/calc）均失败，根因在 `xrt.h` 在 build 模式 trim 宏组合下编译报错（`xrtGlobalData xCore` 处 `';' expected` / `__xrt_sNullBytes` 重定义）以及 `xl6_command_argv_to_arg` 链接未解析。**非本工具代码问题**，且修复需改动编译器/runtime 源码（任务禁止）。`xl run` 路径全功能通过。
- 已知限制：`chr` 只支持可打印 ASCII（32..126）；控制字符打印不出来（输出提示信息）。

## 运行 / 构建

```
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\radix\radix.xl -- 255 10 16
xl build projects\dev\radix\radix.xl -o projects\dev\radix\radix.exe     :: 当前环境失败，见 BUG-19
```
