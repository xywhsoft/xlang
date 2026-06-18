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
extern "xrtTimer" func timer() -> float64
extern "errno" var int errno
extern type HANDLE = point
```

含义：

- 字符串是 C 侧真实符号名。
- xlang 名称是语言内使用的名称。
- 类型签名决定调用时的 C ABI 转换。

## 3. xlang 声明到 C

xlang -> C 使用同一套 `export` 语法族，建议增加显式 C 符号名写法。

```xlang
export "app_add" func add(int left, int right) -> int
	return left + right
end

export "app_counter" var int counter = 0

export "app_version" const int version = 1
```

含义：

- `export "app_add"` 表示生成一个 C ABI 可见符号 `app_add`。
- xlang 内部名称仍然是 `add`。
- C 侧通过生成的头文件声明并调用。

建议规则：

- `export func name(...)`：只导出为 xlang 模块符号。
- `export "c_symbol" func name(...)`：导出为 xlang 模块符号，同时生成 C ABI wrapper。
- `export "c_symbol" var ...`：导出 C 可访问变量符号。
- `export "c_symbol" const ...`：导出 C 可访问常量符号。

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

## 6. custom C API 映射

`custom` 是 C API 进入 xlang 的 OOP 声明机制。

当前方案已确认：DEMO6 的 `custom` 默认采用 native opaque handle 方案，不沿用 DEMO5 中“custom 强制进入动态对象 + payload”的实现路径。

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
custom xxx
	extern "xxx_create" create()
	extern "xxx_destroy" destroy()
	extern "xxx_add" func add(int param) -> int
end
```

调用：

```xlang
auto a = xxx()
a.add(1)
```

期望生成方向：

```c
XL_Custom_xxx a;
a.__handle = xxx_create();
xxx_add(a.__handle, 1);
xxx_destroy(a.__handle);
```

规则：

- custom 实例内部保存一个黑盒 C 指针。
- custom 成员函数的第一个 C 参数固定为黑盒指针 `self`。
- xlang 调用 custom 方法时，编译器直接把内部句柄作为第一个参数传给 C 函数。
- custom 方法调用不能触发动态查找、反射、装箱或额外分配。
- custom 默认不绑定动态对象字典，不生成动态对象 header，不进入动态对象 method table。
- 后续如果支持 `xxx* self` 这类 typed pointer receiver，必须作为显式增强能力，不影响第一阶段统一黑盒指针规则。
- 如果 custom 声明了 `extern create`，`Type(...)` 调用对应 C 构造函数并保存返回句柄。
- 如果 custom 没有声明 `extern create`，`Type(point_value)` 表示直接绑定已有 C 指针，不调用任何构造函数。
- 如果 custom 声明了 `extern destroy`，编译器在变量生命周期结束处对非空句柄插入清理调用。
- 如果 custom 没有声明 `extern destroy`，xlang 不释放句柄，也不猜测释放方式。
- custom 的默认初始化值是空句柄；普通方法调用不自动做空句柄检查，而是直接把句柄传给 C 函数。
- 第一阶段不引入手写 `delete` 语句，也不把普通关闭方法自动识别为生命周期结束。

## 7. 类型范围

第一阶段只允许 C ABI 能直接表达的类型。
完整 ABI 规则见 `ABI设计.md`。

建议首批支持：

- `bool` -> C `bool`
- `int` / `int8` / `uint8` / `int16` / `uint16` / `int32` / `uint32` / `int64` / `uint64`
- `float` / `float32` / `float64`
- `string` 参数与 C UTF-8 只读字符串指针互转，xlang 导出到 C 时第一阶段不直接返回 `string`
- `point` -> C 黑盒指针

第一阶段建议不直接导出：

- `var`
- `array/list/set/dict`
- `class/custom`
- `error`
- `future/task/channel/generator`
- 多返回值函数
- 重载函数的同名 C 导出

处理规则：

- 不支持的类型在 `check` 或 `cgen` 阶段直接报错。
- 不做自动回退到 `xvalue` 的兼容方案。
- 如果确实要跨 C 暴露复杂对象，应显式设计为 `point` 句柄或 XRT 类型。

## 8. 重载与符号名

C 没有 xlang 风格的重载，因此 C ABI export 必须有唯一 C 符号。

建议规则：

```xlang
export "math_add_int" func add(int a, int b) -> int
	return a + b
end

export "math_add_float" func add(float a, float b) -> float
	return a + b
end
```

如果多个 `export "same_symbol"` 指向不同函数，编译时报错。

如果重载函数使用 `export func add(...)` 但没有 C 符号，只影响 xlang 模块导出，不生成 C 头文件声明。

重载解析的完整规则见 `函数重载设计.md`。

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
extern "c_symbol" func c_name(...) -> Type
export "c_symbol" func xl_name(...)
export "c_symbol" func xl_name(...) -> Type
```

这是一套对称声明模型：

- `extern`：C 符号进入 xlang。
- `export`：xlang 符号进入 C。

配套命令：

```bat
xl.exe cgen <file.xl> --emit <file.c> --emit-header <file.h>
xl.exe build <file.xl> -shared -o <file.dll> --emit-header <file.h>
```
