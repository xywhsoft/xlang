# P2 header 与 needs 细则

本文细化导出头文件生成和 runtime needs 扫描规则。header 生成决定 C 侧如何看见 xlang 导出的 ABI；needs 扫描决定某段 xlang 程序最终会注入哪些运行时 helper。

## CGEN-HDR-001 头文件外壳

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 使用 `xl --emit-header` 或构建 DLL/混编目标时导出头文件 |
| 生成规则描述 | header 输出 include guard、`stdbool.h/stdint.h/stddef.h`、`xl6_ssize_t` typedef、C++ `extern "C"` 外壳和 `xlInit/xlUnit` 生命周期声明。guardName 为空时使用 `XL6_GENERATED_HEADER_H`。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:723`、`xl6_cgen_header.c:735`、`xl6_cgen_header.c:745` |
| 对应生成的 C 代码 | `#ifndef XL6_GENERATED_HEADER_H` / `extern "C" {` / `void xlInit(void); void xlUnit(void);` |
| 中间形态伪代码 | `emit header prologue; emit lifecycle; emit declarations; emit epilogue` |
| 实现同等功能手写的 C 代码 | 手写头文件同样需要 include guard、C++ 兼容和初始化 API 声明。 |
| 额外依赖的封装代码 | `xl6CgenHeaderLifecycleDecls`。 |

## CGEN-HDR-002 header UTF-8 标识符编码

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func 计算(int 值) int` |
| 生成规则描述 | header 独立实现 UTF-8 codepoint 扫描。C 标识符允许 ASCII 字母、数字和下划线；其他 codepoint 编码为 `_uXXXX_` 或 `_uXXXXXX_`，保证中文标识符能稳定映射到 C 名称。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:9`、`xl6_cgen_header.c:49`、`xl6_cgen_header.c:64` |
| 对应生成的 C 代码 | `int64_t _u8BA1__u7B97_(int64_t _u503C_);` |
| 中间形态伪代码 | `for cp in utf8(name): emit ascii or _uHEX_` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行选择稳定转义命名规则。 |
| 额外依赖的封装代码 | header 内部标识符编码器。 |

## CGEN-HDR-003 header C 字符串符号

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func "c_name"() int` |
| 生成规则描述 | 导出名如果以引号形式保存，header 输出时去掉首尾引号，直接使用内部字符串作为 C 符号；普通文本则原样输出。该规则用于显式指定 C ABI 名称。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:143` |
| 对应生成的 C 代码 | `int64_t c_name(void);` |
| 中间形态伪代码 | `if text is quoted: emit inner text else emit text` |
| 实现同等功能手写的 C 代码 | 手写头文件直接写目标 C 符号名。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-HDR-004 header 类型别名解析

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `typedef MyInt as int` / `export func f(MyInt a)` |
| 生成规则描述 | header 类型映射前会解析 type alias 和 extern type alias。解析最多跟随 32 层，避免循环 alias 造成无限递归；泛型类型节点不在该路径展开。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:163`、`xl6_cgen_header.c:178`、`xl6_cgen_header.c:226` |
| 对应生成的 C 代码 | `int64_t f(int64_t a);` |
| 中间形态伪代码 | `resolveAlias(type) up to 32 hops` |
| 实现同等功能手写的 C 代码 | 手写头文件通常直接写最终 C 类型。 |
| 额外依赖的封装代码 | header alias 查找。 |

## CGEN-HDR-005 header C 类型映射

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func f(c_size n, string s, point p)` |
| 生成规则描述 | header 使用面向 C ABI 的类型映射。`c_*` 类型映射到对应 C 原生类型；xlang `int/int64` 到 `int64_t`，`float/float64` 到 `double`，`string` 到 `const char*`，`point` 到 `void*`，class/union 映射到生成的 `XL_Class_*` / `XL_Union_*`。未知类型输出 `void`。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:244`、`xl6_cgen_header.c:250`、`xl6_cgen_header.c:317`、`xl6_cgen_header.c:344` |
| 对应生成的 C 代码 | `void f(size_t n, const char* s, void* p);` |
| 中间形态伪代码 | `xlang type -> C ABI type string` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工对齐 ABI 类型。 |
| 额外依赖的封装代码 | class/union 类型名编码。 |

## CGEN-HDR-006 header union 声明与依赖

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export union Value ... end` |
| 生成规则描述 | header 输出 union 前，先递归输出字段类型依赖的 class/union。每个 union 只输出一次，字段按声明顺序映射到 C union 字段。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:394`、`xl6_cgen_header.c:427`、`xl6_cgen_header.c:503` |
| 对应生成的 C 代码 | `typedef union XL_Union_Value { int64_t i; const char* s; } XL_Union_Value;` |
| 中间形态伪代码 | `emit deps; typedef union Name { fields... } Name;` |
| 实现同等功能手写的 C 代码 | 手写头文件需要先声明依赖类型，再声明 union。 |
| 额外依赖的封装代码 | emitted union 去重表。 |

## CGEN-HDR-007 header class 声明与继承布局

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export class Child extends Parent ... end` |
| 生成规则描述 | header 输出 class 前先输出父类和字段类型依赖。继承使用 `base` 字段内嵌父类 C struct，字段按声明顺序输出。每个 class 只输出一次。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:562`、`xl6_cgen_header.c:582`、`xl6_cgen_header.c:619` |
| 对应生成的 C 代码 | `typedef struct XL_Class_Child { XL_Class_Parent base; int64_t value; } XL_Class_Child;` |
| 中间形态伪代码 | `emit base; emit field deps; struct { base; fields }` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护同样的继承嵌套布局。 |
| 额外依赖的封装代码 | emitted class 去重表。 |

## CGEN-HDR-008 header inline union 字段

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class 中嵌套 `union ... end` 字段 |
| 生成规则描述 | class 字段如果是 inline union，header 会先输出 inline union 字段依赖，再在 class struct 内直接输出 `union { ... } name;`。匿名 inline union 没有字段名时只输出 union 块。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:363`、`xl6_cgen_header.c:469`、`xl6_cgen_header.c:605` |
| 对应生成的 C 代码 | `union { int64_t a; const char* b; } data;` |
| 中间形态伪代码 | `inline union -> nested C union field` |
| 实现同等功能手写的 C 代码 | 手写 C 直接在 struct 内写匿名或具名 union。 |
| 额外依赖的封装代码 | inline union 依赖扫描。 |

## CGEN-HDR-009 header export 函数原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func pair() (int, string)` |
| 生成规则描述 | header 只暴露 C ABI wrapper 原型：单返回值作为返回类型；无返回值和多返回值输出 `void`；多返回值追加 `out0/out1/...` 指针参数。参数类型使用 header C ABI 映射。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:660`、`xl6_cgen_header.c:667`、`xl6_cgen_header.c:692` |
| 对应生成的 C 代码 | `void pair(int64_t* out0, const char** out1);` |
| 中间形态伪代码 | `export func -> C ABI prototype with out params` |
| 实现同等功能手写的 C 代码 | 手写头文件同样需要把多返回值改成 out 参数。 |
| 额外依赖的封装代码 | 无；对应实现由 export wrapper 生成。 |

## CGEN-HDR-010 header export 变量声明

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export var int counter` / `export const int version = 1` |
| 生成规则描述 | header 对 export 变量输出 `extern` 声明；export const 额外带 `const`。类型使用 header C ABI 映射，符号名使用 header C 字符串符号规则。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:708` |
| 对应生成的 C 代码 | `extern const int64_t version;` |
| 中间形态伪代码 | `export var/const -> extern declaration` |
| 实现同等功能手写的 C 代码 | 手写头文件直接写 `extern` 声明。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-HDR-011 header 输出顺序

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 同时导出 class/union/function/var |
| 生成规则描述 | header 输出顺序为：外壳和 lifecycle；遍历 root 输出 union/class 及其依赖；释放去重表；遍历输出 export 函数原型；再输出 export 变量/常量声明；最后关闭 C++ 外壳和 include guard。 |
| 生成器文件名和行号 | `xl6_cgen_header.c:723`、`xl6_cgen_header.c:761`、`xl6_cgen_header.c:782`、`xl6_cgen_header.c:789` |
| 对应生成的 C 代码 | `types...; export function prototypes...; extern vars...;` |
| 中间形态伪代码 | `emit prologue -> types -> funcs -> vars -> epilogue` |
| 实现同等功能手写的 C 代码 | 手写头文件必须保证类型先于函数原型出现。 |
| 额外依赖的封装代码 | emitted union/class 去重数组。 |

## CGEN-NEEDS-001 module needs 总收集

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 任意程序 |
| 生成规则描述 | `xl6CgenEmitC` 开始时先收集匿名函数、泛型实例、runtime type desc，再计算各类 runtime flag。flag 决定后续是否注入 string/time/typeConvert/functionVar/xvalue/error/async/标准库 wrapper 等运行时代码。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:1`、`xl6_cgen_module.c:35`、`xl6_cgen_module.c:41` |
| 对应生成的 C 代码 | `hasStringRuntime = xl6CgenProgramNeedsStringRuntime(&ctx, root);` |
| 中间形态伪代码 | `collect metadata; scan needs; emit selected runtimes` |
| 实现同等功能手写的 C 代码 | 手写 C 不需要扫描，但必须人工包含需要的 helper。 |
| 额外依赖的封装代码 | needs 扫描函数。 |

## CGEN-NEEDS-002 basic needs 简单语法触发

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `x in range(...)` / `a ** b` / `a ~= b` / time literal / function 类型 / xvalue 容器 |
| 生成规则描述 | `needs_basic` 不依赖 cgen 类型上下文，直接递归 AST。range contains、整数幂、function runtime、近似比较、time literal、xvalue runtime 都在这里按语法或类型名触发。 |
| 生成器文件名和行号 | `xl6_cgen_needs_basic.c:236`、`xl6_cgen_needs_basic.c:255`、`xl6_cgen_needs_basic.c:276`、`xl6_cgen_needs_basic.c:304`、`xl6_cgen_needs_basic.c:322`、`xl6_cgen_needs_basic.c:340` |
| 对应生成的 C 代码 | `if (xl6CgenNodeNeedsIntPow(root)) xl6CgenEmitIntPowHelper(out);` |
| 中间形态伪代码 | `walk AST; if pattern found return true` |
| 实现同等功能手写的 C 代码 | 手写 C 只链接实际使用的函数；编译器自动判断。 |
| 额外依赖的封装代码 | range/int-pow/approx/time/xvalue/function runtime。 |

## CGEN-NEEDS-003 标准库 runtime needs

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `crypto.sha256` / `url.host` / `query.encode` / `regex.find` / `process.exec` / `dir.list` / `path.join` / `http.server.create` |
| 生成规则描述 | 标准库 runtime needs 通过内部函数名触发，例如 `__xl_std_runtime_sha256_hex`、`__xl_std_runtime_url_part`。每个命名空间都有一个 `Is*RuntimeCall` 和一个递归 `NodeNeeds*Runtime`。 |
| 生成器文件名和行号 | `xl6_cgen_needs_basic.c:77`、`xl6_cgen_needs_basic.c:94`、`xl6_cgen_needs_basic.c:112`、`xl6_cgen_needs_basic.c:130`、`xl6_cgen_needs_basic.c:145`、`xl6_cgen_needs_basic.c:218` |
| 对应生成的 C 代码 | `if (hasUrlRuntime) xl6CgenEmitUrlRuntime(out);` |
| 中间形态伪代码 | `if call name in runtime namespace set: need wrapper` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工 include 对应 wrapper。 |
| 额外依赖的封装代码 | 各标准库 wrapper。 |

## CGEN-NEEDS-004 string runtime 上下文扫描

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 字符串 `+`、字符串比较、字符串 `in`、字符串 `+=` |
| 生成规则描述 | string runtime 是否需要依赖类型推断，因此 `needs.c` 会在扫描声明、函数、create、class/custom 时临时注册变量类型、`self` 和参数类型。泛型函数如果还没有具体泛型上下文则跳过，具体实例会在 program 级扫描中补扫。 |
| 生成器文件名和行号 | `xl6_cgen_needs.c:1`、`xl6_cgen_needs.c:16`、`xl6_cgen_needs.c:173` |
| 对应生成的 C 代码 | `if (xl6CgenBinaryPlusIsString(ctx,node)) return 1;` |
| 中间形态伪代码 | `scan with type environment; string op -> need string runtime` |
| 实现同等功能手写的 C 代码 | 手写 C 直接调用字符串 helper 或普通 C 表达式。 |
| 额外依赖的封装代码 | string runtime。 |

## CGEN-NEEDS-005 type convert runtime 上下文扫描

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `int("123")` / `string(123)` / `a.toString()` |
| 生成规则描述 | 类型转换 runtime 在出现 string 到 int/float/time，或 int/float/time/point/var 到 string，或隐式 `toString`，或 `__xl_std_runtime_take_cstr` 时触发。扫描同样维护临时变量类型、循环目标类型、self 和泛型上下文。 |
| 生成器文件名和行号 | `xl6_cgen_needs.c:203`、`xl6_cgen_needs.c:241`、`xl6_cgen_needs.c:392` |
| 对应生成的 C 代码 | `if (xl6CgenTypeConvertCallNeedsRuntime(ctx,node)) return 1;` |
| 中间形态伪代码 | `if conversion cannot be plain C cast -> need type convert runtime` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用 parse/format helper。 |
| 额外依赖的封装代码 | type convert runtime。 |

## CGEN-NEEDS-006 function-var runtime 上下文扫描

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function f = foo; f(1)` / `var v = foo; v(1)` / `obj.fn()` |
| 生成规则描述 | function-var runtime 在函数类型变量调用、var 变量调用、成员函数值调用、typed container item 包含 function 类型、var 初始化为 function 值时触发。扫描时会临时维护变量类型和成员类型推断。 |
| 生成器文件名和行号 | `xl6_cgen_needs.c:422`、`xl6_cgen_needs.c:594` |
| 对应生成的 C 代码 | `if (xl6CgenIsFunctionType(functionType) || type == var) return 1;` |
| 中间形态伪代码 | `if call target is function value or var callable -> need function-var runtime` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用 function invoke/xcall helper。 |
| 额外依赖的封装代码 | function-var runtime；可能级联 xvalue runtime。 |

## CGEN-NEEDS-007 泛型实例 needs 补扫

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `foo::<string>()` 内部使用字符串运行时 |
| 生成规则描述 | program 级 needs 扫描先扫 root，再遍历 `ctx->genericInstanceCount`，设置当前泛型参数/实参后对每个具体泛型函数补扫。这样泛型定义本身不会误触发 runtime，但已实例化泛型内部使用的运行时不会漏掉。 |
| 生成器文件名和行号 | `xl6_cgen_needs.c:173`、`xl6_cgen_needs.c:392`、`xl6_cgen_needs.c:594` |
| 对应生成的 C 代码 | `ctx->currentGenericParams = xl6CgenFuncTypeParams(instance.func); ctx->currentGenericArgs = instance.typeArgs;` |
| 中间形态伪代码 | `scan root; for each generic instance scan with concrete args` |
| 实现同等功能手写的 C 代码 | 手写 C 只编译具体实例；编译器需要补扫实例体。 |
| 额外依赖的封装代码 | 泛型实例表。 |

## CGEN-NEEDS-008 runtime 级联依赖

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `async`、`dir.list`、`string.split`、`http.server.create`、有 main 函数 |
| 生成规则描述 | 某些 runtime 触发后会强制打开其他 runtime：async 需要 error runtime；xvalue runtime 在 var/容器/toString/runtime type desc/main 时打开；std string 和 dir wrapper 强制 xvalue；http server 强制 function-var 和 xvalue。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:47`、`xl6_cgen_module.c:48`、`xl6_cgen_module.c:57`、`xl6_cgen_module.c:60` |
| 对应生成的 C 代码 | `hasErrorRuntime = xl6CgenNodeNeedsErrorRuntime(root) || hasAsyncRuntime;` |
| 中间形态伪代码 | `needs flags -> cascade flags -> emit runtimes in dependency order` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己确保底层 helper 先于依赖 helper 出现。 |
| 额外依赖的封装代码 | module runtime 注入顺序。 |
