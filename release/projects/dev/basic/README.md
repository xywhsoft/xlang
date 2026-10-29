# basic — BASIC 语言解释器（用 xlang 编写）

一个逐行执行、行号驱动的经典 BASIC 解释器，验证 xlang 的词法分析、递归下降表达式
求值、行号跳转程序计数器、FOR/NEXT 循环帧、动态类型变量与一维数组等能力。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\basic\basic.xl                          :: 运行内置测试套件
xl run projects\dev\basic\basic.xl -- run projects\dev\basic\demo.bas
xl run projects\dev\basic\basic.xl -- test
xl build projects\dev\basic\basic.xl -o projects\dev\basic\basic.exe
basic.exe run <file.bas>     :: 构建产物直接执行
```

## 支持的语句

| 语句 | 说明 |
|---|---|
| `PRINT expr_list` | `;` 直接拼接、`,` 对齐到 14 列制表位；尾部 `;`/`,` 抑制换行 |
| `[LET] var = expr` | 标量赋值，LET 可省略 |
| `[LET] A(i) = expr` | 数组元素赋值 |
| `IF expr THEN line [ELSE line]` | 条件跳转（真=-1，假=0） |
| `FOR v=a TO b [STEP s] ... NEXT [v]` | 含零次迭代与结构化 NEXT 匹配 |
| `GOTO line` / `GOSUB line` / `RETURN` | 跳转与子程序调用栈 |
| `DIM var(size) [, ...]` | 一维数组声明（下标从 0 开始） |
| `REM ...` / `END` / `STOP` | 注释与终止 |

## 表达式与函数

- 运算：`+ - * / MOD`、比较 `= <> < > <= >=`、`AND OR NOT`、括号、字符串 `+` 拼接
- 内建函数：`ABS INT SQR LEN LEFT$ RIGHT$ MID$ CHR$ ASC`（`$` 后缀可省略）
- 比较与逻辑结果遵循经典 BASIC：真=`-1`，假=`0`
- 数字字面量无小数点为 int，否则 float；`/` 为实数除法，整数值结果按整数打印

## 内置测试

`basic test` 运行 33 项断言，覆盖算术优先级、比较/逻辑、全部内建函数，以及 6 个
完整程序：Fibonacci 前 10 项（`0 1 1 2 3 5 8 13 21 34`）、GOSUB/RETURN、DIM 数组、
嵌套 FOR、字符串函数、GOTO+IF 计数。
