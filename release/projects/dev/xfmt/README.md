# xfmt - xlang 代码格式化器（gofmt 风格）

通过 `xl parse <file.xl> --format json` 获取 AST，递归遍历并按规范重新输出 .xl 源码。

## 用法

```bat
cd /d D:\GIT\x-lang\release

:: 格式化后输出到 stdout
xl run projects\dev\xfmt\xfmt.xl -- projects\dev\<tool>\<tool>.xl

:: 原地覆写
xl run projects\dev\xfmt\xfmt.xl -- projects\dev\<tool>\<tool>.xl --write

:: 只检查（已格式化返回 0，否则返回 1 并报告首处差异）
xl run projects\dev\xfmt\xfmt.xl -- projects\dev\<tool>\<tool>.xl --check

:: 自带用例
xl run projects\dev\xfmt\xfmt.xl -- test

:: build 出独立 exe 后用法相同
xl build projects\dev\xfmt\xfmt.xl -o projects\dev\xfmt\xfmt.exe
projects\dev\xfmt\xfmt.exe <file.xl> [--write|--check]
```

## 规则

- **4 空格缩进**；`end` / `then` / `do` / `else` / `elseif` / `finally` 与对应关键字在同一列对齐。
- **运算符周围空格**：`a + b`、`n = 5`、`x == y`、`self.x = self.x + 1`。
- **空行控制**：顶层声明之间 1 空行；class 方法之间 1 空行；连续 field 之间不空行；函数体内无连续空行。
- **超长签名换行**：函数/方法签名 > 80 列且参数 ≥ 2 时，按「每参数一行」展开（无尾随逗号），闭括号与 `as` 同处一行。
- **main 入口**：`func main(...) as int`（xl check 拒绝展开后的 `array<var> arg`，xfmt 强制还原为 `...`）。
- **运算符优先级**：按需补括号，保证 `(a + b) * c`、`a - (b - c)`、`not (a == b)` 等语义不丢。
- **多返回 / 多赋值 / 多变量声明**：`return a, b`、`a, b = f()`、`auto x, y = f()`。
- **修饰符还原**：从 `flags` 位图还原 `public/private/protected/static/override/final/virtual/native/async/extern`。
- **完整覆盖 AST 节点**：if/elseif/else、while/end、do/while/end、for、try/catch/finally、return/throw/break/continue/assert/yield、anon_func（含 `=>` 箭头体）、typed literal（`array[1,2,3]`/`dict<int>{...}`/`set<int>{...}`/`list[0:0]`）、`is`/`as`、`::<T>` 泛型实例化、`obj?.safe`/`obj?[i]`、f-string、`#doc/#end` 文档块。

## 已知限制（AST-based 固有）

- **行注释 `//` 会丢失**：xl parse 不保留行注释；`--write` 会移除它们（建议先用 `--check` 预览差异）。
- **`#doc ... #end` 文档块原样重发**到对应声明上方。
- **表达式括号按运算符优先级保守重加**：源码中冗余的括号可能被去掉，必要的括号会被补上。
- **格式化结果可能改变字面量写法**（如 `0x10` → 与 AST text 一致的形式），但语义不变。

## 自带用例（cases/）

| 文件 | 说明 |
|---|---|
| `spaces.xl` | 已格式化的运算符空格样本（--check 应通过） |
| `indent.xl` | 已格式化的 if/end 缩进样本 |
| `messy.xl` | 各种缺空格 / 错缩进 / 错位 end 的「脏」样本（--check 应失败，--write 后通过 xl check） |

## 验证

xfmt 已对 dev 目录下 30+ 真实工具源码做 round-trip 验证（format → xl check 通过）：
calc / tree / ts / todo / passgen / utf8count / jsonfmt / stats / sorter / convert /
envinfo / genid / hashcalc / geo / diff / mdtoc / bigint / timer / cal / guess /
contacts / codec / csvjson / duptop / jq / httpreq / psearch / portscan / sudoku /
xlint（1000+ 行，最复杂）。

`matrix.xl` 已使用当前属性/方法命名口径，可正常参与 round-trip；格式化器自测同时
覆盖幂等性、`--check`、`--write` 和格式化后 `xl check`。

## 幂等性

`xfmt` 的输出再次过 `xfmt` 完全相同（已在 xlint.xl 1000+ 行源码上以 SHA-256 验证）。
