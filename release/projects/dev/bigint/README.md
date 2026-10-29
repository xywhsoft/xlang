# bigint - 任意精度整数运算工具

用字符串模拟任意精度十进制整数运算（超出 int64 范围），逐位模拟手工竖式，不依赖任何大数库。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\bigint\bigint.xl -- add 99999999999999999999 1
xl run projects\dev\bigint\bigint.xl -- mul 12345678901234567890 98765432109876543210
xl run projects\dev\bigint\bigint.xl -- pow 2 200
xl run projects\dev\bigint\bigint.xl -- fact 50
xl run projects\dev\bigint\bigint.xl -- fib 100
xl run projects\dev\bigint\bigint.xl -- selftest
```

| 子命令 | 说明 |
|---|---|
| `add <a> <b>` | 大整数加法 |
| `sub <a> <b>` | 大整数减法 |
| `mul <a> <b>` | 大整数乘法 |
| `pow <base> <exp>` | 大整数幂（exp 为非负普通整数，快速幂） |
| `fact <n>` | 阶乘 n!（n 为非负普通整数） |
| `fib <n>` | 斐波那契第 n 项（fib(0)=0, fib(1)=1） |
| `selftest` | 内置自检 |

`a` / `b` / `base` 可为任意长度十进制串，可带前导 `+` / `-`。`exp` / `n` 为普通非负整数。

## 示例

```
bigint add 99999999999999999999 1            -> 100000000000000000000
bigint sub 100000000000000000000 1           -> 99999999999999999999
bigint mul 12345678901234567890 98765432109876543210
                                             -> 1219326311370217952237463801111263526900
bigint pow 2 64                              -> 18446744073709551616
bigint pow 2 200                            -> 1606938044258990275541962092341162602522202993782792835301376
bigint fact 50                              -> 30414093201713378043612608166064768844377641568960512000000000000
bigint fib 100                              -> 354224848179261915075
```

## 设计要点

- 大整数存储为十进制字符串（可选前导符号）；`normalize` 统一去前导零、归零符号。
- `addAbs` / `subAbs` / `mulAbs` 在「纯数字串」上做逐位竖式，借助 `string.reverse` 从低位向高位累加，再用 `string.byteAt - 48` 取数字位。
- `mulAbs` 用竖式：对 b 的每一位调 `mulDigit`（大数 × 单数字），按位补零（`string.repeat("0", j)`）后用 `addAbs` 累加，无需临时数组。
- 带符号四则由 `signOf` / `rawDigits` 拆分符号与量级，比较 `cmpAbs` 决定结果符号。
- `pow` 用平方求幂（O(log exp) 次乘法），`fact` / `fib` 迭代累乘 / 累加。
- `exp` / `n` 是普通 `int`；大数本身绝不进入 int64。

## 验证

`xl run` 与 `xl build` 两条路径均 `EXIT=0`：

```
xl run  projects\dev\bigint\bigint.xl -- selftest   -> selftest OK
xl build projects\dev\bigint\bigint.xl -o projects\dev\bigint\bigint.exe
```

## 注意 / 手感记录

- `~=` 在 xlang 是「近似相等」（`string.approx` 语义），**不是**「不等于」。不相等要用 `!=`。本工具初版误用 `~=` 做「不等」判断，导致 `assertEq("0","0")` 误判失败。详见 `../BUGS.md` BUG-B1。
- `string.byteAt` 按**字节**索引，`string.len` / `string.reverse` 按**字符**索引；本工具只处理 ASCII 数字串，二者一致，可安全混用。
