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
func add(int a, int b) -> int
    return a + b
end

export "math_add_i64" func add_i64(int64 a, int64 b) -> int64
    return a + b
end
```

其中：

- `add` 是 source name。
- `fn:<module>.add` 是 overload group id。
- `fn:<module>.add(int,int)->int` 是 signature id。
- `xl6_f_add__<hash>` 是 internal ABI symbol 的方向。
- `math_add_i64` 是 C export symbol。

## 3. signature id

signature id 是 ABI 的核心稳定文本。

它必须使用规范化后的类型名，不能直接使用源码原文。

普通函数：

```text
fn:<module>.<name>(<param-types>)-><return-types>
```

方法：

```text
method:<module>.<Type>.<name>(<param-types>)-><return-types>
```

构造：

```text
create:<module>.<Type>(<param-types>)->()
```

析构：

```text
destroy:<module>.<Type>()->()
```

custom extern 方法：

```text
custom:<module>.<Type>.<name>(<param-types>)-><return-types>
```

规则：

- 无返回值记为 `->()`。
- 单返回值记为 `->int`、`->string` 等。
- 多返回值记为 `->(int,string)`。
- 参数列表只记录类型，不记录参数名。
- 可选参数默认值不进入 signature id。
- 可变参数进入 signature id，例如 `(...array<var>)` 或 `(...dict<var>)`。
- 泛型函数声明使用规范化类型参数名，例如 `T0`、`T1`。
- 泛型实例使用实例化后的真实类型。

示例：

```text
fn:app.add(int,int)->int
fn:app.pair()->(int,string)
method:app.Vec2.length2()->int
create:app.Image(string)->()
custom:app.Image.resize(int,int)->int
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
cm      custom 方法
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
export "math_add_i64" func add(int64 a, int64 b) -> int64
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
export "add" func add(int a, int b) -> int
    ...
end

export "add" func add(float a, float b) -> float
    ...
end
```

## 9. C ABI extern

C 符号进入 xlang 使用 `extern`。

```xlang
extern "xrtSleep" func sleep(uint32 ms)
extern "xrtTimer" func timer() -> float64
```

规则：

- 字符串是 C 真实符号。
- xlang 名称进入普通重载组。
- 可以用多个 C 符号声明同一个 xlang 重载名。

示例：

```xlang
extern "math_add_i64" func add(int64 a, int64 b) -> int64
extern "math_add_f64" func add(float64 a, float64 b) -> float64
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

## 11. class ABI

class 默认只作为 xlang 内部 ABI。

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
- class 方法 C 侧第一个参数是实例指针。
- class 构造和析构也是内部 ABI 函数。
- class 不默认出现在生成给 C 用户的公开头文件里。

如果要对 C 暴露 class，第一阶段建议用 `point` 句柄或显式 C wrapper。

## 12. custom ABI

custom 是 C ABI 的轻量入口。

建议内部布局：

```c
typedef struct XL_Custom_Image {
    xptr handle;
} XL_Custom_Image;
```

规则：

- custom 实例只保存句柄。
- custom extern 方法直接调用用户声明的 C 符号。
- custom helper 方法使用 internal ABI symbol。
- custom 不生成动态 method table。
- custom 不公开内部句柄字段给 xlang 代码。
- custom 如果声明 `extern create`，ABI 元信息必须记录 create 对应的 C 符号和返回句柄类型。
- custom 如果没有声明 `extern create`，ABI 元信息必须标记该类型只能通过已有指针绑定初始化。
- custom 如果声明 `extern destroy`，ABI 元信息必须记录 destroy 对应的 C 符号，cgen 在生命周期结束处对非空句柄插入清理调用。
- custom 如果没有声明 `extern destroy`，cgen 不释放句柄。

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
fn app.add(int,int)->int
  group: fn:app.add
  symbol: xl6_f_add__6F2A91C013FE18B0

export math_add_i64
  signature: fn:app.add_i64(int64,int64)->int64
  wrapper: xl6_wrap_math__add__i64__91AA0FBB3221A017
```

## 16. 暂缓能力

以下 ABI 能力暂缓：

- 多返回值直接导出到 C。
- `var` 直接导出到 C。
- 容器直接导出到 C。
- class/custom 直接作为 C 公开值类型。
- C 调用约定属性，例如 stdcall/fastcall。
- C 结构体按值互传。
- 字符串返回自动分配和自动释放协议。
- 动态对象 ABI。
