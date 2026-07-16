# radix - 进制转换器

在各种进制（2 / 8 / 10 / 16）之间转换数字，附带 ASCII 码查询与批量转换。
用于验证 xlang 整数位运算、int64 大数算术与 `int.format` 字符串格式化。

## 用法

```
radix <value> <from_base> <to_base>      单值转换
radix ascii <char>                       字符 -> ASCII 码
radix chr <code>                         ASCII 码 -> 字符
radix batch <from> <to> <v1> <v2> ...    批量转换
radix probe                              探测 int.format 行为
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
- **交叉对照**：每次转换都同时调用 `int.format(value, format)`（format ∈ `""`/`"o"`/`"b"`/`"X"`），手写结果与标准库不一致时打印 `[note]`。
- `chr` 使用 `int.format(code, "c")` 生成字符，并把输入限制在可打印 ASCII 码点 32..126。

## 边界

- `chr` 只接受可打印 ASCII（32..126）；控制字符会返回清晰提示。
- `parseRadix` 使用 `int64`，超出有符号 64 位范围的输入不属于本工具范围。
- `int.format(-255, "X")`、`"o"`、`"b"` 均保留负号，已由 `probe` 命令验证。

## 运行 / 构建

```
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\radix\radix.xl -- 255 10 16
xl build projects\dev\radix\radix.xl -o projects\dev\radix\radix.exe
```
