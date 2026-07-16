# dev 实战验证项目群

## 目录
- [dev 实战验证项目群](#dev-实战验证项目群)
  - [目标](#目标)
  - [目录约定](#目录约定)
  - [运行与验证](#运行与验证)
  - [xlang 关键语法速查（开发时务必遵守）](#xlang-关键语法速查（开发时务必遵守）)
  - [已确认 BUG 与绕过方式（第 1 批发现，修复中）](#已确认-bug-与绕过方式（第-1-批发现，修复中）)
  - [已修复 BUG（不再需要绕过，以下能力已正常）](#已修复-bug（不再需要绕过，以下能力已正常）)
  - [仍需注意的限制](#仍需注意的限制)
  - [BUG 记录](#bug-记录)
    - [BUG-<编号>: <简短标题>](#bug-编号-简短标题)

本目录通过编写一批真实可用的小工具，持续验证 xlang 语言能力、发现 BUG 和手感问题。

## 目标

- 用真实项目压测 xlang 各项特性（语法、类型、标准库、并发、持久化、C 混编）。
- 发现 BUG 和手感不正确的地方，记录到 `BUGS.md`。
- 工具本身要能跑、能用，不是玩具占位。

## 目录约定

每个工具一个子目录：

```
dev/<tool_name>/
    <tool_name>.xl      主源码
    README.md           说明（可选，有 BUG 时必写）
    *.c                 C companion（如需 native-backed class）
```

## 运行与验证

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\<tool_name>\<tool_name>.xl
xl build projects\dev\<tool_name>\<tool_name>.xl -o projects\dev\<tool_name>\<tool_name>.exe
```

每个工具必须同时通过 `xl run` 和 `xl build` 两条路径。

## xlang 关键语法速查（开发时务必遵守）

- 输出：`print(args...)` 不加换行；`println(args...)` 加换行。参数间自动加空格。
- 函数返回类型用 `as`，不是 `->`：`func f(int a) as int`。
- 语句分隔：换行或 `;`。`()`/`[]`/`{}` 内可换行。
- 控制流：`if ... then ... elseif ... else ... end`；`for x in ... ... end`（**无 `do`**）；`while cond do ... end`（**必须有 `do`**）；`do ... while cond`。
- 类型：`var` = 动态类型；`auto` = 类型推断；无 `void`。
- 容器字面量：`[1,2,3]` = array；`{}` = dict；`array[...]`/`list[...]`/`dict{...}`/`set{...}` 显式。
- 转换：`int(expr)`、`float(expr)`、`string(expr)` 等构造函数，不是 `as`。
- `as` 是运行时类型检查向下转换（checked downcast），不是数值转换。
- 字符串拼接用 `+`。
- 已知限制：函数参数内的数组元素二元表达式可能需要先提取到变量。
- 标准 API 入口参考：`D:\GIT\x-lang\DEMO6功能清单.md` 第 14 节。
- 范例参考：`D:\GIT\x-lang\demo6\release\examples\` 下的 `.xl` 文件。

## 已确认 BUG 与绕过方式（第 1 批发现，修复中）

## 已修复 BUG（不再需要绕过，以下能力已正常）

以下 BUG 均已修复并通过验证，**可以直接使用对应写法**：

- derive json 序列化容器内 class 实例 ✅
- array append/push/insert/remove/removeValue/contains/indexOf/clear/reverse/pop ✅
- array `+` array 拼接 ✅
- `Type<T>[...]` / `Type<T>{...}` 构造表达式（`array<int>[1,2,3]`、`set<int>{1,2}`）✅
- `dict.remove(d, key)` / `d.remove(key)` ✅
- `list<T>` json.stringify ✅
- try/catch 内 return/break/continue ✅
- 数组元素二元表达式在函数参数内 ✅
- `type(expr)` 可进 println / string() ✅
- xl run 透传 main 精确返回码 ✅
- build exe 支持 UTF-8 argv ✅
- class 方法体内读取 list/array 字段返回正确值 ✅
- 容器（array/list）作函数参数正常传递 ✅
- `func f(var d)` 可绑定 array 实参 ✅
- Buffer/codec.percent/hex 在 build 正常 ✅
- async 函数内读全局容器 ✅
- `func f() as var` 可 `return float(...)` ✅
- list<string> swap 不再别名缓冲 ✅
- `int.format` 负数非十进制保留符号 ✅
- `T x`（无括号）触发无参 `create()` ✅
- `json.parse` compact 模式保留键序 ✅
- json key order in compact stringify ✅

## 仍需注意的限制

| 限制 | 说明 |
|---|---|
| **无 stdin API** | 交互式工具用命令行参数驱动，不能从控制台读输入 |
| **native-backed class 不可按值传参** | 设计决策：带 destroy 的不可复制资源；操作逻辑内联到单函数 |
| **json pretty 用 TAB 缩进** | XRT 底层未提供配置机制；需 2 空格时 `string.replace(s,"\t","  ")` |
| **json pretty 键序可能 hash 重排** | compact 模式保序，pretty 模式仍可能重排 |
| **`string.pop()` 链接错误** | `xl6_xrt_value_to_string_release` 未解析；int/float pop 正常 |
| **auto 无法从 pop() 推断** | `auto p = a.pop()` 报错；用显式类型 `int p = a.pop()` |
| **list 无 append** | list 是 int64 稀疏存储，用 `li[index] = value` 扩容 |
| **`#` 不是注释** | `//` 才是注释；`#` 开启时间字面量 |

**正向验证通过**（可放心使用）：class 递归方法、generator、异常 throw/catch（含 return）、derive json（含容器）、random/crypto/math/time/string/dir/file/regex API、UTF-8 文件名、递归函数、var 字典字段访问、class 运算符/索引/属性协议、async/await/task.all、TCP/HTTP 网络、HttpClientRequest 全流程、float 全精度、`while cond do ... end`。

## BUG 记录

所有发现的 BUG / 手感问题统一记录到 `BUGS.md`，格式：

```
### BUG-<编号>: <简短标题>
- 工具：<tool_name>
- 复现：<最小复现代码或步骤>
- 现象：<实际行为>
- 期望：<期望行为>
- 绕过：<当前用的绕过方式>
- 严重度：高/中/低
```
