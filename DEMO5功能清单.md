# DEMO5 功能清单

本文档用于 DEMO6 重构前盘点 DEMO5 已经实现或已经被测试覆盖的语言能力、写法、标准库、原生桥、模块包能力和工具链能力。

口径说明：

- 主要来源：`demo5/README.md`、`demo5/docs/README.md`、`dev/xlang第五版语言标准支持覆盖率.txt`、`dev/xlang第五版增强标准库支持覆盖率.txt`、`demo5/src/lexer/xl_lexer.l`、`demo5/src/parser/xl_parser.y`、`demo5/src/runtime/xl_std_*.c`、`demo5/release/examples`、`demo5/test/xl_test.c`。
- 这是一份“DEMO5 现状清单”，不是 DEMO6 兼容承诺。
- 语法以当前 lexer/parser 为准；历史草案里出现但当前语法没有落地的内容会单独标注。
- 标准库以 `demo5/src/runtime/xl_std_*.c` 中声明导出的接口为准。
- 当前已知红点：运行时回归选择器曾出现 `examples/full/native_error_bridge.xl` 失败，期望退出码 31，实际退出码 1。其他运行时用例当次统计为 1306/1307 通过；lexer smoke 为 3/3 通过。

## 1. 工具链与工程形态

DEMO5 当前主链路：

- 前端：flex lexer + lemon parser。
- 中间层：AST、语义检查、类型系统、模块解析、lowering。
- 后端：C 代码生成、libtcc 执行/编译。
- 运行时：XRT、内建标准库、动态库加载、模块初始化、错误/异步/生成器执行器。
- 调试：`xldbg`、断点/单步/帧变量、DAP probe V1。
- 示例：`demo5/release/examples` 下约 942 个 `.xl` 示例，其中 `full` 目录约 392 个。
- 回归：`demo5/test/xl_test.c` 内有大量编译、运行、语义错误、模块、XLE、调试、native binding 用例。

常见工具入口：

- `release/xl.exe run <file.xl>`：运行源码。
- `release/xl.exe cgen <file.xl> --dump-c`：生成/查看 C。
- `release/xl5_test.exe lexer`：lexer smoke。
- `release/xldbg.exe ...`：调试链路 smoke。

## 2. 词法基础

编码与标识符：

- 源文件按 UTF-8 处理。
- 标识符支持 ASCII 与 UTF-8 字符，因此中文标识符可用。
- 大小写敏感。
- 关键字不能作为普通标识符使用。

空白与分隔：

- 空格、tab 等普通空白被忽略。
- 换行是语句分隔符。
- 分号 `;` 也可作为语句分隔符。

注释：

- 行注释：`// comment`。
- 块注释：`/* comment */`。
- 未闭合块注释会产生 lexer error。
- `#` 不是普通行注释；行首 `#...` 被模块扫描器用于链接/绑定指令，`#...#` 被 lexer 用于 time 字面量。

当前 lexer 保留字：

```text
null true false
func function const var typedef as
if then else elseif end
for in while do
break continue return
async await new delete yield
try catch finally throw assert
import export extern include
class custom extends union create destroy get let nocopy
and or not
```

说明：

- `include` 已进入词法保留字表，但当前主 parser 未看到可用的 include 语句产生式；按“保留字/未来口径”处理。

## 3. 字面量

整数：

- 十进制：`123`、`1_000_000`。
- 十六进制：`0x2A`、`0X2A`、`&h2A`、`&H2A`。
- 八进制：`0o52`、`0O52`、`&o52`、`&O52`。
- 二进制：`0b101010`、`0B101010`、`&b101010`、`&B101010`。
- 数字中可使用 `_` 分隔。

浮点：

- 小数：`1.25`。
- 科学计数：`1e3`、`1.2e-3`。
- 数字中可使用 `_` 分隔。

字符串：

- 普通字符串：`"hello\n"`。
- raw string：`r"hello"` / `R"hello"`。
- 单引号 raw string：`'hello'`。
- 模板字符串：`` `hello ${name}` ``，lexer 层以 backtick token 化，模板语义由后续阶段处理。

时间：

- time literal：`#2026-06-18#` 形式，规则为 `#` 到下一处 `#`，中间不能换行。

布尔与空：

- `true`、`false`。
- `null`。

容器字面量：

- 空数组：`[]`。
- 数组：`[1, 2, 3]`。
- 带索引写法的 list literal：`[0: "a", 1: "b"]`，内部通过 list indexed literal 路径处理。
- 空 dict：`{}`。
- set：`{1, 2, 3}`。
- dict：`{"a": 1, "b": 2}`。
- 显式容器类型：`list{1, 2}`、`set{1, 2}`、`dict{"a": 1}`。
- 泛型容器显式类型：`array<int>{1, 2}`、`list<string>{"a"}`、`dict<int>{"a": 1}` 等。

## 4. 运算符

赋值：

- `=`
- `+=`、`-=`、`*=`、`/=`、`%=`
- `**=`
- `?=` 空值赋值

算术：

- `+`、`-`、`*`、`/`、`%`
- `**` 幂运算，右结合。

比较：

- `==`、`!=`
- `<`、`<=`、`>`、`>=`
- `~=` 近似相等。
- `in` 成员/包含判断。

逻辑：

- `and`、`or`、`not`
- `!`

位运算：

- `&`、`|`、`^`
- `~`
- `<<`、`>>`

空值/条件：

- `??` 空合并。
- `?:` 三元条件表达式。

成员与索引：

- `obj.member`
- `obj?.member`
- `obj[index]`
- `obj?[index]`

范围/展开：

- `..` 范围 token。
- `...` vararg 参数/调用展开。
- `**` 同时也是幂运算；在调用参数位置可出现 `**expr` 字典展开。注意 lexer 没有 `***` token，当前 parser 的 dict pack/展开使用 `**` 形态。

箭头：

- `=>` 用于箭头匿名函数。

## 5. 类型系统

基本类型：

- `bool`
- `int`
- `int8`、`uint8`
- `int16`、`uint16`
- `int32`、`uint32`
- `int64`、`uint64`
- `float`
- `float32`、`float64`
- `string`
- `time`
- `point`
- `null`
- `void`

兼容/历史别名：

- `bytes` 当前按源码注释保留兼容，折叠为 `string`。
- 精确数值类型与 `int`/`float` 存在部分别名/兼容转换覆盖，例如 `int32` 可与 `int` 互转，`float64` 可与 `float` 互转；窄化、bool/number、string/number、var 到具体类型等错误路径有语义测试覆盖。

容器类型：

- `array`
- `array<T>`
- `list`
- `list<T>`
- `set`
- `set<T>`
- `dict`
- `dict<T>`，当前常见形态是 string key + value type。
- `range`

动态与推断：

- `var`：动态值类型。
- `auto`：局部声明推断类型。
- `type`：类型值。
- `object`：对象基类。

函数类型：

- `function<Return(Param1, Param2)>`
- 多返回函数类型：`function<int,string()>` 形态已在 grammar 中覆盖。
- 可声明函数类型变量、容器里的函数值、泛型函数值。

并发/异步/生成器：

- `future<T>`
- `task`
- `channel`
- `generator<T>` 或 generator 相关内部协议。

错误/模块：

- `error`
- `module`

类/自定义类型：

- `class Name extends Base`
- `custom Name extends Base`
- `union Name`

类型构造：

- `Type(expr)` 可作为类型构造/转换写法。
- `new Type(...)` 创建对象。
- `delete obj` 删除对象。

类型别名：

```xlang
typedef MyInt = int
typedef Handler = function<int(string)>
typedef Names = list<string>
```

外部类型：

```xlang
extern type NativeHandle = point
```

## 6. 声明

变量声明：

```xlang
var x
var x = 1
int n
int n = 1
array<int> values = array<int>{1, 2}
function<int(int)> f
```

常量声明：

```xlang
const int answer = 42
const name = "x"
```

自动推断：

```xlang
auto value = expr
```

多返回解构：

```xlang
auto a, b = f()
auto a, _ = f()
```

限制：

- 当前 parser 明确限制非 `auto` 的多目标声明/解构。
- `auto` 参数、`auto` 返回等错误路径有语义测试覆盖。
- 未初始化变量、默认值、const 赋值、复合赋值 const 错误都有覆盖。

顶层：

- 支持顶层函数、类型、extern、typedef、import、export、const/var 等。
- 顶层模块初始化错误传播、模块缓存、重复导入等有覆盖。

## 7. 函数

普通函数：

```xlang
func add(int a, int b) as int
	return a + b
end
```

无返回：

```xlang
func log(string text) as void
	println(text)
end
```

多返回：

```xlang
func pair() as int, string
	return 1, "one"
end

auto id, name = pair()
```

异步函数：

```xlang
async func load() as int
	return await future_value
end
```

说明：

- `async func foo() as int` 写逻辑返回类型。
- 调用结果按 `future<int>` 语义进入 await/task 链路。
- `await` 只允许在 async 上下文，错误路径有测试。

默认参数：

```xlang
func f(int a, int b = 1) as int
	return a + b
end
```

命名参数调用：

```xlang
f(a = 1, b = 2)
```

vararg：

```xlang
func collect(... items) as int
	return len(arg)
end
```

dict pack/关键字 pack：

```xlang
func call(** options) as int
	return len(arg)
end
```

说明：

- `...` 与 `**` 是当前 parser 里的参数包/调用展开形态。
- 覆盖率文档曾用 `***` 描述 dict pack，但 lexer/parser 现状是 `**`。
- 函数体中注入保留的 `arg`，`...` 的 `arg` 类型为 `array<var>`，`**` 的 `arg` 类型为 `dict<var>`。
- `...` 与 `**` 混用/冲突、`arg` 保留名等错误路径有覆盖。

调用展开：

```xlang
f(...values)
f(**options)
```

重载：

- 同名函数可按参数数量/类型重载。
- 编译期重载解析。
- 歧义、候选列表、`var` 优先级、泛型推断失败等诊断有测试覆盖。

泛型函数：

```xlang
func id<T>(T value) as T
	return value
end

auto n = id<int>(1)
```

已覆盖能力：

- 显式类型实参。
- 单类型参数、多类型参数。
- 嵌套容器类型。
- 泛型函数值。
- 泛型类型方法。
- 泛型推断错误。

方法/类型方法：

```xlang
func string.double(string self) as string
	return self + self
end

func list<int>.sum(list<int> self) as int
	...
end
```

说明：

- 支持 `func Type.name(...)` 形态。
- 支持 `expr.method(...)` 调用。
- 支持模块导入/导出类型方法。
- `var` 持有基础类型/容器时的方法调用、`length` 属性等有覆盖。

匿名函数：

```xlang
auto f = func(int x) as int
	return x + 1
end

auto g = func(int x) as int => x + 1
```

闭包：

- 支持捕获外部变量。
- 支持匿名函数放入变量、容器、作为 `var` 传递。
- 支持多返回匿名函数。
- async/generator 与闭包组合有测试覆盖。

函数 ABI/元数据：

- 生成 ABI symbol、signature id、dispatch key。
- 模块 manifest 中记录 export name、symbol、kind、overloadCount。
- 原生桥使用函数签名做 point -> function 转换与 dlsym 调用。

## 8. 表达式能力

基础：

- 字面量、变量引用、成员引用、索引引用。
- 括号表达式。
- 一元 `+`、`-`、`not`、`!`、`~`。
- 二元算术、比较、逻辑、位运算。
- `??`、`?:`、`?=`。

调用：

- 普通调用：`f(a, b)`。
- 方法调用：`obj.method(a)`。
- 泛型调用：`f<T>(value)`。
- 对象调用：`obj(...)`，通过 `op_call` 分发。

成员：

- 普通成员：`obj.name`。
- 安全成员：`obj?.name`。
- 动态成员、对象虚分发、基类变量持有子类对象后的 late dispatch 有测试。

索引：

- `obj[index]`
- `obj[index] = value`
- 安全索引：`obj?[index]`
- 自定义/类通过 `op_index`、`op_index_set` 分发。

属性：

- 内建/容器 `length`。
- 自定义 `get name()` / `let name(...)` 属性。

对象生命周期：

- `new Type(...)`
- `delete obj`
- 删除共享 typed object 错误、delete 要求 object lvalue、use-after-delete 有语义/运行测试。

yield 表达式：

```xlang
auto next = (yield value)
```

说明：

- `yield` 可作为语句，也可作为括号表达式。
- resume value、yield top-level 错误、yield expr value 错误有覆盖。

## 9. 控制流

if：

```xlang
if cond
	...
elseif other
	...
else
	...
end
```

then 形态：

```xlang
if cond then
	...
end

if cond then return 1 else return 2
if cond then return 1
```

限制：

- 条件必须是 bool；number/null 等条件错误有测试。

while：

```xlang
while cond do
	...
end
```

do-while：

```xlang
do
	...
while cond
```

for-in：

```xlang
for value in values
	...
end

for index, value in values
	...
end

for key, value in dict_value
	...
end

for index, value in range(2, 10, 2)
	...
end
```

已覆盖 iterable：

- `range(...)`
- `array`
- `list`
- `set`
- `dict`
- generator
- object/custom iterator/for-in 分发
- `var` 持有容器或 generator

break/continue：

```xlang
break
continue
break 2
continue 2
```

说明：

- 支持跳出/继续多层循环。
- break/continue 与析构、try/finally、async/generator lowering 的交互有覆盖。

return：

- 单返回。
- 多返回。
- finally 中 return 行为有测试。

assert：

```xlang
assert cond
assert cond, "message"
```

throw/try：

```xlang
try
	...
catch err
	...
finally
	...
end
```

变体：

- `try/catch/end`
- `try/catch/finally/end`
- `try/finally/end`
- finally 与 return/break/continue/throw/pending exception 交互有覆盖。

## 10. 错误系统

内建 error：

- `Error(message)`
- `Error(message, code)`
- `Error(message, code, data)`

错误类型族：

- `TypeError`
- `ValueError`
- `IOError`
- `TimeoutError`
- `CancelledError`
- `NotImplementedError`
- `NativeError`

字段/属性：

- `type`
- `message`
- `code`
- `data`
- `stack`
- `cause`

语义：

- `throw error_value`
- sync exception frame。
- async rejected propagation。
- native error bridge 有实现与测试，但当前存在一个红点，见本文档开头。
- `assert` 失败进入错误路径。

诊断：

- lexer/parser/semantic/source line 诊断。
- JSON diagnostics。
- parser 首个错误停止策略。
- 类型 expected/actual。
- overload candidates。
- generic inference error。
- extern unresolved。
- import search path error。
- runtime exception source。
- binding trace / binding missing header / binding error directive。

## 11. 异步、future、task、channel

async/await：

```xlang
async func f() as int
	return 1
end

async func main_async() as int
	auto value = await f()
	return value
end
```

已覆盖场景：

- await 出现在赋值、调用参数、if/while/do-while 条件或主体。
- await string/container 方法结果。
- 多 await。
- async 早返回。
- async try/catch/finally。
- async 嵌套 try/finally、三层嵌套。
- async throw 传播。
- async closure。
- async main 返回 bool/int 等 smoke。

future：

- `future<T>` 类型。
- `std.future.ready(value)` 返回 ready future 语义。
- `std.future.wait(future<T>)` await future。

task：

- `std.task.ready = 1`
- `std.task.done = 2`
- `task.spawn(future<T>)`
- `task.all(future<T>, future<T>)`
- `task.race(future<T>, future<T>)`
- `task.cancel(future<T>)`
- `task.delay(ms)`

说明：

- 当前 `std.task` 是最小语义实现，`all/race/cancel` 更接近第一阶段行为，不是完整生产调度器。
- task/future synchronization 在分支、循环、嵌套流程、merge/select/reassign/ternary/while 等用例中有覆盖。

channel：

```xlang
auto ch = std.channel.make()
std.channel.send(ch, 1)
auto result = std.channel.recv(ch)
```

接口：

- `make()` / `make(capacity)`
- `send(channel, var/int/float/string/bool/error)`
- `recv(channel) as array<var>`
- `len(channel)`
- `capacity(channel)`
- `closed(channel)`
- `close(channel)`

说明：

- 覆盖率文档描述过 `channel.recv()` 返回多结果 `bool, var`；当前 runtime std module 声明为 `array<var>`。DEMO6 设计时需要统一这个口径。

## 12. 生成器

生成器函数：

```xlang
func numbers() as generator<int>
	yield 1
	yield 2
end
```

已覆盖能力：

- `yield` 语句。
- `(yield expr)` 表达式。
- resume value。
- lazy throw。
- generator 参数/状态捕获。
- for-in 直接迭代 generator。
- wrapper generator。
- generator return。
- branch resume。
- while/loop-control。
- try/catch/finally/nested try。
- closure + yield/resume。
- class/object 生命周期穿过 generator 状态机。
- async generator stage1。

限制/错误：

- 顶层 yield 错误。
- 非 generator 上下文 yield 错误。
- generator 参数未知类型等语义错误有覆盖。

## 13. 类、custom、对象

class：

```xlang
class Vec2 extends object
	var x as int
	var y as int

	create(int x, int y)
		self.x = x
		self.y = y
	end

	destroy()
		...
	end

	func sum() as int
		return self.x + self.y
	end
end
```

custom：

```xlang
custom Bag
	var value as int

	func bump() as int
		self.value = self.value + 1
		return self.value
	end
end
```

说明：

- `class` 与 `custom` 都支持字段、方法、继承、create/destroy、属性、operator、index/call 分发。
- `custom` 还用于 XRT payload/resource/dict 等桥接场景。

继承：

- `class Child extends Base`
- `custom Meter extends object`
- 基类变量持有子类对象时支持虚/late dispatch。
- 基类 create chain、base 参数 create、默认 create、继承布局有覆盖。

字段：

```xlang
var name as type
var name as type = expr
int count
const int flag = 1
```

构造/析构：

```xlang
create(...)
	...
end

extern "native_create" create(...)

destroy()
	...
end

extern "native_destroy" destroy()
```

已覆盖生命周期：

- block exit。
- return。
- assignment。
- member assignment。
- loop break/continue。
- throw/assert。
- pending exception。
- try/finally。
- nested try/finally。
- container append/assign/clear/remove/release。
- generator/async state cleanup。
- copy、move、copy elision、use-after-move。
- `nocopy`。

属性：

```xlang
get value() as int
	return ...
end

let value(int next)
	...
end
```

operator overload：

```xlang
func op_add(T rhs) as T
	...
end

func op_eq(T rhs) as bool
	...
end

func op_neg() as T
	...
end

func op_call(int value) as int
	...
end

func op_index(int index) as int
	...
end

func op_index_set(int index, int value) as int
	...
end
```

已见 operator 分发：

- `+` -> `op_add`
- `==` -> `op_eq`
- unary `-` -> `op_neg`
- call -> `op_call`
- index get/set -> `op_index` / `op_index_set`
- object/base 动态分发覆盖。

union：

```xlang
union Value
	var i as int
	var s as string
end
```

已覆盖：

- union lifecycle。
- active tag。
- active destroy。
- inactive read error。

对象运行时能力：

- `std.object.field_names/field_count/...`
- weak reference：`weak`、`alive`、`lock`、`release`。
- refcount/unique。
- collect。
- var 持有 class/container 时的 copy/destroy。

动态类型：

- `std.types.DynamicTypeDesc`
- `std.types.DynamicObject`
- 运行时创建动态类型、添加字段/方法、绑定方法、创建实例、读写动态字段。

## 14. 模块系统

import：

```xlang
import std.io
import std.io as io
import std.io, std.str
import "path/to/module.xl"
import r"path/to/module.xl"
```

已覆盖：

- module path。
- string path。
- raw string path。
- alias。
- import list。
- member/list import 相关 grammar。
- import search path diagnostics。
- std priority。
- `XLANG_HOME`/环境包路径相关测试。

export：

```xlang
export name
export value_expr
export const int answer = 42
export var cache = {}
export func f() as int
	return 1
end
export get value() as int
	return 1
end
export let value(int next)
end
```

模块初始化：

- 顶层执行作为模块 init。
- init 错误传播。
- 模块缓存。
- transitive import。
- private/public libs。
- descriptor/export/init ABI symbol。

模块元信息：

- `std.module.key`
- `abi_id`
- `init_symbol`
- `exports_symbol`
- `descriptor_symbol`
- `export_count`
- `exports`
- `has_export`
- `export_symbol`

## 15. 原生桥、extern、binding、动态库

extern 函数：

```xlang
extern "native_symbol" func add(int a, int b) as int
extern func puts(string text) as int
extern "native_async" async func read() as int
```

extern 方法：

```xlang
extern "native_len" func string.native_len(string self) as int
```

extern 变量/类型：

```xlang
extern "errno" var int errno
extern type NativeInt = int
```

已覆盖：

- extern same name。
- extern async。
- extern var：point/string/bool/float/exact numeric/class 等。
- class ABI layout / nested class ABI。
- unresolved extern diagnostic。

源内指令：

```xlang
#link m
#libpath "../lib"
#incpath "../include"
#loadlib "../bin/native.dll"
#binding "header.h"
```

说明：

- 这些由模块扫描器处理，不是普通 parser statement。
- 指令行处理后会从源码流中剥离或注入 binding 结果。
- `#link` 支持库名；`#libpath/#incpath/#loadlib/#binding` 使用 quoted value。
- `#loadlib` 支持相对模块路径归一化或绝对/平台路径。

binding 头文件能力：

- basic typedef。
- struct / multiline struct / struct declaration。
- enum。
- union opaque。
- opaque type。
- define。
- include。
- extern C。
- forward declaration。
- attribute。
- condition。
- comments。
- alias。
- callback。
- missing header/error directive/trace diagnostics。

动态库 API：

- `std.sys.loadlib(path) as var`
- `std.sys.dlopen(path) as var`
- `std.sys.dlsym(lib, name) as point`
- `std.sys.dlclose(lib) as bool`
- `lib.proc(name) as point`
- `lib.close()`
- `lib.path()`
- `lib.loaded()`

point/function bridge：

- `point` 可桥接成 `function<...>`。
- dlsym signature matrix。
- exact numeric/class ABI/var bridge。
- dlopen/dlclose/path/loaded 覆盖。

链接/平台：

- link auto platform suffix。
- project app/pkgbuild link libs。
- XLE artifact linkLibs/loadLibs。

## 16. XLE 包与 release libs

XLE/package manifest：

- `format: xle-1`
- `package.name`
- `package.version`
- `package.abiVersion`
- `package.xlangAbi`
- `package.abiId`
- `artifact.kind`
- `artifact.path`
- `artifact.cSources`
- `artifact.linkLibs`
- `dependencies`
- `modules`
- `functions`

artifact kind：

- `source`
- `c-source`
- native/static/mixed 等在测试与模块链接代码路径中有覆盖。

已覆盖包能力：

- source package。
- c-source package。
- native/static/mixed package。
- env-only package。
- transitive dependencies。
- multi-version dependencies。
- private/public libs。
- package build。
- source/csource/native/static/mixed examples。

`demo5/release/libs` 当前包目录：

```text
clipboard
crc32
dinov3
imagehash
lz4
lzma
md4c
md5
miniz
onnxruntime
opencv
pkg.math
rapidocr
sqlite
stb_image
stb_image_resize2
stb_image_write
stb_truetype
x.com
x.mail.imap
x.mail.mime
x.mail.pop3
x.mail.smtp
x.redis
x.ui.auto
x.uia
x.volc.core
x.volc.phone
x.volc.sts
xfeat
zstd
```

官方扩展覆盖率文档中已实现/部分实现：

- `x.xml`
- `x.yaml`
- `x.ini`
- `x.csv`
- `x.archive.zip`
- `x.archive.tar`
- `x.compress.gzip`
- `x.resource`
- `x.image.bmp`

覆盖率文档中标记未实现/不完整：

- `x.db.sqlite`
- `x.db.mysql`
- `x.db.redis`
- `x.cache.redis`
- `x.compress.zlib`
- `x.image.png`
- `x.image.jpeg`

注意：

- release libs 中实际已有 `sqlite`、`x.redis`、`opencv`、`onnxruntime`、`rapidocr` 等包目录；这表示 DEMO5 release 包侧已有实验/集成样例，但与覆盖率文档里的“官方扩展完成度”不是同一个口径。DEMO6 需要区分“包存在”“示例可跑”“语言标准承诺”三件事。

## 17. 标准库总览

当前 runtime std modules：

```text
std.array
std.assert
std.atomic
std.channel
std.clock
std.core
std.crypto
std.debug
std.dict
std.dir
std.encode
std.env
std.error
std.fmt
std.fs
std.future
std.hash
std.host
std.http
std.httpd
std.io
std.iter
std.json
std.list
std.log
std.math
std.mime
std.module
std.multipart
std.net
std.net.dns
std.net.ip
std.net.proxy
std.net.tcp
std.net.tls
std.net.udp
std.net.url
std.net.ws
std.object
std.os
std.pack
std.path
std.process
std.query
std.rand
std.range
std.regex
std.set
std.str
std.sync
std.sys
std.sys.env
std.sys.file
std.sys.mem
std.sys.net
std.sys.process
std.sys.wait
std.task
std.template
std.thread
std.time
std.trace
std.types
```

已废弃/迁移口径：

- `std.conv` 移除。
- `std.data` 更名为 `std.pack`。
- `std.json` 只保留核心 encode/decode。
- `std.c.*` 取消。
- `std.xrt.*` 取消。

## 18. 标准库 API 清单

### std.core

```xlang
func identity(int x) as int
func print(int/float/string/bool x) as void
func println() as void
func println(int/float/string/bool x) as void
func len(string/array/list/set/dict x) as int
const int version = 5
```

### std.io

```xlang
func print(int/float/string/bool x) as void
func println() as void
func println(int/float/string/bool x) as void
func eprint(int/float/string/bool x) as void
func eprintln() as void
func eprintln(int/float/string/bool x) as void
func read_line() as string
func flush() as void
```

### std.str

```xlang
func empty(string text) as bool
func byte_len(string text) as int
func contains(string text, string part) as bool
func index_of(string text, string part) as int
func last_index_of(string text, string part) as int
func slice(string text, int start, int count) as string
func repeat(string text, int count) as string
func split(string text, string part) as list
func split_lines(string text) as list
func starts_with(string text, string part) as bool
func ends_with(string text, string part) as bool
func trim/trim_left/trim_right(string text) as string
func lower/upper(string text) as string
func replace(string text, string part, string value) as string
func reverse(string text) as string
func pad_left/pad_right(string text, int width, string fill) as string
```

### std.math

```xlang
func abs(int value) as int
func abs(float value) as float
func min/max(int left, int right) as int
func min/max(float left, float right) as float
func clamp(int value, int low, int high) as int
func clamp(float value, float low, float high) as float
func floor/ceil/round/trunc(float value) as float
func sqrt(int value) as int
func sqrt(float value) as float
func pow(int value, int exp) as int
func pow(float value, float exp) as float
func sin/cos/tan/exp/log/log10(float value) as float
const float pi
const float e
```

### std.rand

```xlang
func int32() as int
func bool() as bool
func range(int min, int max) as int
func text(int len) as string
func text_from(string template, int len) as string
```

### std.time / std.clock

```xlang
std.time.now() as time
std.time.to_string(time value) as string
std.time.now_text/date_text/time_text() as string
std.time.relative(time value, time base) as string
std.time.parse(string value) as time
std.time.format(time value, string pattern) as string
std.time.parse_format(string value, string pattern) as time
std.time.sleep(int ms) as void
std.time.is_leap_year(int year) as bool
std.time.millis_per_second
std.time.seconds_per_day

std.clock.now() as time
std.clock.text/date/time_of_day() as string
std.clock.sleep(int ms) as void
std.clock.millis_per_second
std.clock.seconds_per_day
```

### std.fmt

```xlang
func int_text(int value, string pattern) as string
func float_text(float value, string pattern) as string
func bool_text(bool value) as string
```

### std.regex

```xlang
func is_match(string pattern, string text) as bool
func find(string pattern, string text) as string
func captures(string pattern, string text) as list<string>
func capture_count(string pattern) as int
func replace_first(string pattern, string text, string value) as string
func replace_all(string pattern, string text, string value) as string
```

### std.hash / std.crypto / std.encode / std.pack

```xlang
std.hash.hash32(string text) as int
std.hash.hash64/hash64_micro/hash64_nano(string text) as string
std.hash.sha256/sha512(string text) as string

std.crypto.sha256_hex(string text) as string
std.crypto.hmac_sha256(string key, string text) as string
std.crypto.hmac_sha256_hex(string key, string text) as string
std.crypto.hmac_sha256_hex_key(string hex_key, string text) as string
std.crypto.hmac_sha512(string key, string text) as string

std.encode.hex_encode/hex_decode(string text) as string
std.encode.base64_encode/base64_decode(string text) as string

std.pack.write_u8(int value) as string
std.pack.write_u16_le/write_u16_be(int value) as string
std.pack.write_u32_le/write_u32_be(int value) as string
std.pack.read_u8(string data, int offset) as int
std.pack.read_u16_le/read_u16_be(string data, int offset) as int
std.pack.read_u32_le/read_u32_be(string data, int offset) as int
```

### std.array / std.list / std.set / std.dict / std.iter / std.range

```xlang
std.array.len(array values) as int
std.array.append(array values, var item) as void
std.array.join(array values, string sep) as string
std.array.clear(array values) as void
std.array.remove(array values, int index) as bool

std.list.len(list values) as int
std.list.append(list values, var item) as void
std.list.join(list values, string sep) as string
std.list.clear(list values) as void
std.list.remove(list values, int index) as bool

std.set.len(set values) as int
std.set.has(set values, var item) as bool
std.set.clear(set values) as void
std.set.remove(set values, var item) as bool

std.dict.len(dict values) as int
std.dict.has(dict values, string key) as bool
std.dict.clear(dict values) as void
std.dict.remove(dict values, string key) as bool

std.iter.count(array/list/set/dict values) as int
std.iter.keys(dict values) as array<string>
std.iter.values(dict values) as array<var>
std.iter.items(set values) as array<var>

std.range.collect(int stop) as array<int>
std.range.collect(int start, int stop) as array<int>
std.range.collect(int start, int stop, int step) as array<int>
```

### std.error / std.assert / std.log / std.trace / std.debug

```xlang
std.error.make(string message) as error
std.error.make(string message, int code) as error
std.error.make(string message, int code, string data) as error
std.error.type_error/value_error/io_error/timeout_error/cancelled_error/not_implemented_error/native_error(string message) as error
std.error.is_null(error value) as bool
std.error.kind/message/code/data/stack/cause/text(...)
std.error.is_type(error value, string type_name) as bool
std.error.is_native(error value) as bool

std.assert.that(bool cond) as void
std.assert.that(bool cond, string message) as void
std.assert.fail() / fail(string message)
std.assert.equal(int/float/string/bool left, right, string message) as void

std.log.debug/info/warn/error(string text) as void
std.log.error(error value) as void

std.trace.write(string text) as void
std.trace.error(error value) as void

std.debug.text(error/var value) as string
std.debug.print(error/var value) as void
std.debug.println(error/var value) as void
```

### std.fs / std.path / std.dir / std.env / std.os / std.host / std.process

```xlang
std.fs.exists/is_file/is_dir(string path) as bool
std.fs.size(string path) as int
std.fs.set_size(string path, int size) as bool
std.fs.access_time/change_time(string path) as time
std.fs.read_all(string path) as string
std.fs.write_all/append_all(string path, string text) as bool
std.fs.mkdir_all(string path) as bool
std.fs.copy/move(string src, string dst) as bool
std.fs.remove(string path) as bool

std.path.join(string left, string right) as string
std.path.name_ext/name/ext/dir(string value) as string
std.path.exists/is_abs(string value) as bool
std.path.random(string head, string foot, int len) as string

std.dir.make(string path) as bool
std.dir.list/files/dirs/tree(string path) as string
std.dir.copy/move(string src, string dst) as bool
std.dir.remove(string path) as bool

std.env.read(string name) as string
std.env.exists(string name) as bool
std.env.set(string name, string value) as bool
std.env.cwd/temp/home() as string

std.os.name/arch/user/host_name/executable() as string
std.host.name/ip/user/os/arch/executable() as string

std.process.exec(string command) as int
std.process.capture(string command) as string
std.process.exec_in(string command, string workdir) as int
std.process.capture_in(string command, string workdir) as string
std.process.terminal_supported() as bool
```

### std.future / std.task / std.channel / std.sync / std.atomic / std.thread

```xlang
std.future.ready(int/float/string/bool/error value)
std.future.wait(future<int/float/string/bool/error> value)

std.task.ready
std.task.done
std.task.spawn(future<T> value)
std.task.all(future<T> left, future<T> right)
std.task.race(future<T> left, future<T> right)
std.task.cancel(future<T> value) as int
std.task.delay(int ms) as int

std.channel.make() as channel
std.channel.make(int capacity) as channel
std.channel.send(channel value, var/int/float/string/bool/error item) as bool
std.channel.recv(channel value) as array<var>
std.channel.len/capacity(channel value) as int
std.channel.closed/close(channel value) as bool

std.sync.mutex() as point
std.sync.mutex_free/lock/try_lock/unlock(point value) as bool
std.sync.cond() as point
std.sync.cond_free/wait/wait_timeout/signal/broadcast(...)
std.sync.rwlock() as point
std.sync.rwlock_free/read_lock/try_read_lock/read_unlock/write_lock/try_write_lock/write_unlock/downgrade/upgrade(...)

std.atomic.make(int value) as point
std.atomic.free(point value) as bool
std.atomic.load(point value) as int
std.atomic.store/exchange(point value, int item) as int
std.atomic.compare_exchange(point value, int expected, int desired) as int
std.atomic.add(point value, int delta) as int

std.thread.current_id() as int
std.thread.sleep(int ms) as void
std.thread.yield_now() as void
```

### std.net 系列

```xlang
std.net.resolve(string host) as string

std.net.dns.resolve(string host) as string
std.net.dns.resolve_all(string host) as array<string>
std.net.dns.reverse(string ip) as string

std.net.ip.parse(string text) as dict<string>
std.net.ip.build(dict<string> value) as string
std.net.ip.normalize(string text) as string
std.net.ip.is_ip/is_ipv4/is_ipv6(string text) as bool

std.net.tcp.listen(string host, int port) as point
std.net.tcp.accept(point listener, int timeout_ms) as point
std.net.tcp.connect(string host, int port) as point
std.net.tcp.send(point socket, string text) as int
std.net.tcp.recv(point socket, int size, int timeout_ms) as string
std.net.tcp.local_port(point socket) as int
std.net.tcp.close(point socket) as bool

std.net.udp.bind(string host, int port) as point
std.net.udp.send_to(point socket, string host, int port, string text) as int
std.net.udp.recv_from(point socket, int size, int timeout_ms) as dict<string>
std.net.udp.local_port(point socket) as int
std.net.udp.close(point socket) as bool

std.net.url.encode_query(dict<string> values) as string
std.net.url.decode_query(string text) as dict<string>
std.net.url.parse(string text) as dict<string>
std.net.url.build(dict<string> parts) as string

std.net.tls.versions() as array<string>
std.net.tls.is_tls_url(string url) as bool
std.net.tls.default_port(string scheme) as int
std.net.tls.sni_host(string url) as string
std.net.tls.fingerprint_sha256(string data) as string
std.net.tls.client_config(string host, bool verify) as dict<string>

std.net.proxy.parse(string url) as dict<string>
std.net.proxy.basic_auth(string user, string pass) as string
std.net.proxy.http_connect(string host, int port, string auth) as string
std.net.proxy.socks5_greeting_hex(bool auth) as string
std.net.proxy.socks5_connect_hex(string host, int port) as string

std.net.ws.key() as string
std.net.ws.accept(string key) as string
std.net.ws.request(string host, string path, string key) as string
std.net.ws.response(string key) as string
std.net.ws.frame_text(string text, bool mask) as string
std.net.ws.unframe(string frame) as dict<string>
```

### std.http / std.httpd / std.mime / std.multipart

```xlang
std.http.headers(dict<string> values) as string
std.http.request(string method, string target, dict<string> headers, string body) as string
std.http.response(int status, string reason, dict<string> headers, string body) as string
std.http.parse_headers(string text) as dict<string>

std.httpd.request(string text) as dict<string>
std.httpd.match(string pattern, string path) as dict<string>
std.httpd.response(int status, dict<string> headers, string body) as string
std.httpd.text(int status, string body) as string

std.mime.by_ext(string ext) as string
std.mime.by_path(string path) as string
std.mime.is_text(string mime) as bool
std.mime.parse(string text) as dict<string>
std.mime.build(dict<string> value) as string

std.multipart.boundary(string content_type) as string
std.multipart.parse(string text, string boundary) as array<dict<string>>
std.multipart.build(array<dict> parts, string boundary) as string
```

### std.json / std.query / std.template

```xlang
std.json.encode(var value) as string
std.json.decode(string text) as var

std.query.encode(dict<string> values) as string
std.query.decode(string text) as dict<string>

std.template.render(string template, dict<string> values) as string
```

### std.sys 系列

```xlang
std.sys.loadlib(string path) as var
std.sys.dlopen(string path) as var
std.sys.dlsym(var self, string name) as point
std.sys.dlclose(var self) as bool
var.proc(var self, string name) as point
var.close(var self) as bool
var.path(var self) as string
var.loaded(var self) as bool

std.sys.env 与 std.env 同组能力
std.sys.file.exists/is_file/is_dir/size/read_all/write_all/append_all/remove
std.sys.mem.alloc(int size) as point
std.sys.mem.free(point value) as bool
std.sys.mem.is_null(point value) as bool
std.sys.mem.fill(point value, int byte_value, int size) as bool
std.sys.mem.copy(point dst, point src, int size) as bool
std.sys.mem.write_string(point value, string text) as int
std.sys.mem.read_string(point value, int size) as string
std.sys.net.resolve(string host) as string
std.sys.process 与 std.process 同组能力
std.sys.wait.sleep(int ms) as void
std.sys.wait.delay(int ms) as int
std.sys.wait.wait(future<T> value)
```

### std.object / std.types / std.module

```xlang
std.object.field_names(var value) as array<string>
std.object.field_count(var value) as int
std.object.field_name_at(var value, int index, string default_value) as string
std.object.has_field(var value, string field_name) as bool
std.object.method_names(var value) as array<string>
std.object.method_count(var value) as int
std.object.method_name_at(var value, int index, string default_value) as string
std.object.has_method(var value, string method_name) as bool
std.object.weak(var value) as point
std.object.alive(point value) as bool
std.object.lock(point value) as var
std.object.release(point value) as bool
std.object.refcount(var value) as int
std.object.unique(var value) as bool
std.object.collect() as int

class std.types.DynamicTypeDesc extends object
class std.types.DynamicObject extends object
std.types.of(var value) as type
std.types.name(type value) as string
std.types.same(type left, type right) as bool
std.types.dynamic(string type_name, type base_type) as DynamicTypeDesc
std.types.add_field/add_method/bind_method/method_impl(...)
std.types.field_names/method_names/field_count/method_count(...)
std.types.new_instance(DynamicTypeDesc desc) as DynamicObject
std.types.is_instance(var value) as bool
std.types.instance_* APIs for field/method/name/base/desc/get/set

std.module.key(module value) as string
std.module.abi_id(module value) as string
std.module.init_symbol/exports_symbol/descriptor_symbol(module value) as string
std.module.export_count(module value) as int
std.module.exports(module value) as array<var>
std.module.has_export(module value, string name) as bool
std.module.export_symbol(module value, string name) as string
```

## 19. 调试与 DAP

`xldbg` 已覆盖：

- breakpoint。
- step-in。
- next。
- step-out。
- frame variables。
- terminated state。
- 生成 C 与 source 映射相关 smoke。

DAP probe V1：

- 有基础协议/探针测试。
- 当前边界更偏 smoke/probe，不应当在 DEMO6 直接按完整 IDE 级 DAP 兼容承诺。

## 20. 编译与语义错误覆盖面

DEMO5 对“错误应该报错”也覆盖了很多：

- const 赋值。
- const 复合赋值。
- auto 参数/返回。
- await outside async。
- bare indexed list 错误。
- bit ops 类型错误。
- coalesce 类型错误。
- container method 类型错误。
- delete 非对象/非 lvalue。
- use-after-delete。
- extern nested/type unknown/unresolved。
- implicit conversion 错误：
  - any/var -> int。
  - bool/number。
  - float -> int。
  - default/assign/return 中的窄化。
  - generic no widen inference。
  - number condition in if/while。
  - null -> bool/int。
  - object conversion/upcast。
  - string + number。
  - string -> number。
  - overload ambiguity。
- legacy link error。
- logic/not 类型错误。
- multi_return_single_bind_error。
- new_requires_call_error。
- point ctor error。
- property length error。
- yield top-level/value error。
- generator param unknown。

## 21. DEMO6 取舍时必须显式决策的点

下面这些不是 DEMO5 缺陷结论，而是重构时最容易踩兼容坑的地方：

- `***` 文档口径与当前 parser 的 `**` dict pack 口径需要统一。
- `channel.recv()` 文档里的 `bool, var` 与 runtime 声明的 `array<var>` 需要统一。
- `include` 是保留字但主语法未落地，需要决定是否删除、保留或实现。
- `#` 行首指令与 `#...#` time literal 共存，需要决定 DEMO6 是否继续让模块扫描器在 parser 前处理指令。
- `bytes` 作为 `string` 兼容别名是否保留。
- `int/int32/int64`、`float/float64` 等精确数值别名/转换边界是否重新设计。
- `var` 动态能力、对象 late dispatch、dynamic type API 是否进入正式标准，还是仅作为实验库。
- `std.task` 当前是最小实现，DEMO6 如果是 release candidate 需要决定调度语义与取消语义是否升级。
- `std.sys` 与 `std.env/fs/process/net` 有明显重叠，是否合并或保留双入口。
- `std.clock` 与 `std.time` 有重叠，是否保留。
- `std.host` 与 `std.os` 有重叠，是否保留。
- `class` 与 `custom` 边界需要明确：payload/resource 能力、XRT 字典能力、动态字段能力是否都应进入正式对象模型。
- `operator overload` 当前依赖 `op_*` 命名约定，DEMO6 是否改为显式 operator syntax。
- `range(...)` for-in 与 `std.range.collect(...)` 的命名/归属需要统一。
- XLE release libs 里“包存在”和覆盖率文档里“官方扩展完成”不是同一层级，需要为 DEMO6 定义包成熟度标签。

## 22. 快速回查索引

推荐回查顺序：

1. `demo5/README.md`：当前主口径总览。
2. `demo5/docs/README.md`：当前文档边界说明。
3. `dev/xlang第五版语言标准支持覆盖率.txt`：语言标准覆盖。
4. `dev/xlang第五版增强标准库支持覆盖率.txt`：增强标准库覆盖。
5. `demo5/src/lexer/xl_lexer.l`：词法真实口径。
6. `demo5/src/parser/xl_parser.y`：语法真实口径。
7. `demo5/src/runtime/xl_std_*.c`：标准库导出真实口径。
8. `demo5/release/examples/full`：功能示例。
9. `demo5/test/xl_test.c`：回归与错误边界。

## 23. 附录：标准库源码声明逐项清单

本附录从 `demo5/src/runtime/xl_std_*.c` 的模块源码字符串抽取，用于避免主文档中的合并写法造成遗漏。

### std.array

```xlang
func len(array values) as int
func append(array values, var item) as void
func join(array values, string sep) as string
func clear(array values) as void
func remove(array values, int index) as bool
```

### std.assert

```xlang
func that(bool cond) as void
func that(bool cond, string message) as void
func fail() as void
func fail(string message) as void
func equal(int left, int right, string message) as void
func equal(float left, float right, string message) as void
func equal(string left, string right, string message) as void
func equal(bool left, bool right, string message) as void
```

### std.atomic

```xlang
func make(int value) as point
func free(point value) as bool
func load(point value) as int
func store(point value, int item) as int
func exchange(point value, int item) as int
func compare_exchange(point value, int expected, int desired) as int
func add(point value, int delta) as int
```

### std.channel

```xlang
func make() as channel
func make(int capacity) as channel
func send(channel value, var item) as bool
func send(channel value, int item) as bool
func send(channel value, float item) as bool
func send(channel value, string item) as bool
func send(channel value, bool item) as bool
func send(channel value, error item) as bool
func recv(channel value) as array<var>
func len(channel value) as int
func capacity(channel value) as int
func closed(channel value) as bool
func close(channel value) as bool
```

### std.clock

```xlang
func now() as time
func text() as string
func date() as string
func time_of_day() as string
func sleep(int ms) as void
const int millis_per_second = 1000
const int seconds_per_day = 86400
```

### std.core

```xlang
func identity(int x) as int
func print(int x) as void
func print(float x) as void
func print(string x) as void
func print(bool x) as void
func println() as void
func println(int x) as void
func println(float x) as void
func println(string x) as void
func println(bool x) as void
func len(string x) as int
func len(array x) as int
func len(list x) as int
func len(set x) as int
func len(dict x) as int
const int version = 5
```

### std.crypto

```xlang
func sha256_hex(string text) as string
func hmac_sha256(string key, string text) as string
func hmac_sha256_hex(string key, string text) as string
func hmac_sha256_hex_key(string hex_key, string text) as string
func hmac_sha512(string key, string text) as string
```

### std.debug

```xlang
func text(error value) as string
func text(var value) as string
func print(error value) as void
func print(var value) as void
func println(error value) as void
func println(var value) as void
```

### std.dict

```xlang
func len(dict values) as int
func has(dict values, string key) as bool
func clear(dict values) as void
func remove(dict values, string key) as bool
```

### std.dir

```xlang
func make(string path) as bool
func list(string path) as string
func files(string path) as string
func dirs(string path) as string
func tree(string path) as string
func copy(string src, string dst) as bool
func move(string src, string dst) as bool
func remove(string path) as bool
```

### std.encode

```xlang
func hex_encode(string text) as string
func hex_decode(string text) as string
func base64_encode(string text) as string
func base64_decode(string text) as string
```

### std.env

```xlang
func read(string name) as string
func exists(string name) as bool
func set(string name, string value) as bool
func cwd() as string
func temp() as string
func home() as string
```

### std.error

```xlang
func make(string message) as error
func make(string message, int code) as error
func make(string message, int code, string data) as error
func type_error(string message) as error
func value_error(string message) as error
func io_error(string message) as error
func timeout_error(string message) as error
func cancelled_error(string message) as error
func not_implemented_error(string message) as error
func native_error(string message) as error
func is_null(error value) as bool
func kind(error value) as string
func message(error value) as string
func code(error value) as int
func data(error value) as string
func stack(error value) as string
func cause(error value) as error
func text(error value) as string
func is_type(error value, string type_name) as bool
func is_native(error value) as bool
```

### std.fmt

```xlang
func int_text(int value, string pattern) as string
func float_text(float value, string pattern) as string
func bool_text(bool value) as string
```

### std.fs

```xlang
func exists(string path) as bool
func is_file(string path) as bool
func is_dir(string path) as bool
func size(string path) as int
func set_size(string path, int size) as bool
func access_time(string path) as time
func change_time(string path) as time
func read_all(string path) as string
func write_all(string path, string text) as bool
func append_all(string path, string text) as bool
func mkdir_all(string path) as bool
func copy(string src, string dst) as bool
func move(string src, string dst) as bool
func remove(string path) as bool
```

### std.future

```xlang
async func ready(int value) as int
async func ready(float value) as float
async func ready(string value) as string
async func ready(bool value) as bool
async func ready(error value) as error
async func wait(future<int> value) as int
async func wait(future<float> value) as float
async func wait(future<string> value) as string
async func wait(future<bool> value) as bool
async func wait(future<error> value) as error
```

### std.hash

```xlang
func hash32(string text) as int
func hash64(string text) as string
func hash64_micro(string text) as string
func hash64_nano(string text) as string
func sha256(string text) as string
func sha512(string text) as string
```

### std.host

```xlang
func name() as string
func ip() as string
func user() as string
func os() as string
func arch() as string
func executable() as string
```

### std.http

```xlang
func headers(dict<string> values) as string
func request(string method, string target, dict<string> headers, string body) as string
func response(int status, string reason, dict<string> headers, string body) as string
func parse_headers(string text) as dict<string>
```

### std.httpd

```xlang
func request(string text) as dict<string>
func match(string pattern, string path) as dict<string>
func response(int status, dict<string> headers, string body) as string
func text(int status, string body) as string
```

### std.io

```xlang
func print(int x) as void
func print(float x) as void
func print(string x) as void
func print(bool x) as void
func println() as void
func println(int x) as void
func println(float x) as void
func println(string x) as void
func println(bool x) as void
func eprint(int x) as void
func eprint(float x) as void
func eprint(string x) as void
func eprint(bool x) as void
func eprintln() as void
func eprintln(int x) as void
func eprintln(float x) as void
func eprintln(string x) as void
func eprintln(bool x) as void
func read_line() as string
func flush() as void
```

### std.iter

```xlang
func count(array values) as int
func count(list values) as int
func count(set values) as int
func count(dict values) as int
func keys(dict values) as array<string>
func values(dict values) as array<var>
func items(set values) as array<var>
```

### std.json

```xlang
func encode(var value) as string
func decode(string text) as var
```

### std.list

```xlang
func len(list values) as int
func append(list values, var item) as void
func join(list values, string sep) as string
func clear(list values) as void
func remove(list values, int index) as bool
```

### std.log

```xlang
func debug(string text) as void
func info(string text) as void
func warn(string text) as void
func error(string text) as void
func error(error value) as void
```

### std.math

```xlang
func abs(int value) as int
func abs(float value) as float
func min(int left, int right) as int
func min(float left, float right) as float
func max(int left, int right) as int
func max(float left, float right) as float
func clamp(int value, int low, int high) as int
func clamp(float value, float low, float high) as float
func floor(float value) as float
func ceil(float value) as float
func round(float value) as float
func trunc(float value) as float
func sqrt(int value) as int
func sqrt(float value) as float
func pow(int value, int exp) as int
func pow(float value, float exp) as float
func sin(float value) as float
func cos(float value) as float
func tan(float value) as float
func exp(float value) as float
func log(float value) as float
func log10(float value) as float
const float pi = 3.141592653589793
const float e = 2.718281828459045
```

### std.mime

```xlang
func by_ext(string ext) as string
func by_path(string path) as string
func is_text(string mime) as bool
func parse(string text) as dict<string>
func build(dict<string> value) as string
```

### std.module

```xlang
func key(module value) as string
func abi_id(module value) as string
func init_symbol(module value) as string
func exports_symbol(module value) as string
func descriptor_symbol(module value) as string
func export_count(module value) as int
func exports(module value) as array<var>
func has_export(module value, string name) as bool
func export_symbol(module value, string name) as string
```

### std.multipart

```xlang
func boundary(string content_type) as string
func parse(string text, string boundary) as array<dict<string> >
func build(array<dict> parts, string boundary) as string
```

### std.net

```xlang
func resolve(string host) as string
```

### std.net.dns

```xlang
func resolve(string host) as string
func resolve_all(string host) as array<string>
func reverse(string ip) as string
```

### std.net.ip

```xlang
func parse(string text) as dict<string>
func build(dict<string> value) as string
func normalize(string text) as string
func is_ip(string text) as bool
func is_ipv4(string text) as bool
func is_ipv6(string text) as bool
```

### std.net.proxy

```xlang
func parse(string url) as dict<string>
func basic_auth(string user, string pass) as string
func http_connect(string host, int port, string auth) as string
func socks5_greeting_hex(bool auth) as string
func socks5_connect_hex(string host, int port) as string
```

### std.net.tcp

```xlang
func listen(string host, int port) as point
func accept(point listener, int timeout_ms) as point
func connect(string host, int port) as point
func send(point socket, string text) as int
func recv(point socket, int size, int timeout_ms) as string
func local_port(point socket) as int
func close(point socket) as bool
```

### std.net.tls

```xlang
func versions() as array<string>
func is_tls_url(string url) as bool
func default_port(string scheme) as int
func sni_host(string url) as string
func fingerprint_sha256(string data) as string
func client_config(string host, bool verify) as dict<string>
```

### std.net.udp

```xlang
func bind(string host, int port) as point
func send_to(point socket, string host, int port, string text) as int
func recv_from(point socket, int size, int timeout_ms) as dict<string>
func local_port(point socket) as int
func close(point socket) as bool
```

### std.net.url

```xlang
func encode_query(dict<string> values) as string
func decode_query(string text) as dict<string>
func parse(string text) as dict<string>
func build(dict<string> parts) as string
```

### std.net.ws

```xlang
func key() as string
func accept(string key) as string
func request(string host, string path, string key) as string
func response(string key) as string
func frame_text(string text, bool mask) as string
func unframe(string frame) as dict<string>
```

### std.object

```xlang
func field_names(var value) as array<string>
func field_count(var value) as int
func field_name_at(var value, int index, string default_value) as string
func has_field(var value, string field_name) as bool
func method_names(var value) as array<string>
func method_count(var value) as int
func method_name_at(var value, int index, string default_value) as string
func has_method(var value, string method_name) as bool
func weak(var value) as point
func alive(point value) as bool
func lock(point value) as var
func release(point value) as bool
func refcount(var value) as int
func unique(var value) as bool
func collect() as int
```

### std.os

```xlang
func name() as string
func arch() as string
func user() as string
func host_name() as string
func executable() as string
```

### std.pack

```xlang
func write_u8(int value) as string
func write_u16_le(int value) as string
func write_u16_be(int value) as string
func write_u32_le(int value) as string
func write_u32_be(int value) as string
func read_u8(string data, int offset) as int
func read_u16_le(string data, int offset) as int
func read_u16_be(string data, int offset) as int
func read_u32_le(string data, int offset) as int
func read_u32_be(string data, int offset) as int
```

### std.path

```xlang
func join(string left, string right) as string
func name_ext(string value) as string
func name(string value) as string
func ext(string value) as string
func dir(string value) as string
func exists(string value) as bool
func is_abs(string value) as bool
func random(string head, string foot, int len) as string
```

### std.process

```xlang
func exec(string command) as int
func capture(string command) as string
func exec_in(string command, string workdir) as int
func capture_in(string command, string workdir) as string
func terminal_supported() as bool
```

### std.query

```xlang
func encode(dict<string> values) as string
func decode(string text) as dict<string>
```

### std.rand

```xlang
func int32() as int
func bool() as bool
func range(int min, int max) as int
func text(int len) as string
func text_from(string template, int len) as string
```

### std.range

```xlang
func collect(int stop) as array<int>
func collect(int start, int stop) as array<int>
func collect(int start, int stop, int step) as array<int>
```

### std.regex

```xlang
func is_match(string pattern, string text) as bool
func find(string pattern, string text) as string
func captures(string pattern, string text) as list<string>
func capture_count(string pattern) as int
func replace_first(string pattern, string text, string value) as string
func replace_all(string pattern, string text, string value) as string
```

### std.set

```xlang
func len(set values) as int
func has(set values, var item) as bool
func clear(set values) as void
func remove(set values, var item) as bool
```

### std.str

```xlang
func empty(string text) as bool
func byte_len(string text) as int
func contains(string text, string part) as bool
func index_of(string text, string part) as int
func last_index_of(string text, string part) as int
func slice(string text, int start, int count) as string
func repeat(string text, int count) as string
func split(string text, string part) as list
func split_lines(string text) as list
func starts_with(string text, string part) as bool
func ends_with(string text, string part) as bool
func trim(string text) as string
func trim_left(string text) as string
func trim_right(string text) as string
func lower(string text) as string
func upper(string text) as string
func replace(string text, string part, string value) as string
func reverse(string text) as string
func pad_left(string text, int width, string fill) as string
func pad_right(string text, int width, string fill) as string
```

### std.sync

```xlang
func mutex() as point
func mutex_free(point value) as bool
func mutex_lock(point value) as bool
func mutex_try_lock(point value) as bool
func mutex_unlock(point value) as bool
func cond() as point
func cond_free(point value) as bool
func cond_wait(point value, point mutex_value) as bool
func cond_wait_timeout(point value, point mutex_value, int ms) as bool
func cond_signal(point value) as bool
func cond_broadcast(point value) as bool
func rwlock() as point
func rwlock_free(point value) as bool
func rwlock_read_lock(point value) as bool
func rwlock_try_read_lock(point value) as bool
func rwlock_read_unlock(point value) as bool
func rwlock_write_lock(point value) as bool
func rwlock_try_write_lock(point value) as bool
func rwlock_write_unlock(point value) as bool
func rwlock_downgrade(point value) as bool
func rwlock_upgrade(point value) as bool
```

### std.sys

```xlang
func loadlib(string path) as var
func dlopen(string path) as var
func dlsym(var self, string name) as point
func dlclose(var self) as bool
func var.proc(var self, string name) as point
func var.close(var self) as bool
func var.path(var self) as string
func var.loaded(var self) as bool
```

### std.sys.env

```xlang
func read(string name) as string
func exists(string name) as bool
func set(string name, string value) as bool
func cwd() as string
func temp() as string
func home() as string
```

### std.sys.file

```xlang
func exists(string path) as bool
func is_file(string path) as bool
func is_dir(string path) as bool
func size(string path) as int
func read_all(string path) as string
func write_all(string path, string text) as bool
func append_all(string path, string text) as bool
func remove(string path) as bool
```

### std.sys.mem

```xlang
func alloc(int size) as point
func free(point value) as bool
func is_null(point value) as bool
func fill(point value, int byte_value, int size) as bool
func copy(point dst, point src, int size) as bool
func write_string(point value, string text) as int
func read_string(point value, int size) as string
```

### std.sys.net

```xlang
func resolve(string host) as string
```

### std.sys.process

```xlang
func exec(string command) as int
func capture(string command) as string
func exec_in(string command, string workdir) as int
func capture_in(string command, string workdir) as string
func terminal_supported() as bool
```

### std.sys.wait

```xlang
func sleep(int ms) as void
async func delay(int ms) as int
async func wait(future<int> value) as int
async func wait(future<float> value) as float
async func wait(future<string> value) as string
async func wait(future<bool> value) as bool
async func wait(future<error> value) as error
```

### std.task

```xlang
const int ready = 1
const int done = 2
async func spawn(future<int> value) as int
async func spawn(future<float> value) as float
async func spawn(future<string> value) as string
async func spawn(future<bool> value) as bool
async func spawn(future<error> value) as error
async func all(future<int> left, future<int> right) as int
async func all(future<float> left, future<float> right) as float
async func all(future<string> left, future<string> right) as string
async func all(future<bool> left, future<bool> right) as bool
async func all(future<error> left, future<error> right) as error
async func race(future<int> left, future<int> right) as int
async func race(future<float> left, future<float> right) as float
async func race(future<string> left, future<string> right) as string
async func race(future<bool> left, future<bool> right) as bool
async func race(future<error> left, future<error> right) as error
func cancel(future<int> value) as int
func cancel(future<float> value) as int
func cancel(future<string> value) as int
func cancel(future<bool> value) as int
func cancel(future<error> value) as int
async func delay(int ms) as int
```

### std.template

```xlang
func render(string template, dict<string> values) as string
```

### std.thread

```xlang
func current_id() as int
func sleep(int ms) as void
func yield_now() as void
```

### std.time

```xlang
func now() as time
func to_string(time value) as string
func now_text() as string
func date_text() as string
func time_text() as string
func relative(time value, time base) as string
func parse(string value) as time
func format(time value, string pattern) as string
func parse_format(string value, string pattern) as time
func sleep(int ms) as void
func is_leap_year(int year) as bool
const int millis_per_second = 1000
const int seconds_per_day = 86400
```

### std.trace

```xlang
func write(string text) as void
func error(error value) as void
```

### std.types

```xlang
class DynamicTypeDesc extends object
class DynamicObject extends object
func of(var value) as type
func name(type value) as string
func same(type left, type right) as bool
func dynamic(string type_name, type base_type) as DynamicTypeDesc
func is_dynamic(DynamicTypeDesc desc) as bool
func dynamic_name(DynamicTypeDesc desc) as string
func dynamic_base(DynamicTypeDesc desc) as type
func add_field(DynamicTypeDesc desc, string field_name, type field_type) as DynamicTypeDesc
func add_method(DynamicTypeDesc desc, string method_name) as DynamicTypeDesc
func bind_method(DynamicTypeDesc desc, string method_name, var method_value) as DynamicTypeDesc
func method_impl(DynamicTypeDesc desc, string method_name, var default_value) as var
func has_bound_method(DynamicTypeDesc desc, string method_name) as bool
func bound_method_names(DynamicTypeDesc desc) as array<string>
func bound_method_count(DynamicTypeDesc desc) as int
func bound_method_name_at(DynamicTypeDesc desc, int index, string default_value) as string
func has_declared_field(DynamicTypeDesc desc, string field_name) as bool
func has_declared_method(DynamicTypeDesc desc, string method_name) as bool
func field_names(DynamicTypeDesc desc) as array<string>
func method_names(DynamicTypeDesc desc) as array<string>
func field_count(DynamicTypeDesc desc) as int
func method_count(DynamicTypeDesc desc) as int
func field_name_at(DynamicTypeDesc desc, int index, string default_value) as string
func field_type_name(DynamicTypeDesc desc, string field_name, string default_value) as string
func field_type_name_at(DynamicTypeDesc desc, int index, string default_value) as string
func method_name_at(DynamicTypeDesc desc, int index, string default_value) as string
func new_instance(DynamicTypeDesc desc) as DynamicObject
func is_instance(var value) as bool
func instance_name(DynamicObject value) as string
func instance_base(DynamicObject value) as type
func instance_desc(DynamicObject value) as DynamicTypeDesc
func instance_has_field(DynamicObject value, string field_name) as bool
func instance_field_names(DynamicObject value) as array<string>
func instance_field_count(DynamicObject value) as int
func instance_has_method(DynamicObject value, string method_name) as bool
func instance_method_names(DynamicObject value) as array<string>
func instance_method_count(DynamicObject value) as int
func instance_method_bound(DynamicObject value, string method_name) as bool
func instance_bound_method_names(DynamicObject value) as array<string>
func instance_bound_method_count(DynamicObject value) as int
func instance_bound_method_name_at(DynamicObject value, int index, string default_value) as string
func instance_method(DynamicObject value, string method_name, var default_value) as var
func instance_field_name_at(DynamicObject value, int index, string default_value) as string
func instance_field_type_name(DynamicObject value, string field_name, string default_value) as string
func instance_field_type_name_at(DynamicObject value, int index, string default_value) as string
func instance_method_name_at(DynamicObject value, int index, string default_value) as string
func set_field(DynamicObject value, string field_name, var field_value) as DynamicObject
func get_field(DynamicObject value, string field_name, var default_value) as var
```
