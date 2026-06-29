# P0 函数调用与 callable 生成细则

本文细化函数调用、重载解析、泛型函数调用、`function` 值、`var` 函数调用、匿名函数、XCall 调用帧、函数签名与多返回值 wrapper 的生成规则。该部分是 xlang 与 C/XRT ABI 沟通的核心。

## CGEN-CALL-001 普通 call 表达式总分派

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `f(1, 2)`、`obj.method()`、`a::<int>(1)` |
| 生成规则描述 | `XL6_AST_CALL` 先按 callee 形态分派：成员调用走 function 字段/线程/同步/channel/隐性方法/class/custom；`base(...)` 与内置并发构造优先处理；泛型实例化走显式泛型函数解析；名字调用再判断 `type()`、类型转换、`var` 函数值、`function` 函数值、extern/export/普通函数。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:185`、`xl6_cgen_expr.c:189`、`xl6_cgen_expr.c:205`、`xl6_cgen_expr.c:214`、`xl6_cgen_expr.c:250` |
| 对应生成的 C 代码 | 根据目标不同生成 `f(args)`、`xl6_var_call(...)`、`xl6_function_call_var(...)`、`((ret(*)(void*, ...))fn.invoke)(fn.env, ...)` 等。 |
| 中间形态伪代码 | `dispatchCall(calleeKind, calleeType, args)` |
| 实现同等功能手写的 C 代码 | 手写 C 通常在源码层已经知道目标函数，不需要这一层动态分派。 |
| 额外依赖的封装代码 | 取决于目标，可能需要 function runtime、xcall wrapper、class/custom method helper。 |

## CGEN-CALL-002 未知目标普通实参输出

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `callee(1, 2)`，其中生成器无法解析 callee |
| 生成规则描述 | 如果 callee 不是已知函数、extern、export 或 function/var 值，则按普通 C 调用输出 callee 表达式和参数。该路径不支持命名参数，遇到命名参数会报错并输出 `0`。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:196`、`xl6_cgen_expr.c:302`、`xl6_cgen_expr.c:306`、`xl6_cgen_expr.c:313` |
| 对应生成的 C 代码 | `callee(1LL, 2LL)` |
| 中间形态伪代码 | `emit(callee); emit("("); emit positional args only; emit(")")` |
| 实现同等功能手写的 C 代码 | 与普通 C 调用一致。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CALL-003 已知函数实参展开

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(int a, int b = 2); f(1)`、`f(b = 2, a = 1)` |
| 生成规则描述 | 已知函数调用使用参数声明表展开实参：固定参数先按名称匹配，再按位置匹配，缺省参数在调用点直接生成默认表达式。可变参数追加为最后一个参数。若已知调用作为表达式语句出现，且固定 `var` 参数的实参是动态二元、动态条件或动态空合并等需要多语句求值的 `var` 表达式，则调用前先生成 `xvalue __xl6_call_arg_N`，再用表达式替换表把原实参替换为该临时值，避免在 C 实参位置生成 GNU 语句表达式。 |
| 生成器文件名和行号 | 常规展开 `xl6_cgen_args_containers.c:546`、`xl6_cgen_args_containers.c:557`、`xl6_cgen_args_containers.c:564`、`xl6_cgen_args_containers.c:573`、`xl6_cgen_expr.c:311`；调用语句实参预求值 `xl6_cgen_stmt_decl_call_args.c:96`、`xl6_cgen_stmt_decl_call_args.c:453`、`xl6_cgen_stmt_decl_call_args.c:483`、`xl6_cgen_stmt_decl_call_args.c:566`、`xl6_cgen_stmt_dispatch.c:244` |
| 对应生成的 C 代码 | 常规：`f(1LL, 2LL)`；`var` 复杂实参语句级化：`xvalue __xl6_call_arg_4 = xvoCreateNull(); { xvalue __xl6_bin_left_5 = makeValue(1LL); xvalue __xl6_bin_right_5 = makeValue(2LL); __xl6_call_arg_4 = xl6_xrt_basic_binary(__xl6_bin_left_5, __xl6_bin_right_5, 0); ... } record_xvalue(__xl6_call_arg_4);` |
| 中间形态伪代码 | `for param in fixedParams: if stmtCall && param is var && arg needs xvalue lowering: tmp = lowerArgBeforeCall(arg); replace arg with tmp; emit knownCall(args); else emit argByNameOrPositionOrDefault(param)` |
| 实现同等功能手写的 C 代码 | 手写 C 调用者必须自己把默认参数补齐，并按 C 参数顺序书写。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CALL-004 `...` 数组可变参数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(int a, ... arg); f(1, "x", true)` |
| 生成规则描述 | 已知函数声明中最后一个参数为 `array<var>` 可变参数时，固定参数之后的位置实参被装箱为 `xvalue` 并打包到 `xl6_xrt_array_varargs(count, ...)`。 |
| 生成器文件名和行号 | `xl6_cgen_args_containers.c:451`、`xl6_cgen_args_containers.c:475`、`xl6_cgen_args_containers.c:579` |
| 对应生成的 C 代码 | `f(1LL, xl6_xrt_array_varargs(2, xvoCreateText((void*)"x", 0, false), xvoCreateBool(1)))` |
| 中间形态伪代码 | `arg = arrayVarargs(extraPositionalArgs)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 xvalue array 并逐个 append。 |
| 额外依赖的封装代码 | 依赖 `xl6_xrt_array_varargs` 运行时 helper。 |

## CGEN-CALL-005 `***` 字典可变参数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(*** arg); f(name = "x")` |
| 生成规则描述 | 已知函数声明中最后一个参数为 `dict<var>` 可变参数时，所有未匹配固定参数的命名实参被打包进 `xl6_xrt_dict_varargs(count, key, value, ...)`。已匹配固定参数的命名实参不会进入字典。 |
| 生成器文件名和行号 | `xl6_cgen_args_containers.c:504`、`xl6_cgen_args_containers.c:523`、`xl6_cgen_args_containers.c:581` |
| 对应生成的 C 代码 | `f(xl6_xrt_dict_varargs(1, "name", xvoCreateText((void*)"x", 0, false)))` |
| 中间形态伪代码 | `kwargs = dictVarargs(namedArgs - fixedParamNames)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 xvalue table 并逐个设置 key/value。 |
| 额外依赖的封装代码 | 依赖 `xl6_xrt_dict_varargs` 运行时 helper。 |

## CGEN-CALL-006 重载解析优先级

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `f(1)`，同时存在多个 `func f(...)` |
| 生成规则描述 | 重载解析遍历根 AST 中同名 `func/async func`，用匹配代价选择候选。固定参数函数优先于 vararg 函数：固定候选唯一则返回；固定候选有歧义时报错；没有固定候选时再考虑 vararg 候选。 |
| 生成器文件名和行号 | `xl6_cgen_resolve_call.c:1`、`xl6_cgen_resolve_call.c:35`、`xl6_cgen_resolve_call.c:58`、`xl6_cgen_resolve_call.c:68`、`xl6_cgen_resolve_call.c:80` |
| 对应生成的 C 代码 | 解析成功后输出被选中的 C 符号，例如 `xl6_f_i64(1LL)`。 |
| 中间形态伪代码 | `fixedBest = best(nonVararg); if unique fixed -> use; else use unique vararg` |
| 实现同等功能手写的 C 代码 | C 本身没有函数重载，手写时需要直接调用不同 C 符号。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CALL-007 隐式泛型函数实例化

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `max(1, 2)`，其中 `max` 是泛型函数 |
| 生成规则描述 | 普通名字调用先调用 `xl6CgenResolveCallFuncWithTypeArgs`。若解析结果带有推断出的类型参数，生成器临时设置 `currentGenericParams/currentGenericArgs`，输出实例化后的函数符号和已知实参，随后登记 owned generic instance。 |
| 生成器文件名和行号 | `xl6_cgen_resolve_call.c:109`、`xl6_cgen_expr.c:280`、`xl6_cgen_expr.c:286`、`xl6_cgen_expr.c:293` |
| 对应生成的 C 代码 | `xl6_max__T_int(1LL, 2LL)` |
| 中间形态伪代码 | `typeArgs = inferGenericArgs(call); emit instantiatedFuncSymbol(typeArgs)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式选择或编写具体类型版本。 |
| 额外依赖的封装代码 | 会触发泛型实例函数生成。 |

## CGEN-CALL-008 显式泛型函数实例化

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `max::<int>(1, 2)` |
| 生成规则描述 | callee 为 `XL6_AST_GENERIC_INSTANT` 时，只支持函数名形式。解析显式类型参数，登记泛型实例，设置当前泛型上下文并输出实例化函数符号。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:214`、`xl6_cgen_expr.c:224`、`xl6_cgen_expr.c:230`、`xl6_cgen_expr.c:238`、`xl6_cgen_expr.c:243` |
| 对应生成的 C 代码 | `xl6_max__T_int(1LL, 2LL)` |
| 中间形态伪代码 | `func = resolveExplicitGeneric(name, typeArgs); emit func<typeArgs>(args)` |
| 实现同等功能手写的 C 代码 | `xl6_max__T_int(1, 2)` |
| 额外依赖的封装代码 | 会触发泛型实例函数生成。 |

## CGEN-CALL-009 extern / export 函数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `extern func cputs(string s); cputs("hi")`、`export func f(); f()` |
| 生成规则描述 | 名字调用先查找 extern/export。extern 输出声明中的 C 符号，export 输出 `xl6_export_` 前缀的内部符号。两者都按已知函数参数规则展开实参。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:162`、`xl6_cgen_function_value.c:179`、`xl6_cgen_expr.c:251`、`xl6_cgen_expr.c:274`、`xl6_cgen_expr.c:277` |
| 对应生成的 C 代码 | `cputs("hi")`、`xl6_export_f()` |
| 中间形态伪代码 | `if extern: emit externSymbol; else if export: emit exportInternalSymbol` |
| 实现同等功能手写的 C 代码 | 与直接 C 函数调用一致。 |
| 额外依赖的封装代码 | export 函数可能有导出表/XAPI 信息，调用本身不额外生成 helper。 |

## CGEN-CALL-010 `type(expr)` 与类型转换调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `type(a)`、`int("123")` |
| 生成规则描述 | 名字调用中 `type` 被特殊处理为类型值表达式；内置转换类型名且已知类型时，调用转成类型转换表达式，不按普通函数解析。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:254`、`xl6_cgen_expr.c:260`、`xl6_cgen_args.c:831` |
| 对应生成的 C 代码 | `xl6_type_id_of_expr` 或转换表达式，例如 `(long long)xvoGetInt(v)`、`xrtStrToInt(...)` 形态。 |
| 中间形态伪代码 | `if callee == type: emit typeOf; if callee is builtin type: emit convert` |
| 实现同等功能手写的 C 代码 | 手写 C 需要直接调用类型转换或类型 id helper。 |
| 额外依赖的封装代码 | 依赖 type id / conversion helper。 |

## CGEN-CALL-011 function 变量直接调用签名检查

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) as int f; f(1)` |
| 生成规则描述 | 静态类型为 `function` 且带签名时，调用点先计算签名 id，并生成三目表达式检查 `f.signature == expectedId`。签名匹配时优先走 `invoke` 直接调用；签名不匹配走集中 helper `xl6_function_fatal(3)`，该 helper 再委托模块级 `xl6_runtime_fatal`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:268`、`xl6_cgen_function_value.c:704`、`xl6_cgen_function_value.c:737`、`xl6_cgen_function_value.c:740`、`xl6_cgen_abi.c:168` |
| 对应生成的 C 代码 | `((f.signature == 0x...ULL) ? ((f.invoke != 0) ? ((long long (*)(void*, long long))f.invoke)(f.env, 1LL) : fallback) : (xl6_function_fatal(3), (long long)0))` |
| 中间形态伪代码 | `require signature; if direct invoke exists -> direct; else fallback xvalue call` |
| 实现同等功能手写的 C 代码 | 手写 C 可直接强转 `invoke` 函数指针，但也应先检查签名。 |
| 额外依赖的封装代码 | 需要 `XL6_Function` runtime、签名 id 与模块级 `xl6_runtime_fatal`。 |

## CGEN-CALL-012 function 变量无签名调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f; f(1, 2)` |
| 生成规则描述 | `function` 类型没有参数/返回签名时，调用退化为 bare func invoke：所有实参装箱为 `xvalue`，调用 `xl6_function_call_var`，结果为 `xvalue`。命名参数在此路径报错。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:524`、`xl6_cgen_function_value.c:536`、`xl6_cgen_function_value.c:704` |
| 对应生成的 C 代码 | `xl6_function_call_var(f, 2, xvoCreateInt(1), xvoCreateInt(2))` |
| 中间形态伪代码 | `function_call_var(fn, boxedArgs...)` |
| 实现同等功能手写的 C 代码 | `xvalue r = xl6_function_call_var(fn, argc, ...);` |
| 额外依赖的封装代码 | 依赖 function var runtime。 |

## CGEN-CALL-013 function 变量直接 invoke

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int, int) as int add; add(1, 2)` |
| 生成规则描述 | 签名匹配且 `invoke != 0` 时，生成器把 `invoke` 强转为 `returnType (*)(void*, paramTypes...)`，第一个参数传 `fn.env`，后续传固定参数和可变参数容器。多返回值返回结构体类型。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:548`、`xl6_cgen_function_value.c:560`、`xl6_cgen_function_value.c:570`、`xl6_cgen_function_value.c:580` |
| 对应生成的 C 代码 | `((long long (*)(void*, long long, long long))add.invoke)(add.env, 1LL, 2LL)` |
| 中间形态伪代码 | `typedInvoke = cast(fn.invoke); result = typedInvoke(fn.env, args...)` |
| 实现同等功能手写的 C 代码 | 与生成代码类似，但手写者需保证签名正确。 |
| 额外依赖的封装代码 | 多返回值需要 return struct typedef。 |

## CGEN-CALL-014 function 变量 fallback 调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) as string f; f(1)`，但 `f.invoke == 0` 且有 `invoke_var/xcall` |
| 生成规则描述 | 如果签名匹配但没有直接 invoke，生成器退回 `xl6_function_call_var` 或 `xl6_function_call_kwargs`，返回 `xvalue` 后按声明返回类型释放转换：int/float/bool/string/time/point/function/type 各自有转换 helper。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:593`、`xl6_cgen_function_value.c:623`、`xl6_cgen_function_value.c:640`、`xl6_cgen_callable_runtime.c:1`、`xl6_cgen_callable_runtime.c:44` |
| 对应生成的 C 代码 | `xl6_xvalue_to_string_release(xl6_function_call_var(f, 1, xvoCreateInt(1)))` |
| 中间形态伪代码 | `xv = dynamicInvoke(fn, boxedArgs); return convertAndRelease<T>(xv)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用动态 invoke，并按目标类型取值和释放。 |
| 额外依赖的封装代码 | 依赖 function var runtime 中的 `xl6_xvalue_to_*_release` helper。 |

## CGEN-CALL-015 var 函数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var f; f(1, name = "x")` |
| 生成规则描述 | 静态类型为 `var` 的 callee 调用走 `xl6_var_call` 或 `xl6_var_call_kwargs`。位置实参装箱为 `xvalue`，命名实参打包为 kwargs dict。运行时通过 `xvoInvoke` 执行，并返回第一个结果或 null。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:264`、`xl6_cgen_function_value.c:670`、`xl6_cgen_function_value.c:681`、`xl6_cgen_callable_runtime.c:44` |
| 对应生成的 C 代码 | `xl6_var_call_kwargs(f, xl6_xrt_dict_varargs(1, "name", xvoCreateText((void*)"x", 0, false)), 1, xvoCreateInt(1))` |
| 中间形态伪代码 | `xvoInvoke(fn, callFrame{argv, kwargs})` |
| 实现同等功能手写的 C 代码 | 手写 C 需要构造 `xrt_call_frame`、`xrt_call_result` 并释放 argv/kwargs。 |
| 额外依赖的封装代码 | 依赖 function var runtime。 |

## CGEN-CALL-016 func 值由函数名构造

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) as int f = add` |
| 生成规则描述 | 当目标类型是 `function`，右值是未绑定变量的名字时，生成器在同名 func/extern/export 中查找唯一签名匹配项，并调用对应 `_make` wrapper 生成 `XL6_Function`。泛型函数不会作为 func 值直接匹配。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:459`、`xl6_cgen_function_value.c:483`、`xl6_cgen_function_value.c:828`、`xl6_cgen_callable_wrappers.c:472` |
| 对应生成的 C 代码 | `XL6_Function f = xl6_function_value_add_make();` |
| 中间形态伪代码 | `selected = findFuncByNameAndFunctionType(name, targetType); emit selected_make()` |
| 实现同等功能手写的 C 代码 | `XL6_Function f = make_add_function();` |
| 额外依赖的封装代码 | 需要为目标函数生成 function value wrapper。 |

## CGEN-CALL-017 func 类型匹配可选参数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func add(int a, int b = 1) as int; func(int) as int f = add` |
| 生成规则描述 | func 类型匹配函数参数时，会考虑 required/fixed 参数数量和可选参数。若 func 类型参数个数在 required 和 fixed 之间，可通过 optional adapter 生成较短签名的 func 值。adapter 补默认值时若参数节点缺少默认表达式，会明确诊断为 `optional adapter parameter is missing a default expression`；声明需要 vararg 但当前 adapter 签名未携带 vararg 时，诊断为 `callable adapter requires a vararg parameter`。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:391`、`xl6_cgen_callable_wrappers.c:747`、`xl6_cgen_callable_wrappers.c:552`、`xl6_cgen_callable_wrappers.c:595`、`xl6_cgen_callable_wrappers.c:606`、`xl6_cgen_callable_wrappers.c:707` |
| 对应生成的 C 代码 | `XL6_Function f = xl6_function_value_add_1_make();`，adapter 内部调用 `add(a, defaultB)` |
| 中间形态伪代码 | `if required <= expectedCount <= fixed: generate prefix adapter` |
| 实现同等功能手写的 C 代码 | 手写 C 需要写一个只接收 `a` 的 wrapper，并在内部补默认参数。 |
| 额外依赖的封装代码 | 需要 optional adapter invoke/xcall/make。 |

## CGEN-CALL-018 多返回值函数 wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func pair() (int, string)` |
| 生成规则描述 | 多返回值函数使用由签名 hash 派生的结构体 `XL6_Function_Return_<id>`。function wrapper 直接返回该结构体；XCall wrapper 则把结构体的每个 `valueN` 分别装箱写入 `xrt_call_result`。 |
| 生成器文件名和行号 | `xl6_cgen_abi.c:194`、`xl6_cgen_abi.c:209`、`xl6_cgen_callable_wrappers.c:270`、`xl6_cgen_callable_frame.c:293`、`xl6_cgen_callable_wrappers.c:373` |
| 对应生成的 C 代码 | `typedef struct XL6_Function_Return_ABCD { long long value0; const char* value1; } XL6_Function_Return_ABCD;` |
| 中间形态伪代码 | `multi = func(); result[0] = box(multi.value0); result[1] = box(multi.value1)` |
| 实现同等功能手写的 C 代码 | C 常用 struct 返回多值，或通过 out 参数返回。 |
| 额外依赖的封装代码 | 需要 return struct typedef 与 xcall result 写入代码。 |

## CGEN-CALL-019 成员 func 字段调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.cb(1)`，其中 `cb` 是 function 字段 |
| 生成规则描述 | 成员字段类型推断为 `function` 时按签名能力分流：参数和单返回值都能经由 xvalue runtime 表达时，单返回/无返回成员调用直接生成 erased call helper，即 `xl6_function_call_var(memberExpr, ...)` 或 `xl6_function_call_kwargs(memberExpr, ...)`，再按声明返回类型做 release 转换；多返回值声明/赋值语句在语句层先生成 `XL6_Function __xl6_member_fn_multi_N = memberExpr;`，再复用 typed direct invoke；非 erased-compatible 的成员 func 表达式语句会在语句层先生成 `XL6_Function __xl6_member_fn_stmt_N = memberExpr;`，无返回值时直接调用，单返回值被忽略时先落到 `__xl6_member_result_N`，多返回值被忽略时先落到 `XL6_Function_Return_* __xl6_member_result_N`，再按每个返回类型 cleanup；class 单返回成员调用处在显式 class 声明初始化、`auto` class 推断初始化、已有 class 赋值、“临时 class 返回值的普通字段初始化”、“临时 class 返回值作为声明初始化固定实参”或“临时 class 返回值作为表达式语句固定实参”时，会生成语句级临时再进入 typed direct invoke。`maker.make(40).value` 先生成 `__xl6_class_field_recv_N`，读取字段后 destroy；`readPayload(maker.make(60))`、`storePayload(maker.make(70), 70)` 和 `expect(readPayload(maker.make(100)) + 1, 101)` 会先生成 `__xl6_class_arg_N`，调用后 destroy；`sink.consume(explicitPayload, 10)` 会直接生成语句级成员 func 临时并调用；`maker.make(80)` 这类忽略 class 返回值的表达式语句会生成 `__xl6_member_result_N` 并 destroy；`factory.build(80)` 这类忽略多返回成员 func 表达式语句会生成 `__xl6_member_result_N.value0/value1` 并逐项 cleanup。签名不匹配兜底使用 `(ClassType){0}` 形式；其他复杂 typed return 表达式位置仍保留为后续 `RISK-CALL-001` 跟踪项。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:190`、`xl6_cgen_function_value.c:593`、`xl6_cgen_function_value.c:861`、`xl6_cgen_function_member.c:136`、`xl6_cgen_expr_support_base.c:247`、`xl6_cgen_expr_support_base.c:255`、`xl6_cgen_core_context.c:73`、`xl6_cgen_expr.c:10`、`xl6_cgen_stmt_decl.c:178`、`xl6_cgen_stmt_decl.c:223`、`xl6_cgen_stmt_decl.c:315`、`xl6_cgen_stmt_decl.c:443`、`xl6_cgen_stmt_decl.c:495`、`xl6_cgen_stmt_decl_call_args.c:225`、`xl6_cgen_stmt_dispatch.c:137`、`xl6_cgen_stmt_dispatch.c:139`、`xl6_cgen_stmt_dispatch.c:142`、`xl6_cgen_value_assign.c:730` |
| 对应生成的 C 代码 | 常规单返回：`xl6_xvalue_to_i64_release(xl6_function_call_var(obj.cb, 1, xvoCreateInt(1)))`；多返回声明/赋值：`XL6_Function __xl6_member_fn_multi_N = obj.cb; XL6_Function_Return_* r = ((__xl6_member_fn_multi_N.signature == ...) ? ... : ...);`；class 单返回语句上下文：`XL6_Function __xl6_member_fn_stmt_N = maker.make; XL_Class_Payload p = ((__xl6_member_fn_stmt_N.signature == ...) ? ... : (xl6_function_fatal(3), (XL_Class_Payload){0}));`；临时 class 字段初始化：`XL_Class_Payload tmp = {0}; XL6_Function fn = maker.make; tmp = invoke(fn, 40); long long v = tmp.value; xl_class_Payload__destroy(&tmp);`；声明初始化临时 class 实参：`XL_Class_Payload arg = {0}; XL6_Function fn = maker.make; arg = invoke(fn, 60); long long v = readPayload(arg); xl_class_Payload__destroy(&arg);`；表达式语句固定实参：`XL_Class_Payload arg = {0}; XL6_Function fn = maker.make; arg = invoke(fn, 70); storePayload(arg, 70LL); xl_class_Payload__destroy(&arg);`；表达式语句嵌套实参：`XL_Class_Payload arg = {0}; XL6_Function fn = maker.make; arg = invoke(fn, 100); expect_i64((readPayload(arg) + 1LL), 101LL); xl_class_Payload__destroy(&arg);`；无返回成员 func 语句：`XL6_Function fn = sink.consume; ((fn.signature == ...) ? ((void (*)(void*, XL_Class_Payload, long long))fn.invoke)(fn.env, explicitPayload, 10LL) : xl6_function_fatal(3));`；忽略单返回值成员 func 语句：`XL6_Function fn = maker.make; XL_Class_Payload r = invoke(fn, 80); xl_class_Payload__destroy(&r);`；忽略多返回值成员 func 语句：`XL6_Function fn = factory.build; XL6_Function_Return_* r = invoke(fn, 80); xl_class_PairBox__destroy(&r.value0); if (r.value1.release != 0) r.value1.release(r.value1.env);` |
| 中间形态伪代码 | `if erasedCompatible(signature) return convert(callVar(member,args)); else if stmtMemberCallNoReturn(call) tmpFn = member; invokeTyped(tmpFn,args); else if stmtMemberCallIgnoredSingleReturn(call) tmpFn = member; tmpResult = invokeTyped(tmpFn,args); cleanup(tmpResult); else if stmtMemberCallIgnoredMultiReturn(call) tmpFn = member; tmpResultStruct = invokeTyped(tmpFn,args); cleanupEach(tmpResultStruct.valueN); else if statementClassReturn(call) tmpFn = member; invokeTyped(tmpFn,args); else if declFromTempClassField(call.field) tmpObj = invokeTyped(tmpFn,args); target = tmpObj.field; destroy(tmpObj); else if declCallHasTempClassArg(call) tmpArg = invokeTyped(tmpFn,args); target = knownCall(tmpArg); destroy(tmpArg); else if exprStmtCallHasTempClassArg(call) tmpArg = invokeTyped(tmpFn,args); knownCall(tmpArg); destroy(tmpArg); else if exprStmtCallHasNestedTempClassArg(call) tmpArg = invokeTyped(tmpFn,args); replaceExpr(argCall,tmpArg); knownCall(replacedArgs); destroy(tmpArg); else fallbackExpr(call)` |
| 实现同等功能手写的 C 代码 | 简单 callable 场景可直接 `xvalue r = xl6_function_call_var(obj.cb, 1, arg);`；多返回或复杂 typed 场景需要先把成员字段保存为 `XL6_Function fn = obj.cb;`，再按签名检查调用 `fn.invoke`。 |
| 额外依赖的封装代码 | 依赖 function runtime、xvalue 转换 helper、多返回 return struct、class 返回值销毁/保留规则和表达式替换表；常规 erased-compatible 成员调用、多返回成员调用、无返回成员 func 表达式语句、忽略单返回值成员 func 表达式语句、忽略多返回值成员 func 表达式语句、class 单返回声明/赋值路径、临时 class 普通字段初始化路径、声明初始化中的临时 class 固定实参路径、表达式语句中的临时 class 固定实参路径和表达式语句中的嵌套临时 class 实参路径不再使用 GNU 语句表达式，其他复杂 typed return 表达式生命周期路径仍作为 `RISK-CALL-001` 继续跟踪。 |

多临时实参补充：`cgen_member_function_class_return.xl` 现在覆盖 `sumPayload(maker.make(110), maker.make(120))` 与 `expect(sumPayload(maker.make(130), maker.make(140)), 270)`。生成器必须为同一个调用中的两个 class 返回成员函数实参分别生成 `__xl6_class_arg_N`，调用后按后进先出顺序 destroy，且不得把成员函数值内联回 C 实参表达式。

## CGEN-CALL-020 成员 var 函数字段调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.fn(1)`，其中 `fn` 字段类型为 `var` |
| 生成规则描述 | 成员字段类型为 `var` 时，直接把成员表达式作为 `xl6_var_call` 或 `xl6_var_call_kwargs` 的 callable 参数生成，不再在表达式内生成 `xvalue` 语句临时。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:785`、`xl6_cgen_function_value.c:944` |
| 对应生成的 C 代码 | `xl6_var_call(obj.fn, 1, xvoCreateInt(1))` |
| 中间形态伪代码 | `varInvoke(memberExpr, args)` |
| 实现同等功能手写的 C 代码 | `xvalue r = xl6_var_call(obj.fn, 1, arg);` |
| 额外依赖的封装代码 | 依赖 function var runtime；该分支已去掉 GNU 语句表达式。 |

## CGEN-CALL-021 匿名函数构造

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto f = func(int x) as int return x + base end` |
| 生成规则描述 | 匿名函数表达式生成 `xl6_anon_N_make(captures...)`。捕获变量按匿名函数信息表顺序作为参数传入 make 函数；有捕获时 make 函数通过 `xrtMalloc(sizeof(XL6_Closure_N))` 创建 closure env，release 路径在引用计数归零后通过 `xrtFree(__env)` 释放；函数体内部访问捕获变量时生成 `__env->name`。若匿名函数信息未被预扫描收集，兜底诊断为 `anonymous function metadata was not collected`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:61`、`xl6_cgen_function_value.c:806`、`xl6_cgen_function_value.c:813`、`xl6_cgen_expr.c:8`、`xl6_cgen_callable_frame.c:1`、`xl6_cgen_callable_wrappers.c:1` |
| 对应生成的 C 代码 | `xl6_anon_1_make(base)`，内部生成 `XL6_Closure_1* __env = (XL6_Closure_1*)xrtMalloc(sizeof(XL6_Closure_1));` |
| 中间形态伪代码 | `makeClosure(anonId, capturedValues)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要定义 env struct、invoke 函数、retain/release 和 make 函数。 |
| 额外依赖的封装代码 | 需要匿名函数 wrapper 与 closure env 代码。 |

## CGEN-CALL-022 XCall 支持条件

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 任意可导出为 callable 的函数 |
| 生成规则描述 | 只有参数与返回值都能通过 `xvalue` 表示时才生成 XCall wrapper。参数可包含 `...` 或 `***`，但 vararg 必须是当前支持的 array/dict 形式。返回类型也必须支持 var invoke value。 |
| 生成器文件名和行号 | `xl6_cgen_callable_frame.c:79`、`xl6_cgen_callable_frame.c:106`、`xl6_cgen_callable_frame.c:116` |
| 对应生成的 C 代码 | 支持时生成 `static bool fn_xcall(void* env, xrt_call_frame* frame, xrt_call_result* result)`。 |
| 中间形态伪代码 | `supports = all params/returns convertible to xvalue && vararg kind valid` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工判断每个类型是否可从 call frame 解包/装包。 |
| 额外依赖的封装代码 | 需要 XCall wrapper。 |

## CGEN-CALL-023 XCall 参数读取与命名参数冲突检查

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `callable(1, b = 2)` |
| 生成规则描述 | XCall wrapper 从 `xrt_call_frame` 取 `Argc/Argv/Kwargs`。固定参数可来自位置或 kwargs，但同一参数不能同时由位置和 kwargs 提供；否则返回 false。 |
| 生成器文件名和行号 | `xl6_cgen_callable_frame.c:154`、`xl6_cgen_callable_frame.c:173` |
| 对应生成的 C 代码 | `__xl6_arg_0 = (__xl6_argc > 0) ? __xl6_argv[0] : ((__xl6_kwargs != 0 && xvoTableExists(__xl6_kwargs, "a", 0)) ? xvoTableGetValue(__xl6_kwargs, "a", 0) : 0);` |
| 中间形态伪代码 | `arg[i] = positional[i] ?? kwargs[paramName] ?? null` |
| 实现同等功能手写的 C 代码 | 手写 C 需要检查位置参数与命名参数是否重复。 |
| 额外依赖的封装代码 | XCall wrapper 内联生成。 |

## CGEN-CALL-024 XCall arity、额外 kwargs 与类型检查

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `callable(...)` |
| 生成规则描述 | XCall wrapper 检查位置参数数量、必填参数是否存在、无 `***` 时 kwargs 是否只包含固定参数名，并按 xvalue runtime type 常量检查固定参数类型。不满足则返回 false。 |
| 生成器文件名和行号 | `xl6_cgen_callable_frame.c:206`、`xl6_cgen_callable_frame.c:238` |
| 对应生成的 C 代码 | `if (__xl6_argc > fixed) return false; if (__xl6_arg_0 == 0) return false; if (xvoType(__xl6_arg_0) != XL6_XVO_DT_INT) return false;` |
| 中间形态伪代码 | `checkArity(); checkRequired(); checkKwRestEmpty(); checkTypes()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要逐个检查并返回 false。 |
| 额外依赖的封装代码 | XCall wrapper 内联生成。 |

## CGEN-CALL-025 XCall 可变参数打包

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(int a, ... arg)`、`func f(int a, *** kw)` |
| 生成规则描述 | XCall wrapper 中 `...` 创建 array，把固定参数之后的 `Argv` 追加进去；`***` 复制 kwargs 或创建空 table，并删除固定参数名，剩余项作为字典可变参数传给真实函数。调用结束后释放 vararg。 |
| 生成器文件名和行号 | `xl6_cgen_callable_frame.c:254`、`xl6_cgen_callable_frame.c:286` |
| 对应生成的 C 代码 | `__xl6_vararg = xvoCreateArray(); for (i = fixed; i < argc; ++i) xvoArrayAppendValue(__xl6_vararg, argv[i], false);` |
| 中间形态伪代码 | `vararg = array(extraPositionals)` 或 `vararg = kwargs - fixedParamNames` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建容器并处理失败释放。 |
| 额外依赖的封装代码 | XCall wrapper 内联生成。 |

## CGEN-CALL-026 XCall 返回值写入

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f() (int, string)` 被作为 callable 调用 |
| 生成规则描述 | wrapper 调真实函数后，将每个返回值通过 `xl6CgenXValueFromTypedCExpr` 装箱，并调用 `xrtCallResultSetValue(result, index, value, true)`。失败时释放临时值并返回 false。 |
| 生成器文件名和行号 | `xl6_cgen_callable_frame.c:293`、`xl6_cgen_callable_wrappers.c:373` |
| 对应生成的 C 代码 | `xvalue v = xvoCreateInt(ret.value0); if (v == 0 || !xrtCallResultSetValue(result, 0, v, true)) return false;` |
| 中间形态伪代码 | `for each return: result[index] = box(returnValue)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要逐个设置 `xrt_call_result`。 |
| 额外依赖的封装代码 | XCall wrapper 内联生成。 |

## CGEN-CALL-027 function value wrapper make

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) as int f = add` |
| 生成规则描述 | 每个非泛型 func/extern/export 都会生成 `_make` wrapper，构造 `XL6_Function`：写入 signature、sig 对象、env、direct invoke、invoke_var、invoke_xcall、retain/release。 |
| 生成器文件名和行号 | `xl6_cgen_callable_wrappers.c:472`、`xl6_cgen_callable_wrappers.c:489`、`xl6_cgen_callable_wrappers.c:499`、`xl6_cgen_callable_wrappers.c:505`、`xl6_cgen_callable_wrappers.c:772` |
| 对应生成的 C 代码 | `XL6_Function fn; fn.signature = 0x...; fn.sig = &sig; fn.env = 0; fn.invoke = fn_invoke; fn.invoke_var = fn_invoke_var; fn.invoke_xcall = fn_xcall;` |
| 中间形态伪代码 | `return XL6_Function{signature, sig, invoke, dynamicInvokeHooks}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要手工填充 `XL6_Function` 所有字段。 |
| 额外依赖的封装代码 | 需要 `_invoke`、可选 `_invoke_var`、可选 `_xcall`。 |

## CGEN-CALL-028 function runtime 结构

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 使用 `function` 类型时隐式需要 |
| 生成规则描述 | function runtime 定义 `xrt_func_sig` 与 `XL6_Function`。`XL6_Function` 保存签名 id、签名指针、env、直接 invoke、var invoke、xcall invoke、retain/release。 |
| 生成器文件名和行号 | `xl6_cgen_callable_runtime.c:1` |
| 对应生成的 C 代码 | `typedef struct XL6_Function { unsigned long long signature; const xrt_func_sig* sig; void* env; void* invoke; xvalue (*invoke_var)(...); bool (*invoke_xcall)(...); void (*retain)(...); void (*release)(...); } XL6_Function;` |
| 中间形态伪代码 | `Function = {signature, sig, env, invoke, invoke_var, invoke_xcall, retain, release}` |
| 实现同等功能手写的 C 代码 | 手写 C 也应保留这些字段才能同时支持 direct call 与 xcall。 |
| 额外依赖的封装代码 | function runtime。 |

## CGEN-CALL-029 function 与 xvalue callable 互转

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var v = f`、`func(...) g = v` |
| 生成规则描述 | function 转 xvalue 时把 `XL6_Function` 装入 `XL6_Function_Callable_Env`，通过 `xrtMalloc` 创建 env，创建 `xrt_callable` 并生成 `xvoCreateCallable`。xvalue 转 function 时从 xvalue 取 `xrt_callable`，构造带 retain/release/xcall/var invoke 的 `XL6_Function`，release 路径通过 `xrtFree` 释放 env，并释放原 xvalue。 |
| 生成器文件名和行号 | `xl6_cgen_callable_runtime.c:40` |
| 对应生成的 C 代码 | `xvalue v = xl6_function_to_xvalue(fn);`<br>`XL6_Function fn = xl6_function_from_xvalue_release(v);` |
| 中间形态伪代码 | `function <-> xrt_callable <-> xvalue callable` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 callable env，并正确转移引用计数。 |
| 额外依赖的封装代码 | function var runtime。 |

## CGEN-CALL-030 签名 id 与签名对象

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int, string) bool` |
| 生成规则描述 | func 签名用 FNV 风格 hash 计算 id，内容包含参数类型列表和返回类型列表。生成 `_make` wrapper 时输出静态 `xrt_func_param[]`、返回类型数组和 `xrt_func_sig`，并在 make 函数体内填充参数名、参数类型、可选参数 flag、返回类型、`...` / `***` 签名 flag 与签名 id。 |
| 生成器文件名和行号 | `xl6_cgen_abi.c:168`、`xl6_cgen_abi.c:181`、`xl6_cgen_abi.c:333`、`xl6_cgen_abi.c:480`、`xl6_cgen_abi.c:504`、`xl6_cgen_callable_wrappers.c:723` |
| 对应生成的 C 代码 | `static xrt_func_param __xl6_sig_params[2u]; static const struct xrt_type_desc* __xl6_sig_returns[1u]; static xrt_func_sig __xl6_sig; __xl6_sig.Params = __xl6_sig_params; __xl6_sig.ReturnTypes = __xl6_sig_returns;` |
| 中间形态伪代码 | `signatureId = hash(paramTypes, returnTypes); sig.Params = paramMetadata(params); sig.ReturnTypes = typeDescRefs(returns)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要同时保持签名 id、参数元数据和返回类型元数据一致，才能被 xlang 调用点、XCall 和 XAPI 长期复用。 |
| 额外依赖的封装代码 | 签名对象由 wrapper 内静态生成；参数/返回类型指针依赖 `xrtType*` 或生成的 type desc 函数。 |

## 本文件观察到的优化风险

| 编号 | 观察 |
| --- | --- |
| RISK-CALL-001 | 继续收窄：erased-compatible 成员 func 单返回/无返回调用已改为 `xl6_function_call_var/kwargs(memberExpr, ...)`，成员 `var` 调用已改为 `xl6_var_call/kwargs(memberExpr, ...)`，多返回成员 func 声明/赋值已改为语句级 `XL6_Function __xl6_member_fn_multi_N` 临时，非 erased-compatible 无返回值、忽略单返回值和忽略多返回值的成员 func 表达式语句已改为语句级 `XL6_Function __xl6_member_fn_stmt_N` 临时，忽略返回值时生成 `__xl6_member_result_N` 并按返回类型 cleanup，class 单返回成员 func 在显式 class 声明初始化、`auto` class 推断初始化、已有 class 赋值、临时 class 普通字段初始化、声明初始化固定实参、表达式语句固定实参和表达式语句嵌套固定实参中已改为语句级临时，复杂返回兜底改为 `(Type){0}`；函数值调用相关的 `({ XL6_Function ... })` 源码模板已清零，剩余风险转为更复杂 typed return 表达式位置、非固定实参和其他生命周期临时化路径仍需继续 helper 化或语句级化。 |
| RISK-CALL-002 | 已收口：`xrt_func_sig` 现在挂接 `xrt_func_param[]` 和返回类型数组，普通 func、匿名 function、optional adapter、`...` 数组可变参数和 `***` 字典可变参数均输出参数/返回类型元数据；`test.bat` 对 `cgen_function_named_value.c` 检查 `Params`、`ReturnTypes`、optional flag、array/dict vararg type 和 vararg/kwargs flags。 |
| RISK-CALL-003 | 已收口：`function` fallback 返回 string、function/xvalue callable env、erased call argv、闭包 closure env 均改为使用 `xrtMalloc/xrtFree`；`cgen_function_named_value.xl`、`cgen_closure.xl`、`cgen_closure_resource_capture.xl` 的生成检查确认对应生成文件不再输出小写 CRT `malloc/free`。 |
| RISK-CALL-004 | 已收口：function/callable runtime、erased wrapper 参数检查、闭包 env 分配和 direct function invoke fallback 的错误路径不再散落生成裸 `exit(2/3)`，而是集中到 `xl6_function_fatal(code)`；`xl6_function_fatal` 再委托模块级 `xl6_runtime_fatal(code)`，后续异常系统成熟后可只替换模块级入口。 |

## 本轮补充：静态容器 func 值 ABI 收窄

本次继续收窄 `RISK-CALL-001` 中 typed container 返回值路径：

- function value 的 direct `invoke` 原型和调用 cast 改为使用 `xl6CgenValueStorageCType`，`array<int>/list<T>/set<T>/dict<T>` 返回值不再退化为动态 `xvalue`，而是分别落到 `xtarray/xtlist/xtset/xtdict`。
- `xl6CgenTypeSupportsVarInvokeValue` 明确拒绝静态 typed container，避免为 typed container 返回值生成不成立的 erased `invoke_var` 路径。
- direct invoke 的 fallback 对 typed container 只允许 `xl6_function_fatal(3)` 并返回空 typed 句柄，不再生成 `xl6_function_call_var(...)`，从而消除三元表达式里 `xtarray` 与 `xvalue` 混用的 C ABI warning。
- 成员 func 返回 typed container 用于声明初始化或赋值时，生成器先输出语句级 `XL6_Function __xl6_member_fn_stmt_N = memberExpr;`，再生成 `((xtarray (*)(void*))__xl6_member_fn_stmt_N.invoke)(...)` 形态。
- 生成位置主要在 `xl6_cgen_expr_support_base.c:92`、`146`，`xl6_cgen_function_value.c:548`、`623`、`685`、`738`、`884`，`xl6_cgen_callable_wrappers.c:241`、`515`，以及 typed container move 接管分支 `xl6_cgen_stmt_decl.c:879`、`xl6_cgen_value_assign.c:704`。
- 守门用例为 `cgen_typed_container_for_in_returned.xl`。`test.bat` 现在检查 `static xtarray xl6_fnvalue_makeValues_invoke`、`XL6_Function __xl6_member_fn_stmt_`、`((xtarray (*)(void*))__xl6_member_fn_stmt_`，并禁止 `xl6_function_call_var(__xl6_member_fn_stmt_`、`(xvalue (*)(void*))__xl6_member_fn_stmt_` 和 `({ XL6_Function __xl6_member_fn_` 回流。

本轮不新增 `CGEN-*` 规则编号，不改变 834 条唯一规则数量；`RISK-CALL-001` 仍不关闭，因为更复杂的 typed return 表达式位置、非固定实参和其他 function 表达式调用路径仍需继续 helper 化或语句级化。
