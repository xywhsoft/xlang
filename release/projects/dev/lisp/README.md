# lisp — Lisp/Scheme 解释器

用 xlang 从零编写的 Lisp/Scheme 解释器：递归下降 Reader + 递归求值器 + 词法闭包。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\lisp\lisp.xl -- run projects\dev\lisp\demo.lisp
xl run projects\dev\lisp\lisp.xl -- test
xl build projects\dev\lisp\lisp.xl -o projects\dev\lisp\lisp.exe
lisp.exe run demo.lisp
lisp.exe test
```

## 数据模型

| Lisp 值 | xlang 表示 |
|---|---|
| nil / 空表 | `null` |
| 整数 / 浮点 | `int` / `float` |
| 字符串 | `string` |
| 布尔 #t/#f | `bool` |
| 符号 | `Sym` 类（derive value） |
| 列表 (a b c) | `var` 数组（空表用 null） |
| 闭包 | `Closure` 类（params/body/env） |
| 内建函数 | `Builtin` 类（按 name 派发） |
| 环境 | `var` 数组 `[parent, k1, v1, k2, v2, ...]`（后向线性查找） |

## 支持的特性

- **Reader**：S-表达式、`'expr` quote、整数/浮点/字符串/符号/布尔/nil、`;` 注释
- **特殊形式**：`quote if define set! lambda let begin cond and or`
- **内建函数**：`+ - * / = < > <= >= car cdr cons list null? pair? number?
  display newline map filter reduce length append reverse not eq? equal?
  boolean? string? symbol? zero?`
- **闭包**：lambda 捕获定义期环境，支持 set! 改闭包状态（make-counter 模式）

## 设计要点

- **环境用 var 数组而非 dict**：递归中分配 var dict 会触发 xlang 的二次方 GC 劣化
  （见 BUG-LP1），改用扁平 var 数组 + 后向线性查找后 fib(25) 从 >7 分钟降到 ~8s。
- **闭包调用快速路径**：eval 内联闭包应用，实参直接求值进调用环境，省去 args 数组。
- **display 自动换行**（简化实现，使 `(display x)` 各自占一行）。

## 压测结果

| 测试 | 结果 | 耗时 |
|---|---|---|
| `(factorial 20)` | 2432902008176640000 | < 1ms |
| `(fib 25)`（242785 次递归调用，3M 次 eval） | 75025 | ~8s |
| 内置测试 31 项 | 全 PASS | ~9s |
