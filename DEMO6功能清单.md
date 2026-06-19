# DEMO6 功能清单

> 生成时间：2026-06-19
>
> 本清单基于当前 `demo6` 目录下的源码、测试用例、范例程序和工具链脚本整理。
> 本文只描述 DEMO6 当前已经实现或已经形成明确验证边界的功能，不把 DEMO5/dev 旧文档作为设计来源。

## 1. 当前总体状态

DEMO6 当前已经从语法原型推进到可检查、可生成 C 代码、可运行、可构建可执行文件的编译器雏形。

已具备的主流程：

- 使用 Flex 作为词法分析器。
- 使用 Lemon 作为语法分析器。
- 具备 AST 构建、AST 文本/JSON dump。
- 具备语义检查阶段。
- 具备 IR 文本/JSON dump。
- 具备 ABI dump。
- 具备 XAPI 接口元数据生成与读取。
- 具备 C 代码生成。
- 具备 TCC 后端构建与运行。
- 具备 `.xl` 与 `.c` 混合构建能力。
- 具备 XRT 链接与运行时生命周期初始化能力。
- 具备 smoke test、feature suite 和 RC0 guard。

当前可执行文件：

- `demo6/release/xl.exe`

主要验证脚本：

- `demo6/build.bat`
- `demo6/test.bat`
- `demo6/test_all.bat`
- `demo6/test_feature_suite.bat`
- `demo6/test_smoke.bat`
- `demo6/rc0_guard.bat`
- `demo6/tools/rc0_guard.ps1`

## 2. 命令行功能

当前 `xl.exe` 已实现以下命令：

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

已实现的命令行细节：

- `lex` 支持文本和 JSON token 输出。
- `parse` 支持 AST 文本树和 JSON 输出。
- `check` 支持普通诊断和 JSON 诊断。
- `ir` 支持文本和 JSON 输出。
- `cgen` 支持输出 C 文件。
- `cgen` 支持同时输出 C 头文件。
- `cgen --dump-dir` 会一次性输出：
  - tokens JSON
  - AST JSON
  - IR
  - ABI JSON
  - XAPI JSON
  - C 文件
  - H 文件
- `abi` 支持文本和 JSON 输出。
- `xapi` 支持生成 `xlang-xapi-1` 格式接口元数据。
- `run` 会生成临时 C 文件、调用 TCC 编译并运行。
- `run -- <args>...` 支持把命令行参数传递给生成程序。
- `run --save-temps` 支持保留临时文件。
- `build` 支持生成 exe。
- `build` 支持 `.xl` 与多个 `.c` 文件一起混合编译。
- `build -shared` 支持共享库/DLL 目标边界。
- `build --emit-header` 支持生成供 C 使用的头文件。
- `--inc-path`、`--lib-path`、`--link`、`--loadlib` 已接入 TCC 命令构造。
- 自动创建独立临时目录，避免不同构建互相覆盖。
- 默认后端为 TCC。
- 当前 build 脚本会链接 XRT 对象文件和 TCC runtime。

## 3. 源文件与词法

已实现的源文件与词法规则：

- 源码按 UTF-8 处理。
- 标识符支持 ASCII 标识符。
- 标识符支持 UTF-8 字符，因此可以使用中文标识符。
- 语言大小写敏感，`Name`、`name`、`NAME` 是不同标识符。
- 关键字大小写敏感。
- 空格、tab、换页、垂直制表等普通空白会被忽略。
- 换行是语句分隔符。
- `;` 也是语句分隔符。
- `()`、`[]`、`{}` 内部的换行不会结束语句。
- 支持 Windows/Unix 换行：`\r\n`、`\n`、`\r`。
- 支持 `//` 单行注释。
- 支持 `/* ... */` 多行注释。
- 多行注释支持嵌套。
- 未闭合多行注释会产生词法错误。
- 未闭合字符串会产生词法错误。
- 未闭合时间字面量会产生词法错误。

已识别的关键字：

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

说明：

- `async`、`await`、`yield`、`try`、`catch`、`finally`、`throw`、`assert`、`include`、`define`、`union`、`get`、`let` 当前属于词法保留/识别层面，不等价于对应高级语义已经实现。
- `self`、`base`、`create`、`destroy` 按上下文关键字方式处理：在 class/custom 特定位置有特殊含义，在普通上下文中可以作为普通标识符使用。

## 4. 字面量

### 4.1 整数字面量

已实现：

- 十进制整数：`123`
- 十进制下划线分隔：`1_000_000`
- 十六进制：`0x2A`、`0X2A`、`&h2A`、`&H2A`
- 八进制：`0o52`、`0O52`、`&o52`、`&O52`
- 二进制：`0b101010`、`0B101010`、`&b101010`、`&B101010`
- 数字中的 `_` 会在代码生成时归一化。

### 4.2 浮点字面量

已实现：

- 小数：`1.25`
- 科学计数：`1e3`、`1.2e-3`
- 数字中允许 `_` 分隔。

### 4.3 字符串字面量

已实现：

- 双引号字符串：`"hello"`
- 单引号字符串：`'hello'`
- 普通字符串支持转义。
- raw string：`r"c:\123.txt"`、`r'c:\123.txt'`
- 三引号多行字符串：`"""..."""`、`'''...'''`
- raw 多行字符串：`r"""..."""`、`r'''...'''`

已保留但未实现：

- 模板字符串 `f"..."`、`f'...'`、`f"""..."""`、`f'''...'''` 已在词法层识别，但解析时会报错，当前阶段暂未实现表达式插值。

### 4.4 时间字面量

已实现：

- `#yyyy-mm-dd#`
- `#hh:mm:ss#`
- `#yyyy-mm-dd hh:mm:ss#`

规则：

- 时间字面量从 `#` 到下一处 `#`。
- 时间字面量内部不能换行。
- 非法时间格式会在语义检查阶段报错。
- 代码生成接入 XRT 时间函数：
  - `xrtTimeSerial`
  - `xrtDateSerial`
  - `xrtDateTimeSerial`
  - `xrtStrToTime`
  - `xrtTimeToStr`

### 4.5 布尔与空值

已实现：

- `true`
- `false`
- `null`

## 5. 类型系统

### 5.1 内置类型注册

DEMO6 已实现类型注册系统。当前内置类型通过 registry 注册进入语义系统。

已注册内置类型：

```text
bool
int int8 uint8 int16 uint16 int32 uint32 int64 uint64
float float32 float64
string
time
point
function
var
type
array list set dict
```

泛型内置容器：

```text
array<T>
list<T>
set<T>
dict<T>
```

说明：

- `array`、`list`、`set`、`dict` 是类型注册系统中的类型名，可用于类型表达式和类型化字面量。
- `function` 是一等公民函数值类型。
- `type` 是类型值类型。
- `var` 是动态值类型。
- `auto` 是关键字，不是注册类型。
- `void` 不存在于 DEMO6 类型系统中；使用 `void` 会被测试为错误。
- 当前阶段不实现 `object` 动态对象模型。

### 5.2 类型别名

已实现：

```xlang
typedef MyInt = int
typedef IntArray = array<int>
extern type CInt = int
```

已支持的规则：

- 类型别名参与语义检查。
- 类型别名参与 ABI/XAPI 输出时会进行 canonical type 归一。
- 检测别名循环。
- 检测别名与重载造成的冲突。
- 检测重复类型声明。
- 检测未知类型。
- 不允许用内置类型名重复声明新类型。

### 5.3 类型值

已实现：

```xlang
auto a = 123
type t = type(a)
if t == int then
    ...
end
```

已支持：

- `type(expr)` 获取表达式的类型值。
- 类型标识符可以作为 type 字面值参与比较。
- `type` 值可与类型名比较。
- `type` 值不允许与普通 `int` 混用。
- 不支持对 `type` 值做大小比较。
- 泛型 class 的 type value 已有测试覆盖。

### 5.4 自动类型推断

已实现：

```xlang
auto a = 1
auto x, y = pair()
```

规则：

- `auto` 支持局部变量声明推断。
- `auto` 支持多返回值解构声明。
- `auto` 不允许用于函数参数。
- `auto` 不允许用于返回类型。
- `auto` 不允许用于字段类型。
- `auto` 不允许用于类型别名。
- `auto` 不允许用于 function 类型签名。

### 5.5 默认初始化

已实现：

- 局部变量可按类型默认初始化。
- 基础类型、class/custom、function、xvalue/容器等都有对应的初始化/清理路径。
- 具备默认初始化测试。

### 5.6 非拷贝类型约束

已实现：

- 带析构语义的 class/custom 会进入资源管理约束。
- 检测不可拷贝对象的赋值。
- 检测不可拷贝对象的拷贝初始化。
- 检测不可拷贝对象作为参数传递。
- 检测不可拷贝对象作为返回值。
- 检测带析构 class/custom 进入 `var` 的受限场景。
- 闭包捕获资源对象有专门限制。

## 6. 变量、常量与作用域

已实现写法：

```xlang
int a
int b = 1
var v = 123
auto c = b + 1
const int max = 10
```

已支持：

- 类型前置变量声明。
- `var` 动态变量声明。
- `auto` 局部推断声明。
- `const Type name = expr` 常量声明。
- 多变量 `auto` 解构声明。
- 全局声明和局部声明语义检查。
- 重复声明检查。
- const 赋值错误检查。
- C 风格的类型前置口径。

## 7. 表达式与运算符

### 7.1 基础算术与赋值

已实现：

```text
+  -  *  /  %
** 
=  +=  -=  *=  /=  %=  **=  ?=
```

说明：

- `**` 为幂运算。
- `**=` 为幂赋值。
- `?=` 为空赋值。
- 整数幂生成辅助函数。

### 7.2 比较与逻辑

已实现：

```text
==  !=  <  <=  >  >=  ~=
and  or  not
!
```

说明：

- `~=` 使用 XRT 近似比较机制。
- 支持 int/float/string/time 等已实现类型的比较约束。
- 语义检查会阻止不合法比较。

### 7.3 位运算

已实现：

```text
&  |  ^  ~  <<  >>
```

### 7.4 空值与条件表达式

已实现：

```xlang
auto x = a ?? b
auto y = cond ? a : b
```

已支持：

- `??` 空合并。
- `? :` 条件表达式。
- `?.` 安全成员访问。
- `?[ ]` 安全索引访问。
- `?=` 空赋值。

### 7.5 成员、索引与调用

已实现：

```xlang
obj.field
obj.method(1, 2)
arr[0]
dict["key"]
obj?.field
arr?[0]
fn(1, 2)
```

已支持：

- 函数调用。
- 方法调用。
- 成员访问。
- 索引读写。
- 多重 postfix 表达式组合。
- 安全成员访问。
- 安全索引访问。
- 索引赋值。
- 多目标索引赋值测试。

### 7.6 `in` 与 range

已实现：

```xlang
if x in values then
    ...
end

for i in range(10)
    ...
end
```

已支持：

- `in` 用于容器成员判断。
- `in` 用于字符串包含判断。
- `in` 用于 `range(...)`。
- `range(...)` 支持 Python 风格方向：
  - `range(stop)`
  - `range(start, stop)`
  - `range(start, stop, step)`
- `range` 参数数量检查。
- `range` step 为 0 的错误检查。
- `..` 范围运算符明确不支持，会提示使用 `range(...)`。

## 8. 控制流

### 8.1 if / elseif / else

已实现：

```xlang
if cond then
    ...
elseif other then
    ...
else
    ...
end
```

也支持省略 `then` 后以换行分隔：

```xlang
if cond
    ...
end
```

已实现内联形式：

```xlang
if cond then return value
if cond then break else continue
```

语义检查：

- `if` 条件必须是 bool。
- `elseif` 条件必须是 bool。

### 8.2 while

已实现：

```xlang
while cond do
    ...
end
```

语义检查：

- `while` 条件必须是 bool。

### 8.3 do while

已实现：

```xlang
do
    ...
while cond
```

### 8.4 for in

已实现：

```xlang
for item in arrayValue
    ...
end

for key, value in dictValue
    ...
end
```

已支持：

- 遍历 `range(...)`。
- 遍历 array。
- 遍历 list。
- 遍历 set。
- 遍历 dict。
- typed container 的遍历。
- 单目标和双目标循环变量。

### 8.5 break / continue

已实现：

```xlang
break
continue
break 2
continue 2
```

规则：

- 支持多层级 break/continue。
- 检测循环外使用。
- 检测层级超过当前循环深度。
- 检测 `continue 0` 等非法层级。
- 代码生成会在跳转前执行必要清理。

## 9. 函数系统

### 9.1 普通函数声明

已实现：

```xlang
func add(int a, int b) -> int
    return a + b
end
```

当前函数签名统一使用：

```text
func name(params) -> returns
```

无返回值函数不写 `->`，不使用 `void`。

### 9.2 多返回值

已实现：

```xlang
func pair() -> (int, string)
    return 1, "one"
end

auto a, b = pair()
```

规则：

- 多返回值使用 `-> (T1, T2)`。
- `return a, b` 支持返回多个值。
- `auto a, b = expr` 支持多返回值解构。
- `a, b = expr` 支持多目标赋值。
- 检测返回值数量不匹配。
- 检测多返回值传入单值参数等错误。

### 9.3 可选参数

已实现：

```xlang
func add(int a, int b = 1) -> int
    return a + b
end
```

规则：

- 参数默认值表示可选参数。
- 可选参数必须位于必选参数之后。
- 参数默认值参与类型检查。
- 调用时可省略尾部可选参数。
- 导出到 C ABI 的函数当前不允许可选参数。
- extern C 函数当前不允许可选参数。

### 9.4 命名参数

已实现：

```xlang
add(a = 1, b = 2)
```

规则：

- 支持按参数名传参。
- 检测重复命名参数。
- 检测缺少必选参数。
- 检测命名参数顺序/位置约束。
- 不允许对裸函数名值使用不明确的命名参数场景。

### 9.5 可变参数

已实现：

```xlang
func sum(...)
    auto values = arg
end

func options(***)
    auto values = arg
end
```

规则：

- `...` 为数组可变参数。
- `***` 为字典可变参数。
- 函数体内自动提供 `arg`。
- `...` 的 `arg` 类型为 `array<var>`。
- `***` 的 `arg` 类型为 `dict<var>`。
- `...` 和 `***` 不能同时存在。
- 可变参数必须位于参数表末尾。
- 检测用户重新声明 `arg` 的错误。
- 调用侧已支持命名参数，因此 `***` 有调用语法基础。
- extern C 函数当前不允许可变参数。
- export C ABI 函数当前不允许可变参数。

### 9.6 函数类型

已实现：

```xlang
function(int, int) -> int f
function(int) -> (int, string) g
function(...) h
function(***) k
```

已支持：

- `function(...) -> ...` 作为类型表达式。
- 函数签名中支持无返回值、单返回值、多返回值。
- 函数签名中支持 `...` 和 `***` 的类型表达。
- 检测 function 类型中参数名错误。
- 检测 vararg 在 function 类型中的位置错误。
- 检测函数值赋值签名不匹配。
- 函数类型可进入 `var` 的动态调用链。

### 9.7 函数值与一等公民函数

已实现：

```xlang
function(int, int) -> int f = add
auto r = f(1, 2)
```

已支持：

- 函数作为值保存。
- 函数值调用。
- 函数值签名检查。
- extern C 函数可进入 function value 场景。
- 裸函数调用与函数值取值分离。
- 重载函数名在裸值场景会进行歧义检查。
- function value 具备 retain/release 清理路径。
- 底层接入 `xrt_callable`、`xrt_func_sig`、`xrt_call_frame`、`xrt_call_result`。

### 9.8 匿名函数与闭包

已实现：

```xlang
auto f = func(int x) -> int
    return x + 1
end

auto g = func(int x) -> int => x + 1
```

已支持：

- 块体匿名函数。
- 箭头表达式匿名函数。
- 捕获外部变量。
- 闭包环境生成。
- 闭包 retain/release。
- 闭包调用。
- 闭包签名检查。
- 检测对捕获变量进行非法赋值。
- 检测捕获带析构 class/custom 等资源对象的危险场景。

### 9.9 函数重载

已实现：

```xlang
func add(int a, int b) -> int
    return a + b
end

func add(float a, float b) -> float
    return a + b
end
```

已支持：

- 普通函数重载。
- 方法重载。
- class 构造重载。
- custom 方法重载。
- 可变参数重载。
- 泛型函数与普通函数重载优先级。
- `var` 重载优先级。
- 重载匹配成本计算。
- 重载歧义检测。
- 禁止仅返回值不同的重载。
- 检测可选参数造成的重载冲突。
- 检测相同 vararg kind 的重载冲突。
- 类型别名参与重载冲突检查。

## 10. 泛型

### 10.1 泛型函数

已实现：

```xlang
func id<T>(T value) -> T
    return value
end

auto a = id(1)
auto b = id::<string>("x")
```

已支持：

- 泛型参数声明。
- 泛型函数类型推断。
- 显式实例化调用 `::<T>`。
- 多个类型参数。
- 泛型参数重复检查。
- 显式类型实参数量检查。
- 未知类型实参检查。
- 泛型推断冲突检查。
- 旧式 `foo<int>()` 调用语法被拒绝。

### 10.2 泛型 class

已实现：

```xlang
class Box<T>
    T value

    create(T v)
        self.value = v
    end
end

Box<int> b = Box<int>(1)
```

已支持：

- 泛型 class 声明。
- 泛型字段。
- 泛型方法。
- 泛型构造函数。
- 显式泛型 class 构造。
- `auto` 推断泛型 class 实例。
- 泛型 class 类型别名。
- 泛型 class type value。
- 泛型 class 方法重载。
- 泛型 class 构造参数数量检查。
- 泛型 class 构造类型检查。
- 禁止 open generic 直接构造。
- 当前限制：泛型 class 继承有语义约束测试。

### 10.3 泛型容器

已实现：

```xlang
array<int> a = array[1, 2, 3]
list<string> b = list[0: "a", 10: "b"]
set<int> c = set{1, 2, 3}
dict<int> d = dict{"a": 1}
```

已支持：

- 静态泛型容器。
- `array<T>` 静态元素类型。
- `list<T>` 静态元素类型，索引为 int64 稀疏索引。
- `set<T>` 静态元素类型。
- `dict<T>` 静态值类型，键当前以字符串为主要路径。
- 泛型容器保存基础类型。
- 泛型容器保存 class。
- 泛型容器保存 custom。
- 泛型容器保存 function。
- typed container 接入 XRT：
  - `xrtTypedArrayCreate/Destroy/AppendValue/Get/Count`
  - `xrtTypedListCreate/Destroy/SetValue/Get/ItemAt/Count`
  - `xrtTypedSetCreate/Destroy/AddValue/ItemAt/Count`
  - `xrtTypedDictCreate/Destroy/SetValue/Get/ItemAt/Count`
- typed container 有清理路径。
- typed container 支持遍历、索引、函数参数与返回等测试场景。

## 11. 容器字面量与动态容器

### 11.1 array

已实现：

```xlang
[] 
[1, 2, 3]
array[]
array[1, 2, 3]
array<int>[1, 2, 3]
```

已支持：

- 空数组字面量。
- 数组字面量。
- 类型前缀数组字面量。
- 动态 `array` 基于 xvalue。
- 静态 `array<T>` 基于 typed container。
- 索引访问。
- 索引赋值。
- `for item in array`。
- `in` 成员判断。
- `length` 安全成员路径。

### 11.2 list

已实现：

```xlang
[0: "a", 10: "b"]
list[]
list[0: "a", 10: "b"]
list<string>[0: "a"]
```

已支持：

- list 是带 int64 稀疏索引的数据结构。
- list 字面量用 `index: value` 形式。
- 动态 `list` 基于 xvalue。
- 静态 `list<T>` 基于 typed container。
- 索引访问。
- 索引赋值。
- `for key, value in list`。
- `in` 成员判断。

### 11.3 dict

已实现：

```xlang
{}
{"a": 1, "b": 2}
dict{}
dict{"a": 1}
dict<int>{"a": 1}
```

已支持：

- `{}` 默认是空字典。
- 键值对 `{key: value}` 是字典字面量。
- `dict{}` 明确空字典。
- 动态 `dict` 基于 xvalue table。
- 静态 `dict<T>` 基于 typed container。
- 字符串键访问。
- 字符串键写入。
- `for key, value in dict`。
- `in` 检查 key 是否存在。

### 11.4 set

已实现：

```xlang
{1, 2, 3}
set{}
set{1, 2, 3}
set<int>{1, 2, 3}
```

已支持：

- `{1, 2, 3}` 是集合字面量。
- `set{}` 明确空集合。
- 动态 `set` 基于 xvalue coll/set。
- 静态 `set<T>` 基于 typed container。
- `for item in set`。
- `in` 成员判断。
- 检测 set 字面量中误写 key/value pair 的错误。

## 12. var / xvalue 动态值

已实现：

```xlang
var v = 123
var s = "hello"
var a = [1, 2, 3]
var f = func(int x) -> int => x + 1
```

当前 `var` 已支持进入 xvalue 的主要类型：

- null
- bool
- int
- float
- string
- time
- point
- function/callable
- array
- list
- set
- dict
- class record
- custom handle

已支持：

- 基础值 boxing/unboxing。
- xvalue 引用计数。
- xvalue 类型描述绑定。
- xvalue callable 保存签名。
- `var` 保存 function 并调用。
- `var` 保存 class/custom 的受限场景。
- `var` 保存动态容器。
- `var` 与类型转换。
- `type(varValue)` 能识别基础类型、function 签名、class/custom/container 类型描述。
- `xvoInvoke` 动态调用链。
- `xvoCreateCallable`、`xvoGetCallable`、`xvoCallableSig` 接入。

资源限制：

- 带析构 class/custom 进入 `var` 的拷贝和返回路径有限制。
- 不允许用 `var` 绕开不可拷贝资源约束。

## 13. 类型转换与隐性类方法

已实现类型转换方向：

```xlang
int("123")
float("1.25")
string(123)
time("2026-06-18")
var(123)
```

已实现/已接入的转换基础：

- int 与字符串转换接入 XRT。
- float 与字符串转换接入 XRT。
- time 与字符串转换接入 XRT。
- xvalue 到 bool/int/float/string/time/point 的读取路径。
- class/custom/function/container 的 xvalue type desc 路径。

已实现隐性类方法方向：

```xlang
auto s = 123.toString()
auto t = true.toString()
auto u = value.toString()
```

当前已验证：

- 基础类型 `toString()`。
- `var`/xvalue 的 `toString()`。
- 容器相关 `length` 安全成员路径。
- 隐性类方法代码生成依赖 XRT 能力和必要的轻量辅助函数。

说明：

- 隐性类方法体系仍属于持续扩展区，但当前已有 `toString` 等基础路径和测试覆盖。

## 14. class

已实现：

```xlang
class Point
    int x
    int y

    create(int ax, int ay)
        self.x = ax
        self.y = ay
    end

    func sum() -> int
        return self.x + self.y
    end

    destroy()
        ...
    end
end
```

已支持：

- class 声明。
- 字段声明。
- 字段默认值。
- 方法声明。
- 方法调用。
- `self` 上下文。
- `create` 构造函数。
- `destroy` 析构函数。
- 构造函数重载。
- 方法重载。
- 字段冲突检查。
- 字段默认值类型检查。
- 构造参数数量检查。
- 析构函数参数检查。
- class 作为普通变量。
- class 作为字段。
- class 作为泛型容器元素。
- class 进入 `var` 的受限场景。
- class 类型描述进入 xvalue。
- class 析构清理自动生成。

继承已支持：

```xlang
class Child extends Base
    create(...)
        base(...)
    end
end
```

已实现继承相关能力：

- `extends` 父类声明。
- 父类字段访问/布局。
- `base(...)` 构造调用。
- 父类析构顺序。
- 检测未知父类。
- 检测缺少父类构造调用。
- 检测父类默认构造不可用。
- 检测 `base` 在 create 外使用。
- 检测父类构造重载不存在。

当前边界：

- DEMO6 当前不实现 `object` 动态字段/元方法模型。
- 当前 class 更接近 C/C++ 风格结构体 + 方法 + 构造析构 + 继承。

## 15. custom

已实现：

```xlang
custom Counter
    extern "counter_create" create(int start)
    extern "counter_destroy" destroy()
    extern "counter_add" func add(int value) -> int
end
```

设计定位：

- `custom` 是轻量类。
- 背后保存 C 黑盒句柄。
- custom 成员函数映射 C 函数。
- C 函数第一个参数由 custom 句柄隐式传递。
- 通常以 `void*` 或对应 C 指针类型承载句柄。

已支持：

- custom 声明。
- extern create。
- extern destroy。
- extern 方法。
- xlang 方法。
- custom 方法调用。
- custom 方法重载。
- custom ownership。
- custom 空句柄析构保护。
- custom 绑定 `point` 等 C 数据路径。
- custom 作为 class 字段。
- custom 作为泛型容器元素。
- custom 进入 `var` 的受限场景。
- custom 类型描述进入 xvalue。
- 检测 custom 特殊成员名错误。
- 检测重复 C symbol。
- 检测不支持的 extern 类型。
- 检测 extern custom 返回类型错误。
- 检测 custom extern vararg 错误。
- 检测 custom 方法重载冲突。

当前边界：

- custom 是否自动管理内存取决于是否声明 `create`/`destroy`。
- 无 `destroy` 的 custom 不自动释放外部资源。
- 有 `destroy` 的 custom 会在作用域退出时生成清理。

## 16. C 混编与 ABI

### 16.1 extern C 函数

已实现：

```xlang
extern "puts" func puts(string text) -> int
```

已支持：

- 声明 C 函数进入 xlang。
- C symbol 与 xlang 名分离。
- bool、整数宽度、float、string 等基础类型映射。
- extern string return 测试。
- extern C 函数可作为 function value。
- 检测 extern C symbol 类型冲突。
- 检测同名 C symbol kind 冲突。

当前限制：

- extern C 函数不支持多返回值。
- extern C 函数不支持可选参数。
- extern C 函数不支持 xlang vararg。
- extern C 函数不支持泛型。
- extern C 函数参数和返回类型只允许当前 C ABI 可表达的类型集合。

### 16.2 extern C 变量

已实现：

```xlang
extern "global_count" var int count
```

已支持：

- C 全局变量声明进入 xlang。
- 读取 C 变量。
- 写入 C 变量。
- bool 与基础整数宽度测试。

### 16.3 export 到 C

已实现：

```xlang
export "xl_add" func add(int a, int b) -> int
    return a + b
end

export "xl_count" var int count = 0
export "xl_answer" const int answer = 42
```

已支持：

- 导出函数为 C symbol。
- 导出变量为 C symbol。
- 导出 const 为 C symbol。
- 生成 C 头文件。
- C host 调用 xlang 导出函数。
- C host 访问 xlang 导出变量。
- bool、基础整数宽度导出。
- 类型别名导出时 canonical 化。
- 多返回值 export 具备 ABI/header 测试。

当前限制：

- export 函数不支持可选参数。
- export 函数不支持 vararg。
- export 函数不支持泛型。
- export string return 当前被语义检查限制。

### 16.4 `.xl` + `.c` 混合构建

已实现：

```bat
xl build main.xl helper.c -o app.exe
```

能力：

- build 命令接受一个 `.xl` 模块和多个 `.c` 输入。
- 自动生成 xlang C 文件。
- 自动生成 xlang H 文件。
- 自动生成 C wrapper，把 `xrt.h`、生成头文件、用户 C 源一起纳入构建。
- 用户 C 文件不需要手写导入生成头文件即可调用 xlang 导出符号。
- 自动把生成模块和 C 源传给 TCC。

当前边界：

- 当前 build 只接受一个 `.xl` 主模块；更多 xlang 模块通过 import/XAPI 方向推进。
- C 源不会被 xlang 解析；只作为 C 编译输入。

### 16.5 链接与加载

已实现：

- `--link` 传递对象文件、库文件或系统库。
- `--lib-path` 传递库搜索路径。
- `--inc-path` 传递 include 搜索路径。
- `--loadlib` 支持运行时加载库路径处理。
- build/run 会避免重复显式链接 XRT/TCC runtime 的部分场景。
- 当前 build 脚本要求存在 `..\..\xrt\release\x64\xrt.o`。

## 17. 程序入口与生命周期

### 17.1 xlang main

已实现 main 约定：

```xlang
func main(...) -> int
    ...
end
```

已支持：

- xlang 源中存在 `main` 时，代码生成显式 C `main`。
- C `main` 会初始化 xlang 运行环境。
- 命令行参数融合到 xlang 内部 `arg`，类型为 `array<var>`。
- `main` 返回 `int`。
- XAPI 中记录 entry kind 为 `xlang-main`。

### 17.2 script main

已实现：

- 顶层可执行语句会生成脚本式入口。
- XAPI 中记录 entry kind 为 `script-main`。
- 顶层没有运行语句且无 main 时，不生成 C main。

### 17.3 C main

已支持边界：

- 如果 xlang 没有 `main` 且无顶层运行语句，可假定 C 代码提供 main。
- 由 C 提供 main 时，需要显式调用生命周期函数。

生成头文件提供：

```c
XL_InitOptions xlInitOptions(int argc, char** argv);
int xlInit(const XL_InitOptions* options);
void xlUnit(void);
```

### 17.4 DLL/shared 入口

已实现：

- `xlDllAttach(void* module)`
- `xlDllDetach(int processTerminating)`
- Windows 下生成 `DllMain`。
- 非 Windows 方向预留 constructor/destructor 形式。
- `-shared` 构建时禁止包含 xlang main 或顶层可执行语句。

### 17.5 XRT 生命周期

已实现：

- `xlInit`/`xlUnit` 内部维护 init count。
- 可通过 `XL6_USE_XRT_LIFECYCLE` 接入 `xrtInit`/`xrtUnit`。
- `XL_InitOptions` 带 size/version 检查，便于 ABI 演进。

## 18. XAPI / import / package

### 18.1 XAPI 生成

已实现：

```bat
xl xapi module.xl --emit module.xapi.json
```

XAPI 当前格式：

```json
{
  "format": "xlang-xapi-1"
}
```

当前 XAPI 包含：

- compiler 信息。
- target 信息。
- module 信息。
- entry 信息。
- exports。
- types。
- imports。
- links/loadlibs/cSources/docs 占位数组。

exports 已包含：

- kind：function/var/const
- name
- cSymbol
- type
- callingConvention
- varargMode
- requiredParamCount
- fixedParamCount
- params
- returns
- optional 标记
- vararg 标记

types 已包含：

- class 声明名。
- custom 声明名。

imports 已包含：

- import target。
- mode。
- resolved。
- resolvedKind。
- manifest 路径。

### 18.2 XAPI 读取

已实现：

- 严格读取 `xlang-xapi-1`。
- 读取 package/interface 的 XAPI JSON。
- 对格式错误的 XAPI 产生诊断。
- 对缺少 `package.xapi.json` 的 package 产生诊断。

当前边界：

- XAPI loader 是当前格式的专用严格读取器，不是通用 JSON 框架。

### 18.3 import 展开

已实现：

```xlang
import counter
import "local/math"
```

已支持：

- import package 名。
- import 路径字符串。
- package resolver 查找：
  - 直接 `.xapi.json` 文件。
  - 相对路径。
  - `xlang.package.json`
  - `package.xapi.json`
  - `packages/<name>`
  - `lib/<name>`
- 读取 XAPI 后自动注入可支持的 extern 声明。
- 注入 extern C 函数。
- 注入 extern C 变量。
- `check`、`ir`、`cgen` 都走 CompileUnit 的 import 展开流程。

当前 import 自动注入限制：

- 只自动注入 calling convention 为 `cdecl` 的函数。
- 不自动注入 vararg 函数。
- 不自动注入可选参数函数。
- 不自动注入多返回值函数。
- 不自动注入 class/custom 高级类型。
- unsupported XAPI export 会被跳过，不会强行注入。

## 19. ABI 与 callable

已实现：

- ABI 文本/JSON dump。
- callable descriptor 模块。
- calling convention 名称：
  - 当前主要落地 `cdecl`。
- vararg mode：
  - none
  - array vararg
  - dict vararg
- required param count。
- fixed param count。
- xlang function signature 到 ABI/XAPI 的描述。
- XRT callable 接入：
  - `xrt_callable`
  - `xrt_func_sig`
  - `xrt_call_frame`
  - `xrt_call_result`
- `var` 保存 callable 时通过函数对象保存签名，而不是把签名直接塞进 xvalue struct。

当前边界：

- stdcall 等其他调用约定尚未完整作为语言层声明落地。
- 多返回值、可选参数、vararg 在 xcall 内部模型更自然，但 C ABI 对外导出仍保持受限。

## 20. C 代码生成

已实现的 C 生成能力：

- 生成 C11 风格代码。
- 生成函数。
- 生成全局变量。
- 生成局部变量。
- 生成表达式。
- 生成控制流。
- 生成 class 结构体与方法。
- 生成 custom handle 包装。
- 生成泛型函数/泛型 class 实例化符号。
- 生成 function value wrapper。
- 生成 closure 环境。
- 生成 callable frame 调用。
- 生成 xvalue 动态值运行时代码片段。
- 生成 typed container 调用。
- 生成生命周期函数。
- 生成 main/script main。
- 生成 DLL/shared 入口。
- 生成 export 头文件。
- 生成多返回值辅助结构。
- 生成作用域退出清理。
- 生成 break/continue 前清理。

当前工程结构已经把 cgen 拆分为多个关注点文件，包括：

- ABI
- 参数处理
- callable frame/runtime/wrapper
- closure
- core context/generic/lookup/names/state/type
- class/custom 声明
- entry/main/lifecycle
- expression
- function value
- header
- module
- runtime
- statement
- symbol
- generic type
- type inference
- value assignment/object
- xvalue runtime

## 21. 诊断能力

已实现：

- 词法错误诊断。
- 语法错误诊断。
- 语义错误诊断。
- 代码生成错误诊断。
- 文本诊断输出。
- JSON 诊断输出。
- CompileUnit phase 信息，用于显示错误发生阶段。
- `check --json` 输出 success/diagnostics。

已覆盖的典型错误：

- 未闭合注释。
- 未闭合字符串。
- 未闭合时间字面量。
- 模板字符串暂未实现。
- 语法错误。
- 未知类型。
- 重复声明。
- const 赋值。
- return 出现在函数外。
- return 数量不匹配。
- return 类型不匹配。
- if/while 条件类型错误。
- break/continue 越界。
- 参数默认值类型错误。
- 调用参数类型错误。
- 缺少必选参数。
- 重复命名参数。
- 多返回值错误使用。
- 泛型数量错误。
- 泛型推断冲突。
- 重载歧义。
- C ABI 不支持类型/参数形态。
- import package 缺失接口文件。
- XAPI 格式错误。

## 22. 测试与范例覆盖

### 22.1 范例程序

当前 `demo6/examples` 包含：

- `hello.xl`
- `literals.xl`
- `containers.xl`
- `generics.xl`
- `generic_identity.xl`
- `anonymous_functions.xl`
- `class_custom.xl`
- `class_point.xl`
- `custom_counter.xl`
- `c_hello.xl`
- `c_variables.xl`
- `c_export.xl`
- `second_stage.xl`

### 22.2 feature suite

当前 `demo6/examples/feature_suite` 包含：

- `00_language_basics.xl`
- `01_expressions_control_flow.xl`
- `02_functions.xl`
- `03_types_and_values.xl`
- `04_containers.xl`
- `05_generics.xl`
- `06_function_values_closures.xl`
- `07_class_custom.xl`
- `08_c_interop_exports.xl`
- `09_c_export_abi.xl`
- `host.c`
- `host_exports.c`
- `run_feature_suite.bat`

覆盖主题：

- UTF-8 与中文标识符。
- 大小写敏感。
- 注释。
- 语句分隔。
- 变量、常量、typedef。
- 字面量。
- 运算符。
- 控制流。
- 函数、多返回值、可选参数、命名参数、可变参数。
- 类型值、类型转换、隐性类方法、var。
- 容器与泛型容器。
- 泛型函数与泛型 class。
- function value 与闭包。
- class/custom。
- C extern/export。
- C ABI 头文件与 host 调用。

### 22.3 test/cases 覆盖

当前 `demo6/test/cases` 已包含大量正向和反向测试，覆盖：

- call args/defaults/named/vararg。
- numeric/string/time literals。
- comments/layout/case-sensitive/unicode identifiers。
- operators/null coalesce/safe member/safe index/index access。
- control flow/inline if/do while/loop levels。
- containers/in/for-in/range/static typed containers。
- type aliases/type values/type conversions/type identifiers。
- generics/generic function/generic infer/generic class/generic alias/generic overload。
- overloads/overload costs/overload varargs。
- anonymous functions/function types/function values/function signature mismatch。
- closures/resource capture。
- class fields/methods/create/destroy/inheritance/base create/overloads。
- custom create/destroy/methods/ownership/null destroy/overloads/C host。
- var runtime values/class/custom/function objects。
- extern functions/vars/bool/string return/primitive widths。
- export functions/vars/const/bool/primitive widths/multi-return/header host。
- import metadata/XAPI use/XAPI var use/missing/broken/unsupported XAPI。
- main entry/shared build/lifecycle。
- C mixed build/link/loadlib。
- 大量语义负例。

## 23. 当前明确未实现或受限的能力

以下能力当前不应被视为 DEMO6 已完成特性：

- `void` 类型不存在，并且会被拒绝。
- `object` 动态对象、任意字段、元方法当前不实现。
- 模板字符串 `f"..."` 只保留语法入口，当前会报错。
- `..` 范围运算符不支持，当前使用 `range(...)`。
- `async`/`await`/`yield` 尚未实现。
- `try`/`catch`/`finally`/`throw` 尚未实现。
- `assert` 尚未作为语言语义落地。
- `include`/`define` 尚未实现。
- `union` 尚未实现。
- `get`/`let` 属性语义尚未实现。
- 完整 package manager 尚未完成，目前是 package resolver + XAPI import 闭环。
- import 自动注入目前只覆盖可安全映射为 extern C 的函数和变量。
- class/custom 的完整 XAPI 消费和跨包类型导入仍未完成。
- stdcall 等非 cdecl 调用约定尚未形成完整语言声明与生成闭环。
- C ABI 对 optional/vararg/generic/multi-return 的兼容仍保持受限。
- 当前 build 只接受一个 `.xl` 主模块。
- 当前 XAPI loader 是专用严格读取器，不是完整 JSON 解析基础设施。

## 24. 关键源码位置

编译器入口：

- `demo6/main.c`
- `demo6/src/driver/xl6_cli.c`

词法/语法：

- `demo6/src/lexer/xl6_lexer.l`
- `demo6/src/parser/xl6_parser.y`

AST/IR/诊断：

- `demo6/src/ast/xl6_ast.*`
- `demo6/src/ast/xl6_ast_dump.*`
- `demo6/src/ir/xl6_ir_dump.*`
- `demo6/src/diag/xl6_diag.*`

语义与类型：

- `demo6/src/sem/xl6_sem.c`
- `demo6/src/sem/xl6_type_registry.*`
- `demo6/src/type/xl6_type_model.*`

ABI/XAPI/callable：

- `demo6/src/abi/xl6_abi_dump.*`
- `demo6/src/abi/xl6_callable.*`
- `demo6/src/abi/xl6_xapi_dump.*`
- `demo6/src/abi/xl6_xapi_load.*`

import/package/compile unit：

- `demo6/src/compile/xl6_compile_unit.*`
- `demo6/src/compile/xl6_imports.*`
- `demo6/src/package/xl6_package.*`

C 代码生成：

- `demo6/src/cgen/xl6_cgen.*`
- `demo6/src/cgen/xl6_cgen_entry.*`
- `demo6/src/cgen/xl6_cgen_lifecycle.*`
- `demo6/src/cgen/xl6_cgen_runtime.*`
- `demo6/src/cgen/xl6_cgen_header.c`
- `demo6/src/cgen/xl6_cgen_xvalue_runtime.c`
- `demo6/src/cgen/xl6_cgen_function_value.c`
- `demo6/src/cgen/xl6_cgen_closure.c`
- `demo6/src/cgen/xl6_cgen_type_generic.c`
- `demo6/src/cgen/xl6_cgen_type_infer.c`

构建后端：

- `demo6/src/backend/xl6_build_backend.*`

输出抽象：

- `demo6/src/common/xl6_output.*`

## 25. 总结

当前 DEMO6 已经完成了语言基础、类型系统、函数系统、容器、泛型、class/custom、C 混编、XAPI/import、可执行文件生成等大量基础能力。

它目前的核心优势是：

- 类型口径比 DEMO5 更集中。
- `void` 已从语言中移除。
- `function`、`type`、`var`、class/custom、typed container 都已经进入统一类型与代码生成链路。
- C 混编从声明、导出、头文件、混合构建到生命周期有了闭环。
- XAPI 已经从“扩展库设想”进入编译器可生成、可读取、可注入的工程链路。
- 测试覆盖已经包含大量正向运行和负向语义验证。

仍需重点收口的方向：

- import 对 class/custom/function/复杂 ABI 的扩展。
- XAPI 元数据的长期稳定格式。
- XRT 中 typed container、callable、type desc、xvalue 的最终库级实现收敛。
- 标准库/隐性类方法的系统化扩充。
- 跨平台 shared library 入口与非 TCC 后端。
- 更成熟的错误恢复、诊断文本和调试体验。
