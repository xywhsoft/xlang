# xrepl - xlang 交互式 REPL（批量脚本模式）

由于 xlang 无 stdin API，本工具用「批量脚本模式」模拟交互式 REPL：读取一个
`.xrepl` 文件（每行一条表达式/语句），逐行执行并打印结果。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\xrepl\xrepl.xl -- <script.xrepl>
xl run projects\dev\xrepl\xrepl.xl -- test
xl run projects\dev\xrepl\xrepl.xl -- -h
```

构建后：

```bat
xl build projects\dev\xrepl\xrepl.xl -o projects\dev\xrepl\xrepl.exe
projects\dev\xrepl\xrepl.exe <script.xrepl>
projects\dev\xrepl\xrepl.exe test
```

> 注意：`xl run <tool>.xl <args>` 必须用 `--` 分隔构建参数与运行期 argv，
> 否则裸参数（如 `test`）会被当作链接库。

## 工作原理

- **变量持久化**：维护一个不断增长的「前缀程序」（已成功执行语句的源码）。
  执行第 N 行时生成 `前缀 + 当前行`，写入临时 `.xl` 文件后 `xl run`。
  每次从头重放已积累的语句，因此前一条语句定义的变量/函数在后一条可用。
- **表达式自动打印**：识别为「表达式」的行包成 `println(expr)`；
  「声明/赋值/控制流/print」原样执行。
- **输出去重**：前缀重放会重复产生历史输出，通过缓存「前缀输出」并取 stdout
  末尾增量，只显示当前行的新输出。
- **多行块**：括号未闭合或 `func/if/for/while/try...end` 未闭合时跨物理行续接。
- **命令历史**：每条执行的逻辑行追加写入 `xrepl_history.txt`。

## .xrepl 示例

```
1 + 2
string.len("hello")
auto x = [1,2,3]
x.length
string.upper("abc")
```

输出：

```
> 1 + 2
3
> string.len("hello")
5
> auto x = [1,2,3]
> x.length
3
> string.upper("abc")
ABC
```

## 注意事项（xlang 限制）

- **字符串无 `.length` 属性**：`"hello".length` 是非法 xlang（编译期拒绝）。
  字符串长度用 `string.len(s)`；`.length` 仅用于 array/容器变量。
  REPL 对此类非法行会显示编译诊断并跳过（不影响后续行）。
- **字面量作接收者**：`[1,2,3].length`、`"x".method()` 等「字面量.成员」
  形式在 xlang 中不支持（接收者必须是变量名）。请先赋值给变量再访问。
