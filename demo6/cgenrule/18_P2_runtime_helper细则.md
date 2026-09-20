# P2 runtime helper 细则

本文细化 `xl6_cgen_runtime.c`、`xl6_cgen_lifecycle.c`、`xl6_cgen_main.c`、`xl6_cgen_main_wrapper.c`、`xl6_cgen_xvalue_runtime.c` 和 `xl_runtime.h`。这些规则多数不是单一语法节点本体，而是为了表达式、动态值、脚本入口和生命周期提供生成期或共享运行时 helper。生成期 emitter 是 `static`，通过 `xl6_cgen.c` 的分片 include 顺序进入同一个翻译单元；跨生成单元共享的 helper 统一由运行时头提供。

## CGEN-RTH-001 `~=` 近似比较原型注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `a ~= b` |
| 生成规则描述 | 当模块需要近似比较时，输出 `xrtIntApprox`、`xrtNumApprox`、`xrtTimeApprox` 原型。字符串近似原型由字符串 runtime 输出。该规则只声明 XRT 能力，不复制近似算法。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:3` |
| 对应生成的 C 代码 | `bool xrtIntApprox(long long a, long long b);`、`bool xrtNumApprox(double a, double b);`、`bool xrtTimeApprox(long long a, long long b);` |
| 中间形态伪代码 | `if needsApprox: emitApproxProtos()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行声明并调用 XRT 近似比较函数。 |
| 额外依赖的封装代码 | 无额外算法封装；依赖 XRT。 |

## CGEN-RTH-002 time 字面量和时间转换原型注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `#2026-06-18#`、`#12:30:00#`、`time("2026-06-18")` |
| 生成规则描述 | 当模块需要时间 runtime 时，输出 `xrtTimeSerial`、`xrtDateSerial`、`xrtDateTimeSerial`、`xrtStrToTime` 原型。时间解析和序列化能力保持在 XRT。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:11` |
| 对应生成的 C 代码 | `long long xrtDateTimeSerial(...);`、`long long xrtStrToTime(const char* sTime, size_t iSize);` |
| 中间形态伪代码 | `if needsTime: emitTimeProtos()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要直接调用 XRT 时间函数构造 `long long` 时间值。 |
| 额外依赖的封装代码 | 无；只注入 XRT 原型。 |

## CGEN-RTH-003 字符串基础 runtime 原型注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | 字符串比较、查找、近似比较、拼接 |
| 生成规则描述 | 字符串 runtime 输出 `xrtMalloc`、`xrtStrComp`、`xrtFindStr`、`xrtStrApprox` 原型，并包含 `stdbool.h`、`stddef.h`、`string.h`。这些原型服务于字符串表达式、动态字符串比较和 `in`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:20` |
| 对应生成的 C 代码 | `int xrtStrComp(...)`、`const char* xrtFindStr(...)`、`bool xrtStrApprox(...)` |
| 中间形态伪代码 | `if needsString: emitStringRuntimeProtos()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要包含字符串相关 XRT 声明。 |
| 额外依赖的封装代码 | 字符串拼接 helper 见下一条。 |

## CGEN-RTH-004 字符串 `+` 拼接 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"a" + "b"` |
| 生成规则描述 | 当静态字符串拼接需要运行时支持时，生成 `xl6_xrt_str_concat`。它用 `strlen` 计算两边长度，用 `xrtMalloc` 分配新缓冲区，任一边为空时按空串处理，分配失败返回空串。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:30` |
| 对应生成的 C 代码 | `static const char* xl6_xrt_str_concat(const char* left, const char* right) { ... }` |
| 中间形态伪代码 | `len = len(left)+len(right); text = xrtMalloc(len+1); copy; return text` |
| 实现同等功能手写的 C 代码 | 手写 C 通常要写一次 `malloc + memcpy + 0` 的拼接逻辑。 |
| 额外依赖的封装代码 | `xl6_xrt_str_concat`。 |

## CGEN-RTH-005 整数幂 `**` helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `a ** b`，且两边为整数路径 |
| 生成规则描述 | 当模块需要整数幂时，生成 `xl6_int_pow_i64`。它使用快速幂循环，负指数返回 0，正指数通过位移逐步平方。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:54` |
| 对应生成的 C 代码 | `static long long xl6_int_pow_i64(long long base, long long exp) { ... }` |
| 中间形态伪代码 | `if exp < 0 return 0; while exp>0: if odd result*=base; exp>>=1; base*=base` |
| 实现同等功能手写的 C 代码 | 手写 C 可以调用 `pow` 后转整数，但会引入浮点误差；该 helper 保持整数路径。 |
| 额外依赖的封装代码 | `xl6_int_pow_i64`。 |

## CGEN-RTH-006 range `in` 包含判断 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `x in range(start, stop, step)` |
| 生成规则描述 | 当模块需要 range contains 时，生成 `xl6_range_contains_i64`。正步长使用 `[start, stop)`，负步长使用 `(stop, start]`，并通过取模判断是否落在步长上；步长为 0 返回 false。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_runtime.c:75` |
| 对应生成的 C 代码 | `static int xl6_range_contains_i64(long long value, long long start, long long stop, long long step) { ... }` |
| 中间形态伪代码 | `if step==0 false; check bounds; return distance % abs(step) == 0` |
| 实现同等功能手写的 C 代码 | 手写 C 需要手动处理正负步长边界。 |
| 额外依赖的封装代码 | `xl6_range_contains_i64`。 |

## CGEN-RTH-007 导出头文件生命周期声明

| 项目 | 内容 |
|---|---|
| xlang 写法 | 编译 DLL、导出库或生成给 C 使用的头文件 |
| 生成规则描述 | 头文件生成时输出 `XL_InitOptions`、`xlInitOptions`、`xlInit`、`xlUnit`、`xlDllAttach`、`xlDllDetach` 声明，并用 `XL6_INIT_OPTIONS_DEFINED` 防止重复定义。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_lifecycle.c:3` |
| 对应生成的 C 代码 | `typedef struct XL_InitOptions { ... } XL_InitOptions;` 和生命周期函数声明。 |
| 中间形态伪代码 | `emitLifecycleDeclsForHeader()` |
| 实现同等功能手写的 C 代码 | 手写 C 头文件需要公开初始化选项和生命周期 API。 |
| 额外依赖的封装代码 | 无；这是导出声明。 |

## CGEN-RTH-008 生成文件内嵌生命周期结构和 XRT 生命周期开关

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意生成 C 文件 |
| 生成规则描述 | 生成 C 文件中无条件输出 `XL_InitOptions` 定义，并在未定义 `XL6_NO_XRT_LIFECYCLE` 时默认启用 `XL6_USE_XRT_LIFECYCLE`。若未使用 XRT 单头模式，则声明 `xrtInit` 和 `xrtUnit`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_lifecycle.c:25` |
| 对应生成的 C 代码 | `#define XL6_USE_XRT_LIFECYCLE 1`、`void* xrtInit();`、`void xrtUnit();` |
| 中间形态伪代码 | `emitInitOptions(); if not noXrtLifecycle: enableXrtLifecycle()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行决定是否接管 XRT 初始化。 |
| 额外依赖的封装代码 | 生命周期 runtime。 |

## CGEN-RTH-009 `xlInitOptions` 与 `xlInit` 初始化规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | C main 调用 `xlInit(&options)`，或生成的 main 自动调用 |
| 生成规则描述 | `xlInitOptions` 填充结构体大小、版本、argc/argv 和保留字段。`xlInit` 校验 size/version，维护 `xl__runtime_init_count` 引用计数，并在首次初始化时调用 `xrtInit`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_lifecycle.c:52` |
| 对应生成的 C 代码 | `XL_InitOptions xlInitOptions(...)`、`int xlInit(const XL_InitOptions* options)` |
| 中间形态伪代码 | `if options invalid return 0; if init_count==0: xrtInit(); init_count++; return 1` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在程序入口显式做版本校验和初始化计数。 |
| 额外依赖的封装代码 | `xlInitOptions`、`xlInit`。 |

## CGEN-RTH-010 `xlUnit` 反初始化规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | C main 结束前调用 `xlUnit()`，或生成的 main 自动调用 |
| 生成规则描述 | `xlUnit` 在计数小于等于 0 时直接返回；否则递减引用计数，并在归零时调用 `xrtUnit`。这允许嵌入场景重复进入和退出 xlang runtime。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_lifecycle.c:82` |
| 对应生成的 C 代码 | `void xlUnit(void) { if (count<=0) return; count--; if(count==0) xrtUnit(); }` |
| 中间形态伪代码 | `if init_count <= 0 return; init_count--; if init_count == 0: xrtUnit()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要保证每次初始化都有匹配释放，且不重复卸载 XRT。 |
| 额外依赖的封装代码 | `xlUnit`。 |

## CGEN-RTH-011 DLL attach/detach 跨平台入口

| 项目 | 内容 |
|---|---|
| xlang 写法 | 编译共享库或 DLL 目标 |
| 生成规则描述 | 共享库模式下，Windows 生成 `DllMain`，进程 attach/detach 时调用 `xlDllAttach` / `xlDllDetach`；非 Windows 平台生成 constructor/destructor 属性函数。默认 attach/detach 只是占位，方便后续扩展库生命周期挂钩。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_lifecycle.c:94` |
| 对应生成的 C 代码 | `int __stdcall DllMain(...)` 或 `__attribute__((constructor/destructor))` 函数。 |
| 中间形态伪代码 | `if XL6_BUILD_SHARED: emit platform shared-library entry` |
| 实现同等功能手写的 C 代码 | 手写跨平台 DLL 入口需要分别处理 Windows 与 ELF/Mach-O 构造析构机制。 |
| 额外依赖的封装代码 | `xlDllAttach`、`xlDllDetach`、平台入口函数。 |

## CGEN-RTH-012 argv 到 xlang `arg` 转换 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) ... end` |
| 生成规则描述 | 生成 xlang main wrapper 前，输出 `xl6_argv_to_arg`。它创建 xvalue array，把每个 `argv[i]` 包装成文本 xvalue 并追加；任何一步失败都会释放已创建对象并返回 0。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_main_globals.c:1` |
| 对应生成的 C 代码 | `static xvalue xl6_argv_to_arg(int argc, char** argv) { ... }` |
| 中间形态伪代码 | `arg = xvoCreateArray(); for argv: append xvoCreateText(argv[i]); on fail cleanup` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己构造动态数组并处理引用释放。 |
| 额外依赖的封装代码 | `xl6_argv_to_arg`。 |

## CGEN-RTH-013 xlang main 返回值判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) int ... end` 或无返回 main |
| 生成规则描述 | 通过 main 函数 AST 的返回列表判定是否有一个返回值。当前生成器只区分“有一个返回值”和“无返回值”，有返回值路径会把结果作为进程退出码。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_main_globals.c:25` |
| 对应生成的 C 代码 | 不直接生成 C；影响 main wrapper 分支。 |
| 中间形态伪代码 | `returnsInt = returns != null && returns.childCount == 1` |
| 实现同等功能手写的 C 代码 | 手写 C main 直接决定是否返回函数结果。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-RTH-014 xlang main wrapper 初始化与参数准备

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) ... end` |
| 生成规则描述 | 生成 `int main(int argc, char** argv)`，先创建 `XL_InitOptions` 并调用 `xlInit`；失败返回 1。然后调用 `xl6_argv_to_arg` 构造 `__xl6_arg`，失败时调用 `xlUnit` 并返回 1。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_main_wrapper.c:1-17` |
| 对应生成的 C 代码 | `XL_InitOptions __xl6_options = xlInitOptions(argc, argv); if(!xlInit(...)) return 1; __xl6_arg = xl6_argv_to_arg(...);` |
| 中间形态伪代码 | `init runtime; arg = argvToArg(); if fail unit and return 1` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在 main 开头显式初始化 runtime 和参数数组。 |
| 额外依赖的封装代码 | C main wrapper、argv helper。 |

## CGEN-RTH-015 main 有返回值时的退出码与错误处理

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) int ... end` |
| 生成规则描述 | wrapper 调用固定入口 `xlang_main(__xl6_arg)`，保存为 `__xl6_code`。如果错误 runtime 启用且存在未清错误，则先输出未捕获错误诊断，再清理错误、释放 `__xl6_arg`、调用 `xlUnit`、返回 1；否则释放 `__xl6_arg`、调用 `xlUnit` 并返回 `__xl6_code`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_main_wrapper.c:18-35` |
| 对应生成的 C 代码 | `__xl6_code = xlang_main(__xl6_arg); if (xl6_error_has()) { xl6_error_print_uncaught(); xl6_error_clear(); xvoUnref(__xl6_arg); xlUnit(); return 1; } xvoUnref(__xl6_arg); xlUnit(); return __xl6_code;` |
| 中间形态伪代码 | `code = main(arg); if error: printUncaught(); clear/unit/return 1; unit; return code` |
| 实现同等功能手写的 C 代码 | 手写 C main 需要手动把语言 main 返回值映射为进程退出码。 |
| 额外依赖的封装代码 | 错误 runtime 可选依赖，包含 `xl6_error_print_uncaught`。 |

## CGEN-RTH-016 main 无返回值时的默认退出码

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) ... end` 无返回类型 |
| 生成规则描述 | `main` 固定暴露为 `int xlang_main(xvalue arg)`。当 xlang main 未显式返回时，函数体清理局部资源后补 `return 0;`，wrapper 仍按统一 int 入口调用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_decl_proto.c:10`、`demo6/src/cgen/xl6_cgen_func.c:36`、`demo6/src/cgen/xl6_cgen_main_wrapper.c:18` |
| 对应生成的 C 代码 | `int xlang_main(xvalue arg) { ... return 0; }` 与 `__xl6_code = xlang_main(__xl6_arg); ... return __xl6_code;` |
| 中间形态伪代码 | `code = xlang_main(arg); if error: printUncaught(); clear/releaseArg/unit/return 1; releaseArg; unit; return code` |
| 实现同等功能手写的 C 代码 | 手写 C main 需要自己约定无返回语言入口的退出码。 |
| 额外依赖的封装代码 | 错误 runtime 可选依赖，包含 `xl6_error_print_uncaught`。 |

## CGEN-RTH-017 类型转换 runtime 原型注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `int(x)`、`float(x)`、`string(x)`、`time(x)`、动态值转字符串 |
| 生成规则描述 | 类型转换 runtime 输出当前 XRT 数值写入、编码和动态值字符串转换入口，并保护性定义 `xvalue`、`xstrview` 与 Base64 配置类型。它还输出 `xl6_xrt_value_to_i64/f64/bool/time/point_release` helper，供拥有型 `xvalue*` 到静态标量的转换使用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_xvalue_runtime.c:1`、`demo6/src/cgen/xl6_cgen_xvalue_runtime.c:54`、`demo6/src/cgen/xl6_cgen_xvalue_runtime.c:68`、`demo6/src/cgen/xl6_cgen_xvalue_runtime.c:437`、`demo6/release/include/xl_runtime.h:12` |
| 对应生成的 C 代码 | `bool xrtIntWrite(...)`、`bool xrtNumWrite(...)`、`void* xrtHexEncodeNew(...)`、`static long long xl6_xrt_value_to_i64_release(xvalue* value)` 等。 |
| 中间形态伪代码 | `emitConvertProtos(); emit xvalue typedef if missing; emit guarded releaseConvertHelpers()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行声明所有转换入口并处理 xvalue 生命周期。 |
| 额外依赖的封装代码 | release conversion helper 使用 `XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS` 宏守住重复输出；类型转换 helper 见后续条目。 |

## CGEN-RTH-018 临时 C 字符串复制与接管释放 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `string(x)`、`x.toString()` 中需要接管 XRT 返回字符串 |
| 生成规则描述 | `xl6_xrt_temp_cstr` 使用 `xrtTemp` 复制普通 C 字符串，`xl6_xrt_take_cstr` 先复制为临时字符串，再用 `xrtFree` 释放原始 XRT 返回内存。空指针或分配失败返回空串。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_xvalue_runtime.c:235`、`demo6/src/cgen/xl6_cgen_xvalue_runtime.c:251` |
| 对应生成的 C 代码 | `static const char* xl6_xrt_temp_cstr(...)`、`const char* xl6_xrt_take_cstr(void* value)` |
| 中间形态伪代码 | `tmp = xrtTemp(strlen+1); copy; if take: xrtFree(value); return tmp` |
| 实现同等功能手写的 C 代码 | 手写 C 需要明确谁释放 XRT 返回的堆字符串。 |
| 额外依赖的封装代码 | `xl6_xrt_temp_cstr`、`xl6_xrt_take_cstr`。 |

## CGEN-RTH-019 基本数值和 xvalue 转字符串 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `string(123)`、`string(uint64Value)`、`string(1.25)`、`string(varValue)` |
| 生成规则描述 | 有符号整数、无符号整数和浮点分别通过 `xrtIntWrite`、`xrtUIntWrite`、`xrtNumWrite` 写入栈缓冲区。动态值转字符串调用统一的 `xl6_xrt_value_string_owned`，再接管返回文本。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_xvalue_runtime.c:376-434` |
| 对应生成的 C 代码 | `xl6_xrt_i64_to_string`、`xl6_xrt_u64_to_string`、`xl6_xrt_f64_to_string`、`xl6_xrt_value_to_string` |
| 中间形态伪代码 | `buffer = xrtToStr(value); return tempCopy(buffer)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己选择缓冲区、格式化和释放策略。 |
| 额外依赖的封装代码 | 有符号、无符号、浮点和 xvalue 字符串转换 helper。 |

## CGEN-RTH-020 xvalue 转字符串并释放输入

| 项目 | 内容 |
|---|---|
| xlang 写法 | 临时 `var` 表达式参与 `string(...)` 或拼接后需要释放 |
| 生成规则描述 | `xl6_xrt_value_to_string_release` 先把 `xvalue*` 转为临时 C 字符串，再对输入值调用 `xrtValueRelease`。该 helper 用于表达式生成器处理临时动态值所有权。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_xvalue_runtime.c:427` |
| 对应生成的 C 代码 | `static const char* xl6_xrt_value_to_string_release(xvalue* value) { ... xrtValueRelease(value); ... }` |
| 中间形态伪代码 | `text = valueToString(value); unref(value); return text` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在转换后手动释放临时 xvalue。 |
| 额外依赖的封装代码 | `xl6_xrt_value_to_string_release`。 |

## CGEN-RTH-021 xvalue runtime 类型、调用帧和 XRT 原型注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var`、动态容器、func 值、typed container、class/native-backed class 装箱 |
| 生成规则描述 | 共享运行时头声明 `xvalue`、typed container 句柄、`xl6_type_desc`、`xl6_func_sig`、`xl6_call_frame`、`xl6_call_result` 等结构，以及当前 XRT/value 操作入口。动态 list/dict `for-in` 通过 `xrtValueIterCreate/Next/Destroy` 构建快照，不再依赖旧 AVL 遍历器。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:12`、`demo6/release/include/xl_runtime.h:48`、`demo6/release/include/xl_runtime.h:121`、`demo6/release/include/xl_runtime.h:163`、`demo6/release/include/xl_runtime.h:198`、`demo6/release/include/xl_runtime.h:208`、`demo6/release/include/xl_runtime.h:6517`、`demo6/release/include/xl_runtime.h:6572` |
| 对应生成的 C 代码 | `typedef struct xvalue xvalue;`、`struct xl6_type_desc { ... };`、`struct xl6_call_frame { ... };`、`typedef struct XL6_XrtIterSnapshot { ... } XL6_XrtIterSnapshot;` |
| 中间形态伪代码 | `emit xvalue typedefs; emit type desc structs; emit callable frame structs; emit XRT/xvo prototypes; emit dynamic iteration snapshot helpers` |
| 实现同等功能手写的 C 代码 | 手写 C 需要包含或声明 xvalue 与 XRT 类型系统接口。 |
| 额外依赖的封装代码 | 类型/调用帧结构声明、动态遍历 snapshot helper；具体实现依赖 XRT。 |

## CGEN-RTH-022 xvalue 类型码与 XRT 类型 kind 常量

| 项目 | 内容 |
|---|---|
| xlang 写法 | 动态类型分派、`type(expr)`、动态容器判断 |
| 生成规则描述 | xvalue runtime 输出 `XL6_XVO_DT_*` 类型码和 `XL6_XRT_TYPE_KIND_*` 常量。生成的动态比较、长度、包含、类型 ID 等 helper 都依赖这些常量进行分派。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:87`、`demo6/release/include/xl_runtime.h:243` |
| 对应生成的 C 代码 | `#define XL6_XVO_DT_INT 2`、`#define XL6_XRT_TYPE_KIND_RECORD 13u` 等。 |
| 中间形态伪代码 | `emit dynamic type code constants` |
| 实现同等功能手写的 C 代码 | 手写 C 通常需要包含统一类型码定义。 |
| 额外依赖的封装代码 | 常量定义。 |

## CGEN-RTH-023 runtime 类型名 ID 与动态 `type(value)` 规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `type(v)`，其中 `v` 是 `var`、function、class、native-backed class 或容器 |
| 生成规则描述 | `xl6_xrt_type_name_id` 使用固定 FNV-1a 风格哈希和 `type:` 前缀生成类型名 ID。`xl6_xrt_type_value_id` 根据当前 value kind 返回内置类型 ID；function 优先使用 callable 签名 ID；容器/class/native-backed class 优先读取运行时类型描述名称，否则退回 `var`。可选 `release` 会在结束时释放输入 `xvalue*`。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:4584`、`demo6/release/include/xl_runtime.h:4645` |
| 对应生成的 C 代码 | `static inline uint64 xl6_xrt_type_name_id(...)`、`static inline uint64 xl6_xrt_type_value_id(...)` |
| 中间形态伪代码 | `switch valueKind(value): builtin -> builtinId; func -> sigId; runtimeDesc -> descNameId; default -> varId` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己约定类型 ID 和 func 签名 ID 映射。 |
| 额外依赖的封装代码 | 类型 ID helper。 |

## CGEN-RTH-024 xvalue 绑定类型描述与引用保留 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | class/native-backed class/typed container 装箱到 `var`，或动态值跨表达式保留 |
| 生成规则描述 | `xl6_xrt_bind_type` 通过语言运行时桥给 `xvalue*` 绑定类型描述；`xl6_xrt_retain` 对非空值调用 `xrtValueRetain` 并返回自身。前者用于保持 `var` 内部的真实类型元数据，后者用于表达式临时值转移。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:4637`、`demo6/release/include/xl_runtime.h:4168` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_bind_type(...)`、`static inline xvalue* xl6_xrt_retain(...)` |
| 中间形态伪代码 | `if value && desc: setTypeDesc(value, desc); if value: addRef(value)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要手动维护动态值的类型描述和引用计数。 |
| 额外依赖的封装代码 | `xl6_xrt_bind_type`、`xl6_xrt_retain`。 |

## CGEN-RTH-025 typed item 装箱为空值兜底

| 项目 | 内容 |
|---|---|
| xlang 写法 | `array<T>`、`list<T>`、`set<T>`、`dict<T>` 转为动态容器 |
| 生成规则描述 | `xl6_xrt_typed_box_or_null` 对空 item 返回 null value；非空 item 调用 `xl6TypeBoxValue`，若装箱失败也返回 null。它是 typed container 转动态容器和复制时的共同兜底。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5980` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_typed_box_or_null(const xl6_type_desc* type, const void* item)` |
| 中间形态伪代码 | `if item == null return xrtValueNull(); boxed = xl6TypeBoxValue(type,item); return boxed ? boxed : null` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在每次取 typed item 时做空值和装箱失败处理。 |
| 额外依赖的封装代码 | `xl6_xrt_typed_box_or_null`。 |

## CGEN-RTH-026 typed container 转动态容器 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var v = array<int>[1, 2]` 或 typed container 作为 `var` 返回 |
| 生成规则描述 | 分别生成 typed array/list/set/dict 到 xvalue array/list/set/table 的转换 helper。每个 helper 创建对应动态容器，逐项读取 typed item，调用装箱 helper，再写入动态容器。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:6218`、`demo6/release/include/xl_runtime.h:6248`、`demo6/release/include/xl_runtime.h:6300`、`demo6/release/include/xl_runtime.h:6350` |
| 对应生成的 C 代码 | `xl6_xrt_typed_array_to_xvalue`、`xl6_xrt_typed_list_to_xvalue`、`xl6_xrt_typed_set_to_xvalue`、`xl6_xrt_typed_dict_to_xvalue` |
| 中间形态伪代码 | `result = create dynamic container; for each typed item: boxed = box(item); set/append boxed` |
| 实现同等功能手写的 C 代码 | 手写 C 需要分别遍历四种 typed container 并装箱。 |
| 额外依赖的封装代码 | 四种 typed container 转换 helper。 |

## CGEN-RTH-027 typed container 深复制 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | typed container 赋值、返回、临时值需要复制 |
| 生成规则描述 | 分别生成 typed array/list/set/dict copy helper。每个 helper 创建同 item type 的 XRT typed container，遍历原容器并把装箱后的 xvalue 写入新容器，写入后释放临时 boxed value。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:6233`、`demo6/release/include/xl_runtime.h:6284`、`demo6/release/include/xl_runtime.h:6335`、`demo6/release/include/xl_runtime.h:6367` |
| 对应生成的 C 代码 | `xl6_xrt_typed_array_copy`、`xl6_xrt_typed_list_copy`、`xl6_xrt_typed_set_copy`、`xl6_xrt_typed_dict_copy` |
| 中间形态伪代码 | `copy = create typed container(type); for each item: boxed = box(item); typedSet(copy, boxed); unref(boxed)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要写四套容器复制循环。 |
| 额外依赖的封装代码 | 四种 typed container copy helper。 |

## CGEN-RTH-028 动态索引安全读取 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `v[i]`、`v["key"]`，其中 `v` 是 `var` |
| 生成规则描述 | `xl6_xrt_index_safe_get_i64` 对动态 array/int-map 和 typed container 统一读取；`xl6_xrt_index_safe_get_key` 对动态 object 和 typed dict 读取。空值、不支持类型或未命中均返回 null value。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:4912`、`demo6/release/include/xl_runtime.h:4920` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_index_safe_get_i64(...)`、`static inline xvalue* xl6_xrt_index_safe_get_key(...)` |
| 中间形态伪代码 | `if null return nullGet; switch type: array/list/table else nullGet` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先判断 xvalue 类型再调用对应 getter。 |
| 额外依赖的封装代码 | 两个 safe get helper。 |

## CGEN-RTH-029 动态容器长度 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `v.length` 或依赖动态容器长度的 for-in |
| 生成规则描述 | `xl6_xrt_member_safe_length` 返回 value int 或 null；`xl6_xrt_member_length` 返回原生 `uint32`。两者通过当前 value count 与 typed container count 支持 array/list/set/object 和 typed container，空值或不支持类型返回 null/0。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5031`、`demo6/release/include/xl_runtime.h:5015` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_member_safe_length(...)`、`static inline uint32 xl6_xrt_member_length(...)` |
| 中间形态伪代码 | `if typed: typedCount; else if container: xrtValueCount; default null/0` |
| 实现同等功能手写的 C 代码 | 手写 C 需要对每种动态容器分别读取 count。 |
| 额外依赖的封装代码 | 两个 length helper。 |

## CGEN-RTH-030 动态基础比较、相等、近似和排序 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `varA == varB`、`varA ~= varB`、`varA < varB` |
| 生成规则描述 | 动态比较先通过 `xl6_xrt_type_is_numeric` 和 `xl6_xrt_value_to_double` 统一数值路径；`xl6_xrt_basic_compare` 支持 null、数值、字符串、time；`basic_equal` 使用 `xrtValueEqual` 并为 typed handle 保留类型与句柄恒等语义；`basic_approx` 调用当前 UTF-8、数值和时间近似入口；`basic_order` 将比较结果映射到 `< <= > >=`。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5061`、`demo6/release/include/xl_runtime.h:5104`、`demo6/release/include/xl_runtime.h:5127`、`demo6/release/include/xl_runtime.h:5155` |
| 对应生成的 C 代码 | `xl6_xrt_basic_compare`、`xl6_xrt_basic_equal`、`xl6_xrt_basic_approx`、`xl6_xrt_basic_order` |
| 中间形态伪代码 | `if numeric compare numeric; else if string compare xrtStrComp; else if time compare time; approx uses XRT approx` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己维护动态类型比较矩阵。 |
| 额外依赖的封装代码 | 动态比较 helper 组。 |

## CGEN-RTH-031 动态字符串拼接与二元运算 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `varA + varB`、`varA - varB`、`varA * varB`、`varA / varB`、`varA & varB`、位移等动态运算 |
| 生成规则描述 | `xl6_xrt_value_text_concat` 处理两个文本 value 的 `+`，array 的 `+` 由 `xl6_xrt_array_concat` 创建新数组并保留元素。`xl6_xrt_basic_binary` 根据 op 编号分派数值与位运算；不合法类型、除零、浮点位运算和非法移位返回 null。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5191`、`demo6/release/include/xl_runtime.h:5200`、`demo6/release/include/xl_runtime.h:5227` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_value_text_concat(...)`、`static inline xvalue* xl6_xrt_array_concat(...)`、`static inline xvalue* xl6_xrt_basic_binary(...)` |
| 中间形态伪代码 | `if op==add && both text: concat; if not numeric: null; if bit op require int; else numeric arithmetic` |
| 实现同等功能手写的 C 代码 | 手写 C 需要写动态类型检查和运算分派。 |
| 额外依赖的封装代码 | 文本拼接和动态二元运算 helper。 |

## CGEN-RTH-032 动态真假值与一元运算 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `if varValue then`、`not varValue`、`-varValue`、`~varValue` |
| 生成规则描述 | `xl6_xrt_value_truthy` 委托统一的 `xrtValueTruthy` 契约；`xl6_xrt_basic_unary` 支持数值正负号，整数额外支持按位取反，非法类型返回 null。`xl6_xrt_value_truthy_release` 和 `xl6_xrt_basic_unary_release` 在取得结果后释放输入 `xvalue*`。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5273`、`demo6/release/include/xl_runtime.h:5281`、`demo6/release/include/xl_runtime.h:5301`、`demo6/release/include/xl_runtime.h:5308` |
| 对应生成的 C 代码 | `static inline int xl6_xrt_value_truthy(...)`、`static inline xvalue* xl6_xrt_basic_unary(...)`、`static inline int xl6_xrt_value_truthy_release(...)`、`static inline xvalue* xl6_xrt_basic_unary_release(...)` |
| 中间形态伪代码 | `truthy = xrtValueTruthy(value); unary requires numeric; release helper calls base helper then xrtValueRelease(input)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要固定动态真假表和一元运算矩阵。 |
| 额外依赖的封装代码 | truthy、unary 和单操作数 release helper。 |

## CGEN-RTH-033 动态 `in` 包含判断 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `item in values`，其中 `values` 是 `var` |
| 生成规则描述 | `xl6_xrt_value_contains` 根据 values 类型分派：typed container 使用统一 contains 桥；array/list 使用 `xrtValueIterCreate/Next/Destroy`；set 调用 `xrtValueSetHas`；object 仅接受字符串 key；字符串调用 `xrtStrContains`。item 在所有路径结束后释放，拥有型 receiver 由 `xl6_xrt_value_contains_release` 继续释放。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5326`、`demo6/release/include/xl_runtime.h:5369` |
| 对应生成的 C 代码 | `xl6_xrt_value_contains`、`xl6_xrt_value_contains_release`、`xl6_xrt_array_contains`、`xl6_xrt_list_contains`、`xl6_xrt_set_contains` |
| 中间形态伪代码 | `switch valuesKind: typed/array/list/set/object/text; compare item; xrtValueRelease(item); return found` |
| 实现同等功能手写的 C 代码 | 手写 C 需要分别处理五类动态包含语义。 |
| 额外依赖的封装代码 | contains helper 组。 |

## 本轮补充：临时 xvalue 成员 receiver release helper

本次继续压实动态成员访问的临时 receiver 生命周期，不新增 `CGEN-*` 规则编号，只补充 `CGEN-EXPR-011`、`CGEN-EXPR-012` 对应 runtime helper 形态：

- 成员 key 访问统一调用 `xl6_record_field_get_owned(values, key, releaseReceiver)`。`makeObj().name` 传入 `releaseReceiver = 1`，借用 receiver 传入 `0`，不再展开 GNU 语句表达式。
- 安全成员 key 访问沿用同一拥有权参数；`makeObj()?.name` 和 `makeObj()?.nested?.name` 的每一级都按 receiver 是否拥有决定释放。
- `?.length` 的临时 receiver 新增 `xl6_xrt_member_safe_length_release(values)`，内部调用 `xl6_xrt_member_safe_length(values)` 后释放 receiver。
- 普通 `.length` 的临时 owned xvalue receiver 新增 `xl6_xrt_member_length_release(values)`，内部调用 `xl6_xrt_member_length(values)` 后释放 receiver。
- 生成位置为 `demo6/src/cgen/xl6_cgen_value_object_member.c:1`、`demo6/src/cgen/xl6_cgen_value_object_member.c:60`、`demo6/src/cgen/xl6_cgen_value_object_member.c:360`；字段读取 helper 由 `demo6/src/cgen/xl6_cgen_record_field.c:602` 输出，长度释放 helper 位于 `demo6/release/include/xl_runtime.h:5044`、`demo6/release/include/xl_runtime.h:5037`。

守门用例为 `cgen_dynamic_member_access.xl` 和 `cgen_safe_member.xl`。`test.bat` 检查 retain/release helper 形态，并禁止 `({ xvalue __xl6_member_values_` 回流。

## CGEN-RTH-034 动态数组可变参数构造 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `[a, b, c]` 动态数组字面量，或 `...` 可变参数打包 |
| 生成规则描述 | `xl6_xrt_array_varargs` 创建 xvalue array，并从 C varargs 中读取若干 xvalue 逐项追加。追加失败时释放当前 item，最终返回 array；创建失败返回 null。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5843` |
| 对应生成的 C 代码 | `static inline xvalue* xl6_xrt_array_varargs(uint32 count, ...) { ... }` |
| 中间形态伪代码 | `arr = createArray(); for each vararg xvalue: append(colloc=true)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式创建 xvalue array 并逐项 append。 |
| 额外依赖的封装代码 | `xl6_xrt_array_varargs`。 |

## CGEN-RTH-035 动态 list/set/dict 字面量构造 helper

| 项目 | 内容 |
|---|---|
| xlang 写法 | `[1: "a"]`、`set{1, 2}`、`{"a": 1}` 或 `***` 字典参数打包 |
| 生成规则描述 | `xl6_xrt_list_literal` 从 varargs 读取 index/value 成对写入 int-map；`xl6_xrt_set_literal` 读取 value 写入 set；`xl6_xrt_dict_varargs` 读取 key/value 成对写入 object。创建失败返回 null，写入失败释放当前 item 和部分构造结果。 |
| 生成器文件名和行号 | `demo6/release/include/xl_runtime.h:5865`、`demo6/release/include/xl_runtime.h:5888`、`demo6/release/include/xl_runtime.h:5910` |
| 对应生成的 C 代码 | `xl6_xrt_list_literal`、`xl6_xrt_set_literal`、`xl6_xrt_dict_varargs` |
| 中间形态伪代码 | `create container; read varargs as pairs/items; set with colloc=true; cleanup failed item` |
| 实现同等功能手写的 C 代码 | 手写 C 需要分别构造 list/set/table 并处理每个元素所有权。 |
| 额外依赖的封装代码 | list/set/dict literal helper。 |
