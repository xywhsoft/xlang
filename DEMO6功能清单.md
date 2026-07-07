# DEMO6 功能清单

> 更新日期：2026-07-07
>
> 修订说明：本次根据 `demo6` 源码、`tools/check_*.ps1` 守门脚本和 `xl.exe` 实际 help 文本对齐口径。原 `xl xapi` 命令、`xlang-xapi-1` schema、`.xapi` 产物已不存在；接口元数据能力已并入 `abi` 阶段（`.abi.json` / `.abi.txt`），仅承载 C ABI 导出符号。所有数字基线更新到当前实测值。
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
- 提供 package/import/ABI 元数据/扩展库基础链路。
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
xl lex <file.xl> [--format text|json] [--emit output.tokens]
xl parse <file.xl> [--format tree|json] [--emit output.ast]
xl check <file.xl> [--json]
xl ir <file.xl> [--format text|json] [--emit output.xlir]
xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]
xl abi <file.xl> [--json] [--emit output.abi.txt]
xl run <file.xl|file.c|file.xlp> [helper.c ...] [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]
xl dbg <file.xl> [--break line] [--stop-on-entry] [--commands file] [--save-temps] [-- <args>...]
xl dap
xl build <file.xl|file.c|file.xlp> [helper.c ...] [-o <output.exe|dll>] [-shared] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]
xl impdef <file.dll> -o <output.def>
```

命令行为：

- `lex`：输出 token 文本或 JSON。
- `parse`：输出 AST 文本树或 JSON。
- `check`：只执行语义检查，可输出 JSON 诊断。
- `ir`：输出中间 IR 文本或 JSON。
- `cgen`：只生成 C，可同时生成头文件。
- `cgen --dump-dir`：一次性输出 token、AST、IR、ABI、C、H 等阶段产物。
- `abi`：输出 ABI 信息（C ABI 导出符号表，含 export 函数/变量/常量、extern 声明和类的 export 方法）。
- `run`：生成 C 后挂载到动态 VFS，调用内嵌 TCC 编译到内存并直接调用 `xlang_main(xvalue arg)`；不生成临时 `app.exe`，也不启动独立子进程执行脚本。
- `run -- <args>...`：把命令行参数传给 xlang 程序。
- `run --save-temps`：显式导出动态 VFS 内的 `module.c`、`module.h`、`module.abi.json`、`csrc_N.c` 等调试产物。
- `build`：生成 native 可执行程序；省略 `-o` 时，默认在当前工作目录使用第一个输入文件的文件名生成输出，Windows 为 `文件名.exe`，Linux 为 `文件名`；显式提供 `-o` 时按 `-o` 路径输出。
- `build`：生成 exe。
- `build -shared`：进入共享库/DLL 构建边界。
- `build --emit-header`：输出供 C 调用的头文件。
- `dbg`：启动调试器运行脚本，支持 `--break`、`--stop-on-entry`、`--commands` 等。
- `dap`：启动 DAP（Debug Adapter Protocol）服务，供编辑器集成。
- `impdef`：从 DLL 生成 import definition（`.def`）。
- `--inc-path`、`--lib-path`、`--link`、`--loadlib`：向 TCC 后端注入 include、lib、链接库和运行库。

内嵌 TCC 规则：

- TCC 文件查找顺序固定为：动态 VFS -> 外部文件系统 -> 静态 VFS。
- 动态 VFS 保存本次编译刚生成的 C 源、生成头和 C 混编 wrapper，优先级最高。
- 外部文件系统优先于静态 VFS，允许用户用外部头文件、库文件、def 文件覆盖内置兜底资源。
- 静态 VFS 分为运行清单和编译清单：`xl.dll` 只携带脚本运行所需的 TCC include 与 libtcc1 资源，`xl.exe` / `xlc.dll` 携带完整编译兜底资源。
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
for in is while do
break continue return
async await yield
try catch finally throw assert
import export extern include define
class extends union derive get let
public private protected static native virtual override final
and or not
```

上下文关键字：

- `self`
- `base`
- `create`
- `destroy`
- `object` 不是保留字，它是标准库注入的普通基础类名。

## 4. 字面量

整数：

```xlang
123
1_000_000
0x2A
0o52
0b101010
```

整数前缀只保留小写规范形式：`0x`、`0o`、`0b`。`0X`、`0O`、`0B` 以及 `&H`、`&O`、`&B` 等 VB 风格前缀不属于 DEMO6 语法。

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
f"hello {name}"
rf"c:\tmp\{name}.txt"
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
- f-string 已实现 `{expr}` 表达式插值，支持 `{{` / `}}` 字面量大括号和 `rf` raw f-string。
- raw f-string 使用 `rf"..."` / `rf'''...'''` 形式；插值表达式仍会解析，普通转义不处理。
- f-string 中空表达式、未闭合大括号、错误大括号配对会产生解析错误。
- 历史 `0X` / `&H` / `&B` 等写法在语言中不存在，不提供兼容或专门友好提示。

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
future<T>
generator<T>
channel<T>
```

用户定义类型：

```text
class
object
native-backed class
union
typedef alias
```

函数值类型：

```xlang
func(int) as int proc
func(string, int) as (int, string) mapper
func(int, ...) as int varargProc
func(string, ***) as int namedVarargProc
```

类型相关能力：

- 内置类型通过类型注册系统进入语义层。
- `auto` 是局部推断关键字。
- `var` 是动态值类型。
- `type` 是真实的类型值类型。
- `type(expr)` 可以取得表达式类型值。
- 类型标识符可作为类型值参与比较。
- `typedef` 支持类型别名。
- 泛型参数支持 `<T is Constraint>` 约束语法。
- 多个泛型约束使用 `and` 组合，例如 `<T is json and cloneable>`。
- 泛型约束支持内置能力约束和 class 继承约束，并在实例化时执行语义检查。
- 泛型容器支持静态元素类型。
- `future<T>`、`generator<T>`、`channel<T>` 服务 async/yield/channel 能力。
- `object` 是标准库普通 class，不是特殊黑盒类型。
- `void` 不属于 DEMO6 类型系统。

运行时类型判断与向下转换：

```xlang
var value = 123

if value is int then
    int n = value as int
end

class Base
end

class Child extends Base
end

Child child
var boxed = child

if boxed is Base then
    Base base = boxed as Base
end
```

已实现规则：

- `expr is Type` 返回 `bool`。
- `is` 用于判断运行时值是否满足目标类型。
- `is` 对非 `var` 的静态表达式会尽量在编译期折叠为常量。
- `is` 支持基础类型、`null`、class 继承关系、union、native-backed class、函数签名类型和静态泛型容器 descriptor。
- `is` 判断 class 时支持 `Child is Base`。
- `is` 判断函数值时保留完整 `func(...) as ...` 签名，不退化为裸 `function`。
- `is` 判断 `array<T>`、`list<T>`、`set<T>`、`dict<T>` 时使用静态泛型容器 descriptor，`array<int>` 与 `array<string>` 不相等。
- `expr as Type` 表示显式 checked downcast。
- `as` 成功时返回目标类型的静态值。
- `as` 失败时触发运行时 fatal，当前阶段不返回 nullable/option。
- `as` 当前支持从 `var` 转出基础值、`string`、`time`、`point`、`type`、class、union、native-backed class。
- `as` 也允许目标类型与源静态类型完全一致的 no-op 写法。
- `as` 不用于普通数值转换；普通转换继续使用 `int(expr)`、`float(expr)`、`string(expr)` 等构造/转换函数。
- `is auto`、`is var`、`as auto`、`as var` 不合法。
- `as null` 不合法，`is null` 合法。

当前边界：

- `type(expr)` 仍表示精确类型身份，不执行 subtype 判断。
- `is/as` 不改变 `type()` 的语义。
- 当前不实现控制流类型收窄，例如 `if v is int then` 内部不会自动把 `v` 改写为 `int`。
- 当前不支持 `as array<T>`、`as list<T>`、`as set<T>`、`as dict<T>` 转出静态泛型容器；容器 downcast 后续需要结合 XRT typed container 生命周期继续设计。

## 6. 变量、常量和初始化

基本写法：

```xlang
int a
int b = 1
var v = 123
auto c = b + 1
const int max = 10
export int exportedCounter = 1
export var exportedValue = "hello"
export const int exportedMax = 10
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
- 支持模块级导出变量和常量，写法为 `export Type name = value`、`export var name = value`、`export const Type name = value`。
- `var` 在变量声明中只表示动态类型，不作为 `extern/export` 变量声明的额外标记词。

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

func sum(...) as int
    return arg.length
end

func open(string path, ***) as var
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
- `func foo() -> int` 不属于 DEMO6 语法；函数、函数类型、匿名函数统一使用 `as`。
- 函数值类型使用 `func(...) as T`，不使用完整单词 `function` 作为签名写法。
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
- typed container 可承载基础类型、class、native-backed class、union、function 值。
- `list` 使用 int64 稀疏索引，字面量写作 `[index: value]`。
- 容器支持索引读写、索引复合赋值和安全索引 `?[...]`。
- 容器赋值和进入 `var` 时保持当前实现的复制/引用计数语义，避免裸 C 指针泄漏。

for-in 示例：

```xlang
for value in [1, 2, 3]
    print(value)
end

for index, value in [0: "a", 2: "b"]
    print(index, value)
end

for key, value in {"a": 1, "b": 2}
    print(key, value)
end
```

## 11. class、object、native-backed class、union

class 示例：

```xlang
class Point
    int x = 0
    int y = 0

    create(int x, int y)
        self.x = x
        self.y = y
    end

    func length2() as int
        return self.x * self.x + self.y * self.y
    end
end

Point p = Point(3, 4)
print(p.length2())
```

继承与 `base` 示例：

```xlang
class Base
    int seed = 0

    create(int seed)
        self.seed = seed
    end
end

class Child extends Base
    int extra = 0

    create(int seed, int extra)
        base(seed)
        self.extra = extra
    end

    func sum() as int
        return self.seed + self.extra
    end
end
```

native-backed class 示例：

```xlang
class Counter
    native point handle
    extern "counter_open" create(int seed)
    extern "counter_close" destroy()
    extern "counter_add" func add(int value) as int
    extern "counter_value" func value() as int
end

auto c = Counter(40)
print(c.add(2))
```

union 示例：

```xlang
union NumberBits
    int64 i
    float64 f
end
```

object 动态字段示例：

```xlang
class Actor extends object
    int id = 0
end

auto a = Actor()
a.name = "hero"
a["score"] = 30
print(a.name)
print(a["score"])
```

属性、访问控制、静态成员和 virtual 示例：

```xlang
class Entity
    private int raw = 0
    static int total = 0

    virtual get hp() as int
        return self.raw
    end

    virtual let hp(int value)
        self.raw = value
    end

    static func add(int value) as int
        Entity.total += value
        return Entity.total
    end
end

class Player extends Entity
    override get hp() as int
        return 100
    end
end
```

属性维度参数示例：

```xlang
class Meter
    private array<int> values = [0, 0, 0]

    get value() as int
        return self.values[0]
    end

    let value(int v)
        self.values[0] = v
    end

    get value(int idx) as int
        return self.values[idx]
    end

    let value(int idx, int v)
        self.values[idx] = v
    end
end
```

协议重载示例：

```xlang
class Grid
    int base = 0

    create(int base)
        self.base = base
    end

    func __item__(int x) as int
        return self.base + x
    end

    func __item__(int x, int y) as int
        return self.base + x * 10 + y
    end

    func __setItem__(int x, int y, int value)
        self.base = value + x * 10 + y
    end

    func __contains__(int value) as bool
        return value == self.base
    end

    func __call__(int add) as int
        return self.base + add
    end
end

auto g = Grid(100)
print(g[2])
print(g[2, 3])
g[2, 3] = 7
print(30 in g)
print(g(5))
```

derive 与持久化示例：

```xlang
class Packet
    derive copy
    derive clone
    derive value

    int id = 0
    var values = []
end

auto copied = copy(Packet())
auto cloned = clone(Packet())
string text = json.stringify(Packet())
auto p = json.parse::<Packet>(text)
```

当前已实现：

- class 字段。
- class 方法。
- class 构造与析构生命周期。
- class 继承与 `base` 调用。
- class 默认初始化与清理。
- `public` / `private` class 成员访问控制；默认 `public`，`private` 只允许当前 class 内访问。
- `static` class 字段和方法；通过类型名访问，static 方法不拥有 `self`。
- `get` / `let` class 属性；支持零维、多维属性参数和属性重载。
- `override` / `final` / `virtual` class 方法与属性规则检查。
- virtual 方法、getter 和 setter 生成 vtable，并支持实例方法内的动态派发。
- native-backed class 对接 C opaque handle。
- native-backed class 成员函数第一个参数映射底层指针。
- native-backed class 生命周期映射。
- C 风格 union。
- union 字段访问。
- union C ABI 元数据。
- object 作为标准库普通基类。
- `class X extends object` 动态字段存储。
- class/object 字段协议 `__field__` / `__setField__`。
- class/object 索引协议 `__item__` / `__setItem__`。
- 协议方法签名校验与双下划线保留名检查。
- `create(...)`、`__item__(...)`、`__setItem__(...)`、`__contains__(...)`、`__call__(...)` 和运算符协议支持重载，复用普通 class 方法重载解析。
- 多维索引只对 class 协议和属性维度参数开放，普通 array/list/dict 当前仍是单索引。
- `__field__` 只处理静态字段表中不存在的点号字段；`__item__` 处理显式 `obj[key]` 索引，两者语义分离。
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
- `derive value`、`derive json`、`derive xson`、`derive string`、`derive hash`、`derive equals` 已进入语义校验。
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
- 字段级 `@json(...)`、`@xson(...)`、`@persist(...)` 持久化注解。
- `@json("name")` / `@xson("name")` 可分别指定 JSON/XSON 字段名。
- `@persist(name="name")` 可为 JSON/XSON 提供通用字段名。
- `@persist(skip)` 可跳过字段持久化。
- `@persist(include)` 可允许 private 字段参与自动持久化。
- `omit=empty` / `omit=null` 可控制空值或 null 值省略输出。
- 泛型 class 支持 `__iter__` / `__next__` 迭代协议，返回类型中的泛型实参会在实例化代码中正确替换。
- native-backed class 支持 `native point handle`，并支持 `extern "c_symbol" create/destroy/func` 将 C opaque handle 函数组映射为 class 生命周期和成员方法。
- native-backed class 可混合普通 xlang 字段、方法与 C extern 成员。
- class 成员函数支持 `export "c_symbol" func method(...)` 导出 C ABI wrapper。
- class 成员 C ABI 导出会在生成头文件和 ABI dump 中记录 owner/class 信息。
- class 成员多返回值导出为 C 输出指针参数。

当前未实现并已进入下一轮设计：

- `and` / `or` 短路逻辑的协议化方案。
- 泛型约束的 trait/protocol bound 方法集合推导。
- 泛型 class 成员直接导出 C ABI 的专用实例化 wrapper。

## 12. 异常、断言与文档注释

异常：

```xlang
try
    throw "boom"
catch e
    print(e.kind)
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

````xlang
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
````

已实现规则：

- `throw` 抛出字符串描述。
- `try` / `catch` / `finally`。
- `catch e` 中的 `e.kind` 当前为 `"Error"`，`e.message` 为 `throw` 传入的字符串。
- 异常路径资源清理。
- `assert` 语句。
- `#doc ... #end` 文档块。
- doc 字段支持 `desc:`、`param name:`、`return name:`、`note:`、`example:`。
- doc 字段内容按缩进收集多行文本；`example:` 中的代码围栏只作为文档文本保存，不参与语法解析。
- doc 字段进入语义校验。
- example 内容作为文档文本保存，不解析为代码。

## 13. 并发、异步与任务

当前已实现基础能力：

- `async` 函数。
- `await`。
- future/task 基础运行模型。
- 异步异常传播。
- `yield` 生成器。
- thread 基础封装。
- channel 基础能力。
- mutex/sem/cond/rwlock 同步原语。
- 并发相关范例与测试。

async / await 示例：

```xlang
async func addAsync(int a, int b) as int
    return a + b
end

future<int> f = addAsync(20, 22)
int answer = await f

auto autoFuture = addAsync(1, 2)
auto value = await autoFuture

future<int> a = addAsync(1, 2)
future<int> b = addAsync(3, 4)
array<int> allValues = await task.all([a, b])
int firstValue = await task.race([addAsync(10, 1), addAsync(20, 2)])
```

生成器示例：

```xlang
func numbers(int count) as generator<int>
    int i = 0
    while i < count do
        i += 1
        yield i
    end
end

for value in numbers(3)
    print(value)
end
```

channel 示例：

```xlang
channel<int> ch = channel::<int>(2)
ch.send(123)

int received = 0
if ch.recvInto(received) then
    print(received)
end

ch.close()
```

`channel<T>` 当前只允许 `bool`、整数、浮点、`time`、`point` 等可按值复制的元素类型。`string`、`var`、`class`、`func`、`generator<T>`、`array<int>` 这类静态容器等涉及所有权、动态值或 move-only 句柄的类型会在语义阶段拒绝，避免进入当前 `memcpy` 发送路径。

`recvInto(target)` 是阻塞接收，`tryRecvInto(target)` 是非阻塞接收；两者的 `target` 都是写入目标，必须是具名变量，不能是 `const`，不能是匿名函数外层只读捕获变量，并且类型必须等于 `channel<T>` 的元素类型。

thread 与同步原语示例：

```xlang
func worker() as int
    return 7
end

thread t = thread(worker)
t.wait()
print(t.exitCode())

mutex m = mutex()
m.lock()
m.unlock()

sem gate = sem(0, 2)
gate.post()
gate.waitTimeout(10)

cond c = cond()
c.signal()
c.broadcast()

rwlock rw = rwlock()
rw.readLock()
rw.readUnlock()
rw.writeLock()
rw.downgrade()
rw.readUnlock()
```

当前边界：
- `async` 函数返回 `future<T>`；无显式返回值的 async 函数可被 `await`。
- `async` 函数参数会进入异步执行帧，因此不能按值声明 `future<T>`、`generator<T>`、`channel<T>`、`sem`、`thread` 等不可复制资源句柄。
- `future<T>` 支持重复 `await` 已完成结果。
- `task.all` / `task.race` 接收非空的 `[future<T>, ...]` 数组字面量或 `array<future<T>>`，并要求所有 future 的完成值类型一致。
- `task.all` 返回 `future<array<T>>`，因此常见写法是 `array<T> values = await task.all(jobs)`。
- `task.race` 返回 `future<T>`，因此常见写法是 `T value = await task.race(jobs)`。
- `task.all` 已覆盖 `int`、`string`、`bool`、`float`、`class`、`func` 等完成值；数组字面量输入和 `array<future<Class>>` / `array<future<func(...) as T>>` typed-array 输入均已进入守门。`func` 聚合结果按 `array<func(...) as T>` 使用，建议先取到具名 `func` 变量再调用。
- `task.race` 已覆盖 `class` 与 `func` 完成值；typed-array 输入同样支持 `class` 与 `func`。`class` 完成值当前必须先保存为 `future<Class>` 再 `await`，不开放 `Class value = await task.race(...)` 这种临时 class 聚合 await 写法。
- 具名 pending `task.all` / `task.race` group 的 `task.cancel(...)` 已覆盖基础值、`class` 与 `func` source；取消后 source future 的后续 `await` 必须继续抛出 `future cancel requested.`，worker 晚到完成不能覆盖取消态。普通 `async func` 的非重复数组字面量输入与 `array<future<int>>` typed-array 输入，均已覆盖 sleep 后再次 await source 仍保持取消态。
- 普通 `async func` 产生的 `future<string>`、`future<bool>`、`future<float>` 进入 `array<future<T>>` typed-array `task.all` / `task.race` 后，已覆盖 group cancel、source 首次 await 取消、等待 worker 晚到完成后 source repeated await 仍保持取消态。
- `task.race` 中某个 source future 已经被显式取消时，group 必须进入取消态，并取消仍在 pending 的 loser；数组字面量输入和 `array<future<int>>` typed-array 输入均已覆盖 loser late await 仍保持取消态。该规则同时覆盖“先入组再取消 winner”和“先取消 winner 再创建 group”的 completed-canceled 路径，winner repeated cancel 仍返回 true。
- `task.all` 中某个 source future 已经被显式取消时，group 必须进入取消态，但不会主动取消其他 source；另一个 source 应正常完成。已取消 source 在数组字面量输入和 `array<future<int>>` typed-array 输入下都必须支持首次 await、late await 与 repeated cancel，且始终保持取消态；该规则同时覆盖“先入组再取消 source”和“先取消 source 再创建 group”的 completed-canceled 路径。
- 已完成的具名 `task.all` / `task.race` future 支持 repeated await；`task.cancel(completedFuture)` 返回 `false`，post-cancel await 仍返回原完成值。该规则已覆盖基础值、`class` 与 `func`；其中 `int`、`string`、`bool`、`float`、`class`、`func` 同时覆盖数组字面量输入和 typed-array 输入。
- `task.all` 的内部取消转发生命周期已经收紧：聚合启动时会先持有内部 group future 的本地引用，再建立取消转发，成功、启动失败和转发失败路径都会释放本地引用，避免 worker 过快完成时释放内部状态导致竞态。
- `task.cancel(...)` 只接受 `future<T>` / `task<T>`；`channel<T>`、`generator<T>`、`sem`、`thread` 等资源句柄不是 task 句柄，必须使用各自的专用方法或生命周期规则，不能传给 `task.cancel`。
- `task.all` / `task.race` 的失败传播已覆盖基础值、`string`、`class` 与 `func`；已失败的具名 task future 支持 repeated await，`task.cancel(failedFuture)` 返回 `false`，post-cancel await 仍保留原始错误。`task.all` 中某个 source 快速失败时，group 必须传播原始错误，但不会取消仍在 pending 的 sibling；数组字面量输入和 `array<future<int>>` typed-array 输入均已覆盖慢 sibling 后续正常 `await`。`task.race` 中某个 source 快速失败时，仍在 pending 的 loser future 会被取消，后续 repeated await 保持取消态；如果 race 创建前 winner 已经失败，数组字面量输入和 `array<future<int>>` typed-array 输入也都必须覆盖 loser 首次 await 与 sleep 后 late await 均保持取消态。
- 当前 typed future bridge 不支持 `task.all/race` 聚合 `future<var>`，也不支持完成值本身为 `array/list/set/dict` 的 `future<array/list/set/dict>`，也不支持 `future<future<T>>`、`future<generator<T>>`、`future<channel<T>>`、`future<sem>`、`future<thread>` 等资源句柄完成值。该限制同时适用于 `[future<T>, ...]` 数组字面量输入和 `array<future<T>>` typed-array 输入；这些 future 可以直接 `await`，但暂不能再交给 `task.all/race` 二次聚合。
- `task.spawn` 只接收具备完整签名的 `func` 值；当前桥接不支持 `var` 返回值、`***` 字典可变参数、typed container 参数/返回值、动态 `array/list/set/dict` 参数/返回值、`future<T>` 参数/返回值、`generator<T>` 参数/返回值、`channel<T>` / `sem` / `thread` 等资源句柄参数/返回值。
- `task.spawn` 已覆盖 `...` 数组可变参数函数值，包括 `func(int, ...) as int` 和零固定参数的 `func(...) as int` adapter；pending vararg future 取消后，worker 晚到完成不能覆盖取消态。
- `task.spawn(...)` 产生的 `future<int/string/bool/float>` 进入 typed-array `task.all` / `task.race` 后，已覆盖 completed future 的 repeated await、completed cancel 返回 `false`、post-cancel await 仍返回原完成值。
- `task.spawn(...)` 产生的 `future<string>` 进入 pending `task.all` / `task.race` group 后，取消 group 必须同步取消 source；worker 晚到完成后，source future 的后续 `await` 仍必须保持 `future cancel requested.`。
- `task.spawn(...)` 产生的 `future<bool>` / `future<float>` 进入 pending `task.all` / `task.race` group 后，也已覆盖 group cancel、source cancel、worker 晚到完成后 source repeated await 仍保持取消态。
- `task.spawn(...)` 产生的 `future<int>` 进入 `array<future<int>>` typed-array 后，已覆盖快失败竞态：`task.all` 传播原始错误但不取消慢 source，慢 source 后续可正常 `await` 且 completed cancel 返回 `false`；`task.race` 传播原始错误并取消慢 loser，loser 首次 await 与 sleep 后 late await 均保持 `future cancel requested.`。
- `task.spawn(...)` 产生的 `future<string>` 进入 `array<future<string>>` typed-array 后，也已覆盖同一快失败竞态；该路径额外锁定 `xl6_future_await_cstr_copy` 字符串复制 await helper，避免 race loser late completion 或 all 慢源 await 破坏字符串结果生命周期。
- `task.spawn(...)` 产生的 `future<bool>` / `future<float>` 进入 `array<future<bool>>` / `array<future<float>>` typed-array 后，同样覆盖快失败竞态；bool 路径锁定 `xrtTypeBool()` 与 `sizeof(int)` await，float 路径锁定 `xrtTypeFloat()` 与 `sizeof(double)` await，确保 all/race 的失败、取消、late await 语义与 int/string 对齐。
- generator 支持 `for value in gen`，异常可从生成器传播到外层 `try/catch`。
- `generator<T>` 当前元素类型和 generator 函数参数同样只开放 `bool`、整数、浮点、`time`、`point` 等可按值复制类型；`string`、`var`、`class`、`func`、静态容器等复杂值暂不进入生成器帧。
- `thread` 入口支持普通函数值和捕获闭包，但会进行可复制性检查，避免把不可复制资源跨线程复制。

当前守门状态：

- `RISK-ASYNC-001` 已收口并保持发布级守门。当前仍会继续补异步和任务边界测试，但这是压实质量基线，不表示该风险处于开放状态。

## 14. 标准库

当前标准库采用短命名空间，默认进入 xlang 环境，用户不需要手写 `extern` 或 `import`。能够由 XRT 提供的能力必须优先绑定 XRT；确实没有必要在 XRT 重复实现的底层能力才绑定 CRT，例如 `math`。

调用示例：
```xlang
print("hello")
println("xlang", 6)

auto value = math.sqrt(9.0)
auto parts = string.split("a,b", ",")
auto now = time.now()
auto text = file.readText("data.txt")
```

当前标准库公开函数为 406 个，另有标准库常量 46 个；全局输出入口 `print` / `println` 也默认进入环境（这两条因空命名空间未计入守门脚本 `check_stdlib_coverage.ps1` 的 406 公开计数，实际公开函数合计 408 个）。用户可直接调用的当前列表如下：

- 全局：`print`、`println`。
- `math` 常量：`pi`、`tau`、`e`、`nan`、`inf`。
- `math` 函数：`abs`、`acos`、`asin`、`atan`、`atan2`、`cbrt`、`ceil`、`clamp`、`cos`、`deg`、`exp`、`exp2`、`expm1`、`floor`、`fract`、`hypot`、`isFinite`、`isInf`、`isNaN`、`log`、`log10`、`log1p`、`log2`、`max`、`min`、`mod`、`pow`、`rad`、`round`、`sign`、`sin`、`sqrt`、`tan`、`trunc`。
- `string`：`approx`、`byteAt`、`byteLen`、`charAt`、`chars`、`compare`、`contains`、`count`、`endsWith`、`equals`、`filter`、`formatFloat`、`formatInt`、`hasAny`、`indexOf`、`insert`、`isBlank`、`isEmpty`、`isUtf8`、`join`、`lastIndexOf`、`left`、`len`、`like`、`lower`、`ltrim`、`mid`、`padCenter`、`padLeft`、`padRight`、`remove`、`removeAt`、`repeat`、`replace`、`reverse`、`right`、`rtrim`、`sim`、`slice`、`split`、`splitLines`、`startsWith`、`trim`、`upper`、`bytes`。
- `time`：`addDays`、`addHours`、`addMinutes`、`addMonths`、`addQuarters`、`addSeconds`、`addWeeks`、`addYears`、`approx`、`clock`、`datePart`、`dateString`、`day`、`dayOfYear`、`daysInMonth`、`daysInYear`、`diffDays`、`diffHours`、`diffMinutes`、`diffMonths`、`diffQuarters`、`diffSeconds`、`diffWeeks`、`diffYears`、`firstDayOfMonth`、`firstDayOfWeek`、`firstDayOfYear`、`format`、`formatPattern`、`fromDate`、`fromDateTime`、`fromTime`、`fromUnix`、`hour`、`inRange`、`isLeapYear`、`isSameDay`、`isSameMonth`、`isSameYear`、`lastDayOfMonth`、`lastDayOfWeek`、`lastDayOfYear`、`localToUTC`、`minute`、`month`、`now`、`nowUTC`、`parse`、`parsePattern`、`quarter`、`rangeOverlap`、`relative`、`second`、`sleep`、`timePart`、`timer`、`timeString`、`timezoneOffset`、`today`、`toUnix`、`tryParse`、`tryParsePattern`、`utcToLocal`、`weekday`、`weekOfMonth`、`weekOfYear`、`year`。
- `random`：`range`、`seed`、`string`、`u32`、`u64`。
- `path`：`abs`、`appDir`、`cwd`、`dir`、`exists`、`ext`、`home`、`isAbs`、`join`、`name`、`nameExt`、`normalize`、`random`、`rel`、`sep`、`setCwd`、`temp`、`withExt`、`withName`。
- `file`：`accessTime`、`appendText`、`appendTextAsync`、`attr`、`changeTime`、`copy`、`copyAsync`、`delete`、`deleteAsync`、`exists`、`info`、`move`、`moveAsync`、`readText`、`readTextAsync`、`rename`、`replace`、`setAttr`、`setSize`、`size`、`touch`、`writeText`、`writeTextAsync`、`writeTextAtomic`。
- `dir`：`copy`、`create`、`createAll`、`delete`、`dirs`、`exists`、`files`、`list`、`move`。
- `json`：`parse`、`valid`、`read`、`stringify`、`write`。
- `xson`：`parse`、`valid`、`read`、`stringify`、`write`。
- `codec`：`base64Decode`、`base64DecodeBytes`、`base64Encode`、`base64EncodeBytes`、`formUrlDecode`、`formUrlEncode`、`hexDecode`、`hexDecodeBytes`、`hexEncode`、`hexEncodeBytes`、`percentDecode`、`percentEncode`。
- `encoding` 常量：`autoDetect`、`binary`、`oem`、`utf8`、`utf16`、`utf16BE`、`utf32`、`utf32BE`、`bom`。
- `encoding`：`charSize`、`isUtf8`、`isUtf8Bytes`、`detect`、`detectBytes`、`toBytes`、`fromBytes`、`convertBytes`、`name`。
- `hash`：`u32`、`u32WithSeed`、`u64`、`u64WithSeed`、`u64Micro`、`u64MicroWithSeed`、`u64Nano`、`u64NanoWithSeed`。
- `crypto`：`sha1`、`sha256`、`sha384`、`sha512`、`sha256Stream`、`sha384Stream`、`sha512Stream`、`hmacSha256`、`hmacSha384`、`hmacSha512`、`hkdfSha256`、`hkdfSha384`、`randomHex`、`chacha20Encrypt`、`chacha20Decrypt`、`chacha20Poly1305Encrypt`、`chacha20Poly1305Decrypt`、`aes128GcmEncrypt`、`aes128GcmDecrypt`、`aes256GcmEncrypt`、`aes256GcmDecrypt`、`ed25519Keypair`、`ed25519PublicKey`、`ed25519Sign`、`ed25519Verify`、`x25519Keypair`、`x25519SharedSecret`、`x448Keypair`、`x448SharedSecret`、`rsaPssVerify`、`rsaPkcs1Verify`。
- `id`：`xid`。
- `net`：`localIP`、`localMAC`、`localName`。
- `tcp`：`connect`、`connectAsync`、`listen`。
- `udp`：`bind`。
- `url`：`authority`、`fragment`、`host`、`isAbsolute`、`isSecure`、`normalizePath`、`path`、`port`、`query`、`resolve`、`scheme`、`target`、`userInfo`、`valid`。
- `query`：`append`、`decode`、`encode`、`pair`、`valid`。
- `regex`：`find`、`isMatch`。
- `process`：`exec`、`exitCode`、`success`。
- `log` 常量：`traceLevel`、`debugLevel`、`infoLevel`、`warnLevel`、`errorLevel`、`fatalLevel`、`offLevel`、`textFormat`、`simpleFormat`、`jsonFormat`。
- `log`：`create`、`default`、`setLevel`、`getLevel`、`levelName`、`addConsole`、`addFile`、`addRollingFile`、`write`、`trace`、`debug`、`info`、`warn`、`error`、`fatal`、`flush`。
- `template`：`compile`、`render`、`renderFile`；预编译对象使用 `Template`，支持 `render`、`nodeCount`、`exprCount`、`argCount`、`stringPoolSize`。
- `http`：`get`、`post`、`request`；对象化客户端使用 `HttpClientRequest`、`HttpClientResponse`。
- `http.server` 常量：`allowDotfiles`、`allowBackslash`、`sameSiteUnspecified`、`sameSiteLax`、`sameSiteStrict`、`sameSiteNone`、`cookieSecure`、`cookieHttpOnly`、`cookiePartitioned`、`cookieSameParty`。
- `http.server`：`create`、`createTls`、`tlsHost`、`removeTlsHost`、`app`、`destroyApp`、`appRoute`、`appRouteRequest`、`appRouteStreamRequest`、`appGet`、`appPost`、`appAny`、`appGetRequest`、`appPostRequest`、`appAnyRequest`、`appPostStreamRequest`、`appAnyStreamRequest`、`appStatic`、`appStaticEx`、`appError`、`setApp`、`reloadApp`、`host`、`removeHost`、`error`、`route`、`routeRequest`、`routeStreamRequest`、`get`、`post`、`any`、`getRequest`、`postRequest`、`anyRequest`、`postStreamRequest`、`anyStreamRequest`、`chunkText`、`chunkBytes`、`static`、`staticEx`、`destroy`、`port`、`requestCount`、`connectionCount`、`start`、`drain`、`stop`、`mimeByPath`；对象化路由使用 `HttpRequest`、`HttpResponse`。

常用默认参数：
- `string.compare(left, right, ignoreCase = false)`、`string.equals(...)`、`string.contains(...)`、`string.indexOf(...)`、`string.lastIndexOf(...)`、`string.count(...)`、`string.startsWith(...)`、`string.endsWith(...)`、`string.like(...)`。
- `string.formatInt(value, format = "")`、`string.formatFloat(value, format = "")`、`string.trim(text, chars = "")`、`string.ltrim(...)`、`string.rtrim(...)`、`string.slice(text, start, count = -1)`、`string.mid(text, start, count = -1)`。
- `string.join(values, sep = "")`、`string.padLeft(text, width, pad = " ")`、`string.padRight(text, width, pad = " ")`、`string.padCenter(text, width, pad = " ")`、`string.remove(text, part, ignoreCase = false)`。
- `time.fromDateTime(year, month, day, hour = 0, minute = 0, second = 0)`、`time.firstDayOfWeek(value, startDay = 1)`、`time.lastDayOfWeek(value, startDay = 1)`、`time.format(value, format = 0)`。
- `random.seed(seed, seq)`、`random.string(len = 16, chars = "")`、`path.join(first, second, third = "", fourth = "")`、`path.random(head = "", foot = "", len = 12)`。
- `file.readText(path, charset = 65001)`、`file.writeText(path, text, charset = 65001)`、`file.writeTextAtomic(path, text, charset = 65001)`、`file.appendText(path, text, charset = 65001)`、`file.open(path, readOnly = false, charset = 65001)`。
- `file.copy(source, target, overwrite = true)`、`file.move(source, target, overwrite = true)`、`file.rename(source, target, overwrite = false)`。
- `dir.list(path, recursive = false)`、`dir.files(path, recursive = false)`、`dir.dirs(path, recursive = false)`、`dir.count(path, recursive = false)`、`dir.size(path, recursive = true)`、`dir.glob(path, pattern = "*", recursive = false)`、`dir.entries(path, recursive = false)`、`dir.match(path, pattern = "*", recursive = false)`、`dir.walk(path, pattern = "*")`。
- `json.stringify(value, pretty = false)`、`json.write(path, value, pretty = false)`、`xson.stringify(value, pretty = false)`、`xson.valid(text, ignoreUnsupported = false)`。
- `encoding.detect(text, withBom = true)`、`encoding.detectBytes(data, size, withBom = true)`、`encoding.toBytes(text, toCharset = encoding.utf8)`、`encoding.fromBytes(data, size, fromCharset = encoding.utf8)`。
- `process.exec(command, timeoutMs = 0)`、`process.success(command, timeoutMs = 0)`、`process.exitCode(command, timeoutMs = 0)` 保留 shell 便捷路径；`process.run(command, timeoutMs = 0)`、`process.runFile(program, args = [], timeoutMs = 0)` 返回结构化 `ProcessResult`；`process.cmd(program, args = [])`、`process.shell(command)` 创建 pipeline 阶段；`process.pipeline(commands, timeoutMs = 0)` 返回 `PipelineResult`；`process.start(command)`、`process.startFile(program, args = [])` 返回可交互 `Process`。
- `log.create(name = "xlang")` 创建独立 `Logger`；`log.default()` 包装默认 logger；`log.addConsole(level = log.traceLevel, color = true, format = log.textFormat)`、`log.addFile(path, level = log.infoLevel, format = log.textFormat)`、`log.addRollingFile(path, maxSize = 10485760, maxBackup = 5, level = log.infoLevel, format = log.textFormat)` 添加输出目标；`log.trace/debug/info/warn/error/fatal(message)` 写入默认 logger。
- `template.compile(text, bracket = "{}")` 编译模板文本；`template.render(text, data = null, bracket = "{}")` 一次性渲染模板；`template.renderFile(path, data = null, bracket = "{}")` 通过 XRT UTF-8 文件层读取模板并渲染；`Template(text, bracket).render(data)` 适合重复渲染。
- `http.get(url, timeoutMs = 5000)`、`http.post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 5000)`、`http.request(method, url, body = "", contentType = "", timeoutMs = 5000)` 提供阻塞式 HTTP/HTTPS client，返回 `HttpClientResponse`。`HttpClientRequest(url = "")` 支持 `method(method)`、`url(url)`、`header(name, value)`、`body(data, size, contentType)`、`text(data, contentType = "text/plain; charset=utf-8")`、`timeout(timeoutMs)`、`idleTimeout(timeoutMs)`、`verifyPeer(value)`、`execute()`；`HttpClientResponse` 支持 `status()`、`ok()`、`reason()`、`header(name)`、`contentLength()`、`body()`、`bodyData()`、`bodySize()`、`bodyBytes()`。`bodyData/bodySize` 是响应生命周期内的字节视图，`bodyBytes()` 返回独立复制的 `Buffer`。当前 client API 是同步阻塞模型；异步 HTTP client 归入后续统一异步网络运行时。
- `http.server.create(port = 0, host = "127.0.0.1", backlog = 128, recvLimit = 1048576, bodyLimit = 1048576, headerLimit = 100, headerBytesLimit = 65536, workerCount = 1, headerTimeoutMs = 30000, bodyTimeoutMs = 30000, idleTimeoutMs = 60000, writeTimeoutMs = 30000, multipartPartLimit = 1000, multipartPartSizeLimit = 0)` 创建 HTTP server 句柄；`http.server.get/post/any(server, pattern, handler)` 注册到 server 当前默认 app，handler 签名为 `func(string, string, string, string) as string`；`http.server.getRequest/postRequest/anyRequest(server, pattern, handler)` 注册对象化路由，handler 签名为 `func(point, point) as bool`，函数内用 `HttpRequest(reqHandle)` 和 `HttpResponse(respHandle)` 访问请求与响应；`http.server.route(server, method, pattern, handler)` 和 `http.server.routeRequest(server, method, pattern, handler)` 支持 `GET`、`HEAD`、`POST`、`PUT`、`DELETE`、`PATCH`、`OPTIONS`、`ANY`，也支持 `GET|POST`、`GET,POST` 这类组合方法字符串；`http.server.postStreamRequest/anyStreamRequest/routeStreamRequest` 注册流式请求体路由；`pattern` 支持静态段、`{id}` 变量段和尾部 `*` 通配符；`http.server.static(server, mount, root)` 挂载默认静态目录；`http.server.staticEx(server, mount, root, indexNames = "index.html;index.htm", cacheControl = "", chunkSize = 65536, flags = 0)` 可设置 index 文件名列表、`Cache-Control`、发送块大小和静态目录安全 flag；`http.server.mimeByPath(path)` 根据扩展名返回 Content-Type。
- `http.server.static/staticEx/appStatic/appStaticEx` 的静态文件响应由 XRT `xweb` 统一处理：自动输出 `ETag`、`Last-Modified`、`Accept-Ranges: bytes`；支持 `If-None-Match` 返回 `304 Not Modified`；支持单段 `Range: bytes=...` 返回 `206 Partial Content` 和 `Content-Range`；无效 range 返回 `416 Range Not Satisfiable`；`If-Range` 不匹配时忽略 range 并返回完整文件。
- 流式请求体路由三段 handler：`beginHandler` 为 `func(point) as bool`，`chunkHandler` 为 `func(point, point, int) as bool`，`endHandler` 为 `func(point, point) as bool`。`chunkHandler` 中的 `point data` 只在当前回调期间有效，需要保存时调用 `http.server.chunkText(data, size)` 复制为 `string`，或调用 `http.server.chunkBytes(data, size)` 复制为独立 `Buffer`。当前 xlang 高层 `endHandler` 必须同步写出响应，暂不支持异步结束；XRT/xweb C API 已支持 `StreamRouteAsync` 系列异步 body end。
- `http.server.create` / `createTls` 默认使用 XRT owned engine：每个 server 拥有独立网络 engine，`workerCount` 对该 engine 生效，`http.server.destroy(server)` 会释放 server 与对应 engine。C 侧如果直接使用 XRT，可显式传入外部 `xnetengine*` 来让多个 server 共享同一个事件循环。
- `http.server.create` / `createTls` 的 `recvLimit` 是底层连接接收缓冲上限，`bodyLimit` 是单个 HTTP 请求 body 上限，`headerLimit` 是请求头数量上限，`headerBytesLimit` 是请求头总字节上限，`multipartPartLimit` 是 multipart part 数量上限，`multipartPartSizeLimit` 是单个 multipart part body 字节上限；`multipartPartLimit = 0` 或 `multipartPartSizeLimit = 0` 表示关闭对应限制。请求 body 或 multipart 超限时底层 XRT 直接返回 `413 Payload Too Large`，请求头超过限制时返回 `431 Request Header Fields Too Large`，这些拒绝路径都不会进入路由 handler。`headerTimeoutMs` 控制请求头读完前的最长等待时间，`bodyTimeoutMs` 控制固定长度 body 读完前的最长等待时间，`idleTimeoutMs` 控制 keep-alive 空闲连接保留时间，`writeTimeoutMs` 控制响应提交后发送队列排空的最长等待时间；header/body 超时返回 `408 Request Timeout`，idle 超时静默关闭连接，write 超时直接关闭连接。
- `http.server.createTls(certFile, keyFile, port = 0, host = "127.0.0.1", backlog = 128, recvLimit = 1048576, bodyLimit = 1048576, headerLimit = 100, headerBytesLimit = 65536, workerCount = 1, headerTimeoutMs = 30000, bodyTimeoutMs = 30000, idleTimeoutMs = 60000, writeTimeoutMs = 30000, multipartPartLimit = 1000, multipartPartSizeLimit = 0)` 创建 HTTPS server 句柄，底层使用 XRT `xtlsconfig` 的证书文件路径能力。
- `http.server.tlsHost(server, host, certFile, keyFile)` 为 HTTPS server 增加 SNI 证书映射；`http.server.removeTlsHost(server, host)` 移除映射。TLS 握手时 XRT 按 SNI host 匹配证书；没有匹配时使用 `createTls` 的默认证书。
- `http.server.requestCount(server)` 返回 server 启动后的累计请求数；`http.server.connectionCount(server)` 返回当前连接数量；`http.server.drain(server, timeoutMs = -1)` 优雅停止监听并等待连接结束，`timeoutMs = 0` 表示只发起 drain 并立即返回，负数表示无限等待；`http.server.stop(server)` 是强制停止。
- `http.server.error(server, handler)` 设置默认 app 错误处理器；`http.server.appError(app, handler)` 设置指定 app 错误处理器。handler 签名为 `func(int, string, point, point) as bool`，参数依次为状态码、错误文本、request 句柄和 response 句柄。
- `HttpRequest` 支持 `method()`、`methodId()`、`target()`、`path()`、`query()`、`header(name)`、`headerCount()`、`headerName(index)`、`headerValue(index)`、`param(name)`、`paramCount()`、`paramName(index)`、`paramValue(index)`、`queryValue(name)`、`queryCount()`、`queryName(index)`、`queryValueAt(index)`、`queryHasValue(index)`、`cookie(name)`、`cookieCount()`、`cookieName(index)`、`cookieValueAt(index)`、`contentType()`、`isFormUrlEncoded()`、`formValue(name)`、`formCount()`、`formName(index)`、`formValueAt(index)`、`formHasValue(index)`、`isMultipartForm()`、`multipartValue(name)`、`multipartCount()`、`multipartName(index)`、`multipartText(index)`、`multipartIsFile(index)`、`multipartFileName(index)`、`multipartContentType(index)`、`multipartSize(index)`、`multipartBytes(index)`、`contentLength()`、`localAddr()`、`localPort()`、`remoteAddr()`、`remotePort()`、`tlsSNI()`、`body()`、`bodyData()`、`bodySize()`、`bodyBytes()`；`headerName/headerValue/paramName/paramValue/queryName/queryValueAt/cookieName/cookieValueAt/formName/formValueAt/multipartName/multipartText/multipartFileName/multipartContentType` 越界或负索引返回空字符串；`tlsSNI()` 在 HTTPS 且客户端提供 SNI 时返回 host，否则返回空字符串；其中 `bodyData/bodySize` 是请求生命周期内的二进制 body 字节视图，`bodyBytes()` 返回独立复制的 `Buffer`，`multipartBytes(index)` 返回对应 part body 的独立 `Buffer`。
- `HttpResponse` 支持 `status(code)`、`header(name, value)`、`addHeader(name, value)`、`cookie(name, value, path = "/", maxAge = -1, sameSite = http.server.sameSiteUnspecified, flags = 0)`、`deleteCookie(name, path = "/")`、`text(text, contentType = "text/plain; charset=utf-8")`、`jsonText(text)`、`body(data, size, contentType)`、`reserveBody(size)`、`appendText(text)`、`appendBody(data, size)`、`start()`、`write(text)`、`send(data, size)`、`finish()`、`file(path)`、`redirect(url, status = 302)`；`reserveBody/appendText/appendBody` 用于在响应提交前逐步构造普通响应体，适合避免多次字符串拼接；`cookie` 追加 `Set-Cookie` 响应头，`maxAge < 0` 表示会话 Cookie，`flags` 可组合 `http.server.cookieSecure`、`http.server.cookieHttpOnly`、`http.server.cookiePartitioned`、`http.server.cookieSameParty`；`start/write/send/finish` 用于流式响应，未设置 `Content-Length` 时底层自动使用 chunked 响应。
- `http.server.app()` 创建独立 app；`http.server.appRoute/appRouteRequest/appRouteStreamRequest` 支持通用方法字符串，`http.server.appGet/appPost/appAny`、`http.server.appGetRequest/appPostRequest/appAnyRequest`、`http.server.appPostStreamRequest/appAnyStreamRequest`、`http.server.appStatic/appStaticEx` 将路由和静态目录注册到 app；`http.server.setApp(server, app)` 替换默认 app；`http.server.reloadApp(server, app)` 用于热替换默认 app；`http.server.host(server, host, app)` 将 Host 虚拟主机绑定到 app；`http.server.removeHost(server, host)` 移除虚拟主机。server 会持有 app 引用，`destroyApp(app)` 只释放当前 xlang 侧 app 句柄；如果 app 已绑定到 server，路由闭包会由 XRT route 生命周期托管。
  app 族函数的完整调用形态：`http.server.appGet(app, pattern, handler)`、`http.server.appPost(app, pattern, handler)`、`http.server.appAny(app, pattern, handler)`、`http.server.appRoute(app, method, pattern, handler)`、`http.server.appGetRequest(app, pattern, handler)`、`http.server.appPostRequest(app, pattern, handler)`、`http.server.appAnyRequest(app, pattern, handler)`、`http.server.appRouteRequest(app, method, pattern, handler)`、`http.server.appPostStreamRequest(app, pattern, beginHandler, chunkHandler, endHandler)`、`http.server.appAnyStreamRequest(app, pattern, beginHandler, chunkHandler, endHandler)`、`http.server.appRouteStreamRequest(app, method, pattern, beginHandler, chunkHandler, endHandler)`、`http.server.appStatic(app, mount, root)`、`http.server.appStaticEx(app, mount, root, indexNames = "index.html;index.htm", cacheControl = "", chunkSize = 65536, flags = 0)`、`http.server.setApp(server, app)`、`http.server.reloadApp(server, app)`、`http.server.host(server, host, app)`、`http.server.removeHost(server, host)`。`flags` 可组合 `http.server.allowDotfiles` 与 `http.server.allowBackslash`。

文件信息结构：

```xlang
FileInfo info = file.info("data.txt")
println(info.path, info.exists, info.isFile, info.isDir, info.size)
```

`FileInfo` 字段为：`path`、`exists`、`isFile`、`isDir`、`size`、`attr`、`accessTime`、`changeTime`。

流式文件对象：

```xlang
File f = file.open("data.txt", false)
f.seek(0, file.seekSet)
f.writeText("hello")
f.seek(0, file.seekEnd)
f.seek(-5, file.seekCur)
string text = f.readText()
println(f.pos, f.size, f.eof, text)
```

- `File` 是 native-backed class，底层为 XRT `xfile`，对象销毁时自动关闭文件句柄。
- `file.seekSet`、`file.seekCur`、`file.seekEnd` 分别表示从文件开头、当前位置、文件末尾定位。
- `f.seek(offset, whence = file.seekSet)` 返回移动后的字节位置。
- `f.pos` 返回当前游标位置，`f.size` 返回文件末尾位置，`f.eof` 表示游标是否到达文件末尾。
- `f.readText(size = -1)` 读取指定字节数；`-1` 表示从当前游标读取到文件末尾。
- `f.writeText(text)` / `f.write(text)` 从当前游标写入 UTF-8 文本，返回写入字节数。
- `f.setEOF()` 将文件截断到当前游标位置。

目录结构化项：

```xlang
array items = dir.match("data", "*.txt", true)
var first = items[0]
println(first["name"], first["path"], first["size"])
```

`dir.entries`、`dir.match`、`dir.walk` 返回 `var` 字典数组。字段为：`dir`、`name`、`path`、`isFile`、`isDir`、`enter`、`leave`、`size`、`attr`、`accessTime`、`changeTime`。`dir.walk` 包含目录 enter/leave 事件，`dir.entries` 与 `dir.match` 不返回 leave 事件。

Buffer 对象：

```xlang
Buffer buf = Buffer()
buf.append("abc")
buf.appendByte(100)
int size = buf.len()
string text = buf.text()
```

- `Buffer` 是 native-backed class，底层使用 XRT `xbuffer`，对象销毁时自动释放句柄。
- `Buffer(step)` 可指定自增长步长；`reserve`、`clear`、`append`、`appendBytes`、`appendByte`、`byteAt`、`setByte`、`text`、`data`、`len`、`capacity` 已进入范例和回归。
- `buf[index]` / `buf[index] = value` 通过 `__item__` / `__setItem__` 协议映射到字节读写。

随机数对象：

```xlang
Random rng = Random(uint64(12345), uint64(7))
uint32 a = rng.u32()
uint64 b = rng.u64()
int dice = rng.range(1, 6)
string token = rng.string(16, "0123456789abcdef")
rng.seed(uint64(12345), uint64(7))
```

- `random.seed` 重置当前线程默认随机源，影响 `random.u32/u64/range/string`。
- `Random(seed, seq)` 创建独立 `Random` 对象，适合游戏、模拟、测试等需要复现随机序列的场景。
- `Random` 是 native-backed class，底层使用 XRT `xrtRandObj`，对象销毁时自动释放 XRT 句柄。

TCP/UDP 网络对象：

```xlang
TcpServer server = TcpServer(tcp.listen(0))
future<point> accepted = server.acceptAsync(3000)
TcpStream client = TcpStream(tcp.connect("127.0.0.1", server.port(), 3000))
TcpStream peer = TcpStream(await accepted)
client.send("ping")
string text = peer.recv(3000)

Buffer bytes = Buffer()
bytes.appendByte(0)
bytes.appendByte(255)
client.sendBytes(bytes.data(), c_size(bytes.len()))
Buffer received = peer.recvBytes(3000)

UdpSocket sock = UdpSocket(udp.bind(0))
bool ok = sock.send("127.0.0.1", uint16(sock.port()), "hello")
sock.sendBytes("127.0.0.1", uint16(sock.port()), bytes.data(), c_size(bytes.len()))
UdpPacket packet = UdpPacket(sock.recv(3000))
Buffer packetBytes = packet.bytes()
```

- `TcpStream`、`TcpServer`、`UdpSocket`、`UdpPacket` 均为 native-backed class。
- `tcp.connect`、`tcp.connectAsync`、`tcp.listen`、`udp.bind` 返回 `point`，调用者必须显式构造对应 class，避免网络句柄发生隐式复制。
- TCP 已支持同步/异步 connect、accept、文本 send/recv、二进制 `sendBytes/readBytes/recvBytes`、drain 和 close。
- UDP 已支持同步/异步文本 send/recv，并支持二进制 `sendBytes` 与 `UdpPacket.bytes()`。
- 二进制发送采用 `point data, c_size size` 字节视图参数，例如 `stream.sendBytes(buf.data(), c_size(buf.len()))`。这是因为 `Buffer` 是不可复制 native-backed class，不能作为按值参数传入函数。
- XRT 底层已补齐 TCP/UDP 配置型 API 与二进制便捷 API：`xrtNetTcpConnectEx`、`xrtNetTcpListenEx`、`xrtNetUdpBindEx`、`xrtNetStreamReadBytes`、`xrtNetStreamRecvBytes`、`xrtNetStreamSendBytes`、`xrtNetUdpSendBytes`、`xrtNetDgramPacketBytesCopy`。
- xlang 标准库已把上述二进制能力封装到 `Buffer`/字节视图写法中，C/XRT 对照范例和 xlang 范例输出在主回归中比较一致性。

当前边界：
- 标准库命名空间只在成员访问位置生效，例如 `math.sqrt`、`math.pi`；写成已存在命名空间但不存在符号时会报错。
- `math.pi`、`math.tau`、`math.e`、`math.nan`、`math.inf` 是标准库常量，编译期直接重写为字面量，不产生运行时函数调用。
- 文件、目录、路径能力优先通过 XRT 处理，避免 Windows 下 CRT 对 UTF-8 路径支持不足的问题。
- `file.*Async` 返回 `future<string>` 或 `future<bool>`，当前覆盖 read/write/append/copy/move/delete 的 UTF-8 文本和基础文件操作。
- `string.len(text)` 按 UTF-8 字符数量计数，`string.byteLen(text)` 才是原始字节长度。
- `for c in "1234你好"` 默认按 UTF-8 字符迭代，双目标 `for i, c in text` 的 `i` 为 0 基字符索引；需要逐字节处理时使用 `string.bytes(text)`。
- `encoding` 负责字符集探测和 UTF-8/UTF-16/UTF-32 字节转换；普通文本返回 `string`，非 UTF-8 字节结果返回不可复制的 `Buffer`。`encoding.fromBytes`、`encoding.detectBytes` 等字节视图函数接收 `point data, c_size size`，常见写法是 `encoding.fromBytes(buf.data(), c_size(buf.len()), encoding.utf16)`。
- `crypto` 当前是文本/HEX 级 API：输入字符串按 UTF-8 原始字节处理；摘要、HMAC、HKDF、随机字节、ChaCha20 密文、AEAD 密文、Ed25519/X25519/X448 密钥和签名统一使用大写 HEX；AEAD 解密校验失败返回空字符串，签名/密钥参数无效时返回空字符串或 `false`。
- XRT TLS 状态机不直接放入 `crypto` 命名空间，后续应作为 `net.tls`、HTTP client 或 native-backed class 形式设计。

## 15. C 混编、ABI 与导出

已实现能力：

- `extern` 声明 C 函数、变量、常量进入 xlang。
- `export` 导出 xlang 函数、变量、常量到 package 命名空间。
- `export "c_symbol"` 显式生成 C ABI 导出符号。
- `export extern "c_symbol"` 声明 C 全局变量并公开到 xlang package 命名空间。
- 生成 C 头文件。
- 生成 ABI dump。
- `.xl` 与 `.c` 混合构建。
- C 侧可调用 xlang 导出函数。
- xlang 可直接调用声明后的 C 函数。
- C ABI 类型用于跨平台精确对齐。
- 可选参数导出到 C 时默认值失效，C 侧按普通参数签名调用。
- 多返回值、可变参数等 xlang 特性通过 xcall/封装边界与 C ABI 区分。
- class 成员函数可使用 `export "c_symbol" func method(...)` 导出 C ABI wrapper。
- 成员导出 wrapper 的第一个 C 参数固定为 `XL_Class_Name* self`。
- 成员多返回值导出为 `out0`、`out1` 等输出指针参数。
- 成员导出符号进入生成头文件和 ABI dump；ABI kind 为 `class_method`，并记录 `owner`。
- 当前阶段不支持 static 成员导出为 class method C ABI。
- 当前阶段不支持泛型 class 成员直接导出 C ABI。

变量与常量声明示例：

```xlang
extern "errno" int errno

export int packageCounter = 1
export var packageValue = "hello"
export const int packageVersion = 6

export "app_counter" int counter = 0
export "app_version" const int version = 1

export extern "host_counter" int hostCounter
```

规则：

- `extern "c_symbol" Type name` 只在当前 xlang 编译单元声明 C 全局变量。
- `export Type name = value` / `export const Type name = value` 只导出为 xlang package 符号，不进入 C 头文件。
- `export "c_symbol" Type name = value` / `export "c_symbol" const Type name = value` 同时导出 package 符号和 C ABI 符号。
- `export extern "c_symbol" Type name` 不拥有变量存储，只把 C 全局变量以公开 package 符号重新导出。
- `var` 只表示动态类型；旧 `extern "c_symbol" var int name`、`export "c_symbol" var int name = value` 写法不属于 DEMO6。

main 与入口规则：

- xlang `func main(...) as int` 会生成固定 C `main`。
- 生成的 C `main` 负责初始化 xlang/XRT 运行环境。
- 命令行参数转换为 xvalue 数组形式传入 xlang main。
- xlang main 返回值作为进程退出码。
- 顶层可执行语句可以和显式 `func main(...) as int` 共存；执行顺序固定为先执行顶层语句，再调用显式 `main`。
- 顶层语句和显式 `main` 共存时，生成器会把用户 `main` 降低为内部 `__xl6_user_main(xvalue arg)`，再由外层 `xlang_main(xvalue arg)` 统一调度。
- 如果 `.xl` 中没有 main，exe 目标假定 C 源提供 main。
- C main 场景下，C 代码需要显式调用 `xlInit` 和 `xlUnit`。
- DLL 目标不要求 main。

## 16. ABI 元数据、import、package 与扩展库

import 基础写法：

```xlang
import counter

func main(...) as int
    return counter.counterAdd(10, 32)
end
```

接口元数据当前由 `abi` 阶段承载（`.abi.json` / `.abi.txt`），不是独立的 XAPI 产物。`.abi.json` 当前只枚举 C ABI 导出符号：export 函数、export 变量、export 常量、extern 声明，以及类的 export 方法（kind 为 `class_method`，记录 `owner`）。class 完整字段布局、union 定义、derive 关系、protocol、`#doc` 文档块等 richer 元信息当前不进入 abi dump，后续需要单独设计。

已实现能力：

- ABI dump（`.abi.json` / `.abi.txt`）枚举 C ABI 导出符号。
- export 函数、export 变量、export 常量、extern 声明的 ABI 记录。
- 类的 export 方法 ABI 记录（含 owner）。
- package fixture。
- import 读取扩展库 ABI/声明并注入。
- import/package 与 C 混编测试闭环。
- 扩展库基础分发信息。
- 同一个项目中同一扩展库的多版本共存测试闭环。
- package resolver 基础能力。
- 相对路径 import 作用域测试。
- 导入类型不污染全局命名空间的测试。
- import 加载时对未知字段忽略、错误 JSON 和无效声明诊断。

当前原则：

- ABI 元数据是机器生成的导出符号表。
- xlang 环境保持显式导入，不做 C 符号自动污染。
- C 环境可以由 xlang 工具链自动注入必要头文件和声明。
- 扩展库优先由 xlang 声明层导出接口信息，不在当前阶段引入 C 解析器自动反推接口。
- package 依赖允许把显式导入信息写入元数据，由编译器在 import 时解析依赖链。
- 多版本共存以包名、版本和导出符号隔离为目标，避免 Python 式单环境依赖冲突。

当前边界：

- 当前 ABI 元数据只覆盖 C ABI 导出符号，不承载 class 完整布局、union、derive、protocol、doc 等 richer 元信息；这些后续需要单独设计。
- 当前已有 package/import 基础链路和测试夹具，但还不是完整包管理器。
- C 函数不会自动进入 xlang 环境；xlang 侧仍必须通过 `extern`、标准库或 `import` 获得干净声明。
- 历史 `xl xapi` 命令、`xlang-xapi-1` schema、`.xapi` / `.xapi.json` 产物已不存在，不提供兼容回退（详见 §20）。

## 17. DLL 嵌入式运行库

当前产物边界：

- `xl.dll`：轻量嵌入式运行版 DLL。
- `xlc.dll`：带 exe 编译能力的编译版 DLL。
- `xl.def`：DLL import definition。
- `xlc.def`：编译版 DLL import definition。
- `release` 不再生成或保留 `libtcc.dll`；`xl.dll` / `xlc.dll` 均静态内嵌 libtcc，并通过各自的 VFS 清单零外部依赖运行。
- `xlc_dll_embed_string.exe`：字符串脚本运行和编译 exe 验证程序。
- `xl_dll_null_context.exe`：空上下文防御验证程序。
- `xl_dll_threaded_host.exe`：多线程宿主验证程序。
- `xlc_api_embed.exe`：xlc.dll 嵌入式 API 验证程序。
- `xl_dll_xrt_exports.exe`：xl.dll XRT 符号导出验证程序。
- `xl_run_demo.exe`：DLL 运行范例程序。

已实现 API 方向：

- 创建/销毁语言上下文。
- 设置工作目录。
- 设置临时目录和 saveTemps 开关。
- 添加 include/lib/link/loadlib 依赖。
- 运行字符串脚本。
- 运行脚本文件。
- 运行时传入宿主构造的 `xvalue arg`。
- 编译字符串脚本到 exe。
- 编译脚本文件到 exe。
- 注册 C 函数进入上下文。
- 向执行环境注入当前进程内的 XRT/xlang 运行时符号。
- DLL API 边界由守门脚本维护，当前公开 API 数量基线为 70（xl.h 39 个 + xlc.h 31 个）。

公开 API：
```c
XL_ContextOptions xlDefaultContextOptions(void);
int xlRuntimeInit(void);
void xlRuntimeUnit(void);

XL_Context* xlCreateContext(const XL_ContextOptions* options);
void xlDestroyContext(XL_Context* ctx);

int xlSetWorkDir(XL_Context* ctx, const char* path);
int xlSetTempDir(XL_Context* ctx, const char* path);
int xlSetSaveTemps(XL_Context* ctx, int enabled);

int xlAddIncludePath(XL_Context* ctx, const char* path);
int xlAddLibraryPath(XL_Context* ctx, const char* path);
int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath);
int xlLoadDependencyLibrary(XL_Context* ctx, const char* path);
int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn);

int xlRunScriptString(XL_Context* ctx, const char* source);
int xlRunScriptFile(XL_Context* ctx, const char* path);
int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue arg);
int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue arg);

int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe);
int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe);

const char* xlGetLastError(XL_Context* ctx);
void xlClearLastError(XL_Context* ctx);
```

运行入口：
- xlang `func main(...) as int` 通常降低为 `int xlang_main(xvalue arg)`。
- 若同一编译目标同时包含顶层可执行语句和显式 `main`，外层入口仍为 `int xlang_main(xvalue arg)`，用户 `main` 降低为内部 `int __xl6_user_main(xvalue arg)`。
- DLL/API 和 CLI `xl run` 运行路径不调用 C `main`，也不生成临时 exe；脚本编译到 `TCC_OUTPUT_MEMORY` 后执行 `tcc_relocate()`，再通过 `tcc_get_symbol("xlang_main")` 直接调用。
- `xlRunScriptStringArg` / `xlRunScriptFileArg` 要求宿主传入的 `arg` 是 `array` 类型的 `xvalue`。

## 18. 代码生成与工程结构

当前代码生成状态：

- cgen 已拆分为多个独立职责文件。
- cgen 规则已形成对照表。
- 当前 cgen C 文件基线为 104 个。
- 当前 cgen 最大单文件为 `xl6_cgen_async_runtime.c`，约 1101 行。
- 当前 cgenrule 唯一规则基线为 756 条。
- 当前 cgenrule Markdown 文档基线为 43 个。
- 当前 cgen 单文件行数受守门脚本约束。
- C 输出路径已经从大量直接 `fprintf` 逐步收口到统一输出抽象。
- XRT buffer/file/time/memory 能力优先用于工具链与运行时。
- 旧语法、旧路径、旧格式不做兼容回退。

当前风险状态：

- 风险总账基线为 21 条，当前 21 条已收口，0 条开放。
- `RISK-ASYNC-001`、`RISK-CALL-001`、`RISK-LOOP-002` 已进入发布级守门；后续修改 async/task/callable/loop cleanup 时必须继续复核，不能回退为开放风险。

## 19. 范例与测试

当前测试资产：

- `demo6/test/cases` 下有 701 个 `.xl` 测试用例。
- 负例诊断命令基线为 442 条。
- 负例诊断产物基线为 441 个。
- 负例诊断内容断言基线为 442 条。
- package/extlib fixture 基线为 48 个。
- 标准库测试基线为 26 个。
- 标准库公开函数当前为 406 个（外加全局 `print` / `println` 2 个；守门脚本 `check_stdlib_coverage.ps1` 因正则未覆盖空命名空间，当前报 406），标准库常量当前为 46 个。
- DLL API 基线为 70 个。
- 风险总账基线为 21 条，当前 21 条已收口，0 条开放。

当前范例资产：

- `demo6/release/examples` 下有 127 个 `.xl` 范例。
- `demo6/release/examples` 下有 35 个 C/H 宿主、混编和辅助 fixture 文件。
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
xl build test.xl
xl build test.xl -o test.exe
test.exe
```

混合 C 构建：

```bat
cd demo6\release
xl build main.xl helper.c -o app.exe
```

## 20. 当前明确不属于已实现稳定口径的能力

DEMO6 是 xlang 准备发布的第一个候选口径，原则是不背负 DEMO5 或开发期旧设计的历史包袱。旧写法如果已经被收口，就视为语言中不存在，而不是兼容、映射或特殊友好提示。

以下能力不是当前根清单中的已实现稳定功能：

- `and` / `or` 短路逻辑的协议化方案。
- 完整 package manager。
- 完整调试器。
- 泛型约束的 trait/protocol bound 方法集合推导。
- 泛型 class 成员直接导出 C ABI 的专用实例化 wrapper。
- 更完整的跨平台 DLL 入口封装。

以下旧口径不属于 DEMO6 语法或运行模型：
- `func foo() -> int` 箭头返回值写法。
- `function(int) as int` 完整单词函数签名写法。
- `0X`、`0O`、`0B` 大写数字前缀。
- `&H`、`&O`、`&B` 及其大小写变体。
- `extern "c_symbol" var int name` 这类 `extern/export` 变量声明中的额外 `var` 标记词。
- `export "c_symbol" var int name = value` 这类旧 C ABI 变量导出写法。
- `deepcopy()` 兼容别名。
- `xl xapi` 命令、`xlang-xapi-1` schema、`.xapi` / `.xapi.json` 产物（接口元数据已并入 `abi` 阶段，仅承载 C ABI 导出符号）。
- `lib/tcc/include/...` 这类对上层源码目录有依赖的内置资源路径。

这些能力应先进入 `demo6/docs` 的权威设计文档，再进入实现与测试。

## 本轮风险压实补充

当前 `RISK-ASYNC-001`、`RISK-CALL-001`、`RISK-LOOP-002` 继续保持发布级关闭口径；这不表示后续停止补测，而是要求相关模块改动必须持续带着这三条风险线复核。

- `RISK-ASYNC-001`：`task.spawn(...)` 临时 future 进入 `task.all/race` 时，已经覆盖 int/string/bool/float 的“owned 临时源 + borrowed 空源”组合，空源错误必须优先传播。
- `RISK-CALL-001`：成员 function 返回 string 后进入命名参数和重排命名参数已进入 cgen 守门，不允许回退 GNU statement expression。
- `RISK-LOOP-002`：嵌套 while 的 `break 2` / `continue 2` 已验证局部 class 按 `inner -> outer -> goto` 顺序释放，和 range-for 路径对齐。

### task.spawn 动态 var 参数承载位守门

`task.spawn(varArgJob, dynamicVarExpr)` 中的动态 `var` 实参必须先生成 `xvalue __xl6_task_arg_N` 临时量，再传入 `xl6_task_spawn_values(...)`。当前守门覆盖：

- 声明初始化、赋值、return、表达式语句。
- 条件语句、循环条件、`range(...)` 循环边界。
- typed container 字面量、async typed-container return。
- 普通调用实参、命名实参、多返回声明/赋值 RHS。
- `assert`、`throw`、`yield`。
- `value in range(...)` 与 f-string 插值。
- typed array 索引读与索引写目标，例如 `values[await task.spawn(...)]` 和 `values[await task.spawn(...)] = 77`。
- typed array 安全索引与空合并转换，例如 `int(values?[await task.spawn(...)] ?? 88)`。
- `for ... in` 的 typed-container iterable 与 generator iterable，例如 `for item in makeArray(await task.spawn(...))`。

### task.spawn 聚合 null-first 输入守门

`task.all/race` 数组字面量输入已经覆盖两种 source 顺序：

- `[task.spawn(...), missingFuture]`：owned 临时 future 在前，borrowed 空 future 在后。
- `[missingFuture, task.spawn(...)]`：borrowed 空 future 在前，owned 临时 future 在后。

两种顺序都必须抛出 `task.all source future is null` 或 `task.race source future is null`，并释放已经由当前聚合调用拥有的临时 `task.spawn` future。

### task.spawn typed-array null-first 输入守门

显式 `array<future<T>>` 输入也已经覆盖 null-first 顺序：

```xlang
future<int> missingSpawnFuture
array<future<int>> jobs = array[missingSpawnFuture, task.spawn(addJob, 180, 1)]

int state = 0
try
    array<int> values = await task.all(jobs)
    state = values.length
catch e
    if e.message == "task.all source future is null" then
        state = 42
    end
end
assert state == 42

int tail = await jobs[1]
assert tail == 181
```

规则：

- typed array 输入走 `xl6_task_all_from_array` / `xl6_task_race_from_array`，和数组字面量的 `xl6_task_all_values` / `xl6_task_race_values` 是两条不同生成路径。
- 第一个 source 为 null 时，聚合必须优先抛出 source-null 错误。
- 聚合失败不能吞掉 typed array 中后续 source future；用户仍可通过 `await jobs[1]` 显式等待原 `task.spawn` future。
- 当前守门覆盖 `int`、`string`、`bool`、`float` 的 all/race 四组路径，并锁定生成形态和运行行为。
