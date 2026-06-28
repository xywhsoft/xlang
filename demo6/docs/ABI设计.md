# xlang DEMO6 ABI 设计

本文档记录 DEMO6 的 ABI 设计口径。

ABI 设计必须服从 DEMO6 第一绝对原则：xlang 能够和 C 完美混编。

## 1. 总原则

DEMO6 必须区分两层 ABI：

- xlang 内部 ABI：用于 xlang 编译单元、模块、泛型实例、重载函数、class 方法之间的内部连接。
- C ABI：用于 C 代码通过头文件和 C 符号调用 xlang，或 xlang 调用 C 符号。

两层 ABI 不能混在一起：

- xlang 内部 ABI 可以使用编译器生成的稳定符号。
- C ABI 必须使用用户显式声明的 C 符号，或者生成头文件中明确出现的 C 符号。
- 函数重载只属于 xlang 内部语义，不直接暴露给 C。
- 面向 C 的 `export "c_symbol"` 必须是唯一 C 符号。

## 2. 名称层级

DEMO6 中同一个函数可能有多种名称。

建议固定以下名称层级：

```text
source name
overload group id
signature id
internal ABI symbol
C export symbol
```

含义：

- source name：源码中写的名称，例如 `add`。
- overload group id：重载组，例如 `fn:math.add` 或 `method:Vec2.add`。
- signature id：完整签名文本，用于唯一描述某个函数声明。
- internal ABI symbol：编译器生成的 C 标识符，用于内部 C 代码连接。
- C export symbol：用户通过 `export "..."` 指定的真实 C ABI 符号。

示例：

```xlang
func add(int a, int b) as int
    return a + b
end

export "math_add_i64" func add_i64(int64 a, int64 b) as int64
    return a + b
end
```

其中：

- `add` 是 source name。
- `fn:<module>.add` 是 overload group id。
- `fn:<module>.add(int,int) as int` 是 signature id。
- `xl6_f_add__<hash>` 是 internal ABI symbol 的方向。
- `math_add_i64` 是 C export symbol。

## 3. signature id

signature id 是 ABI 的核心稳定文本。

它必须使用规范化后的类型名，不能直接使用源码原文。

普通函数：

```text
fn:<module>.<name>(<param-types>) as <return-types>
```

方法：

```text
method:<module>.<Type>.<name>(<param-types>) as <return-types>
```

构造：

```text
create:<module>.<Type>(<param-types>) as ()
```

析构：

```text
destroy:<module>.<Type>() as ()
```

custom extern 方法：

```text
custom:<module>.<Type>.<name>(<param-types>) as <return-types>
```

规则：

- 无返回值记为 `as ()`。
- 单返回值记为 `as int`、`as string` 等。
- 多返回值记为 `as (int,string)`。
- 参数列表只记录类型，不记录参数名。
- 可选参数默认值不进入 signature id。
- 可变参数进入 signature id，例如 `(...array<var>)` 或 `(...dict<var>)`。
- 泛型函数声明使用规范化类型参数名，例如 `T0`、`T1`。
- 泛型实例使用实例化后的真实类型。

示例：

```text
fn:app.add(int,int) as int
fn:app.pair() as (int,string)
method:app.Vec2.length2() as int
create:app.Image(string) as ()
custom:app.Image.resize(int,int) as int
```

## 4. 类型规范化

ABI 中的类型名必须规范化。

建议规则：

- 内置类型使用固定小写名称，例如 `int64`、`float64`、`string`。
- `int` 代表 xlang 默认整数类型，不等同于 C 的 `int`。
- `float` 代表 xlang 默认浮点类型。
- `typedef` 默认是透明别名，进入 ABI 时展开为底层类型。
- `array < int >`、`array<int>` 统一为 `array<int>`。
- class/custom 类型使用模块限定名，例如 `<module>.Vec2`。
- 泛型类型参数按声明顺序规范化为 `T0`、`T1`。

说明：

- `func f(int a)` 和 `typedef MyInt = int; func f(MyInt a)` 在 ABI 上视为同一参数类型。
- 如果后续需要不透明别名，必须单独设计，不能让普通 `typedef` 自动承担不透明类型语义。

### 4.1 C ABI 兼容类型

DEMO6 提供一组 `c_*` 类型用于 C ABI 边界。

这些类型不是 xlang 推荐的日常编程类型。普通 xlang 代码应优先使用 `int`、`float`、`bool`、`string` 等语言类型；只有在 `extern`、`export`、`custom`、DLL API、C 混编和头文件生成等需要精确匹配 C ABI 的场景下，才使用 `c_*` 类型。

| xlang 类型 | 生成 C 类型 | 说明 |
| --- | --- | --- |
| `c_char` | `char` | C 原生 `char`，不承诺 signedness |
| `c_schar` | `signed char` | 明确有符号字符/8 位整数语义 |
| `c_uchar` | `unsigned char` | 明确无符号字符/8 位整数语义 |
| `c_short` | `short` | C 原生 `short` |
| `c_ushort` | `unsigned short` | C 原生 `unsigned short` |
| `c_int` | `int` | C 原生 `int`，不等同于 xlang `int` |
| `c_uint` | `unsigned int` | C 原生 `unsigned int` |
| `c_long` | `long` | C 原生 `long`，长度跟随平台 ABI |
| `c_ulong` | `unsigned long` | C 原生 `unsigned long` |
| `c_longlong` | `long long` | C 原生 `long long` |
| `c_ulonglong` | `unsigned long long` | C 原生 `unsigned long long` |
| `c_size` | `size_t` | C 标准库尺寸类型 |
| `c_ssize` | `xl6_ssize_t` | 与 `size_t` 等宽的有符号尺寸类型 |
| `c_ptrdiff` | `ptrdiff_t` | C 标准指针差值类型 |
| `c_intptr` | `intptr_t` | 可容纳指针的有符号整数 |
| `c_uintptr` | `uintptr_t` | 可容纳指针的无符号整数 |
| `c_bool` | `bool` | C99 `_Bool` / `bool` |
| `c_float` | `float` | C 原生 `float` |
| `c_double` | `double` | C 原生 `double` |
| `c_wchar` | `wchar_t` | C 原生宽字符类型，长度跟随平台 ABI |

`c_ssize` 需要特别说明：C 标准没有统一的 `ssize_t`，POSIX 和 Windows 对它的提供方式不同。DEMO6 在生成 C 文件和导出头文件时统一声明：

```c
typedef intptr_t xl6_ssize_t;
```

因此 xlang 源码使用 `c_ssize`，生成的 C ABI 使用 `xl6_ssize_t`。这样可以和 `c_intptr` 在语义上区分开，同时保留后续切换到平台原生 `ssize_t` / `SSIZE_T` 的空间。

## 5. internal ABI symbol

internal ABI symbol 是生成 C 代码时使用的内部函数名。

它必须满足：

- 是合法 C 标识符。
- 稳定、可重复。
- 不依赖编译顺序。
- 不依赖内存地址或随机哈希。
- 能处理中文标识符和 UTF-8 标识符。
- 不要求人类完全可读，但应保留一点名称线索便于调试。

建议格式：

```text
xl6_<kind>_<encoded-name>__<hash16>
```

其中：

- `xl6` 表示 DEMO6 internal ABI。
- `kind` 表示符号种类。
- `encoded-name` 是源码名称的稳定 C 标识符编码。
- `hash16` 是 signature id 的 64 位稳定哈希，写成 16 位十六进制。

建议 kind：

```text
f       普通函数
m       class 方法
nm      native-backed class extern 方法
ctor    create
dtor    destroy
g       泛型实例
wrap    C ABI wrapper
```

示例方向：

```text
xl6_f_add__6F2A91C013FE18B0
xl6_m_Vec2_length2__A771003E9337C912
xl6_ctor_Image__D0A15AE39DF70120
xl6_cm_Image_resize__7B516E82D6E9A81F
```

## 6. C 标识符编码

为了支持中文标识符，internal ABI symbol 不能简单丢弃非 ASCII 字符。

建议编码规则：

- ASCII 字母和数字原样保留。
- `_` 编码为 `__`，避免和转义前缀冲突。
- 其他 Unicode 码点编码为 `_uXXXX_` 或 `_uXXXXXX_`。
- 最终符号始终带 `xl6_` 前缀，因此不需要担心源码名以数字开头。

示例方向：

```text
add      -> add
add_one  -> add__one
长度     -> _u957F__u5EA6_
```

说明：

- 编码只用于 internal ABI symbol。
- C export symbol 不做编码，必须完全使用用户写在字符串中的 C 符号。

## 7. 稳定哈希

internal ABI symbol 建议使用 signature id 的稳定哈希。

建议第一阶段使用 FNV-1a 64-bit。

规则：

- 哈希输入必须是 UTF-8 的 signature id。
- 哈希输出固定 16 位大写十六进制。
- 哈希只用于缩短内部 C 符号，不作为唯一语义来源。
- 调试 dump 必须能输出 signature id 和 internal ABI symbol 的映射表。

## 8. C ABI export

C ABI export 必须显式指定 C 符号。

```xlang
export "math_add_i64" func add(int64 a, int64 b) as int64
    return a + b
end
```

生成方向：

```c
int64_t math_add_i64(int64_t a, int64_t b);
```

规则：

- C export symbol 完全等于字符串内容。
- 同一模块中不能有两个导出使用同一个 C export symbol。
- C export symbol 不参与 xlang 重载解析。
- C export wrapper 内部调用被选中的 internal ABI symbol。
- 重载函数要导出给 C 时，每个导出 wrapper 必须有不同 C 符号。

错误示例：

```xlang
export "add" func add(int a, int b) as int
    ...
end

export "add" func add(float a, float b) as float
    ...
end
```

## 9. C ABI extern

C 符号进入 xlang 使用 `extern`。

```xlang
extern "xrtSleep" func sleep(uint32 ms)
extern "xrtTimer" func timer() as float64
```

规则：

- 字符串是 C 真实符号。
- xlang 名称进入普通重载组。
- 可以用多个 C 符号声明同一个 xlang 重载名。

示例：

```xlang
extern "math_add_i64" func add(int64 a, int64 b) as int64
extern "math_add_f64" func add(float64 a, float64 b) as float64
```

限制：

- 同一个 C 符号不能在同一模块中声明为不同 C ABI 签名。
- C extern 不自动生成重载 wrapper。
- 如果 C 侧只有一个模糊符号，xlang 侧必须只声明一个准确签名。

## 10. C ABI 类型映射

第一阶段 C ABI 只支持能稳定表达的类型。

建议映射：

```text
xlang bool     -> C bool
xlang int      -> C int64_t
xlang int8     -> C int8_t
xlang uint8    -> C uint8_t
xlang int16    -> C int16_t
xlang uint16   -> C uint16_t
xlang int32    -> C int32_t
xlang uint32   -> C uint32_t
xlang int64    -> C int64_t
xlang uint64   -> C uint64_t
xlang float    -> C double
xlang float32  -> C float
xlang float64  -> C double
xlang point    -> C xptr
```

`string` 需要单独处理所有权：

- C -> xlang 参数：`string` 对应 C 的 UTF-8 只读字符串指针，xlang 如需保存必须复制。
- C -> xlang 返回：`string` 表示 C 返回 UTF-8 只读字符串指针，xlang 立即复制为 xlang string。
- xlang -> C 参数：`string` 对应 UTF-8 只读字符串指针，调用期间有效。
- xlang -> C 返回：第一阶段不直接支持，必须显式改成 `point`、out 参数、XRT string 句柄或用户自定义 wrapper。

第一阶段不直接进入 C ABI：

- `var`
- `array/list/set/dict`
- `class/custom`
- 多返回值
- 泛型声明
- 捕获闭包
- 带可选参数的原函数
- 带 `...` 或 `***` 的原函数

## 11. class 数据 ABI

class 数据 ABI 采用“数据尽量透明，行为显式导出”的原则。

也就是说，C 侧可以通过生成的头文件、XRT 类型和 XAPI 元数据理解并操作 xlang 数据结构；但 C 侧不能默认直接调用所有 xlang 成员函数，成员函数需要显式 export 或通过 xcall/协议接口调用。

建议内部 C 布局：

```c
typedef struct XL_Class_Vec2 {
    int64_t x;
    int64_t y;
} XL_Class_Vec2;
```

规则：

- class 字段顺序按源码声明顺序。
- 继承时父类字段作为子类布局前缀。
- 字段类型使用 XRT 或 ABI 文档中定义的 C 可见类型表达。
- `string`、`array`、`list`、`set`、`dict`、`function`、object 动态字段等复合数据，应通过 XRT 类型和协议 API 向 C 暴露。
- 泛型已实例化类型应生成具体布局和类型元数据。
- 泛型未实例化类型只能作为 XAPI 元数据存在，C 不能直接构造值。
- 带 virtual 的 class 必须在元数据中描述 vtable 或等价动态派发表。
- C 可以读取生成头文件中的结构声明，但对资源字段、私有字段和协议字段的写入责任由 ABI 文档约束。

## 12. native-backed class ABI

原 `custom` 的 C opaque handle 能力并入 native-backed class ABI。

建议内部布局：

```c
typedef struct XL_Class_Image {
    xptr handle;
} XL_Class_Image;
```

规则：

- native-backed class 是 class，不另设独立 custom ABI。
- native 字段保存 C 句柄、C 指针或未来明确支持的 typed pointer。
- extern 成员方法直接调用用户声明的 C 符号。
- extern 成员方法的 C 侧第一个参数固定接收 native 句柄或实例指针，由编译器自动传入。
- xlang helper 方法使用 internal ABI symbol。
- native-backed class 不生成动态 method table，除非 class 自身声明 virtual 能力。
- 如果声明 `__create__`，ABI 元信息必须记录构造对应的 C 符号、返回句柄类型和失败策略。
- 如果没有声明 `__create__`，ABI 元信息必须标记该类型只能通过已有指针或默认构造规则初始化。
- 如果声明 `__destroy__`，ABI 元信息必须记录析构对应的 C 符号，cgen 在生命周期结束处对非空资源插入清理调用。
- 如果没有声明 `__destroy__`，cgen 不释放 native 资源。

### 12.1 class 成员函数 C export ABI

xlang 成员函数只有显式导出时才进入 C 公开 API。

推荐写法：

```xlang
class Player
    int hp

    export "player_get_hp" func getHp() as int
        return self.hp
    end

    export "player_set_hp" func setHp(int value)
        self.hp = value
    end
end
```

生成给 C 的公开声明方向：

```c
int64_t player_get_hp(XL_Class_Player* self);
void player_set_hp(XL_Class_Player* self, int64_t value);
```

规则：

- C export wrapper 的第一个参数是实例指针，static 方法没有该参数。
- `export "symbol"` 是推荐形式，确保 C 头文件符号稳定、可读。
- 如果重载成员函数需要导出到 C，必须为每个重载提供不同的 C 符号。
- 没有显式 C 符号的 `export func` 只进入 xlang 模块导出，不生成 C 头文件声明。
- 可选参数导出到 C 时默认值失效，C 声明必须包含完整参数列表。
- `...` 导出到 C 时映射为 `xarray` 或等价 XRT array 类型参数。
- `***` 导出到 C 时映射为 `xdict` 或等价 XRT dict 类型参数。
- 多返回值导出到 C 时使用 out 参数或 xcall wrapper，不能伪装成多个 C 返回值。
- `var`、`function`、容器、object 动态字段等复杂值导出到 C 时必须使用 XRT 类型和元数据描述。
- internal ABI symbol 仍使用 signature id 保证唯一性，不要求人工可读。
- C export symbol 是公开契约，不能随 internal ABI symbol 变化自动改名。

## 13. 模块 ABI

DEMO6 模块应生成模块 ABI 元信息。

建议模块元信息至少包含：

```text
module name
abi version
source hash
export count
export name
export kind
signature id
internal ABI symbol
C export symbol
```

说明：

- xlang 模块导入使用 signature id 和 internal ABI symbol。
- C 用户使用头文件和 C export symbol。
- 模块 ABI 元信息用于调试、绑定、动态加载和版本检查。

## 14. ABI 版本

DEMO6 第一阶段建议固定：

```text
xlang abi version = 6
```

规则：

- ABI version 改变时，模块二进制缓存必须失效。
- signature id 规则改变时，ABI version 必须改变。
- 类型布局规则改变时，ABI version 必须改变。
- C export symbol 是用户控制的，不随 internal ABI symbol 改名自动迁移。

## 15. 调试输出

为了便于调试，`xl.exe` 应提供 ABI dump。

建议命令：

```bat
xl.exe abi module.xl
xl.exe abi module.xl --json
```

建议输出：

```text
fn app.add(int,int) as int
  group: fn:app.add
  symbol: xl6_f_add__6F2A91C013FE18B0

export math_add_i64
  signature: fn:app.add_i64(int64,int64) as int64
  wrapper: xl6_wrap_math__add__i64__91AA0FBB3221A017
```

## 16. 暂缓能力

以下 ABI 能力暂缓：

- 多返回值作为多个 C 返回值直接导出；应使用 out 参数或 xcall wrapper。
- `var` 脱离 XRT 类型信息直接裸导出。
- 容器脱离 XRT 类型和 XAPI 元数据直接裸导出。
- class 按值传参与按值返回。
- C 调用约定属性，例如 stdcall/fastcall。
- C 结构体按值互传。
- 字符串返回自动分配和自动释放协议。
- object 动态字段的裸结构访问；应通过 XRT dict 或 object 协议 API。
