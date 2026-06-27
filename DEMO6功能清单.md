# DEMO6 功能清单

> 更新日期：2026-06-27
>
> 本清单用于快速核对当前 `demo6/release/xl.exe` 的已实现能力、测试入口和验收边界。
> 本文只描述 DEMO6 当前代码、测试、范例和守门脚本已经覆盖的能力；`demo3`、`demo4`、`demo5`、`dev` 目录中的材料只能作为历史参考，不能作为 DEMO6 设计来源。

## 1. 当前总体状态

DEMO6 当前已经具备完整的四阶段编译管线：

1. 词法分析：Flex。
2. 语法分析：Lemon。
3. 语义检查与 IR。
4. C 代码生成与 TCC 后端构建/运行。

当前可执行文件：

- `demo6/release/xl.exe`

当前已经具备的主流程：

- 读取 UTF-8 `.xl` 源文件。
- 输出词法 token。
- 输出 AST 文本树或 AST JSON。
- 执行语义检查。
- 输出 IR 文本或 IR JSON。
- 输出 ABI 文本或 ABI JSON。
- 输出 XAPI 接口元数据。
- 生成 C 源文件。
- 生成供 C 使用的头文件。
- 直接运行 `.xl` 脚本。
- 构建 Windows exe。
- 构建共享库/DLL 目标边界。
- 混合编译 `.xl` 与 `.c` 文件。
- 自动链接 xlang/XRT/TCC 所需基础运行时。
- 通过内嵌 TCC 与内置 VFS 资源降低外部依赖。
- 提供 DLL 形式的嵌入式运行库。
- 提供标准库默认注册。
- 提供 package/import/XAPI/扩展库基础链路。
- 提供主回归、功能范例、标准库范例和 RC0 工程守门。

主要验证脚本：

- `demo6/build.bat`
- `demo6/test.bat`
- `demo6/test_all.bat`
- `demo6/test_feature_suite.bat`
- `demo6/test_smoke.bat`
- `demo6/rc0_guard.bat`
- `demo6/tools/rc0_guard.ps1`

## 2. 命令行功能

当前 `xl.exe` 支持以下命令：

```bat
xl lex <file.xl> [--format text|json]
xl parse <file.xl> [--format tree|json] [--emit output.ast]
xl check <file.xl> [--json]
xl ir <file.xl> [--format text|json] [--emit output.xlir]
xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]
xl abi <file.xl> [--json] [--emit output.abi.txt]
xl xapi <file.xl> [--emit output.xapi.json]
xl run <file.xl> [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]
xl build <file.xl> -o <output.exe> [-shared] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]
```

命令行为：

- `lex`：输出 token 文本或 JSON。
- `parse`：输出 AST 文本树或 JSON。
- `check`：只执行语义检查，可输出 JSON 诊断。
- `ir`：输出中间 IR 文本或 JSON。
- `cgen`：只生成 C，可同时生成头文件。
- `cgen --dump-dir`：一次性输出 token、AST、IR、ABI、XAPI、C、H 等阶段产物。
- `abi`：输出 ABI 信息。
- `xapi`：输出 `xlang-xapi-1` 接口元数据。
- `run`：生成临时 C，调用 TCC 编译并运行。
- `run -- <args>...`：把命令行参数传给 xlang 程序。
- `run --save-temps`：保留临时构建目录。
- `build`：生成 exe。
- `build -shared`：进入共享库/DLL 构建边界。
- `build --emit-header`：输出供 C 调用的头文件。
- `--inc-path`、`--lib-path`、`--link`、`--loadlib`：向 TCC 后端注入 include、lib、链接库和运行库。

内嵌 TCC 规则：

- `xl.exe` 优先使用自身所在目录及命令行指定路径下的外部文件。
- 外部文件不存在时，才使用内置 VFS 中的必要资源。
- 内置资源使用当前正式虚拟路径，不为旧路径做兼容映射。
- `run` 与 `build` 走同一套 TCC 文件解析和 VFS 规则。

## 3. 源文件与词法

已实现规则：

- 源文件按 UTF-8 处理。
- 支持 ASCII 标识符。
- 支持 UTF-8 标识符，因此可以使用中文标识符。
- 语言大小写敏感。
- 关键字大小写敏感。
- 普通空白被忽略。
- 换行是语句分隔符。
- `;` 也是语句分隔符。
- `()`、`[]`、`{}` 内部换行不结束语句。
- 支持 `//` 单行注释。
- 支持 `/* ... */` 多行注释。
- 多行注释支持嵌套。
- 未闭合注释、字符串、时间字面量会产生词法错误。

已识别关键字：

```text
null true false
func const var auto typedef as
if then else elseif end
for in while do
break continue return
async await yield
try catch finally throw assert
import export extern include define
class custom extends union get let
and or not
```

上下文关键字：

- `self`
- `base`
- `create`
- `destroy`

## 4. 字面量

整数：

```xlang
123
1_000_000
0x2A
0o52
0b101010
```

浮点：

```xlang
1.25
1e3
1.2e-3
```

字符串：

```xlang
"hello"
'hello'
r"c:\temp\a.txt"
r'c:\temp\a.txt'
"""
multi line
"""
'''
multi line
'''
```

时间：

```xlang
#2026-06-18#
#12:30:45#
#2026-06-18 12:30:45#
```

布尔与空值：

```xlang
true
false
null
```

当前边界：

- 普通字符串支持转义。
- raw string 不处理转义。
- 多行字符串支持单引号三引号和双引号三引号。
- 时间字面量内部不能换行。
- 模板字符串语法占位已存在，但表达式插值暂缓实现。
- 数字前缀只保留当前小写规范形式。

## 5. 类型系统

基础类型：

```text
null
bool
int int8 uint8 int16 uint16 int32 uint32 int64 uint64
float float32 float64
string
time
point
var
type
```

C ABI 类型：

```text
c_char c_schar c_uchar
c_short c_ushort
c_int c_uint
c_long c_ulong
c_longlong c_ulonglong
c_size c_ssize
c_ptrdiff
c_intptr c_uintptr
c_bool
c_float c_double
c_wchar
```

容器类型：

```text
array array<T>
list list<T>
set set<T>
dict dict<T>
```

函数值类型：

```xlang
func(int) as int proc
func(string, int) as (int, string) mapper
```

类型相关能力：

- 内置类型通过类型注册系统进入语义层。
- `auto` 是局部推断关键字。
- `var` 是动态值类型。
- `type` 是真实的类型值类型。
- `type(expr)` 可以取得表达式类型值。
- 类型标识符可作为类型值参与比较。
- `typedef` 支持类型别名。
- 泛型容器支持静态元素类型。
- `void` 不属于 DEMO6 类型系统。

## 6. 变量、常量和初始化

基本写法：

```xlang
int a
int b = 1
var v = 123
auto c = b + 1
const int max = 10
```

已实现规则：

- 变量声明采用 C 风格类型前置。
- 支持全局变量和局部变量。
- 支持默认初始化。
- 支持重复声明检查。
- 支持 const 赋值约束。
- 支持 `auto` 推断。
- 支持多返回值解构声明。
- 支持资源类型生命周期检查。

## 7. 函数系统

函数声明：

```xlang
func hello()
    print("hello")
end

func add(int a, int b) as int
    return a + b
end

func pair() as (int, string)
    return 1, "one"
end
```

参数能力：

```xlang
func log(string text, int level = 1)
    print(text)
end

func sum(...)
    return arg.len()
end

func open(string path, ***)
    return arg["mode"]
end

func move(int x, int y)
end

move(y = 2, x = 1)
```

已实现规则：

- 使用 `as` 声明返回类型。
- 无 `as` 表示无返回值。
- 多返回值使用 `as (T1, T2)`。
- 支持可选参数。
- 支持命名参数。
- 支持数组可变参数 `...`，函数体内通过 `arg` 访问。
- 支持字典可变参数 `***`，函数体内通过 `arg` 访问。
- `...` 与 `***` 不能同时出现。
- 命名参数优先匹配已声明形参。
- 未匹配的命名参数进入 `***` 参数字典。
- 可选参数导出到 C 环境时，默认值语义不导出，C 侧按普通参数调用。
- 支持函数值赋值与调用。
- 支持匿名函数与闭包。
- 支持函数重载。
- 支持泛型函数显式实例化。

函数值示例：

```xlang
func add(int a, int b) as int
    return a + b
end

func(int, int) as int proc = add
print(proc(1, 2))
```

匿名函数示例：

```xlang
auto inc = func(int a) as int => a + 1
```

泛型函数示例：

```xlang
func max<T>(T a, T b) as T
    if a > b then
        return a
    end
    return b
end

int a = max::<int>(1, 2)
```

## 8. 表达式与运算符

算术与赋值：

```text
+  -  *  /  %  **
=  +=  -=  *=  /=  %=  **=  ?=
```

比较与逻辑：

```text
==  !=  <  <=  >  >=  ~=
and  or  not  !
```

位运算：

```text
&  |  ^  ~  <<  >>
```

空值与条件：

```xlang
auto x = a ?? b
auto y = cond ? a : b
a ?= b
```

成员、调用和索引：

```xlang
obj.field
obj.method(1, 2)
arr[0]
dict["key"]
obj?.field
arr?[0]
```

范围与成员判断：

```xlang
for i in range(0, 10)
    print(i)
end

if "a" in dict then
    print("hit")
end
```

## 9. 控制流

条件：

```xlang
if a > 0 then
    print("positive")
elseif a == 0 then
    print("zero")
else
    print("negative")
end
```

循环：

```xlang
while i < 10
    i += 1
end

do
    i += 1
while i < 10

for i in range(0, 10)
    if i == 5 then
        break
    end
    continue
end
```

已实现规则：

- `if` / `elseif` / `else` / `end`
- `while`
- `do while`
- `for in`
- `break`
- `continue`
- `return`
- 多返回值 `return`

## 10. 容器

数组：

```xlang
array<int> a = array<int>[1, 2, 3]
auto b = [1, 2, 3]
```

列表：

```xlang
list<string> a = list<string>[0: "a", 2: "b"]
```

字典：

```xlang
dict<int> d = dict<int>{"a": 1, "b": 2}
auto empty = {}
```

集合：

```xlang
set<int> s = set<int>{1, 2, 3}
set<int> empty = set<int>{}
```

已实现规则：

- `[]` 默认数组字面量。
- `{}` 默认字典字面量。
- `array[...]` 明确数组。
- `list[...]` 明确列表。
- `dict{...}` 明确字典。
- `set{...}` 明确集合。
- 泛型容器使用静态元素类型。
- `var` 可承载动态容器值。
- 容器运行时依托 XRT/xvalue 基础设施。

## 11. class、custom、union

class 示例：

```xlang
class Point
    int x
    int y

    func create(int ax, int ay)
        self.x = ax
        self.y = ay
    end

    func sum() as int
        return self.x + self.y
    end
end
```

custom 示例：

```xlang
custom Timer
    extern func reset(self)
    extern func elapsed(self) as float64
end
```

union 示例：

```xlang
union NumberBits
    int64 i
    float64 f
end
```

当前已实现：

- class 字段。
- class 方法。
- class 构造与析构生命周期。
- class 继承与 `base` 调用。
- class 默认初始化与清理。
- custom 对接 C opaque handle。
- custom 成员函数第一个参数映射底层指针。
- custom 生命周期映射。
- C 风格 union。
- union 字段访问。
- union C ABI/XAPI 元数据。
- object 作为标准库普通基类。
- `class X extends object` 动态字段存储。
- class/object 字段协议 `__field__` / `__setField__`。
- class/object 索引协议 `__item__` / `__setItem__`。
- 协议方法签名校验与双下划线保留名检查。
- class 命名变量接收者的运算符协议：`__add__`、`__sub__`、`__mul__`、`__div__`、`__mod__`、`__pow__`。
- class 命名变量接收者的一元协议：`__neg__`、`__pos__`、`__not__`、`__bitNot__`。
- class 命名变量接收者的比较协议：`__eq__`、`__ne__`、`__lt__`、`__le__`、`__gt__`、`__ge__`、`__approx__`。
- `in` 对 class 命名变量接收者调用 `__contains__`。
- class 命名变量接收者可通过 `__call__` 支持 `obj(...)` 调用。
- class 临时值接收者支持运算符、字段、索引、`in`、`__call__` 和 `.length` 协议调用。
- `__len__` 可将 `.length` 作为只读协议属性暴露给 class；静态字段 `length` 优先于协议。
- class 构造表达式可作为成员/协议临时接收者，例如 `Sized(2).length`。
- class 方法支持多返回值，并可参与解构声明、解构赋值和成员调用返回类型推断。
- 普通 class 支持 `__iter__` / `__next__` 迭代协议。
- `__iter__() as IteratorClass` 创建迭代器，`IteratorClass.__next__() as (bool, T)` 返回是否存在下一项和值。
- `for value in obj` 绑定迭代项类型 `T`；`for index, value in obj` 绑定 `int` 索引和 `T` 值。
- `derive` 能力开关语法与语义校验。
- `derive copy` / `derive clone` 自动生成 `__copy__` / `__clone__`。
- `derive moveOnly` 与复制能力冲突检查。
- `copy()` / `clone()` 与 class、var、容器和 function 值的复制语义。
- `clone()` 固定表示深复制；`deepcopy()` 不属于 DEMO6 公开函数，不提供兼容别名。
- 自动 copy/clone 对 `var` 字段分别使用 XRT `xvoCopy` / `xvoDeepCopy`。
- `derive value` 自动生成 class 与 xvalue table 的互转协议。
- `derive json` / `derive xson` 基于 `derive value` 接入标准库持久化。
- 自动生成 `__toValue__()` / `__fromValue__(var value)` 协议。
- `json.stringify(class)` / `xson.stringify(class)` 序列化 class 实例。
- `json.parse::<Class>(text)` / `xson.parse::<Class>(text)` 反序列化为 class 实例。
- `json.write(path, class)` / `json.read::<Class>(path)` 文件持久化入口。
- `xson.write(path, class)` / `xson.read::<Class>(path)` 文件持久化入口。

当前未实现并已进入下一轮设计：

- `and` / `or` 短路逻辑的协议化方案。
- 泛型 class 的 `__iter__` / `__next__` 迭代协议。
- 字段级 json/xson 配置块。

## 12. 异常、断言与文档注释

异常：

```xlang
try
    throw "boom"
catch e
    print(e.message)
finally
    print("done")
end
```

断言：

```xlang
assert a > 0
assert a > 0, "a must be positive"
```

文档注释：

```xlang
#doc
desc:
    Adds two integers.
param a:
    First value.
param b:
    Second value.
return value:
    Sum result.
example:
    ```xlang
    print(add(1, 2))
    ```
#end
func add(int a, int b) as int
    return a + b
end
```

已实现规则：

- `throw` 抛出字符串描述。
- `try` / `catch` / `finally`。
- 异常路径资源清理。
- `assert` 语句。
- `#doc ... #end` 文档块。
- doc 字段进入 XAPI 元数据。
- example 内容作为文档文本保存，不解析为代码。

## 13. 并发、异步与任务

当前已实现基础能力：

- `async` 函数。
- `await`。
- future/task 基础运行模型。
- 异步异常传播。
- thread 基础封装。
- channel/sync 基础能力。
- 并发相关范例与测试。

当前开放风险：

- `RISK-ASYNC-001` 仍在风险总账中开放，表示异步体系仍需要继续压实边界和压力测试。

## 14. 标准库

当前标准库采用短命名空间，优先绑定 XRT，必要时绑定 CRT。

已纳入标准库范围的能力包括：

- 基础输出与运行时辅助。
- math 数学函数。
- string 字符串函数。
- time 时间函数。
- path/file/dir 文件系统函数。
- json/xson 序列化函数。
- url/query/regex 文本与网络辅助函数。
- random 随机函数。
- hash/crypto 摘要与编码辅助函数。
- codec 编解码函数。
- process 进程辅助函数。
- HTTP server 基础能力。
- 标准库函数默认注册到 xlang 环境。
- 标准库 DLL C 符号导出基线。

当前边界：

- HTTP client 暂缓进入标准库。
- 能够由 XRT 提供的能力必须优先使用 XRT。
- XRT 暂缺但属于基础运行时的能力，应以 XRT 命名风格补充，后续回流 XRT。
- CRT 只用于确实没有必要在 XRT 重复实现的底层能力。

## 15. C 混编、ABI 与导出

已实现能力：

- `extern` 声明 C 函数、变量、常量进入 xlang。
- `export` 导出 xlang 函数给 C。
- 生成 C 头文件。
- 生成 ABI dump。
- 生成 XAPI 接口元数据。
- `.xl` 与 `.c` 混合构建。
- C 侧可调用 xlang 导出函数。
- xlang 可直接调用声明后的 C 函数。
- C ABI 类型用于跨平台精确对齐。
- 可选参数导出到 C 时默认值失效，C 侧按普通参数签名调用。
- 多返回值、可变参数等 xlang 特性通过 xcall/封装边界与 C ABI 区分。

main 与入口规则：

- xlang `main(...) as int` 会生成固定 C `main`。
- 生成的 C `main` 负责初始化 xlang/XRT 运行环境。
- 命令行参数转换为 xvalue 数组形式传入 xlang main。
- xlang main 返回值作为进程退出码。
- 如果 `.xl` 中没有 main，exe 目标假定 C 源提供 main。
- C main 场景下，C 代码需要显式调用 `xlInit` 和 `xlUnit`。
- DLL 目标不要求 main。

## 16. XAPI、import、package 与扩展库

已实现能力：

- `xlang-xapi-1` 接口元数据。
- XAPI strict schema。
- doc 元数据输出。
- package fixture。
- import 读取 XAPI 并注入声明。
- import/package 与 C 混编测试闭环。
- 扩展库基础分发信息。
- 多版本共存的设计口径。
- package resolver 基础能力。

当前原则：

- XAPI 是机器生成的接口元数据。
- XAPI 是 xlang 与 C、扩展库、文档生成之间的接口信息中枢。
- 用户不需要手写 XAPI。
- xlang 环境保持显式导入，不做 C 符号自动污染。
- C 环境可以由 xlang 工具链自动注入必要头文件和声明。

## 17. DLL 嵌入式运行库

当前产物边界：

- `xl.dll`：嵌入式运行库 DLL。
- `xl.def`：DLL import definition。
- `libtcc.dll`：动态编译能力所需 TCC 动态库副本。
- `xl_dll_embed_string.exe`：字符串脚本运行验证程序。
- `xl_dll_null_context.exe`：空上下文防御验证程序。
- `xl_dll_threaded_host.exe`：多线程宿主验证程序。

已实现 API 方向：

- 创建/销毁语言上下文。
- 设置工作目录。
- 添加 include/lib/link/loadlib 依赖。
- 运行字符串脚本。
- 运行脚本文件。
- 编译字符串脚本到 exe。
- 编译脚本文件到 exe。
- 注册 C 函数进入上下文。
- 向执行环境注入当前进程内的 XRT/xlang 运行时符号。
- DLL API 边界由守门脚本维护，当前公开 API 数量基线为 21。

## 18. 代码生成与工程结构

当前代码生成状态：

- cgen 已拆分为多个独立职责文件。
- cgen 规则已形成对照表。
- 当前 cgen 文件基线为 66 个。
- 当前 cgenrule 唯一规则基线为 835 条。
- 当前 cgen 单文件行数受守门脚本约束。
- C 输出路径已经从大量直接 `fprintf` 逐步收口到统一输出抽象。
- XRT buffer/file/time/memory 能力优先用于工具链与运行时。
- 旧语法、旧路径、旧格式不做兼容回退。

当前开放风险：

- `RISK-ASYNC-001`
- `RISK-CALL-001`
- `RISK-LOOP-002`

## 19. 范例与测试

当前测试资产：

- `demo6/test/cases` 下有 462 个 `.xl` 测试用例。
- 负例诊断命令基线为 250 条。
- 负例诊断产物静态登记基线为 249 个。
- 负例诊断内容断言基线为 250 条。
- package fixture 基线为 17 个。
- 标准库测试基线为 21 个。
- 标准库 public function 基线为 180 个。
- DLL API 基线为 21 个。
- 风险总账基线为 21 条，其中 18 条已收口，3 条开放。

当前范例资产：

- `demo6/examples` 下有 57 个 `.xl` 范例。
- 包含根目录范例。
- 包含 tutorial 范例。
- 包含 feature suite 范例。
- 包含 stdlib 范例。
- 包含 DLL/C 宿主验证范例。

常用验证命令：

```bat
cd demo6
build.bat
test.bat
test_feature_suite.bat
rc0_guard.bat
```

快速运行：

```bat
cd demo6\release
xl run test.xl
```

构建 exe：

```bat
cd demo6\release
xl build test.xl -o test.exe
test.exe
```

混合 C 构建：

```bat
cd demo6\release
xl build main.xl helper.c -o app.exe
```

## 20. 当前明确不属于已实现稳定口径的能力

以下能力不是当前根清单中的已实现稳定功能：

- `and` / `or` 短路逻辑的协议化方案。
- 泛型 class 的 `__iter__` / `__next__` 迭代协议。
- 字段级 json/xson 配置块。
- f-string 表达式插值。
- 完整 package manager。
- HTTP client 标准库。
- 完整调试器。
- 泛型约束系统。
- 更完整的跨平台 DLL 入口封装。

这些能力应先进入 `demo6/docs` 的权威设计文档，再进入实现与测试。
