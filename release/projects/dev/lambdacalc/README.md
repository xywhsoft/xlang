# lambdacalc - 纯 Lambda 演算解释器

用 xlang 编写的纯 Lambda 演算（untyped λ-calculus）解释器。所有 Lambda 项用 `var` 承载
（`Var` / `Abs` / `App` 三种构造子）。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\lambdacalc\lambdacalc.xl eval "(λx.x) y"
xl run projects\dev\lambdacalc\lambdacalc.xl reduce "(λx.λy.x) y" 100 normal
xl run projects\dev\lambdacalc\lambdacalc.xl demo
xl run projects\dev\lambdacalc\lambdacalc.xl test
xl build projects\dev\lambdacalc\lambdacalc.xl -o projects\dev\lambdacalc\lambdacalc.exe
```

## 命令

| 命令 | 说明 |
|---|---|
| `eval "(expr)"` | 解析 + 归约到正规式（normal order） |
| `reduce "(expr)" [max=1000] [normal\|applicative]` | 逐步归约，打印每一步 |
| `demo` | Church 编码演示（数字 / 后继 / 加 / 乘 / 布尔 / Y） |
| `test` | 内置测试套件 |

## 语法

```
x                  变量
(λx.M)             抽象    （反斜杠 \ 也接受）
(M N)              应用    （左结合：M N P = ((M N) P)）
(λx y.M)           多参糖 = (λx.(λy.M))
(let x = M in N)   语法糖 = ((λx.N) M)
```

## 核心机制

- **α-变换**：代换遇捕获时自动换名（`freshVar` 生成 `base0/base1/...`）。
- **β-归约**：捕获避免代换 `subst(x, M, N)`。
- **normal order**：最左最外红式优先（λ 体内部也会归约）。
- **applicative order**：先归约函数位与参数位，对 Y / Ω 发散。

## 前导库（eval/reduce 中可直接引用）

`ZERO ONE TWO THREE FOUR FIVE` · `SUCC PLUS MULT` · `TRUE FALSE IF` · `ISZERO`
· `PAIR FST SND PRED` · `Y FACT`

后定义者引用先定义者，由 `subst` 内联为闭项。

## 验证的 xlang 能力

递归下降 Parser（λ / 应用 / let 糖 / 多参糖）、α-变换与捕获避免代换、
normal/applicative 双归约策略、Church 编码、Y 组合子、归约步骤追踪、
var 动态类型承载多态项、深递归。
