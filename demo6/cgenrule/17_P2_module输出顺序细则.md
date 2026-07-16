# P2 module 输出顺序细则

本文继续细化 `xl6_cgen_module.c` 的规则。这个文件不是单一语法节点的生成器，而是 C 输出的总装配器：它决定运行时片段注入顺序、声明顺序、函数体顺序、`main` 包装策略，以及生成失败时的资源释放。

## CGEN-MODULE-001 模块代码生成入口校验与上下文初始化

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意 `.xl` 模块 |
| 生成规则描述 | `xl6CgenEmitC` 接收 AST 根节点、输出文件和诊断对象；根节点或输出文件为空时直接失败。通过 `xl6CgenContextInit` 建立代码生成上下文，并把 `out`、`diags` 注入上下文。该规则是所有 C 代码输出的入口前置条件。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:1` |
| 对应生成的 C 代码 | 不直接生成 C 代码；只初始化生成状态。 |
| 中间形态伪代码 | `if root == null or out == null: return false; ctx = CgenContext(out, diags)` |
| 实现同等功能手写的 C 代码 | 手写 C 没有此步骤；这对应编译器驱动层初始化。 |
| 额外依赖的封装代码 | `xl6CgenContextInit`、`xl6CgenContextFree`。 |

## CGEN-MODULE-002 预收集匿名函数、泛型实例、运行时类型描述与入口信息

| 项目 | 内容 |
|---|---|
| xlang 写法 | 使用匿名函数、泛型函数、隐性类方法、`main` 或顶层语句的模块 |
| 生成规则描述 | 输出任何 C 代码前，先收集匿名函数、泛型实例、运行时类型描述，并查找 xlang `main` 入口以及是否存在顶层运行语句。这样后续 runtime 注入、原型生成、函数体生成都能基于完整信息一次性排序。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:35` |
| 对应生成的 C 代码 | 不直接生成 C；影响后续生成集合。 |
| 中间形态伪代码 | `collectAnonFuncs(root); collectGenericInstancesFixedPoint(root); collectRuntimeTypeDescs(root); mainFunc = findMain(root); hasTopLevel = hasRuntimeStmt(root)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要开发者自行提前声明所有函数和类型；编译器在这里自动完成依赖发现。 |
| 额外依赖的封装代码 | 匿名函数表、泛型实例表、运行时类型描述表。 |

## CGEN-MODULE-003 运行时需求标志聚合与级联

| 项目 | 内容 |
|---|---|
| xlang 写法 | 使用字符串、动态值、时间、异常、异步、标准库、HTTP、目录等能力 |
| 生成规则描述 | 模块级先扫描 AST 得到各类 runtime 需求，再进行级联修正。例如标准字符串和目录能力强制需要 xvalue；HTTP server 强制需要 function-var 和 xvalue；异步、错误、标准库 context 也会单独置位。该规则保证只注入必要片段，同时避免漏注入依赖 runtime。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:41` |
| 对应生成的 C 代码 | 不直接生成 C；决定后续 runtime 片段是否输出。 |
| 中间形态伪代码 | `needs = scan(root); if stdString or dir: needs.xvalue = true; if http: needs.functionVar = true; needs.xvalue = true` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工包含对应头文件和 helper；xlang 自动根据语义需求装配。 |
| 额外依赖的封装代码 | `xl6CgenNodeNeeds*` 系列扫描函数。 |

## CGEN-MODULE-004 C 文件固定序言与生命周期 runtime 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意可生成 C 的模块 |
| 生成规则描述 | 所有生成文件开头固定输出生成标记、基础 C 头文件、`xl6_ssize_t` 类型别名与模块级 `xl6_runtime_fatal(int code)`，然后无条件输出 xlang 生命周期 runtime。生命周期 runtime 是 `xlInit` / `xlUnit` 等入口的基础，必须早于其他生成片段。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:69`、`demo6/src/cgen/xl6_cgen_module.c:75`、`demo6/src/cgen/xl6_cgen_module.c:79` |
| 对应生成的 C 代码 | `#include <stdbool.h>`、`#include <stdint.h>`、`#include <stdlib.h>`、`#include <stddef.h>`、`typedef intptr_t xl6_ssize_t;`、`static void xl6_runtime_fatal(int code) { exit(code); }` 和生命周期 helper。 |
| 中间形态伪代码 | `emitPrologue(); emitRuntimeFatal(); emitLifecycleRuntime();` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行包含基础头和调用 xlang runtime 初始化函数。 |
| 额外依赖的封装代码 | 模块级 fatal helper 与生命周期 helper，不包含 XRT 本体。 |

## CGEN-MODULE-005 基础 runtime 按依赖顺序注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | 使用 function、字符串、类型转换、time、异常、async 等基础能力 |
| 生成规则描述 | 根据模块需求按固定顺序注入基础 runtime：function runtime 先于近似原型，字符串先于类型转换，错误与异步各自独立输出。顺序固定可避免 helper 间前向依赖失控。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:80` |
| 对应生成的 C 代码 | `xl6CgenEmitFunctionRuntime`、`xl6CgenEmitApproxProtos`、`xl6CgenEmitStringRuntime`、`xl6CgenEmitTypeConvertRuntime`、`xl6CgenEmitTimeRuntime`、`xl6CgenEmitErrorRuntime`、`xl6CgenEmitAsyncRuntime` 的输出片段。 |
| 中间形态伪代码 | `if needs.function: emitFunctionRuntime(); if needs.string: emitStringRuntime(); ...` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工拷贝或包含这些 helper，并保证声明顺序。 |
| 额外依赖的封装代码 | 各基础 runtime helper；XRT 依赖不计入。 |

## CGEN-MODULE-006 xvalue runtime、运行时类型描述与 argv 参数 helper 注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | 使用 `var`、容器、函数值、隐性类类型描述、xlang `main(...)` |
| 生成规则描述 | 只要模块需要动态值 runtime，就输出完整 xvalue runtime 和运行时类型描述。`argc/argv` 到 `xvalue array` 的构造 helper 独立于完整 xvalue runtime：只要存在 xlang `main` 或顶层运行语句，就额外输出轻量 `xvalue` typedef、四个 XRT 原型和 `xl6_argv_to_arg` helper，用于把 C `argc/argv` 映射到 xlang `arg`，避免最小脚本被迫注入整套 xvalue helper。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:105`、`demo6/src/cgen/xl6_cgen_module.c:106`、`demo6/src/cgen/xl6_cgen_module.c:109`、`demo6/src/cgen/xl6_cgen_main.c:1` |
| 对应生成的 C 代码 | 完整 xvalue helper、类型描述表，或仅 `typedef struct xvalue_struct* xvalue;`、`xvoCreateArray`、`xvoCreateText`、`xvoArrayAppendValue`、`xvoUnref` 与 `xl6_argv_to_arg`。 |
| 中间形态伪代码 | `if needs.xvalue: emitXValueRuntime(); emitRuntimeTypeDescs(); if mainFunc or hasTopLevel: emitMainArgvArrayHelper()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工构造动态值数组；只有真正使用动态值能力时才需要维护完整 xvalue helper。 |
| 额外依赖的封装代码 | xvalue runtime、类型描述表、main argv helper。 |

## CGEN-MODULE-007 标准库 wrapper 与专用 helper 注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | 使用 `crypto.*`、`url.*`、`query.*`、`regex.*`、`process.*`、`dir.*`、`string.*`、`path.*`、函数变量、HTTP server、整数幂、`in range` |
| 生成规则描述 | 标准库 wrapper 和专用 helper 在基础 runtime 后按固定顺序注入。HTTP server 依赖 function-var 和 xvalue，因此前面需求聚合阶段已经级联置位。整数幂和 range contains 属于表达式级 helper，也在此统一输出。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:108` |
| 对应生成的 C 代码 | 各 std wrapper、function-var runtime、HTTP server runtime、`int pow` helper、range contains helper。 |
| 中间形态伪代码 | `if needs.crypto: emitCrypto(); ... if needs.http: emitHttpServerRuntime(); if needs.intPow: emitIntPowHelper()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工选择并链接对应库函数封装。 |
| 额外依赖的封装代码 | 标准库 wrapper、function-var runtime、HTTP runtime、表达式 helper。 |

## CGEN-MODULE-008 union、class、native-backed class 声明优先输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `union ... end`、`class ... end`、`class ... native point handle ... end` |
| 生成规则描述 | 模块完成 runtime 注入后，先输出 `union` 声明，再输出 `class` 和 `native-backed class` 声明。这样后续函数原型、方法体和变量声明都可以直接引用结构体类型。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:147` |
| 对应生成的 C 代码 | `typedef union ...`、`typedef struct ...` 等类型声明。 |
| 中间形态伪代码 | `for node in root: if union emitUnionDecls(node); for node in root: if class/native-backed class emitTypeDecls(node)` |
| 实现同等功能手写的 C 代码 | 手写 C 通常也必须先写结构体、联合体声明，再写函数原型。 |
| 额外依赖的封装代码 | 无额外封装；生成的是类型声明本体。 |

## CGEN-MODULE-009 多返回值结构体按函数集合输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f() (int, string)`、泛型多返回、匿名函数多返回、func 类型多返回 |
| 生成规则描述 | 普通非泛型函数和 export 函数的返回结构体先输出；随后输出所有泛型实例的返回结构体；最后输出 func 类型与匿名函数需要的返回结构体。这个顺序保证后续原型和函数体都有可引用的返回类型。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:161` |
| 对应生成的 C 代码 | `typedef struct xl6_ret_... { ... } xl6_ret_...;` |
| 中间形态伪代码 | `emitReturnStructs(normal/export); emitReturnStructs(genericInstances); emitFunctionReturnStructDecls()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工定义一个聚合结构体承载多返回值。 |
| 额外依赖的封装代码 | 多返回值结构体声明。 |

## CGEN-MODULE-010 顶层 extern、export、普通函数和 async 原型输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `extern func`、`extern var`、`export func`、`export var`、普通 `func`、`async func` |
| 生成规则描述 | 顶层声明阶段按 AST 顺序处理：extern 函数输出外部原型，extern 变量输出外部声明，export 函数输出内部原型和 wrapper 原型，export 变量或常量输出全局定义，普通函数输出原型，async 函数输出异步原型。声明和定义在此分离，避免函数体相互调用时缺少原型。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:179` |
| 对应生成的 C 代码 | `extern ...;`、`static ...;`、export wrapper 原型、全局变量定义等。 |
| 中间形态伪代码 | `for decl in root: switch decl.kind: emitExtern/emitExportProto/emitGlobalDef/emitFuncProto/emitAsyncProto` |
| 实现同等功能手写的 C 代码 | 手写 C 通常需要在文件顶部手动写函数原型和全局变量定义。 |
| 额外依赖的封装代码 | export wrapper 原型；外部声明不产生封装代码。 |

## CGEN-MODULE-011 泛型函数实例原型输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `max::<int>(a, b)`、泛型 async 函数实例 |
| 生成规则描述 | 泛型实例收集完成后，为每个实例切换泛型上下文并输出对应的 C 原型。async 泛型实例走 async 原型规则，普通泛型实例走普通函数原型规则。上下文在每个实例后恢复，避免类型参数污染其他实例。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:201` |
| 对应生成的 C 代码 | `static int xl6_f_int(...);` 或对应 async 句柄返回原型。 |
| 中间形态伪代码 | `for inst in genericInstances: enterGeneric(inst); emitProto(inst.func); leaveGeneric()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要为每个具体类型手工写一份函数原型。 |
| 额外依赖的封装代码 | 泛型实例化后的 C 函数原型。 |

## CGEN-MODULE-012 泛型 class 方法原型输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class Box<T> ... func get() T ... end` 的具体实例方法 |
| 生成规则描述 | 泛型 class 的方法原型在普通泛型函数原型后集中输出。该阶段只输出方法原型，不输出方法体；方法体在后续 body 阶段生成。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:216` |
| 对应生成的 C 代码 | `static T_like xl6_Box_int_get(...);` 一类实例化方法原型。 |
| 中间形态伪代码 | `emitGenericClassMethodProtos(ctx)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要为每个泛型 class 实例和方法写独立原型。 |
| 额外依赖的封装代码 | 泛型 class 方法实例原型。 |

## CGEN-MODULE-013 函数值 wrapper 与匿名函数体输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `function<int(int)> f = add`、匿名函数表达式、闭包 |
| 生成规则描述 | 函数值 runtime 需要时，先输出命名函数转换为函数对象所需的 wrapper；随后输出匿名函数实现体。匿名函数已在预收集阶段编号，因此这里可以稳定输出。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:221` |
| 对应生成的 C 代码 | 函数对象 thunk、闭包上下文结构体、匿名函数 C 函数体。 |
| 中间形态伪代码 | `if nodeNeedsFunctionRuntime(root): emitFunctionValueWrappers(); emitAnonFunctions()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式写函数指针适配器和闭包上下文结构。 |
| 额外依赖的封装代码 | 函数值 wrapper、匿名函数实现、闭包辅助结构。 |

## CGEN-MODULE-014 class、native-backed class 与泛型 class 方法体输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class` 方法、`native-backed class` 方法、泛型 class 方法 |
| 生成规则描述 | 普通 class 方法体先按 AST 顺序输出，随后输出 native-backed class 方法体，最后输出泛型 class 实例方法体。这样普通类型方法和 native-backed class 映射方法都早于顶层函数体，顶层函数调用方法时无需后置声明以外的额外处理。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:226` |
| 对应生成的 C 代码 | class 方法 C 函数体、native-backed class 方法映射函数、泛型 class 实例方法体。 |
| 中间形态伪代码 | `emitClassMethods(); emitCustomMethods(); emitGenericClassMethods();` |
| 实现同等功能手写的 C 代码 | 手写 C 需要分别写结构体操作函数和黑盒指针操作函数。 |
| 额外依赖的封装代码 | native-backed class 方法可能生成薄 wrapper；class 方法通常是普通 C 函数体。 |

## CGEN-MODULE-015 顶层普通函数、generator、async 与 export 函数体输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 普通 `func`、返回 `generator<T>` 的函数、`async func`、`export func` |
| 生成规则描述 | 顶层函数体按 AST 顺序输出。普通函数如果返回 generator，则改走 generator 函数生成；普通非 generator 函数走常规函数体；async 函数走 async 函数体；export 函数输出内部实现体和导出 wrapper 体。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:238` |
| 对应生成的 C 代码 | 普通 C 函数体、generator 状态机入口、async 启动函数、export ABI wrapper。 |
| 中间形态伪代码 | `for node in root: if func: emitGeneratorOrFuncBody(); if async: emitAsyncBody(); if export: emitExportInternalAndWrapper()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要分别实现普通函数、状态机、任务入口和 ABI 适配层。 |
| 额外依赖的封装代码 | generator 状态机、async task wrapper、export wrapper。 |

## CGEN-MODULE-016 泛型函数实例函数体输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意实际被调用的泛型函数实例 |
| 生成规则描述 | 每个泛型函数实例切换到对应泛型上下文后输出独立 C 函数体。async 泛型实例走 async body，返回 generator 的实例走 generator body，其他实例走普通函数 body。输出后恢复泛型上下文。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:252` |
| 对应生成的 C 代码 | 针对具体类型实参专门生成的 C 函数体。 |
| 中间形态伪代码 | `for inst in genericInstances: enterGeneric(inst); emitAsyncOrGeneratorOrFuncBody(inst); leaveGeneric()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要为每种类型组合复制并改写一份函数实现。 |
| 额外依赖的封装代码 | 泛型实例函数体本身；不额外生成泛型解释器。 |

## CGEN-MODULE-017 统一生成 C main wrapper 调用 xlang_main

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...) as int ... end` 或存在顶层运行语句的脚本 |
| 生成规则描述 | 如果模块存在 xlang `main` 或顶层运行语句，且生成选项没有抑制发布入口，模块末尾输出显式 C `main` wrapper。该 wrapper 负责初始化 xlang 运行环境、把 `argc/argv` 融合为 `xvalue arg`，调用固定入口 `xlang_main(__xl6_arg)`，释放 `__xl6_arg`，并在退出前调用 `xlUnit`。脚本嵌入目标抑制 C `main`，改为输出错误桥。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:532-540`、`demo6/src/cgen/xl6_cgen_main_wrapper.c:1-62` |
| 对应生成的 C 代码 | `int main(int argc, char** argv) { xlInit(...); __xl6_arg = xl6_argv_to_arg(argc, argv); __xl6_code = xlang_main(__xl6_arg); xvoUnref(__xl6_arg); xlUnit(); return __xl6_code; }` |
| 中间形态伪代码 | `if hasEntry and !suppressMainWrapper: emitMainWrapper(); else if hasEntry: emitScriptErrorBridge()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行初始化 runtime、转换参数、调用固定入口并释放 `xvalue arg`。 |
| 额外依赖的封装代码 | C main wrapper、argv 数组 helper。 |

## CGEN-MODULE-018 模块生命周期与统一 xlang_main

| 项目 | 内容 |
|---|---|
| xlang 写法 | 文件顶层直接写可执行语句，或声明 `func main(...) as int` |
| 生成规则描述 | 只要模块具有入口运行内容，就生成幂等的 `xl_script_module_init(xvalue arg)` 与 `xl_script_module_unit()`。init 负责全局初始化和顶层运行语句，失败时清理已建立状态；unit 等待 async 空闲、逆序释放模块资源并清理参数状态。固定 `xlang_main` 调用 init，按需调用用户 main，最后保证 unit。 |
| 生成器文件名和行号 | 调度 `demo6/src/cgen/xl6_cgen_module.c:532-540`，生命周期和固定入口 `demo6/src/cgen/xl6_cgen_main.c:1080-1188` |
| 对应生成的 C 代码 | `int xl_script_module_init(xvalue arg) { ... } void xl_script_module_unit(void) { ... } int xlang_main(xvalue arg) { if (!xl_script_module_init(arg)) return 0; code = user_main(arg); xl_script_module_unit(); return code; }` |
| 中间形态伪代码 | `module init(top-level/global state); user main if present; module unit(cleanups); return code` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式维护模块初始化标志、失败回滚、异步等待和逆序资源释放。 |
| 额外依赖的封装代码 | 模块 init/unit、固定 `xlang_main`、可选 C main wrapper、argv 数组 helper。 |

## CGEN-MODULE-019 模块 init 与 xlang_main 的异常退出路径

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层语句里使用 `throw`、`assert` 或可能产生 xlang 错误的表达式 |
| 生成规则描述 | 顶层初始化错误先进入 `__xl6_module_init_error`，等待异步、清理参数状态和临时作用域后返回失败。用户 main 返回后若仍有错误，`xlang_main` 进入 `__xl6_script_error`，先调用 module unit 再返回失败；未捕获错误的打印和清理由外层 C main 或嵌入错误桥负责，避免模块入口重复消费错误。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_main.c:1095-1142`、`demo6/src/cgen/xl6_cgen_main.c:1161-1188`、错误桥 `demo6/src/cgen/xl6_cgen_main_wrapper.c:43-62` |
| 对应生成的 C 代码 | `if (xl6_error_has()) goto __xl6_module_init_error; ... if (xl6_error_has()) goto __xl6_script_error; __xl6_script_error: xl_script_module_unit(); return 0;` |
| 中间形态伪代码 | `init error -> rollback init; main error -> module unit; outer host reads/prints error` |
| 实现同等功能手写的 C 代码 | 手写 C 需要区分初始化失败、用户入口失败与宿主诊断职责，并保证所有路径只卸载一次。 |
| 额外依赖的封装代码 | 错误 runtime、模块 init/unit 错误标签、嵌入错误桥。 |

## CGEN-MODULE-020 模块生成状态汇总与上下文资源释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意模块 |
| 生成规则描述 | 输出结束后根据 `ctx.ok` 与诊断数量共同决定生成是否成功，然后释放局部变量表、已输出 class/union 标记、多返回结构体表、清理栈、循环目标栈、try 目标栈、泛型实例、泛型 class 实例、运行时类型描述、匿名函数和 owned type 节点。该规则保证失败路径也能释放编译器内部资源。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_module.c:280`、`demo6/src/cgen/xl6_cgen_module.c:293` |
| 对应生成的 C 代码 | 不生成 C；只影响编译器自身资源生命周期。 |
| 中间形态伪代码 | `ok = ctx.ok && noDiagnostics; freeAllCgenTables(ctx); return ok` |
| 实现同等功能手写的 C 代码 | 手写 C 没有此生成器资源清理步骤。 |
| 额外依赖的封装代码 | 各类 cgen 上下文表的释放函数。 |
