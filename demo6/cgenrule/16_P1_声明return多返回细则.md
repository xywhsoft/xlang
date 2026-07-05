# P1 声明、return 与多返回值细则

本文细化 `xl6_cgen_stmt_decl.c` 与 `xl6_cgen_stmt_dispatch.c` 中声明、block cleanup、语句分派、return、多返回值声明/赋值的生成规则。class/custom 的生命周期细节见 `12_P1_class_custom生命周期细则.md`，本文件只记录它们在声明分支中的入口和与其他声明规则的优先级。

## CGEN-DRET-001 block 作用域 cleanup

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 任意 block：函数体、if/while/try 等 |
| 生成规则描述 | block 进入时记录当前 cleanup 栈位置，逐条生成子语句，block 结束调用 `xl6CgenCloseScope` 清理该 block 内新增的托管变量。cleanup 栈扩容失败时明确诊断为 `out of memory while growing cgen cleanup stack`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:3`、`xl6_cgen_stmt_decl.c:10`、`xl6_cgen_stmt_decl.c:14`、`xl6_cgen_expr_support_base.c:578` |
| 对应生成的 C 代码 | `/* statements */ cleanup_local_values_declared_in_this_block;` |
| 中间形态伪代码 | `mark = cleanupCount; emit statements; closeScope(mark)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在每个提前退出和作用域末尾释放资源。 |
| 额外依赖的封装代码 | cleanup 栈。 |

## CGEN-DRET-002 语句分派与异常传播点

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 声明、赋值、表达式语句、if/while/for、yield 等 |
| 生成规则描述 | `xl6CgenStmt` 按 AST kind 分派到声明、赋值、控制流、异常、循环跳转、yield 等生成器。声明、赋值、普通表达式、控制流和 yield 后会追加 `xl6CgenEmitExceptionPropagate`；throw/assert/try/break/continue 走各自专用路径。未知语句节点兜底诊断为 `C generator does not support statement node: <AST kind>`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:32`、`xl6_cgen_stmt_dispatch.c:39`、`xl6_cgen_stmt_dispatch.c:134`、`xl6_cgen_stmt_dispatch.c:182` |
| 对应生成的 C 代码 | `statement; if (xl6_error_pending()) goto ...;` |
| 中间形态伪代码 | `dispatch(stmt); maybe propagate exception` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在可能抛错的语句后手动检查错误状态。 |
| 额外依赖的封装代码 | error runtime 和异常传播 helper。 |

## CGEN-DRET-003 声明分支优先级

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto a = ...` / `Type a = ...` / `var a = ...` |
| 生成规则描述 | 声明生成先识别 auto 构造 custom/class，再识别显式 custom，再识别显式 class，然后依次进入 auto class、typed container、xvalue、普通 C 类型声明。该优先级保证 class/custom 构造不会被普通 auto 或普通表达式路径吞掉。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:18`、`xl6_cgen_stmt_decl.c:38`、`xl6_cgen_stmt_decl_class_temp.c:16`、`xl6_cgen_stmt_decl_class_temp.c:47`、`xl6_cgen_stmt_decl_class_temp.c:182` |
| 对应生成的 C 代码 | `XL_Custom_File f = {0};` / `XL_Class_Counter c = {0};` / `xvalue v = ...;` |
| 中间形态伪代码 | `decl -> custom? class? autoClass? typedContainer? xvalue? ordinary?` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工选择实际类型和初始化路径。 |
| 额外依赖的封装代码 | 类型推断、class/custom 查找、typed container 判断。 |

## CGEN-DRET-004 auto 类型推断与类型节点持有

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto x = expr` |
| 生成规则描述 | auto 声明用 `xl6CgenInferExprType` 选择 C 类型，同时克隆表达式推断类型节点并交给 cgen context 持有，用于后续变量类型表、cleanup 和成员/索引推断。无初始化时退化为 `int`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:149`、`xl6_cgen_stmt_decl_class_temp.c:152` |
| 对应生成的 C 代码 | `long long x = expr;` 或推断出的具体类型声明。 |
| 中间形态伪代码 | `auto -> infer C type + own inferred type node` |
| 实现同等功能手写的 C 代码 | 手写 C 必须明确写出类型。 |
| 额外依赖的封装代码 | 类型推断。 |

## CGEN-DRET-005 typed container 声明

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `array<int> a = [1, 2]` / `list<string> b = other` |
| 生成规则描述 | typed container 声明会判断是否可从表达式 copy、是否可 move，或需要新建容器。copy 走 typed container copy helper，move 直接使用表达式，字面量初始化则先 create 后逐项填充。声明后注册 typed-container 变量标记和 cleanup。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:182`、`xl6_cgen_value_object.c:132`、`xl6_cgen_value_object.c:143`、`xl6_cgen_value_object.c:209` |
| 对应生成的 C 代码 | `xtarray a = xrtTypedArrayCreate(...); xrtTypedArrayAppend(...);` |
| 中间形态伪代码 | `if canCopy copy(expr) else if canMove expr else create + fill literal` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己创建、复制、移动和释放 typed container。 |
| 额外依赖的封装代码 | typed container runtime 和 cleanup。 |

## CGEN-DRET-006 xvalue 声明

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var v = 123` / `array a = [1,2]` / `dict d` |
| 生成规则描述 | xvalue 类型声明统一生成 `xvalue name = ...`，注册 xvalue cleanup。带初始化时按目标类型装箱；无初始化时按目标类型生成默认 xvalue。若目标类型无法映射为 xvalue 可装箱类型，兜底诊断为 `cannot box unsupported type into var`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:197`、`xl6_cgen_stmt_decl.c:95`、`xl6_cgen_value_object.c:1`、`xl6_cgen_value_object.c:14` |
| 对应生成的 C 代码 | `xvalue v = xvoCreateInt(123);` |
| 中间形态伪代码 | `declare xvalue; value = box(init) or defaultForTarget(type)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用对应 xvoCreate* 并在离开作用域 unref。 |
| 额外依赖的封装代码 | xvalue runtime 和 cleanup。 |

## CGEN-DRET-007 普通托管声明 cleanup 注册

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f` / `future<int> fut` / `channel<int> ch` / `thread t` / `generator<int> g` |
| 生成规则描述 | 普通 C 类型声明前会把变量加入类型表，并按类型注册 function、future、channel、thread、generator、sync 等 cleanup。非托管基础类型注册不会产生实际释放代码。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:208`、`xl6_cgen_stmt_decl_class_temp.c:346` |
| 对应生成的 C 代码 | `XL6_Function f = {0};` 并在作用域结束释放其 env。 |
| 中间形态伪代码 | `addVarType; addManagedCleanupByType; emit C declaration` |
| 实现同等功能手写的 C 代码 | 手写 C 需要逐种资源类型手动释放。 |
| 额外依赖的封装代码 | 各类 cleanup helper。 |

## CGEN-DRET-008 function 声明初始化与借用 retain

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) as int f = foo` / `func() as int f = obj.fn` |
| 生成规则描述 | func 类型声明初始化时不直接 `xl6CgenExpr`，而是走 `xl6CgenFunctionValueExpr`，以生成符合目标签名的 function value。若初始化表达式是借用 function，则声明后 retain function value。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:220`、`xl6_cgen_stmt_decl_class_temp.c:231` |
| 对应生成的 C 代码 | `XL6_Function f = xl6_make_function(...); if (f.retain && f.env) f.retain(f.env);` |
| 中间形态伪代码 | `function decl = functionValueExpr(init); retain if borrowed` |
| 实现同等功能手写的 C 代码 | 手写 C 需要构造 `XL6_Function` 并维护引用计数。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-DRET-009 多返回普通函数调用识别

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `a, b = pair()` / `auto a, b = pair()` |
| 生成规则描述 | 多返回普通函数调用要求右侧是 call，callee 是名称，并能解析到 xlang 函数声明；该函数 returns 数量必须大于 1。否则多返回声明/赋值会报错。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:242` |
| 对应生成的 C 代码 | `XL6_Function_Return_ABCD tmp = xl_func_pair();` |
| 中间形态伪代码 | `resolveCallFunc(name); returns.count > 1` |
| 实现同等功能手写的 C 代码 | 手写 C 直接调用返回结构体的函数。 |
| 额外依赖的封装代码 | 函数重载解析、多返回结构体。 |

## CGEN-DRET-010 多返回 function value 调用识别

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `a, b = f()`，其中 `f` 是 `func(...) (T1,T2)` |
| 生成规则描述 | 如果 callee 是成员表达式，则推断成员字段类型；如果 callee 是名称，则查变量类型表。只有 func 类型且返回数量大于 1 时走 function value 多返回路径。成员 func value 会先临时求值到 `XL6_Function`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:260`、`xl6_cgen_stmt_decl_class_temp.c:307` |
| 对应生成的 C 代码 | `XL6_Function __xl6_member_fn_multi_1 = obj.fn; xl6_function_invoke(...);` |
| 中间形态伪代码 | `if callee type is function with multi returns -> function value invoke` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先取出 function value 再调用 invoke helper。 |
| 额外依赖的封装代码 | function invoke runtime。 |

## CGEN-DRET-011 多返回调用输出

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `a, b = pair()` / `a, b = f()` |
| 生成规则描述 | 多返回调用生成时优先处理 function value；普通已知函数走直接函数符号调用。function value 名称调用直接 `xl6CgenFunctionInvoke`，成员调用会生成 GNU statement expression 临时块。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:297`、`xl6_cgen_stmt_decl_class_temp.c:307` |
| 对应生成的 C 代码 | `XL6_Function_Return_ABCD __xl6_multi_0 = xl_func_pair();` |
| 中间形态伪代码 | `emit multi return call -> struct value` |
| 实现同等功能手写的 C 代码 | 手写 C 直接把调用结果保存到结果结构体。 |
| 额外依赖的封装代码 | function invoke 或直接函数调用。 |

## CGEN-DRET-012 多返回结构体名选择

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 普通多返回函数或 function value 多返回 |
| 生成规则描述 | 普通函数使用 `xl6CgenFuncReturnStructName`；function value 使用函数类型的参数和返回类型计算结构体名。这样相同签名的 function value 和普通函数可以共享结构体形态。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:335` |
| 对应生成的 C 代码 | `XL6_Function_Return_0123456789ABCDEF` |
| 中间形态伪代码 | `if functionType then structName(signature(functionType)) else structName(funcDecl)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要复用或手动选择结果结构体。 |
| 额外依赖的封装代码 | ABI 签名 ID。 |

## CGEN-DRET-013 多返回声明校验

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto a, b = pair()` |
| 生成规则描述 | 多返回声明要求右侧是多返回普通函数调用或多返回 function value 调用，并且左侧名称数量必须和返回值数量一致。不满足时只报错，不生成部分声明。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:397`、`xl6_cgen_stmt_decl_class_temp.c:412`、`xl6_cgen_stmt_decl_class_temp.c:418` |
| 对应生成的 C 代码 | `/* error if counts mismatch */` |
| 中间形态伪代码 | `if returns == null or names.count != returns.count: error` |
| 实现同等功能手写的 C 代码 | 手写 C 编译器会通过结构体字段数量约束，xlang 在生成前显式校验。 |
| 额外依赖的封装代码 | 诊断系统。 |

## CGEN-DRET-014 多返回声明变量生成

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto a, b = pair()` |
| 生成规则描述 | 校验通过后先生成 `__xl6_multi_N` 临时结构体接收调用结果；随后每个非 `_` 名称按对应返回类型声明变量，初始化为 `__xl6_multi_N.valueI`，注册变量类型并按返回类型注册托管 cleanup。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:425`、`xl6_cgen_stmt_decl_class_temp.c:452`、`xl6_cgen_stmt_decl_class_temp.c:467` |
| 对应生成的 C 代码 | `ReturnStruct __xl6_multi_0 = pair(); long long a = __xl6_multi_0.value0; char* b = __xl6_multi_0.value1;` |
| 中间形态伪代码 | `tmp = call(); for name[i]: decl type[i] name = tmp.value[i]` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先接收结构体再拆字段。 |
| 额外依赖的封装代码 | managed cleanup 注册。 |

## CGEN-DRET-015 多返回声明丢弃值

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto _, b = pair()` |
| 生成规则描述 | 左侧名称为 `_` 时不声明变量。若被丢弃值是 xvalue，立即 `xvoUnref`；随后调用 `xl6CgenReleaseReturnedValuePath` 释放 function/class/custom/容器等返回值路径，避免丢弃值泄漏。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:435`、`xl6_cgen_stmt_decl_class_temp.c:448` |
| 对应生成的 C 代码 | `if (__xl6_multi_0.value0 != 0) { xvoUnref(__xl6_multi_0.value0); }` |
| 中间形态伪代码 | `if name == "_": release tmp.value[i]; continue` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己释放未使用的返回资源。 |
| 额外依赖的封装代码 | 返回值路径释放 helper。 |

## CGEN-DRET-016 多返回赋值校验

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `a, b = pair()` |
| 生成规则描述 | 多返回赋值与多返回声明使用相同的右侧解析和数量校验。右侧必须是多返回普通函数调用或多返回 function value 调用；目标数量必须等于返回数量。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:100`、`xl6_cgen_stmt_decl.c:115`、`xl6_cgen_stmt_decl.c:121` |
| 对应生成的 C 代码 | `ReturnStruct __xl6_multi_1 = pair();` |
| 中间形态伪代码 | `validate targets.count == returns.count` |
| 实现同等功能手写的 C 代码 | 手写 C 先调用函数，再分别赋值。 |
| 额外依赖的封装代码 | 诊断系统。 |

## CGEN-DRET-017 多返回赋值到 xvalue 变量

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var a; a, b = pair()` |
| 生成规则描述 | 目标是 xvalue 变量时，赋新值前先 unref 旧值，再把临时结构体对应字段直接移入目标变量。该路径避免旧 xvalue 泄漏，也避免对新返回值重复 retain。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:143` |
| 对应生成的 C 代码 | `if (a != 0) { xvoUnref(a); } a = __xl6_multi_0.value0;` |
| 中间形态伪代码 | `release old xvalue; move returned xvalue into target` |
| 实现同等功能手写的 C 代码 | 手写 C 必须先释放旧 xvalue。 |
| 额外依赖的封装代码 | xvalue runtime。 |

## CGEN-DRET-018 多返回赋值到 function 变量

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func() as int f; f, n = pair()` |
| 生成规则描述 | 目标是 func 类型变量时，赋值前调用 `xl6CgenReleaseFunctionValue` 释放旧 env，然后把返回的 function value 结构体字段写入目标。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:157` |
| 对应生成的 C 代码 | `xl6_release_function(f); f = __xl6_multi_0.value0;` |
| 中间形态伪代码 | `release old function; assign returned function` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护旧 function value 的引用计数。 |
| 额外依赖的封装代码 | function value cleanup。 |

## CGEN-DRET-019 多返回赋值到含 function 字段的 class

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `Holder h; h, n = pair()`，其中 `Holder` 含 function 字段 |
| 生成规则描述 | 目标变量类型如果是需要 function 字段 cleanup 的 class，赋新结构体前先释放目标当前 function 字段，然后整体赋值返回结构体字段。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:165` |
| 对应生成的 C 代码 | `release_function_fields(&h); h = __xl6_multi_0.value0;` |
| 中间形态伪代码 | `release target class function fields; assign returned class value` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先清理旧闭包字段再覆盖结构体。 |
| 额外依赖的封装代码 | class function field cleanup helper。 |

## CGEN-DRET-020 多返回赋值到索引目标

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `arr[i], b = pair()` |
| 生成规则描述 | 索引目标不能直接接收任意 C 类型。生成器先把返回字段按返回类型装箱成临时 xvalue，再调用 `xl6CgenIndexAssignTemp` 复用索引赋值规则写入容器。typed container setValue 成功后必须释放临时 xvalue；动态 dict key 路径还必须先释放临时 key，再释放临时 value。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:56`、`xl6_cgen_stmt_decl.c:186`、`xl6_cgen_stmt_decl.c:197`、`xl6_cgen_value_assign.c:10`、`xl6_cgen_value_assign.c:75`、`xl6_cgen_value_assign.c:1010` |
| 对应生成的 C 代码 | `xvalue __xl6_multi_index_1 = xvoCreateInt(__xl6_multi_0.value0); if (!xl6_xrt_typed_array_set_at(values, (unsigned int)(i), __xl6_multi_index_1)) { xl6_runtime_fatal(3); } xvoUnref(__xl6_multi_index_1);` |
| 中间形态伪代码 | `returned C value -> xvalue temp -> index assignment -> release temp after container takes/copies value` |
| 实现同等功能手写的 C 代码 | 手写 C 需要按容器 API 将值装箱后 set，并在容器完成 copy/retain 后释放临时装箱值。 |
| 额外依赖的封装代码 | xvalue 装箱、索引赋值 helper、XRT typed container setValue；typed array 通过 `xl6_xrt_typed_array_set_at` 集中处理 XRT 1 基 ABI。 |

## CGEN-DRET-021 多返回赋值到普通目标

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.field, local = pair()` |
| 生成规则描述 | 其他目标走通用表达式左值路径：先生成目标表达式，再赋值为临时结构体对应字段。该路径不额外释放旧资源，因此只适合前面特殊分支未覆盖的简单 C 左值。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:192` |
| 对应生成的 C 代码 | `obj.field = __xl6_multi_0.value0;` |
| 中间形态伪代码 | `emitLValue(target) = tmp.value[i]` |
| 实现同等功能手写的 C 代码 | 手写 C 直接赋值。 |
| 额外依赖的封装代码 | 目标表达式生成器。 |

## CGEN-DRET-022 return 多返回值

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `return a, b` |
| 生成规则描述 | 当前函数返回数量大于 1 时，生成临时返回结构体 `__xl6_return_N`，逐字段写入返回表达式。xvalue 返回走目标装箱，function 返回走 function value 生成，普通类型走表达式。function 借用返回会 retain，class 中借用 function 字段也会 retain。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:54`、`xl6_cgen_stmt_dispatch.c:62`、`xl6_cgen_stmt_dispatch.c:72`、`xl6_cgen_stmt_dispatch.c:87` |
| 对应生成的 C 代码 | `ReturnStruct __xl6_return_0; __xl6_return_0.value0 = a; __xl6_return_0.value1 = b; cleanup; return __xl6_return_0;` |
| 中间形态伪代码 | `ret.value[i] = convert(expr[i], returnType[i]); cleanup all; return ret` |
| 实现同等功能手写的 C 代码 | 手写 C 需要定义结果结构体并在 return 前释放局部资源。 |
| 额外依赖的封装代码 | 多返回结构体、function/class retain helper。 |

## CGEN-DRET-023 return typed container

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f() array<int>` / `return [1, 2, 3]` |
| 生成规则描述 | 非 class/custom 函数单返回 typed container 且返回值使用 typed container storage 时，生成 typed container 专用 return：先声明 `__xl6_return_N`，按字面量/create/copy/move 规则构造，必要时填充字面量。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:100`、`xl6_cgen_value_object.c:384` |
| 对应生成的 C 代码 | `xtarray __xl6_return_0 = xrtTypedArrayCreate(...); fill; cleanup; return __xl6_return_0;` |
| 中间形态伪代码 | `return typed container -> build return container before cleanup` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先构造返回容器，再释放局部变量。 |
| 额外依赖的封装代码 | typed container return helper。 |

## CGEN-DRET-024 return 单值

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `return expr` |
| 生成规则描述 | 单返回值会先生成局部临时 `__xl6_return_N`，类型使用当前返回类型或表达式推断类型。xvalue/function 按目标类型转换；其他类型直接表达式。生成临时后执行全函数 cleanup，再 return 临时值。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:103`、`xl6_cgen_stmt_dispatch.c:111`、`xl6_cgen_stmt_dispatch.c:125` |
| 对应生成的 C 代码 | `T __xl6_return_0 = expr; cleanup_all; return __xl6_return_0;` |
| 中间形态伪代码 | `tmp = convert(expr); cleanup; return tmp` |
| 实现同等功能手写的 C 代码 | 手写 C 通常也需要在 return 前保存结果再释放资源。 |
| 额外依赖的封装代码 | cleanup 栈、function/class retain helper。 |

## CGEN-DRET-025 无值 return

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `return` |
| 生成规则描述 | 无返回表达式时不生成临时值，直接执行全函数 cleanup，然后输出 `return;`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:128` |
| 对应生成的 C 代码 | `cleanup_all; return;` |
| 中间形态伪代码 | `cleanup; return` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在 return 前释放局部资源。 |
| 额外依赖的封装代码 | cleanup 栈。 |

## CGEN-DRET-026 async return 分派

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `async func f() int` / `return 1` |
| 生成规则描述 | 如果当前处于 async worker，return 不走普通同步 return 规则，而是直接分派给 `xl6CgenAsyncReturnStmt`。这样 async 返回值写入 future 状态，而不是从 worker C 函数直接返回业务值。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:50` |
| 对应生成的 C 代码 | `xl6_future_set_result(...); return;` |
| 中间形态伪代码 | `if currentAsyncWorker: asyncReturn(stmt)` |
| 实现同等功能手写的 C 代码 | 手写 async worker 需要把结果写入 future 后结束 worker。 |
| 额外依赖的封装代码 | async runtime。 |

## CGEN-DRET-027 for-in 分派入口

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `for item in iterable ... end` |
| 生成规则描述 | for-in 入口先识别 `range(...)`，否则推断 iterable 类型并要求它是 array/list/set/dict/generator/var。generator 走 generator 消费路径；typed container storage 或 typed container return call 走 typed container 遍历；其他走 xvalue 遍历。具体循环体规则见 `08_P0_控制流与循环细则.md`。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:1`、`xl6_cgen_stmt_dispatch.c:8`、`xl6_cgen_stmt_dispatch.c:20` |
| 对应生成的 C 代码 | `for (...) { ... }` / typed iterator / generator next |
| 中间形态伪代码 | `range ? rangeFor : generator ? genFor : typed ? typedFor : xvalueFor` |
| 实现同等功能手写的 C 代码 | 手写 C 根据容器类型选择不同迭代 API。 |
| 额外依赖的封装代码 | range/typed container/xvalue/generator 遍历 helper。 |

## CGEN-DRET-028 表达式语句

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `foo(1)` / `obj.method()` |
| 生成规则描述 | 表达式语句只生成表达式、分号和异常传播检查。不主动释放表达式返回的托管值；返回值生命周期依赖表达式生成内部的临时值策略。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_dispatch.c:132` |
| 对应生成的 C 代码 | `foo(1);` |
| 中间形态伪代码 | `emitExpr(expr); ";" ; propagateException` |
| 实现同等功能手写的 C 代码 | 手写 C 直接调用函数并忽略返回值。 |
| 额外依赖的封装代码 | 异常传播 helper。 |
