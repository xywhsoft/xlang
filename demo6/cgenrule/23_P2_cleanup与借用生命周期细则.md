# P2 cleanup 与借用生命周期细则

本文细化 `xl6_cgen_expr_support_base.c`、`xl6_cgen_expr_cleanup.c` 与 `xl6_cgen_loop_target.c`。这些分片不直接承担完整语法节点输出，但会决定生成 C 代码中的 retain/release、自动析构、cleanup 栈、break/continue 清理边界和借用判定，是当前 cgen 稳定性的核心辅助层之一。

## CGEN-CLEAN-001 C 符号输出时去掉字面量引号

| 项目 | 内容 |
|---|---|
| xlang 写法 | `custom` 的 `destroy` 映射到 `"xxx_destroy"` 或裸符号 |
| 生成规则描述 | 输出 C 符号时，若文本是以单引号或双引号包住的字面量，则去掉首尾引号后原样输出；NULL 文本输出 `_`。该规则主要影响 custom 特殊函数调用的符号形态。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:3` |
| 对应生成的 C 代码 | `xxx_destroy(obj.handle);` |
| 中间形态伪代码 | `if text is quoted: emit inner text; else emit text or "_"` |
| 实现同等功能手写的 C 代码 | 手写 C 直接写函数名。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CLEAN-002 custom/class 析构能力判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `custom Foo ... destroy ... end`、`class Bar ... destroy ... end` |
| 生成规则描述 | custom 通过查找特殊成员 `destroy` 判断是否需要释放；class 通过查找 class 特殊成员 `destroy` 判断直接析构能力。该判定会被声明 cleanup 和 class 递归析构规则复用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:23` |
| 对应生成的 C 代码 | `Foo_destroy(x.handle);` 或 `xl6_Bar_destroy(&b);` |
| 中间形态伪代码 | `hasDestroy = findSpecial(node, "destroy") != null` |
| 实现同等功能手写的 C 代码 | 手写 C 由程序员显式调用析构函数。 |
| 额外依赖的封装代码 | class/custom 特殊成员查找 helper。 |

## CGEN-CLEAN-003 func 类型识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func a`、`func(int) as string cb`、`array<func(int)>` |
| 生成规则描述 | 类型名为 `function` 时被识别为 func 类型；无子节点的 `function` 是裸 function；递归扫描子节点可判断嵌套类型中是否包含 function。该信息决定函数值字段、参数和返回值是否需要 retain/release。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:33` |
| 对应生成的 C 代码 | `XL6_FuncValue cb;` 并在作用域结束释放 env。 |
| 中间形态伪代码 | `isFunction(type) || any child contains function` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行维护函数指针和闭包环境生命周期。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-CLEAN-004 xvalue 家族类型识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var v`、`array a`、`array<var> a`、`dict<var> d` |
| 生成规则描述 | `var` 以及裸 `array/list/set/dict` 或 `<var>` 容器都按 xvalue 家族处理。它们不是 typed container storage，而是以 xvalue 对象表示，需要引用计数释放。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:58` |
| 对应生成的 C 代码 | `xvalue* v = ...; ... xvoUnref(v);` |
| 中间形态伪代码 | `var || array || array<var> || list || list<var> || set || set<var> || dict || dict<var>` |
| 实现同等功能手写的 C 代码 | 手写 C 需要统一动态值结构并维护引用计数。 |
| 额外依赖的封装代码 | xvalue runtime。 |

## CGEN-CLEAN-005 xvalue 运行时类型常量映射

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var v = intValue`、`var v = array[]` |
| 生成规则描述 | 先替换泛型和解析别名，再把 `bool/int/float/string/time/point/array/list/set/dict` 映射到 `XL6_XVO_DT_*` 常量；不支持的类型返回 NULL。该结果决定静态值是否能进入 var invoke 参数/返回值打包。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:92` |
| 对应生成的 C 代码 | `xl6_xvalue_from_int(...)` 或按 `XL6_XVO_DT_INT` 分支打包。 |
| 中间形态伪代码 | `resolve(type) -> runtime data type const or null` |
| 实现同等功能手写的 C 代码 | 手写 C 自行选择装箱函数。 |
| 额外依赖的封装代码 | xvalue 装箱 helper。 |

## CGEN-CLEAN-006 var invoke 可打包值类型判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f = fn; var r = f(1, "x")` |
| 生成规则描述 | 一个类型可用于 var invoke 的条件是：具体化后为 `var`，或能映射到 xvalue 运行时类型常量。class/custom/union 等不可自动进入该路径。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:134` |
| 对应生成的 C 代码 | `xl6_func_invoke_var(..., xvalue_arg_array, ...)` |
| 中间形态伪代码 | `return type == var || runtimeTypeConst(type) != null` |
| 实现同等功能手写的 C 代码 | 手写 C 需要手动构造调用帧和返回值。 |
| 额外依赖的封装代码 | var invoke wrapper。 |

## CGEN-CLEAN-007 匿名函数是否支持 var invoke

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f = func(int a, ... ) as string ... end` |
| 生成规则描述 | 匿名函数只有在固定参数都可打包为 xvalue、最多一个返回值且返回值可打包、额外参数只能是数组可变参数时，才生成 var invoke 路径；多返回值和字典可变参数不进入该路径。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:149` |
| 对应生成的 C 代码 | 生成匿名函数的 var-invoke adapter。 |
| 中间形态伪代码 | `reject dict vararg; reject multi-return; require every param/return supports xvalue` |
| 实现同等功能手写的 C 代码 | 手写 C 需要另外写一个动态调用适配函数。 |
| 额外依赖的封装代码 | 匿名函数信息表和 var-invoke adapter。 |

## CGEN-CLEAN-008 class 直接字段查找与匿名 union 字段展开

| 项目 | 内容 |
|---|---|
| xlang 写法 | `a.field`，字段可能来自 class 内匿名 `union` |
| 生成规则描述 | 在 class body 中查找同名 `XL6_AST_FIELD`；若成员是匿名 union，则递归查找 union 字段。该规则支持匿名 union 字段像 class 直接字段一样参与类型推断。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:186` |
| 对应生成的 C 代码 | `a.field` 或 `a.inlineUnionField` |
| 中间形态伪代码 | `for member in class.body: field match; anonymous union -> find union field` |
| 实现同等功能手写的 C 代码 | 手写 C 按结构体/union 布局访问字段。 |
| 额外依赖的封装代码 | union 字段查找 helper。 |

## CGEN-CLEAN-009 class 继承字段查找

| 项目 | 内容 |
|---|---|
| xlang 写法 | `child.baseField` |
| 生成规则描述 | 先查找当前 class 直接字段；找不到时，如果 class 有基类名称，则递归到基类 class 查找。该路径决定成员类型推断和 function 字段生命周期扫描能否跨继承链工作。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:213` |
| 对应生成的 C 代码 | `child.base.baseField` 或后续成员访问规则生成的等效路径。 |
| 中间形态伪代码 | `field = direct(class); if null and base exists: find(base)` |
| 实现同等功能手写的 C 代码 | 手写 C 显式沿结构体首字段基类访问。 |
| 额外依赖的封装代码 | class 查找 helper。 |

## CGEN-CLEAN-010 成员字段类型推断

| 项目 | 内容 |
|---|---|
| xlang 写法 | `a.b`、`a.b.c`、`arr[i].field`、`self.field` |
| 生成规则描述 | 成员字段类型推断支持 receiver 为 name/member/index。嵌套 member/index 先推断 receiver 类型；union receiver 查 union 字段，class receiver 查继承字段；泛型 class 实例会临时设置当前泛型参数和实参，把字段类型中的 `T` 规范化为具体类型。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:232` |
| 对应生成的 C 代码 | `XL6_Type tmp = a.b;` 中 `tmp` 的 C 类型由该推断结果决定。 |
| 中间形态伪代码 | `receiverType = infer(receiver); if union field clone; if class field clone with generic ctx` |
| 实现同等功能手写的 C 代码 | 手写 C 由字段声明直接决定类型。 |
| 额外依赖的封装代码 | 类型推断、泛型上下文、class/union 查找 helper。 |

## CGEN-CLEAN-011 class 是否需要自动析构

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class A ... destroy ... end`、或 class 字段中包含需清理资源 |
| 生成规则描述 | class 需要析构的条件包括：自身有 `destroy`；字段是 function；字段 custom 有 destroy；字段 class 递归需要析构；基类递归需要析构。只要任一条件满足，局部变量和返回路径都可能生成 class cleanup。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:323` |
| 对应生成的 C 代码 | `xl6_A_destroy(&a);` |
| 中间形态伪代码 | `has own destroy || any field needs cleanup || base needs cleanup` |
| 实现同等功能手写的 C 代码 | 手写 C 需要为每个含资源字段的结构体手写析构流程。 |
| 额外依赖的封装代码 | class/custom 查找和 destroy helper。 |

## CGEN-CLEAN-012 function 路径释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f = closure`、`obj.cb = closure` |
| 生成规则描述 | 对任意 C 路径表达式释放 function value：仅当 `release != 0 && env != 0` 时调用 `release(env)`，随后把 `env` 和 `release` 清零，防止重复释放。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:373` |
| 对应生成的 C 代码 | `if (f.release != 0 && f.env != 0) { f.release(f.env); f.env = 0; f.release = 0; }` |
| 中间形态伪代码 | `if path.release && path.env: release(env); clear env/release` |
| 实现同等功能手写的 C 代码 | 手写 C 必须显式释放闭包环境并避免二次释放。 |
| 额外依赖的封装代码 | function value 约定。 |

## CGEN-CLEAN-013 function 路径保留

| 项目 | 内容 |
|---|---|
| xlang 写法 | `b.cb = a.cb`、返回借用来的 function 字段 |
| 生成规则描述 | 对任意 C 路径表达式保留 function value：仅当 `retain != 0 && env != 0` 时调用 `retain(env)`。该规则用于借用表达式被保存到更长生命周期位置的场景。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:392` |
| 对应生成的 C 代码 | `if (f.retain != 0 && f.env != 0) { f.retain(f.env); }` |
| 中间形态伪代码 | `if path.retain && path.env: retain(env)` |
| 实现同等功能手写的 C 代码 | 手写 C 对闭包环境增加引用计数。 |
| 额外依赖的封装代码 | function value 约定。 |

## CGEN-CLEAN-014 class 中 function 字段递归 retain/release

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class A` 中包含 `func cb`，或嵌套 class 字段包含 function |
| 生成规则描述 | 从基类到当前 class body 递归扫描字段。function 字段按路径 retain/release；嵌套 class 字段继续递归处理。该规则确保 class 值复制、返回或释放时内部闭包环境生命周期正确。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:407` |
| 对应生成的 C 代码 | `if (a.cb.release != 0 && a.cb.env != 0) { a.cb.release(a.cb.env); ... }` |
| 中间形态伪代码 | `visit base; for field: if function ref(path); else if class visit(path.field)` |
| 实现同等功能手写的 C 代码 | 手写 C 必须为每个 function 字段递归写 retain/release。 |
| 额外依赖的封装代码 | function value retain/release helper。 |

## CGEN-CLEAN-015 由类型节点定位 class 并判断 function 字段清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | `T value`，其中 `T` 可能是别名或泛型实参 |
| 生成规则描述 | 先做泛型替换和类型别名解析，再查找 class 定义。若解析得到 class 且该 class 需要析构，则该类型需要 function 字段 cleanup。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:469` |
| 对应生成的 C 代码 | 影响是否生成 `xl6_A_destroy(&a);` 或递归释放字段。 |
| 中间形态伪代码 | `class = findClass(resolveAlias(substituteGeneric(type))); needs = classNeedsDestroy(class)` |
| 实现同等功能手写的 C 代码 | 手写 C 直接知道结构体类型并调用对应析构函数。 |
| 额外依赖的封装代码 | 泛型替换、别名解析和 class 查找 helper。 |

## CGEN-CLEAN-016 class 借用表达式判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `return obj`、`a = other.field`，目标是含 function 字段的 class |
| 生成规则描述 | 只有 name/member/index/await 表达式可能被视为借用。若表达式推断类型与目标 class 类型结构相等，或同名且两边都是 class，则认为是借用；字面量、构造临时值等不按借用处理。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:513` |
| 对应生成的 C 代码 | 借用为真时会额外生成内部 function 字段 retain。 |
| 中间形态伪代码 | `if expr kind borrowable and inferred type equals target class type: borrowed` |
| 实现同等功能手写的 C 代码 | 手写 C 需要知道结构体复制是否共享闭包资源。 |
| 额外依赖的封装代码 | 表达式类型推断和 class 查找 helper。 |

## CGEN-CLEAN-017 借用 class 表达式的 function 字段 retain

| 项目 | 内容 |
|---|---|
| xlang 写法 | `A b = a`，其中 `A` 内部含 function 字段 |
| 生成规则描述 | 若表达式被判定为 class 借用，则对目标路径递归 retain class 中的 function 字段。这样结构体值复制后，闭包环境不会因原变量释放而悬空。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:543` |
| 对应生成的 C 代码 | `if (b.cb.retain != 0 && b.cb.env != 0) { b.cb.retain(b.cb.env); }` |
| 中间形态伪代码 | `if isClassBorrow(expr,type): retainFunctionFields(valuePath)` |
| 实现同等功能手写的 C 代码 | 手写 C 在结构体复制后对内部引用字段加引用。 |
| 额外依赖的封装代码 | class function 字段 retain helper。 |

## CGEN-CLEAN-018 返回值路径释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | `return f`、`return obj` |
| 生成规则描述 | 返回值路径如果是 func 类型，则释放 function path；如果是需要 class function 字段 cleanup 的 class 类型，则释放 class 内 function 字段。该规则用于返回临时保存路径的异常或覆盖释放场景。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:554` |
| 对应生成的 C 代码 | `xl6_result.release(xl6_result.env);` 或 `xl6_A_destroy(&xl6_result);` 的相关字段释放。 |
| 中间形态伪代码 | `if function: release path; else if class needs cleanup: release class function fields` |
| 实现同等功能手写的 C 代码 | 手写 C 根据返回值类型补释放逻辑。 |
| 额外依赖的封装代码 | function/class cleanup helper。 |

## CGEN-CLEAN-019 cleanup 栈项注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意需要作用域退出自动释放的局部声明 |
| 生成规则描述 | cleanup 栈容量从 16 起按 2 倍扩容；变量名先转成 C 标识符再保存；栈项记录 cleanup kind、类型节点和可选 class 类型节点。扩容失败报告 `out of memory while growing cgen cleanup stack`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:565` |
| 对应生成的 C 代码 | 不直接输出；后续作用域结束时按栈项生成释放代码。 |
| 中间形态伪代码 | `grow cleanups; push {cName, kind, typeNode, classTypeNode}` |
| 实现同等功能手写的 C 代码 | 手写 C 不需要 cleanup 栈，但要在每个出口手写释放。 |
| 额外依赖的封装代码 | cgen cleanup 栈。 |

## CGEN-CLEAN-020 custom cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `MyCustom x = ...`，且 custom 定义了 `destroy` |
| 生成规则描述 | 只有 custom 定义存在且有 `destroy` 特殊成员时，才注册 `XL6_CGEN_CLEANUP_CUSTOM`。无析构 custom 不产生自动释放代码。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:592` |
| 对应生成的 C 代码 | `if (x.handle != 0) { my_destroy(x.handle); x.handle = 0; }` |
| 中间形态伪代码 | `if customHasDestroy: add cleanup CUSTOM` |
| 实现同等功能手写的 C 代码 | 手写 C 只在拥有资源所有权时调用 destroy。 |
| 额外依赖的封装代码 | custom destroy 映射。 |

## CGEN-CLEAN-021 class cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `A a`，其中 `A` 有 destroy、基类/字段需要析构或 function 字段 |
| 生成规则描述 | 只有 class 递归判定为需要析构时，才注册 `XL6_CGEN_CLEANUP_CLASS`；同时保存 classTypeNode，以便泛型 class 生成正确的实例化 destroy 符号。普通或泛型 class 值参数建立 owned copy 后也进入同一 cleanup 路径，native-backed class 句柄不在这里做值复制。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:36`、`demo6/src/cgen/xl6_cgen_decl_methods.c:41` |
| 对应生成的 C 代码 | `xl6_A_destroy(&a);` 或泛型 class 的实例化 destroy。 |
| 中间形态伪代码 | `if classNeedsDestroy(class): add cleanup CLASS with classTypeNode` |
| 实现同等功能手写的 C 代码 | 手写 C 按类型显式调用析构函数。 |
| 额外依赖的封装代码 | class destroy helper。 |

## CGEN-CLEAN-022 function cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f = ...` |
| 生成规则描述 | 变量类型为 function 时注册 `XL6_CGEN_CLEANUP_FUNCTION`。非 func 类型不会进入该路径。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:608` |
| 对应生成的 C 代码 | `if (f.release != 0 && f.env != 0) { f.release(f.env); f.env = 0; f.release = 0; }` |
| 中间形态伪代码 | `if isFunctionType(type): add cleanup FUNCTION` |
| 实现同等功能手写的 C 代码 | 手写 C 显式释放闭包环境。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-CLEAN-023 future cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `future<int> f = asyncCall()`、`task<int> t = ...` |
| 生成规则描述 | 类型是 future-like 时注册 `XL6_CGEN_CLEANUP_FUTURE`。这类对象按引用句柄释放，不按 xvalue 或 typed container 释放。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:616` |
| 对应生成的 C 代码 | `if (f != 0) { xFutureRelease(f); f = 0; }` |
| 中间形态伪代码 | `if isFutureLike(type): add cleanup FUTURE` |
| 实现同等功能手写的 C 代码 | 手写 C 显式释放 future 句柄。 |
| 额外依赖的封装代码 | async/future runtime。 |

## CGEN-CLEAN-024 channel cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `channel<int> ch = channel<int>()` |
| 生成规则描述 | 类型是 channel 时注册 `XL6_CGEN_CLEANUP_CHANNEL`。channel 使用独立销毁函数，不参与 typed container cleanup。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:624` |
| 对应生成的 C 代码 | `if (ch != 0) { xrtChannelDestroy(ch); ch = 0; }` |
| 中间形态伪代码 | `if isChannel(type): add cleanup CHANNEL` |
| 实现同等功能手写的 C 代码 | 手写 C 在作用域退出时销毁 channel。 |
| 额外依赖的封装代码 | channel runtime helper。 |

## CGEN-CLEAN-025 thread cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `thread th = thread(funcRef)` |
| 生成规则描述 | 类型是 thread 时注册 `XL6_CGEN_CLEANUP_THREAD`。thread 使用 `xl6_thread_release`，因为它封装了线程句柄和可能的函数环境。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:632` |
| 对应生成的 C 代码 | `if (th != 0) { xl6_thread_release(th); th = 0; }` |
| 中间形态伪代码 | `if isThread(type): add cleanup THREAD` |
| 实现同等功能手写的 C 代码 | 手写 C 要显式释放线程句柄/任务对象。 |
| 额外依赖的封装代码 | thread runtime helper。 |

## CGEN-CLEAN-026 generator cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `generator<int> g = make()` |
| 生成规则描述 | 类型是 generator 时注册 `XL6_CGEN_CLEANUP_GENERATOR`。generator frame/worker 生命周期由专用销毁函数收束。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:640` |
| 对应生成的 C 代码 | `if (g != 0) { xl6_generator_destroy(g); g = 0; }` |
| 中间形态伪代码 | `if isGenerator(type): add cleanup GENERATOR` |
| 实现同等功能手写的 C 代码 | 手写 C 需要释放生成器状态机/frame。 |
| 额外依赖的封装代码 | generator runtime。 |

## CGEN-CLEAN-027 sync handle cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `mutex m = mutex()`、`sem s = sem()`、`cond c = cond()`、`rwlock r = rwlock()` |
| 生成规则描述 | 类型是同步句柄时注册 `XL6_CGEN_CLEANUP_SYNC`。具体销毁函数在输出 cleanup 时按类型名选择。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:648` |
| 对应生成的 C 代码 | `if (m != 0) { xrtMutexDestroy(m); m = 0; }` |
| 中间形态伪代码 | `if isSyncHandle(type): add cleanup SYNC` |
| 实现同等功能手写的 C 代码 | 手写 C 按 mutex/sem/cond/rwlock 类型显式销毁。 |
| 额外依赖的封装代码 | 无额外封装，按同步类型直接选择销毁函数。 |

## CGEN-CLEAN-028 xvalue cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var v`、`array a`、`dict d` |
| 生成规则描述 | 类型属于 xvalue 家族时注册 `XL6_CGEN_CLEANUP_XVALUE`。这与 typed container storage 区分开，释放时调用 xvalue 引用计数接口。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:656` |
| 对应生成的 C 代码 | `if (v != 0) { xvoUnref(v); v = 0; }` |
| 中间形态伪代码 | `if isXValueType(type): add cleanup XVALUE` |
| 实现同等功能手写的 C 代码 | 手写 C 对动态值调用 unref。 |
| 额外依赖的封装代码 | xvalue runtime。 |

## CGEN-CLEAN-029 typed container cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `array<int> a`、`list<string> l`、`set<int> s`、`dict<int> d` |
| 生成规则描述 | 类型是 typed container storage 时注册 `XL6_CGEN_CLEANUP_TYPED_CONTAINER`。裸容器和 `<var>` 容器不走该路径，而是 xvalue cleanup。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:664` |
| 对应生成的 C 代码 | `if (a != 0) { xrtTypedArrayDestroy(a); a = 0; }` |
| 中间形态伪代码 | `if isTypedContainer(type): add cleanup TYPED_CONTAINER` |
| 实现同等功能手写的 C 代码 | 手写 C 对对应 typed container 调用 destroy。 |
| 额外依赖的封装代码 | typed container storage helper。 |

## CGEN-CLEAN-030 error cleanup 注册

| 项目 | 内容 |
|---|---|
| xlang 写法 | `catch err` 或 try/catch 内部错误对象 |
| 生成规则描述 | error cleanup 无条件注册指定名称为 `XL6_CGEN_CLEANUP_ERROR`。它不依赖类型判定，用于异常模型中的错误对象释放。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:672` |
| 对应生成的 C 代码 | `if (err != 0) { xl6_error_free(err); err = 0; }` |
| 中间形态伪代码 | `add cleanup ERROR` |
| 实现同等功能手写的 C 代码 | 手写 C 在 catch 退出时释放错误对象。 |
| 额外依赖的封装代码 | error runtime。 |

## CGEN-CLEAN-031 cleanup 元数据释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | 作用域结束、生成器/函数生成结束 |
| 生成规则描述 | `xl6CgenFreeCleanupsSince` 释放 mark 之后 cleanup 项保存的变量名并回退计数；`xl6CgenFreeCleanups` 释放整块 cleanup 数组并清空容量。它只释放 cgen 元数据，不生成目标 C 代码。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:677` |
| 对应生成的 C 代码 | 不直接输出。 |
| 中间形态伪代码 | `for i in mark..count: free(cleanups[i].name); count = mark` |
| 实现同等功能手写的 C 代码 | 手写 C 不存在 cgen cleanup 元数据。 |
| 额外依赖的封装代码 | cgen cleanup 栈。 |

## CGEN-CLEAN-032 error cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `try ... catch err ... end` |
| 生成规则描述 | cleanup kind 为 error 时输出空指针保护，调用 `xl6_error_free` 后把变量清零。该分支优先于类型节点判定，因此 typeNode 即使为空也能释放 error。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:701` |
| 对应生成的 C 代码 | `if (err != 0) { xl6_error_free(err); err = 0; }` |
| 中间形态伪代码 | `if kind ERROR: emit free error and clear` |
| 实现同等功能手写的 C 代码 | 手写 C 在每个异常出口释放错误对象。 |
| 额外依赖的封装代码 | error runtime。 |

## CGEN-CLEAN-033 动态遍历 snapshot cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 动态 list/dict 遍历内部 snapshot |
| 生成规则描述 | cleanup kind 为 iter snapshot 时输出空指针保护，调用 `xl6_xrt_iter_snapshot_free` 并把变量清零。该分支用于动态 list/dict 遍历产生的稳定快照；动态 set 当前按位置下标遍历，不走 snapshot cleanup。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:735`、`demo6/src/cgen/xl6_cgen_expr_support_base.c:740` |
| 对应生成的 C 代码 | `if (snapshot != 0) { xl6_xrt_iter_snapshot_free(snapshot); snapshot = 0; }` |
| 中间形态伪代码 | `if kind ITER_SNAPSHOT: free snapshot and clear` |
| 实现同等功能手写的 C 代码 | 手写 C 在遍历结束时释放 snapshot 数组、key 拷贝和 retained value。 |
| 额外依赖的封装代码 | `XL6_XrtIterSnapshot` 与动态容器遍历 helper。 |

## CGEN-CLEAN-034 future cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `future<T> f` 或 `task<T> t` 作用域结束 |
| 生成规则描述 | cleanup kind 为 future 时输出空指针保护，调用 `xFutureRelease` 并把句柄清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:735` |
| 对应生成的 C 代码 | `if (f != 0) { xFutureRelease(f); f = 0; }` |
| 中间形态伪代码 | `if future != 0: release; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 对 future 句柄 release。 |
| 额外依赖的封装代码 | future runtime。 |

## CGEN-CLEAN-035 channel cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `channel<T> ch` 作用域结束 |
| 生成规则描述 | cleanup kind 为 channel 时输出空指针保护，调用 channel destroy 并清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:748` |
| 对应生成的 C 代码 | `if (ch != 0) { xrtChannelDestroy(ch); ch = 0; }` |
| 中间形态伪代码 | `if channel != 0: destroy; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 在作用域退出时销毁 channel。 |
| 额外依赖的封装代码 | channel runtime helper。 |

## CGEN-CLEAN-036 thread cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `thread th` 作用域结束 |
| 生成规则描述 | cleanup kind 为 thread 时输出空指针保护，调用 `xl6_thread_release` 并清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:761` |
| 对应生成的 C 代码 | `if (th != 0) { xl6_thread_release(th); th = 0; }` |
| 中间形态伪代码 | `if thread != 0: release; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 释放线程对象或 join/release 约定。 |
| 额外依赖的封装代码 | thread runtime helper。 |

## CGEN-CLEAN-037 generator cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `generator<T> g` 作用域结束 |
| 生成规则描述 | cleanup kind 为 generator 时输出空指针保护，调用 `xl6_generator_destroy` 并清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_support_base.c:774` |
| 对应生成的 C 代码 | `if (g != 0) { xl6_generator_destroy(g); g = 0; }` |
| 中间形态伪代码 | `if generator != 0: destroy; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 释放生成器 frame/状态机。 |
| 额外依赖的封装代码 | generator runtime。 |

## CGEN-CLEAN-038 sync cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `mutex/sem/cond/rwlock` 作用域结束 |
| 生成规则描述 | cleanup kind 为 sync 时，按类型名选择 `xrtMutexDestroy`、`xrtSemDestroy`、`xrtCondDestroy` 或 `xrtRWLockDestroy`；未知同步类型不输出释放。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:238` |
| 对应生成的 C 代码 | `if (m != 0) { xrtMutexDestroy(m); m = 0; }` |
| 中间形态伪代码 | `destroy = map sync type name; if destroy: emit destroy(handle); clear` |
| 实现同等功能手写的 C 代码 | 手写 C 根据同步原语类型调用对应销毁函数。 |
| 额外依赖的封装代码 | 无额外封装，直接选择对应同步销毁函数。 |

## CGEN-CLEAN-039 custom cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `custom` 局部变量作用域结束 |
| 生成规则描述 | cleanup kind 为 custom 时再次查找 `destroy` 特殊成员；若变量 `.handle != 0`，调用映射 destroy 函数并将 handle 清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:266` |
| 对应生成的 C 代码 | `if (x.handle != 0) { xxx_destroy(x.handle); x.handle = 0; }` |
| 中间形态伪代码 | `destroy = find custom destroy; if handle: destroy(handle); clear handle` |
| 实现同等功能手写的 C 代码 | 手写 C 调用 C 库 destroy 函数并清空句柄。 |
| 额外依赖的封装代码 | custom destroy 声明映射。 |

## CGEN-CLEAN-040 function cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f` 作用域结束 |
| 生成规则描述 | cleanup kind 为 function 时，与 function path release 相同：仅当 `release` 和 `env` 均非空时释放环境，并清空 `env/release`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:286` |
| 对应生成的 C 代码 | `if (f.release != 0 && f.env != 0) { f.release(f.env); f.env = 0; f.release = 0; }` |
| 中间形态伪代码 | `if cleanup FUNCTION: release function env and clear` |
| 实现同等功能手写的 C 代码 | 手写 C 调用闭包 release 回调。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-CLEAN-041 xvalue cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var v`、裸动态容器变量作用域结束 |
| 生成规则描述 | cleanup kind 为 xvalue 时，若指针非空，调用 `xvoUnref` 并清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:302` |
| 对应生成的 C 代码 | `if (v != 0) { xvoUnref(v); v = 0; }` |
| 中间形态伪代码 | `if xvalue != 0: unref; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 对动态值调用 unref。 |
| 额外依赖的封装代码 | xvalue runtime。 |

## CGEN-CLEAN-042 typed container cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `array<int>/list<T>/set<T>/dict<T>` 作用域结束 |
| 生成规则描述 | cleanup kind 为 typed container 时，先取得容器具体类型，再按 `array/list/set/dict` 分别调用 `xrtTypedArrayDestroy`、`xrtTypedListDestroy`、`xrtTypedSetDestroy`、`xrtTypedDictDestroy`，最后将变量清零。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:316` |
| 对应生成的 C 代码 | `if (a != 0) { xrtTypedArrayDestroy(a); a = 0; }` |
| 中间形态伪代码 | `kind = typedContainerConcreteType; emit matching destroy; clear` |
| 实现同等功能手写的 C 代码 | 手写 C 对每种 typed container 调用对应 destroy。 |
| 额外依赖的封装代码 | typed container storage helper。 |

## CGEN-CLEAN-043 class cleanup 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `A a` 作用域结束，A 需要析构 |
| 生成规则描述 | 若 cleanup 未被前面的特殊 kind 消费，且 class 递归判定需要析构，则输出 class destroy 调用。泛型 class 实例使用泛型 class 成员符号，非泛型 class 使用普通 class 成员符号；这同样释放函数入口为 class 值参数建立的独立 owned copy。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:450` |
| 对应生成的 C 代码 | `xl6_A_destroy(&a);` 或 `xl6_A__int_destroy(&a);` |
| 中间形态伪代码 | `if classNeedsDestroy: emit genericDestroy or classDestroy(&name)` |
| 实现同等功能手写的 C 代码 | 手写 C 显式调用结构体析构函数。 |
| 额外依赖的封装代码 | class destroy 函数。 |

## CGEN-CLEAN-044 命名 function value retain

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func b = a` |
| 生成规则描述 | 对变量名先转为 C 标识符，再输出 retain 保护块。与路径 retain 不同，该规则输入是 xlang 名称而不是任意 C 路径。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:354` |
| 对应生成的 C 代码 | `if (a.retain != 0 && a.env != 0) { a.retain(a.env); }` |
| 中间形态伪代码 | `cName = cIdent(name); retain function cName` |
| 实现同等功能手写的 C 代码 | 手写 C 对函数环境增加引用计数。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-CLEAN-045 命名 function value release

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f` 变量被覆盖或离开作用域 |
| 生成规则描述 | 对变量名先转为 C 标识符，再输出 release 保护块。此函数只调用 release，不清空 env/release，通常用于赋值覆盖前的释放路径。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:369` |
| 对应生成的 C 代码 | `if (f.release != 0 && f.env != 0) { f.release(f.env); }` |
| 中间形态伪代码 | `cName = cIdent(name); if release/env: release(env)` |
| 实现同等功能手写的 C 代码 | 手写 C 覆盖函数值前释放旧环境。 |
| 额外依赖的封装代码 | function value runtime。 |

## CGEN-CLEAN-046 function 借用表达式判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func b = a`、`func b = obj.cb`、`func b = await futureCb` |
| 生成规则描述 | member 表达式通过成员字段类型推断判断是否是 function；await 表达式通过表达式类型推断判断是否是 function；name 表达式查变量类型。其他表达式不视为 function 借用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:384` |
| 对应生成的 C 代码 | 借用为真时后续生成 retain。 |
| 中间形态伪代码 | `member/await/name -> infer or lookup type -> isFunctionType` |
| 实现同等功能手写的 C 代码 | 手写 C 判断函数值是否来自共享闭包环境。 |
| 额外依赖的封装代码 | 成员类型推断和表达式类型推断 helper。 |

## CGEN-CLEAN-047 xvalue 成员借用表达式判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var b = obj.value`、`var b = v.member`、`var b = obj?.value` |
| 生成规则描述 | member/safe member 的 `length` 永远不算 xvalue 借用。若 receiver 本身是 xvalue 家族，则递归判断 receiver 是否借用；safe member 不按静态字段借用处理；普通 member 会推断字段类型，字段类型是 xvalue 家族则判定为借用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:417` |
| 对应生成的 C 代码 | 借用为真时后续可能生成 `xvoRef` 一类保留逻辑。 |
| 中间形态伪代码 | `member length -> false; receiver xvalue -> recurse; safe member -> false; field type xvalue -> true` |
| 实现同等功能手写的 C 代码 | 手写 C 判断返回的是共享 xvalue 指针还是新对象。 |
| 额外依赖的封装代码 | xvalue borrow helper。 |

## CGEN-CLEAN-048 xvalue 索引/name 借用表达式判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `var b = a[i]`、`var b = v` |
| 生成规则描述 | index/safe index 若 receiver 使用 typed container storage，则不是 xvalue 借用；否则递归判断 receiver。name 表达式若自身使用 typed container storage，也不是 xvalue 借用；否则变量类型为 xvalue 家族时判定为借用。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:417` |
| 对应生成的 C 代码 | 借用为真时需要 retain 动态值；typed container storage 则走值/元素规则。 |
| 中间形态伪代码 | `index: if typed storage receiver false else recurse; name: if typed storage false else isXValueType(varType)` |
| 实现同等功能手写的 C 代码 | 手写 C 按容器实现区分元素所有权。 |
| 额外依赖的封装代码 | typed container storage 判定和 xvalue helper。 |

## CGEN-CLEAN-049 逆序输出 cleanup

| 项目 | 内容 |
|---|---|
| xlang 写法 | 作用域中连续声明多个需清理变量 |
| 生成规则描述 | `xl6CgenEmitCleanupsSince` 从当前 cleanup 栈尾向 mark 逆序输出 cleanup。后声明的变量先释放，符合栈式生命周期和 C 局部对象析构顺序。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:479` |
| 对应生成的 C 代码 | `cleanup_c(); cleanup_b(); cleanup_a();` |
| 中间形态伪代码 | `for i = count downto mark+1: emit cleanup[i-1]` |
| 实现同等功能手写的 C 代码 | 手写 C 通常在退出路径按反向顺序释放资源。 |
| 额外依赖的封装代码 | cleanup 栈。 |

## CGEN-CLEAN-050 作用域关闭

| 项目 | 内容 |
|---|---|
| xlang 写法 | `if/while/for/function` 作用域结束 |
| 生成规则描述 | 作用域关闭时先输出 mark 之后的 cleanup，再释放这些 cleanup 元数据并回退栈计数。该函数是块级作用域稳定退出的统一入口。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_expr_cleanup.c:491` |
| 对应生成的 C 代码 | 生成该作用域内资源的释放语句，然后继续输出后续 C 代码。 |
| 中间形态伪代码 | `emitCleanupsSince(mark); freeCleanupsSince(mark)` |
| 实现同等功能手写的 C 代码 | 手写 C 在每个块出口显式释放资源。 |
| 额外依赖的封装代码 | cleanup 栈。 |

## CGEN-CLEAN-051 loop target 入栈

| 项目 | 内容 |
|---|---|
| xlang 写法 | `while ... break ... continue ... end`、嵌套循环 |
| 生成规则描述 | 进入循环时记录 break cleanup mark、continue cleanup mark、break label id 和 continue label id。数组容量从 8 起按 2 倍扩容，扩容失败报告 `out of memory while growing cgen loop target stack`。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_loop_target.c:1` |
| 对应生成的 C 代码 | 后续 `break`/`continue` 会先释放对应 mark 之后的资源，再 `goto xl6_break_N` 或 `goto xl6_continue_N`。 |
| 中间形态伪代码 | `push {breakMark, continueMark, breakId, continueId}` |
| 实现同等功能手写的 C 代码 | 手写 C 在每个 break/continue 前补局部资源释放。 |
| 额外依赖的封装代码 | loop target 栈。 |

## CGEN-CLEAN-052 loop target 出栈、按层查找和释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | `break`、`break 2`、`continue`、嵌套循环结束 |
| 生成规则描述 | 循环结束时弹出当前 loop target；按层级查找时，level 必须大于 0 且不超过栈深，索引为 `count - level`；cgen 结束时释放整块 loop target 数组并清空计数。 |
| 生成器文件名和行号 | `demo6/src/cgen/xl6_cgen_loop_target.c:26`、`demo6/src/cgen/xl6_cgen_loop_target.c:44` |
| 对应生成的 C 代码 | `goto xl6_break_2;` 或 `goto xl6_continue_3;` 前会使用对应 target 的 cleanup mark。 |
| 中间形态伪代码 | `pop current; target(level)=targets[count-level]; free all on finish` |
| 实现同等功能手写的 C 代码 | 手写 C 为多层 break/continue 手动管理 label 和清理路径。 |
| 额外依赖的封装代码 | loop target 栈。 |
