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
| 生成规则描述 | 已知函数调用使用参数声明表展开实参：固定参数先按名称匹配，再按位置匹配，缺省参数在调用点直接生成默认表达式。可变参数追加为最后一个参数。 |
| 生成器文件名和行号 | `xl6_cgen_args.c:1345`、`xl6_cgen_args.c:1357`、`xl6_cgen_args.c:1365`、`xl6_cgen_args.c:1373`、`xl6_cgen_expr.c:311` |
| 对应生成的 C 代码 | `f(1LL, 2LL)` |
| 中间形态伪代码 | `for param in fixedParams: emit argByNameOrPositionOrDefault(param)` |
| 实现同等功能手写的 C 代码 | 手写 C 调用者必须自己把默认参数补齐，并按 C 参数顺序书写。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CALL-004 `...` 数组可变参数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(int a, ... arg); f(1, "x", true)` |
| 生成规则描述 | 已知函数声明中最后一个参数为 `array<var>` 可变参数时，固定参数之后的位置实参被装箱为 `xvalue` 并打包到 `xl6_xrt_array_varargs(count, ...)`。 |
| 生成器文件名和行号 | `xl6_cgen_args.c:1250`、`xl6_cgen_args.c:1274`、`xl6_cgen_args.c:1380` |
| 对应生成的 C 代码 | `f(1LL, xl6_xrt_array_varargs(2, xvoCreateText((void*)"x", 0, false), xvoCreateBool(1)))` |
| 中间形态伪代码 | `arg = arrayVarargs(extraPositionalArgs)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 xvalue array 并逐个 append。 |
| 额外依赖的封装代码 | 依赖 `xl6_xrt_array_varargs` 运行时 helper。 |

## CGEN-CALL-005 `***` 字典可变参数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func f(*** arg); f(name = "x")` |
| 生成规则描述 | 已知函数声明中最后一个参数为 `dict<var>` 可变参数时，所有未匹配固定参数的命名实参被打包进 `xl6_xrt_dict_varargs(count, key, value, ...)`。已匹配固定参数的命名实参不会进入字典。 |
| 生成器文件名和行号 | `xl6_cgen_args.c:1303`、`xl6_cgen_args.c:1322`、`xl6_cgen_args.c:1382` |
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
| xlang 写法 | `function(int) int f; f(1)` |
| 生成规则描述 | 静态类型为 `function` 且带签名时，调用点先计算签名 id，并生成三目表达式检查 `f.signature == expectedId`。签名匹配时优先走 `invoke` 直接调用；签名不匹配直接 `exit(3)`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:268`、`xl6_cgen_function_value.c:704`、`xl6_cgen_function_value.c:728`、`xl6_cgen_abi.c:168` |
| 对应生成的 C 代码 | `((f.signature == 0x...ULL) ? ((f.invoke != 0) ? ((long long (*)(void*, long long))f.invoke)(f.env, 1LL) : fallback) : (exit(3), (long long)0))` |
| 中间形态伪代码 | `require signature; if direct invoke exists -> direct; else fallback xvalue call` |
| 实现同等功能手写的 C 代码 | 手写 C 可直接强转 `invoke` 函数指针，但也应先检查签名。 |
| 额外依赖的封装代码 | 需要 `XL6_Function` runtime 与签名 id。 |

## CGEN-CALL-012 function 变量无签名调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function f; f(1, 2)` |
| 生成规则描述 | `function` 类型没有参数/返回签名时，调用退化为 bare function invoke：所有实参装箱为 `xvalue`，调用 `xl6_function_call_var`，结果为 `xvalue`。命名参数在此路径报错。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:524`、`xl6_cgen_function_value.c:536`、`xl6_cgen_function_value.c:704` |
| 对应生成的 C 代码 | `xl6_function_call_var(f, 2, xvoCreateInt(1), xvoCreateInt(2))` |
| 中间形态伪代码 | `function_call_var(fn, boxedArgs...)` |
| 实现同等功能手写的 C 代码 | `xvalue r = xl6_function_call_var(fn, argc, ...);` |
| 额外依赖的封装代码 | 依赖 function var runtime。 |

## CGEN-CALL-013 function 变量直接 invoke

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function(int, int) int add; add(1, 2)` |
| 生成规则描述 | 签名匹配且 `invoke != 0` 时，生成器把 `invoke` 强转为 `returnType (*)(void*, paramTypes...)`，第一个参数传 `fn.env`，后续传固定参数和可变参数容器。多返回值返回结构体类型。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:548`、`xl6_cgen_function_value.c:560`、`xl6_cgen_function_value.c:570`、`xl6_cgen_function_value.c:580` |
| 对应生成的 C 代码 | `((long long (*)(void*, long long, long long))add.invoke)(add.env, 1LL, 2LL)` |
| 中间形态伪代码 | `typedInvoke = cast(fn.invoke); result = typedInvoke(fn.env, args...)` |
| 实现同等功能手写的 C 代码 | 与生成代码类似，但手写者需保证签名正确。 |
| 额外依赖的封装代码 | 多返回值需要 return struct typedef。 |

## CGEN-CALL-014 function 变量 fallback 调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function(int) string f; f(1)`，但 `f.invoke == 0` 且有 `invoke_var/xcall` |
| 生成规则描述 | 如果签名匹配但没有直接 invoke，生成器退回 `xl6_function_call_var` 或 `xl6_function_call_kwargs`，返回 `xvalue` 后按声明返回类型释放转换：int/float/bool/string/time/point/function/type 各自有转换 helper。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:593`、`xl6_cgen_function_value.c:623`、`xl6_cgen_function_value.c:640`、`xl6_cgen_callable_runtime.c:1`、`xl6_cgen_callable_runtime.c:40` |
| 对应生成的 C 代码 | `xl6_xvalue_to_string_release(xl6_function_call_var(f, 1, xvoCreateInt(1)))` |
| 中间形态伪代码 | `xv = dynamicInvoke(fn, boxedArgs); return convertAndRelease<T>(xv)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用动态 invoke，并按目标类型取值和释放。 |
| 额外依赖的封装代码 | 依赖 function var runtime 中的 `xl6_xvalue_to_*_release` helper。 |

## CGEN-CALL-015 var 函数调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var f; f(1, name = "x")` |
| 生成规则描述 | 静态类型为 `var` 的 callee 调用走 `xl6_var_call` 或 `xl6_var_call_kwargs`。位置实参装箱为 `xvalue`，命名实参打包为 kwargs dict。运行时通过 `xvoInvoke` 执行，并返回第一个结果或 null。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:264`、`xl6_cgen_function_value.c:670`、`xl6_cgen_function_value.c:681`、`xl6_cgen_callable_runtime.c:40` |
| 对应生成的 C 代码 | `xl6_var_call_kwargs(f, xl6_xrt_dict_varargs(1, "name", xvoCreateText((void*)"x", 0, false)), 1, xvoCreateInt(1))` |
| 中间形态伪代码 | `xvoInvoke(fn, callFrame{argv, kwargs})` |
| 实现同等功能手写的 C 代码 | 手写 C 需要构造 `xrt_call_frame`、`xrt_call_result` 并释放 argv/kwargs。 |
| 额外依赖的封装代码 | 依赖 function var runtime。 |

## CGEN-CALL-016 function 值由函数名构造

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function(int) int f = add` |
| 生成规则描述 | 当目标类型是 `function`，右值是未绑定变量的名字时，生成器在同名 func/extern/export 中查找唯一签名匹配项，并调用对应 `_make` wrapper 生成 `XL6_Function`。泛型函数不会作为 function 值直接匹配。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:459`、`xl6_cgen_function_value.c:483`、`xl6_cgen_function_value.c:828`、`xl6_cgen_callable_wrappers.c:472` |
| 对应生成的 C 代码 | `XL6_Function f = xl6_function_value_add_make();` |
| 中间形态伪代码 | `selected = findFuncByNameAndFunctionType(name, targetType); emit selected_make()` |
| 实现同等功能手写的 C 代码 | `XL6_Function f = make_add_function();` |
| 额外依赖的封装代码 | 需要为目标函数生成 function value wrapper。 |

## CGEN-CALL-017 function 类型匹配可选参数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func add(int a, int b = 1) int; function(int) int f = add` |
| 生成规则描述 | function 类型匹配函数参数时，会考虑 required/fixed 参数数量和可选参数。若 function 类型参数个数在 required 和 fixed 之间，可通过 optional adapter 生成较短签名的 function 值。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:391`、`xl6_cgen_callable_wrappers.c:747`、`xl6_cgen_callable_wrappers.c:552`、`xl6_cgen_callable_wrappers.c:707` |
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

## CGEN-CALL-019 成员 function 字段调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.cb(1)`，其中 `cb` 是 function 字段 |
| 生成规则描述 | 成员字段类型推断为 `function` 时，先用 GNU 语句表达式把成员表达式保存到 `XL6_Function` 临时变量，再调用 `xl6CgenFunctionInvoke`。这样复杂成员表达式不会被重复求值。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:189`、`xl6_cgen_function_value.c:764`、`xl6_cgen_function_value.c:777` |
| 对应生成的 C 代码 | `({ XL6_Function fn = obj.cb; ((fn.signature == ...) ? ... : ...); })` |
| 中间形态伪代码 | `tmp = member; invokeFunction(tmp, args)` |
| 实现同等功能手写的 C 代码 | `XL6_Function fn = obj.cb; call(fn, args);` |
| 额外依赖的封装代码 | 依赖 function runtime；使用 GNU 语句表达式。 |

## CGEN-CALL-020 成员 var 函数字段调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.fn(1)`，其中 `fn` 字段类型为 `var` |
| 生成规则描述 | 成员字段类型为 `var` 时，生成语句表达式保存 `xvalue` 临时变量，再调用 `xl6CgenVarFunctionInvoke`。 |
| 生成器文件名和行号 | `xl6_cgen_function_value.c:789`、`xl6_cgen_function_value.c:795` |
| 对应生成的 C 代码 | `({ xvalue fn = obj.fn; xl6_var_call(fn, 1, xvoCreateInt(1)); })` |
| 中间形态伪代码 | `tmp = member; varInvoke(tmp, args)` |
| 实现同等功能手写的 C 代码 | `xvalue fn = obj.fn; xvalue r = xl6_var_call(fn, 1, arg);` |
| 额外依赖的封装代码 | 依赖 function var runtime；使用 GNU 语句表达式。 |

## CGEN-CALL-021 匿名函数构造

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto f = func(int x) int return x + base end` |
| 生成规则描述 | 匿名函数表达式生成 `xl6_anon_N_make(captures...)`。捕获变量按匿名函数信息表顺序作为参数传入 make 函数；函数体内部访问捕获变量时生成 `__env->name`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:61`、`xl6_cgen_function_value.c:806`、`xl6_cgen_expr.c:8`、`xl6_cgen_callable_frame.c:1`、`xl6_cgen_callable_wrappers.c:1` |
| 对应生成的 C 代码 | `xl6_anon_1_make(base)` |
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
| xlang 写法 | `function(int) int f = add` |
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
| xlang 写法 | `var v = f`、`function(...) g = v` |
| 生成规则描述 | function 转 xvalue 时把 `XL6_Function` 装入 `XL6_Function_Callable_Env`，创建 `xrt_callable` 并生成 `xvoCreateCallable`。xvalue 转 function 时从 xvalue 取 `xrt_callable`，构造带 retain/release/xcall/var invoke 的 `XL6_Function`，并释放原 xvalue。 |
| 生成器文件名和行号 | `xl6_cgen_callable_runtime.c:40` |
| 对应生成的 C 代码 | `xvalue v = xl6_function_to_xvalue(fn);`<br>`XL6_Function fn = xl6_function_from_xvalue_release(v);` |
| 中间形态伪代码 | `function <-> xrt_callable <-> xvalue callable` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 callable env，并正确转移引用计数。 |
| 额外依赖的封装代码 | function var runtime。 |

## CGEN-CALL-030 签名 id 与签名对象

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `function(int, string) bool` |
| 生成规则描述 | function 签名用 FNV 风格 hash 计算 id，内容包含参数类型列表和返回类型列表。生成 `_make` wrapper 时输出静态 `xrt_func_sig`，但当前 `Params/ReturnTypes` 指针为 0，只记录计数与 id。 |
| 生成器文件名和行号 | `xl6_cgen_abi.c:168`、`xl6_cgen_abi.c:181`、`xl6_cgen_abi.c:251`、`xl6_cgen_abi.c:261` |
| 对应生成的 C 代码 | `static const xrt_func_sig sig = { 0x...ULL, "add", 3u, 2u, 0, 1u, 0, 0u };` |
| 中间形态伪代码 | `signatureId = hash("function(" + paramTypes + ")->(" + returnTypes + ")")` |
| 实现同等功能手写的 C 代码 | 手写 C 需要使用同样签名 id，才能被 xlang 调用点接受。 |
| 额外依赖的封装代码 | 签名对象由 wrapper 内静态生成。 |

## 本文件观察到的优化风险

| 编号 | 观察 |
| --- | --- |
| RISK-CALL-001 | function 调用和成员 function 调用大量使用 GNU 语句表达式，未来若强化 MSVC 直编兼容，需要把表达式降级为语句临时或 helper。 |
| RISK-CALL-002 | `xrt_func_sig` 当前只填 signature id、名称、参数/返回数量，`Params/ReturnTypes` 为 0；作为长期接口元数据还不够完整。 |
| RISK-CALL-003 | `function` fallback 返回 string 时使用 `malloc` 拷贝文本，和 XRT 统一内存管理原则存在偏差，后续应评估改为 `xrtMalloc/xrtFree` 或 XRT 字符串对象。 |
| RISK-CALL-004 | direct function invoke 的签名检查是运行期三目表达式，错误路径 `exit(3)` 直接终止；异常系统成熟后可替换为统一运行时错误。 |
