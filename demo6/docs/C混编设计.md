# xlang DEMO6 C 混编设计

本文档记录 DEMO6 中 xlang 与 C 互相声明、调用、导出的设计口径。

当前方案已被用户采纳，是 DEMO6 C 混编设计的正式口径。

## 1. 设计目标

xlang 与 C 混编应通过声明实现。

核心目标：

- C 函数可以很容易声明到 xlang 中。
- xlang 函数也应该可以很容易声明到 C 中。
- 不通过多套兼容回退实现互操作。
- C 侧最终看到的是普通 C ABI：头文件、符号、函数、变量。
- xlang 侧仍然保持清晰的语言声明。

## 2. C 声明到 xlang

C -> xlang 使用 `extern`。

```xlang
extern "xrtSleep" func sleep(uint32 ms)
extern "xrtTimer" func timer() as float64
extern "errno" int errno
extern type HANDLE = point
```

C 原生可变参数使用 `c_vararg` 标记，不使用 xlang 的 `...`：

```xlang
extern "printf" func printf(string fmt, c_vararg) as c_int
```

规则：

- `c_vararg` 只能用于顶层 `extern "c_symbol" func`。
- `c_vararg` 必须是最后一个参数。
- `c_vararg` 不生成 `xvalue arg`，而是直接生成 C `...` 原生调用。
- 调用方必须显式使用 `c_int`、`c_double` 等 ABI 类型保证 C vararg 宽度正确。

xlang 自身的 `...` / `***` 仍然可以进入 C 边界，但它们表示 xlang/xcall 风格的参数包：C 侧函数会收到一个 `xvalue arg`，分别承载 `array<var>` 或 `dict<var>`。

含义：

- 字符串是 C 侧真实符号名。
- xlang 名称是语言内使用的名称。
- 类型签名决定调用时的 C ABI 转换。

## 3. xlang 声明到 C

xlang -> C 使用同一套 `export` 语法族，建议增加显式 C 符号名写法。

```xlang
export "app_add" func add(int left, int right) as int
	return left + right
end

export "app_counter" int counter = 0

export "app_version" const int version = 1
```

含义：

- `export "app_add"` 表示生成一个 C ABI 可见符号 `app_add`。
- xlang 内部名称仍然是 `add`。
- C 侧通过生成的头文件声明并调用。

建议规则：

- `export func name(...)`：只导出为 xlang 模块符号。
- `export "c_symbol" func name(...)`：导出为 xlang 模块符号，同时生成 C ABI wrapper。
- `export Type name = value`：只导出为 xlang package 变量符号。
- `export "c_symbol" Type name = value`：导出为 xlang package 变量符号，同时生成 C 可访问变量符号。
- `export const Type name = value`：只导出为 xlang package 常量符号。
- `export "c_symbol" const Type name = value`：导出为 xlang package 常量符号，同时生成 C 可访问常量符号。
- `export extern "c_symbol" Type name`：声明 C 全局变量，并公开到 xlang package 命名空间。

这样仍然是一套 `export` 机制，只是显式带 C 符号名时才进入 C ABI。

## 4. 生成给 C 使用的产物

当源码包含 C ABI export 时，DEMO6 应生成：

```text
module.xl
  -> module.c
  -> module.h
  -> module.dll / module.lib / module.exe
```

示例命令：

```bat
xl.exe cgen module.xl --emit module.c --emit-header module.h
xl.exe build module.xl -shared -o module.dll --emit-header module.h
```

生成的 `module.h` 示例：

```c
#ifndef APP_MODULE_H
#define APP_MODULE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_init();
int app_shutdown();

int64_t app_add(int64_t left, int64_t right);
extern int64_t app_counter;
extern const int64_t app_version;

#ifdef __cplusplus
}
#endif

#endif
```

C 调用方：

```c
#include "module.h"

int main()
{
	app_init();
	int64_t value = app_add(1, 2);
	app_counter = value;
	app_shutdown();
	return value;
}
```

## 5. C ABI wrapper

xlang 导出到 C 时，不建议直接暴露内部函数实现，而是生成一层简单 C ABI wrapper。

wrapper 负责：

- 确保模块初始化。
- 按声明把 C 参数转为 xlang 内部调用需要的形式。
- 调用 xlang 内部函数。
- 把返回值转回 C ABI。
- 捕获 xlang 异常或运行时错误。
- 错误时设置 XRT/xlang 错误状态，并返回该类型的默认值。

示意：

```c
int64_t app_add(int64_t left, int64_t right)
{
	// 初始化模块
	app_init();

	// 调用内部实现
	return __xl_internal_add(left, right);
}
```

## 6. native-backed class C API 映射

native-backed class 是 C API 进入 xlang 的 OOP 声明机制。

当前方案已确认：DEMO6 不再把 `custom` 作为独立用户侧类型系统继续扩展；原 custom 的 native opaque handle 能力并入 class。

示例 C API：

```c
typedef struct xxx xxx;

xptr xxx_create();
int64_t xxx_destroy(xptr self);
int64_t xxx_add(xptr self, int64_t param);
```

其中 `xptr` 表示 C 侧黑盒指针类型，xlang 侧用 `point` 承接。

对应 xlang 声明：

```xlang
class xxx
    native void* handle

	extern "xxx_create" func __create__()
	extern "xxx_destroy" func __destroy__()
	extern "xxx_add" func add(int param) as int
end
```

调用：

```xlang
auto a = xxx()
a.add(1)
```

期望生成方向：

```c
XL_Class_xxx a;
a.handle = xxx_create();
xxx_add(a.handle, 1);
xxx_destroy(a.handle);
```

规则：

- native-backed class 实例内部保存一个黑盒 C 指针或 C 数据指针。
- extern 成员函数的第一个 C 参数固定为 native 句柄或实例指针。
- xlang 调用 extern 成员方法时，编译器直接把内部句柄作为第一个参数传给 C 函数。
- native-backed extern 方法调用不能触发动态查找、反射、装箱或额外分配。
- native-backed class 默认不绑定动态对象字典；只有继承 `object` 时才具备动态字段能力。
- 后续如果支持 `xxx* self` 这类 typed pointer receiver，必须作为显式增强能力，不影响第一阶段统一黑盒指针规则。
- 如果 class 声明了 extern `__create__`，`Type(...)` 调用对应 C 构造函数并保存返回句柄。
- 如果 class 没有声明 extern `__create__`，可通过显式 native 绑定规则接收已有 C 指针。
- 如果 class 声明了 extern `__destroy__`，最后一个受管理 `xvalue` 引用释放时，由 XRT 类型描述器对非空句柄调用一次析构。
- 如果 class 没有声明 extern `__destroy__`，xlang 不释放句柄，也不猜测释放方式。
- native 资源的默认初始化值是空句柄；普通方法调用不自动做空句柄检查，而是直接把句柄传给 C 函数。
- 第一阶段不引入手写 `delete` 语句，也不把普通关闭方法自动识别为生命周期结束。

## 7. 类型范围

第一阶段只允许 C ABI 能直接表达的类型。
完整 ABI 规则见 `ABI设计.md`。

建议首批支持：

- `bool` -> C `bool`
- `int` / `int8` / `uint8` / `int16` / `uint16` / `int32` / `uint32` / `int64` / `uint64`
- `float` / `float32` / `float64`
- C ABI 专用类型：`c_char` / `c_schar` / `c_uchar` / `c_short` / `c_ushort` / `c_int` / `c_uint` / `c_long` / `c_ulong` / `c_longlong` / `c_ulonglong` / `c_size` / `c_ssize` / `c_ptrdiff` / `c_intptr` / `c_uintptr` / `c_bool` / `c_float` / `c_double` / `c_wchar`
- `string` 参数与 C UTF-8 只读字符串指针互转，xlang 导出到 C 时第一阶段不直接返回 `string`
- `point` -> C 黑盒指针
- 普通 class、泛型 class 实例和 native-backed class -> C `xvalue`；`byref/out` class -> C `xvalue*`

`c_*` 类型只用于 ABI 对齐，不作为 xlang 日常推荐类型。需要固定宽度时使用 `int32`、`uint64` 等类型；需要跟随 C 编译器和目标平台 ABI 时使用 `c_int`、`c_long`、`c_size` 等类型。

第一阶段不做无元数据裸导出：

- `var`
- `array/list/set/dict`
- class 的裸 payload 值；class 正式 C ABI 必须使用 `xvalue` 句柄和 ABI 元数据
- `error`
- `future/task/channel/generator`
- 多返回值函数
- 重载函数的同名 C 导出

处理规则：

- 不支持的类型在 `check` 或 `cgen` 阶段直接报错。
- class 使用 `xvalue` 是固定 C ABI，不是旧写法失败后的兼容回退。
- 如果要跨 C 暴露复杂对象，应显式使用 XRT 类型、ABI 元数据、生成头文件或 C ABI wrapper。
- C 可以访问 xlang 的数据结构，但访问方式必须由 XRT 类型和 ABI 元数据支撑。
- C 调用 xlang 成员函数必须通过显式 `export "c_symbol"` wrapper 或未来的 xcall 入口。

## 8. 重载与符号名

C 没有 xlang 风格的重载，因此 C ABI export 必须有唯一 C 符号。

建议规则：

```xlang
export "math_add_int" func add(int a, int b) as int
	return a + b
end

export "math_add_float" func add(float a, float b) as float
	return a + b
end
```

如果多个 `export "same_symbol"` 指向不同函数，编译时报错。

如果重载函数使用 `export func add(...)` 但没有 C 符号，只影响 xlang 模块导出，不生成 C 头文件声明。

重载解析的完整规则见 `函数重载设计.md`。

## 8.1 class 数据与成员函数导出

C 与 xlang 的 class 混编采用以下原则：

- 数据尽量透明。
- 行为显式导出。
- 复杂语义通过 XRT 类型、ABI 元数据和 wrapper 解释。

数据透明表示：

- C 可以看到生成的 class C 结构体声明。
- C 可以访问 `string`、`array`、`list`、`set`、`dict`、`function`、object 动态字段等 XRT 能表达的数据。
- 泛型已实例化类型可以生成具体 C 类型和元数据。
- 泛型未实例化类型只作为元数据存在，不能直接构造值。
- virtual class、object 动态字段和协议能力通过元数据与运行时 helper 暴露，不靠 C 直接猜测布局。

行为显式导出表示：

- xlang 成员函数不会默认全部进入 C API。
- 需要给 C 调用的成员函数必须显式 `export "c_symbol"`。
- 导出的成员函数生成 C wrapper，第一个参数是实例指针；static 方法没有实例参数。
- 可选参数在 C API 中失效，C wrapper 使用完整参数列表。
- `...` 映射为 XRT array 参数。
- `***` 映射为 XRT dict 参数。
- 多返回值使用 out 参数或 xcall wrapper。
- 重载成员函数必须为每个 C 导出指定不同符号。

## 8.2 C 回调与 xlang func

普通 C 函数指针与 xlang `func` 采用双向、显式签名桥接。

```xlang
extern "c_apply" func cApply(func(c_int) as c_int callback, c_int value) as c_int

func twice(c_int value) as c_int
    return value * 2
end

export "xl_apply" func xlApply(func(c_int) as c_int callback, c_int value) as c_int
    return callback(value)
end
```

生成头文件中的回调是普通 C 函数指针：

```c
typedef int (*XL6_C_Callback_xxx)(int);
int xl_apply(XL6_C_Callback_xxx callback, int value);
```

当前 raw C callback 支持范围：

- 参数：`bool`、所有固定宽度整数、所有 `c_*` 整数、浮点、`point`、`string`。
- 返回：无返回值、`bool`、整数、浮点、`point`。
- `string` 参数在回调期间是只读借用；C 不得在回调返回后继续保存该指针。
- C 回调进入 xlang 后由独立堆对象保存函数指针，并使用 XRT 原子引用计数管理；func 被复制、保存到全局或跨导出 wrapper 生命周期后仍然有效。
- xlang 具名函数和非捕获匿名函数可直接生成 raw C callback 入口，不经过 `var` 或 XCall 装箱。
- 捕获闭包没有单个 C 函数指针能够携带的环境，不能传给只接收 callback 的 C API；此类 API 应显式接收 `callback + userdata`，或改用 XCall。
- 通过变量间接传出捕获闭包时，如果运行期不存在 `c_entry`，边界检查会立即失败，不会把环境指针伪装成函数指针。
- 多返回值、`byref`、`out`、xlang `...` / `***`、C `c_vararg` 和字符串返回不映射为 raw C callback。
- 必选 callback 的 C 参数不得传 `NULL`；可空回调需要在 API 中显式设计为 nullable/handle 语义，不能隐式猜测。

可选参数的默认值属于 xlang 实现，不进入 C 函数指针类型。导出到 C 的回调签名总是完整固定参数表；可选函数可以先适配成某个明确的 `func(...)` 签名，再跨 C 边界。

`__xl_std_runtime_*` 等编译器内部运行时入口仍使用 `XL6_Function` / XCall，不伪装成公开 raw C ABI。

完整双向范例位于 `release/projects/function_interop`。

## 9. 与 DEMO5 的关系

DEMO5 有可参考的部分，但没有形成完整的 xlang -> C 声明方案。

可参考点：

- DEMO5 已支持 `export` 语法，用于 xlang 模块导出。
- DEMO5 生成 `XL_GeneratedModuleExport` 与 `XL_GeneratedModuleDescriptor`。
- DEMO5 为模块生成 `init / exports / descriptor` 三类 ABI 符号。
- `std.module` 可以查询 `key / abi_id / init_symbol / exports_symbol / descriptor_symbol / export_symbol`。
- XLE manifest 中也记录模块导出、ABI id、导出符号等元信息。

不足：

- DEMO5 的 `export` 主要服务 xlang 模块系统，不是面向 C 的头文件/API。
- DEMO5 生成的 xlang 函数实现通常是内部 ABI 名，很多是 `static`，不能直接作为稳定 C API。
- DEMO5 `export_symbol` 更像模块元信息查询，不等价于 C 侧 `extern` 声明。
- DEMO5 没有统一的 `--emit-header` 产物来告诉 C 如何调用 xlang 导出函数。

DEMO6 应保留 DEMO5 “模块 descriptor / export 元信息”的思路，但把 C API 导出设计为明确、简单、可 include 的头文件与 C ABI wrapper。

## 10. 暂定结论

建议 DEMO6 采用：

```xlang
extern "c_symbol" func c_name(...)
extern "c_symbol" func c_name(...) as Type
export "c_symbol" func xl_name(...)
export "c_symbol" func xl_name(...) as Type
```

这是一套对称声明模型：

- `extern`：C 符号进入 xlang。
- `export`：xlang 符号进入 C。

配套命令：

```bat
xl.exe cgen <file.xl> --emit <file.c> --emit-header <file.h>
xl.exe build <file.xl> -shared -o <file.dll> --emit-header <file.h>
```
