# 隐式类型转换 V1 可跟踪规格

状态说明：

- `[ ]` 未开始
- `[~]` 进行中
- `[x]` 完成
- `[!]` 阻塞或需要重新设计

## 0. 范围

V1 只实现安全核心：

- exact / alias
- `int -> float`
- concrete -> `var` / `any`
- class/object instance -> `object`
- `null -> reference/object/container`
- 调用匹配、赋值、return、默认参数、基础表达式共同类型、XLP/native bridge

V1 明确不做：

- `float -> int`
- `string -> number`
- number -> bool
- `var -> concrete`
- 容器协变
- 函数类型 variance
- 用户自定义隐式转换

## 1. 类型转换核心

- [x] 新增转换类型枚举 `XL_ConvKind`
- [x] 新增转换结果结构 `XL_Conversion`
- [x] 新增统一转换 API `xlTypeCanConvertImplicit(...)`
- [x] 支持 exact match，cost=0
- [x] 支持 alias/canonical match，cost=1
- [x] 支持 `int -> float`，cost=2
- [x] 支持 concrete -> `var`，cost=3
- [x] 支持 concrete -> `any`，cost=3
- [x] 支持 object instance -> `object`，cost=3
- [x] 支持 `null -> reference/object/container`，cost=4
- [x] 禁止 `float -> int`
- [x] 禁止 `string -> number`
- [x] 禁止 number -> `string`
- [x] 禁止 number -> `bool`
- [x] 禁止 `bool -> number`
- [x] 禁止 `var -> concrete`
- [x] 为所有禁止项返回可诊断 reason

验收：

- [x] 增加转换 API 单元测试或语义测试
- [x] 覆盖所有 V1 转换表
- [x] 覆盖所有 V1 禁止项

## 2. 函数调用匹配

- [x] 调用匹配改为使用 `XL_Conversion`
- [x] 候选收集仍按名称、receiver/method、arity、默认参数、vararg 初筛
- [x] 每个候选记录参数转换数组
- [x] 任一参数不可转换时淘汰候选
- [x] 计算参数转换 cost 总和
- [x] 使用默认参数时增加 penalty=1
- [x] 使用 vararg 时增加 penalty=2
- [x] 目标参数为 `var` / `any` 时增加 penalty=3
- [x] dynamic fallback 保留 penalty=5，但 V1 默认不启用（V1 保留项）
- [x] 最低分候选获胜
- [x] 同分时 exact 更多者优先
- [x] 同分时非 `var` / `any` 优先
- [x] 同分时非 vararg 优先
- [x] 仍同分时报 ambiguous overload

验收：

- [x] `func f(float); func f(var); f(1)` 选择 `f(float)`
- [x] `func f(int); func f(float); f(1)` 选择 `f(int)`
- [x] `func f(int); func f(float); f(1.0)` 选择 `f(float)`
- [x] `func f(int); f(1.2)` 报禁止 `float -> int`
- [x] 默认参数候选不应压过无默认参数 exact 候选
- [x] vararg 候选不应压过固定参数 exact 候选
- [x] ambiguity 诊断输出候选转换明细

## 3. 泛型调用

- [x] 泛型推导阶段不使用 widening 反推类型参数
- [x] 泛型推导阶段允许 exact / alias / 结构匹配
- [x] 泛型实例化后再做隐式转换检查
- [x] 泛型候选参与同一套 cost 排序
- [x] 泛型实例缓存 key 不包含调用点转换，仅包含实例化类型参数

验收：

- [x] `id<T>(T value); id(1)` 推导 `T=int`
- [x] `take<T>(float x, T y); take(1, "a")` 允许 `x: int -> float`，推导 `T=string`
- [x] 不允许通过 `int -> float` 把 `T` 反推成 `float`
- [x] 泛型和非泛型重载同时存在时，cost 排序稳定

## 4. 赋值和变量初始化

- [x] 变量初始化类型检查使用转换 API
- [x] 赋值语句类型检查使用转换 API
- [x] `float x = 1` 通过
- [x] `int x = 1.2` 报错
- [x] `var x = 1` 通过
- [x] `bool b = 1` 报错
- [x] 错误诊断提示显式转换方式

验收：

- [x] 增加语义测试
- [x] 增加运行测试验证 `int -> float`

## 5. return 检查

- [x] return 值匹配函数声明返回类型时使用转换 API
- [x] 多返回值逐项检查转换
- [x] `func f() as float return 1 end` 通过
- [x] `func f() as int return 1.2 end` 报错
- [x] `func f() as var return 1 end` 通过

验收：

- [x] 单返回值转换测试
- [x] 多返回值转换测试
- [x] 禁止项诊断测试

## 6. 默认参数

- [x] 默认参数表达式匹配参数类型时使用转换 API
- [x] `func f(float x = 1)` 通过
- [x] `func f(int x = 1.2)` 报错
- [x] XLP `defaultValueText` 保持源码可回填
- [x] XLP `defaultValueJson` 保持 AST 元数据
- [x] XLP native/source bridge 生成默认参数声明时保留默认值

验收：

- [x] 本地默认参数转换测试
- [x] XLP 导出默认参数转换测试
- [x] XLP 导入后省略默认参数调用测试

## 7. 表达式共同类型

- [x] 二元数值运算加入共同类型计算
- [x] `int + float -> float`
- [x] `float + int -> float`
- [x] `int + int -> int`
- [x] 不允许 `string + int` 通过隐式转换
- [x] 三元表达式两端共同类型使用转换 API
- [x] 容器字面量元素共同类型使用转换 API
- [x] `[1, 2.0]` 推导为 `array<float>` 或等价存储类型

验收：

- [x] 数值二元运算运行测试
- [x] 三元表达式语义测试
- [x] 容器字面量类型测试

## 8. 条件表达式

- [x] `if` 条件必须是 `bool`
- [x] `while` 条件必须是 `bool`
- [x] 禁止 number -> bool
- [x] 诊断提示显式比较，例如 `x != 0`

验收：

- [x] `if true` 通过
- [x] `if 1` 报错
- [x] `while 1` 报错
- [x] `if x != 0` 通过

## 9. Lower / Codegen

- [x] HIR/MIR 或当前 lowering 层能表达转换节点
- [x] exact / alias 不生成额外代码
- [x] `int -> float` 生成数值 cast
- [x] concrete -> `var` 生成 box helper
- [x] `null -> reference/object/container` 生成目标类型 null
- [x] 禁止项不会进入 codegen

验收：

- [x] dump 中可看到转换节点或等价标记
- [x] C codegen 输出正确 cast
- [x] TCC 运行测试通过

## 10. XLP / Native Bridge

- [x] XLP 函数 ABI 保持声明签名，不记录调用点转换
- [x] XLP import 后调用匹配使用转换 API
- [x] native bridge 参数适配使用转换 API
- [x] `int -> float` bridge 调用通过
- [x] `float -> int` bridge 调用报错
- [x] 默认参数跨包省略调用通过

验收：

- [x] 新增 XLP native 包导出 `func take_float(float x)`
- [x] xlang 调用 `take_float(1)` 通过
- [x] 新增 XLP native 包导出 `func take_int(int x)`
- [x] xlang 调用 `take_int(1.2)` 报错
- [x] XLP dump 中 ABI id 不因调用转换改变

## 11. 诊断

- [x] 每个 `XL_Conversion` 失败项携带 reason
- [x] 类型不匹配诊断显示 source/target
- [x] 有损转换诊断显示显式替代方案
- [x] 重载歧义诊断显示候选转换 cost
- [x] 条件非 bool 诊断提示显式比较

验收：

- [x] golden baseline 覆盖所有关键错误文本
- [x] 不输出内部 enum 名称给用户

## 12. 回归测试清单

- [x] `implicit_exact_basic.xl`
- [x] `implicit_int_to_float_assign.xl`
- [x] `implicit_int_to_float_call.xl`
- [x] `implicit_int_to_float_return.xl`
- [x] `implicit_float_to_int_error.xl`
- [x] `implicit_number_to_bool_error.xl`
- [x] `implicit_string_to_number_error.xl`
- [x] `implicit_overload_cost_basic.xl`
- [x] `implicit_overload_ambiguous_error.xl`
- [x] `implicit_generic_infer_then_convert.xl`
- [x] `implicit_default_param_convert.xl`
- [x] `implicit_multi_return_convert.xl`
- [x] `implicit_binop_numeric_promote.xl`
- [x] `implicit_container_common_type.xl`
- [x] `implicit_xlp_native_int_to_float.xl`
- [x] `implicit_xlp_native_float_to_int_error.xl`
- [x] `implicit_xlp_default_param_cross_package.xl`

## 13. Done Definition

V1 完成需要满足：

- [x] 所有 V1 转换 API 测试通过
- [x] 调用匹配、赋值、return、默认参数都走统一转换 API
- [x] 重载排序稳定且可诊断
- [x] 禁止项均有明确错误
- [x] XLP/native bridge 不破坏 ABI
- [x] `build_gcc.bat` 通过
- [x] 新增 runtime/sem regression 通过
- [x] 文档和 spec 更新到完成状态

## 2026-04-30 implementation progress

- [x] Codegen emits explicit C casts for static numeric alias conversions and `int -> float`.
- [x] Await-replacement codegen accepts `int -> float` where the semantic conversion has already been approved.
- [x] Added regression `implicit_conversion_return_default_multi.xl`.
- [x] Covered `func f() as float return 1 end`.
- [x] Covered `func f(float x = 1)`.
- [x] Covered multi-return semantic conversion `func f() as float, string return 1, "ok" end`.
- [x] Re-ran `build_gcc.bat`.
- [x] Re-ran implicit-conversion sem/runtime regression selectors.

## 2026-04-30 XLP bridge progress

- [x] Added XLP native shared-package `int -> float` call regression.
- [x] Added XLP static-package `int -> float` call regression.
- [x] Added XLP native `float -> int` rejection regression.
- [x] Added C codegen assertion that XLP native `add_half(1)` emits `((double)1)`.
- [x] Re-ran `build_gcc.bat`.
- [x] Re-ran XLP bridge sem/cgen/runtime regression selectors.

## 2026-04-30 diagnostics progress

- [x] Overload failure diagnostics include per-candidate argument conversion details.
- [x] Candidate diagnostics include `actual -> expected`, accepted conversion cost/kind, or failure reason.
- [x] Ambiguous overload diagnostics use the same detailed candidate formatter.
- [x] Added regression needles for `float -> int fail` in local and XLP native calls.
- [x] Re-ran overload diagnostics and implicit-conversion regression selectors.

## 2026-04-30 XLP default progress

- [x] Added manifest default `1` for `pkg.probe.add_half(float value)`.
- [x] Added XLP native/static omitted-argument default-call regressions.
- [x] Verified imported manifest default `int -> float` through sem/cgen/runtime selectors.
- [x] Re-ran `build_gcc.bat`.

## 2026-04-30 common expression progress

- [x] Normal binary operators now reject invalid mixed operands such as `string + int`.
- [x] Numeric binary codegen casts operands to the common numeric result type.
- [x] Ternary codegen casts branches to the resolved common branch type.
- [x] Array/list/set/dict literal codegen boxes elements through the resolved container item type.
- [x] Lowering annotates common-expression conversions for numeric binary operands, ternary branches, and container literal elements.
- [x] Added dedicated list/set/dict literal regressions for element `int -> float` conversion.
- [x] Added sem/cgen/runtime regressions for common-type conversion paths.
- [x] Re-ran `build_gcc.bat` and implicit-conversion regression selectors.

## 2026-04-30 condition progress

- [x] `if`, `elseif`, and `while` conditions now require `bool`.
- [x] Numeric conditions are rejected instead of being implicitly converted to `bool`.
- [x] Condition diagnostics suggest an explicit comparison such as `value != 0`.
- [x] Added failing regressions for `if 1` and `while 1`.
- [x] Added passing regression for explicit numeric comparison in `if` and `while`.
- [x] Re-ran `build_gcc.bat` and implicit-conversion regression selectors.

## 2026-04-30 overload scoring progress

- [x] Verified `func f(int); func f(float); f(1.0)` selects the `float` overload.
- [x] Verified a no-default exact overload beats a matching overload that consumes a default parameter.
- [x] Verified a fixed-parameter exact overload beats a matching vararg overload.
- [x] Verified a concrete `float` parameter beats a `var` parameter for `int -> float`.
- [x] Added cgen needles for the selected overload symbols in `implicit_conversion_overload_cost.xl`.

## 2026-04-30 generic conversion progress

- [x] Generic inference keeps the first exact inferred type for a type parameter.
- [x] If the same type parameter is seen again, an argument may match by implicit conversion to the already inferred type.
- [x] `take_pair(1.0, 2)` now instantiates `T=float` and applies `int -> float` after inference.
- [x] `take_pair(1, 2.0)` still fails because `float -> int` is not an implicit conversion.
- [x] Added sem/cgen/runtime regressions for post-inference conversion and a sem regression for the rejected reverse case.

## 2026-04-30 null conversion progress

- [x] Added sem/cgen/runtime regression for `null -> string`, `null -> array<int>`, `null -> function<int()>`, and `null -> var`.
- [x] Added lower text/JSON regression coverage for `null -> string`, `null -> array<int>`, `null -> function<int()>`, and `null -> var` conversion markers.
- [x] Added sem regression that rejects `null -> int`.
- [x] Verified null reference conversion codegen emits typed null/default values instead of entering forbidden numeric codegen.

## 2026-04-30 dynamic boundary progress

- [x] Added concrete `int -> any` semantic, C codegen, and runtime coverage.
- [x] Added concrete `int -> var` semantic, lower marker, C codegen, and runtime coverage.
- [x] Added object-class instance `DynBox -> any/var` semantic, lower marker, C codegen, and runtime coverage.
- [x] Added lower text/JSON marker coverage for dynamic boxing conversions.
- [x] Fixed C codegen so `any` storage is emitted as `xvalue` and boxes concrete initializers.
- [x] Fixed assignability so dynamic source types (`var` / `any`) no longer pass concrete targets through legacy symmetric type matching.
- [x] Added semantic rejection coverage for `var -> int`.
- [x] Added semantic rejection coverage for `bool -> int`.
- [x] Re-ran `build_gcc.bat`.

## 2026-04-30 object conversion progress

- [x] Added semantic, C codegen, and runtime coverage for object-class instance `ObjBox -> object`.
- [x] Covered `object dyn = box`, `func take_object(object value)`, and `func make_object() as object`.
- [x] Fixed C codegen so `object` storage uses `xvalue` instead of erasing to `void*`.
- [x] Fixed `Type(objectValue)` to query the runtime value type instead of returning the static `object` type.
- [x] Verified object conversion preserves the concrete runtime type across assignment, parameter passing, and return.

## 2026-04-30 exact and alias progress

- [x] Added semantic, C codegen, and runtime coverage for exact matches and numeric family aliases.
- [x] Covered `int -> int32`, `int32 -> int`, `float -> float64`, and `float64 -> float`.
- [x] Verified alias conversion codegen emits explicit C casts at assignment and call sites.

## 2026-04-30 conversion API coverage progress

- [x] Added direct regression coverage for `xlTypeCanConvertImplicit`.
- [x] Verified successful API results for exact, alias, numeric widen, dynamic boxing, and null-reference conversion.
- [x] Verified rejected API results for `float -> int`, `bool -> int`, `var -> int`, `null -> int`, and `string -> float`.

## 2026-04-30 spec reconciliation progress

- [x] HIR and MIR nodes now preserve implicit conversion markers as `convert="actual->expected"` in text dumps and `conversion` in JSON dumps.
- [x] Added lower regression coverage for visible `int->float` conversion markers.
- [x] Added lower regression coverage for return-value conversion markers.
- [x] Added lower regression coverage for assignment RHS and default-argument conversion markers.
- [x] Added lower regression coverage for multi-return element conversion markers.
- [x] Added lower regression coverage for common-expression conversion markers.
- [x] Static numeric exact/alias conversion codegen no longer emits extra C casts; widening conversions still emit casts.
- [x] Candidate diagnostics now use user-facing conversion labels instead of internal enum tokens.
- [x] Re-ran `build_gcc.bat` and implicit-conversion regression selectors.

- [x] Added independent ambiguous-overload regression for tied implicit conversion cost.
- [x] Verified ambiguous overload diagnostics include per-candidate `int -> float` conversion cost and kind details.
- [x] Reconciled the regression checklist with the implemented `implicit_` test selector.

## 2026-04-30 resolver conversion descriptor progress

- [x] Added resolver-local conversion descriptors for context-aware class/object conversions.
- [x] Kept `xlTypeCanConvertImplicit` as the string-only conversion API; class inheritance remains resolver-owned because it requires the class table.
- [x] Added `object_ref` conversion kind for diagnostics.
- [x] Updated assignability, match scoring, and overload candidate diagnostics to share the same resolver conversion path.
- [x] Added semantic regression for object conversion details in a failing overload candidate.
- [x] Re-ran `build_gcc.bat` and the `implicit_` selector.

## 2026-04-30 object common-expression progress

- [x] Ternary branch common-type resolution now uses resolver-aware implicit conversions.
- [x] Equality compatibility now uses resolver-aware implicit conversions.
- [x] Class values can compare with their `object` reference when no user operator overload is declared.
- [x] Added sem/cgen/runtime regression for `ObjBox/object` ternary and equality conversion.
- [x] Added lower text/JSON regression coverage for `ObjBox -> object` ternary and equality conversion markers.
- [x] Re-ran `build_gcc.bat` and the `implicit_` selector.

## 2026-04-30 generic object post-inference progress

- [x] Generic type-argument binding now uses resolver-aware implicit conversions after a type parameter has already been inferred.
- [x] `object` and `any` are accepted as dynamic-compatible runtime values.
- [x] Class operator/index overload scoring now uses resolver-aware conversion scoring.
- [x] Added sem/cgen/runtime regression for `T=object` followed by `ObjBox -> object` post-inference conversion.
- [x] Added lower text/JSON marker regressions for numeric and object post-inference conversions.
- [x] Re-ran `build_gcc.bat` and the `implicit_` selector.

## 2026-04-30 generic overload cost progress

- [x] Generic function candidates are scored after side-effect-free type inference and parameter substitution.
- [x] Generated generic instances are excluded from source-level overload candidate lookup; calls resolve through the generic base.
- [x] Same-score generic/non-generic candidates prefer the non-generic overload for stable resolution.
- [x] Generic inference failure details are preserved in no-overload diagnostics.
- [x] Added sem/cgen/runtime regression for `pick<T>(T)` versus `pick(float)` and dynamic boxing fallback.
- [x] Re-ran `build_gcc.bat` and the full `implicit_` selector.

## 2026-04-30 explicit diagnostic and XLP default progress

- [x] Declaration and assignment mismatch diagnostics now include an explicit-cast hint.
- [x] Added a sem regression needle for `use explicit cast 'int(value)'`.
- [x] Source XLP package dumps now have coverage for `defaultValueText` and non-empty `defaultValueJson`.
- [x] Source XLP import now has sem/runtime coverage for omitted default arguments.
- [x] XLP ABI stability has coverage for call-site `int -> float` conversion that must not become an `int` ABI signature.
- [x] Re-ran `build_gcc.bat`, `implicit_`, `xlp_source_default`, and `xlp_external_package`.

## 2026-04-30 object lower marker progress

- [x] Lower conversion marker annotation now uses resolver-owned class metadata for `ObjBox -> object`.
- [x] Added lower regression coverage for object conversion markers at call and return sites.
- [x] Verified HIR dump preserves `convert="ObjBox->object"` for return value, variable initialization, and call argument nodes.
- [x] Refactored lower conversion marker eligibility into a shared helper so expression and statement lowering use the same rule path.
- [x] Added lower JSON regression coverage for numeric and object conversion markers, requiring both HIR and MIR copies to be present.

## 2026-04-30 object upcast progress

- [x] Added dedicated `ChildBox -> BaseBox` implicit object upcast example.
- [x] Covered upcast assignment, parameter passing, and return value conversion.
- [x] Added sem/lower JSON/cgen/runtime regression coverage under the `implicit_` selector.
- [x] Added unrelated object-class rejection coverage so sibling object classes do not implicitly convert.
- [x] Re-ran `build_gcc.bat`, focused upcast selectors, and the full `implicit_` selector.

## 2026-04-30 class null conversion progress

- [x] Added dedicated `null -> NullBox` implicit object-class conversion example.
- [x] Added dedicated `null -> object` implicit built-in object conversion example.
- [x] Added xvalue-backed `null` comparison regression for `array<T>`, `function<...>`, and `var` values.
- [x] Covered class-typed null variable initialization, parameter passing, and return value conversion.
- [x] Fixed C codegen for class/object xvalue `== null` and `!= null` comparisons to test `xvoType(value) == XVO_DT_NULL`, not only C pointer null.
- [x] Added sem/lower JSON/cgen/runtime regression coverage under the `implicit_` selector.
- [x] Re-ran `build_gcc.bat`, focused null-object selectors, and the full `implicit_` selector.

## 2026-04-30 negative boundary progress

- [x] Added semantic rejection coverage for `any -> int`.
- [x] Added semantic rejection coverage for `object -> concrete class` downcast.
- [x] Added semantic rejection coverage for `null -> bool`.

## 2026-04-30 dynamic assignment/return progress

- [x] Added assignment-statement coverage for `int -> any`, `int -> var`, `class -> any`, and `class -> var`.
- [x] Added return-value coverage for `int -> any`, `int -> var`, `class -> any`, and `class -> var`.
- [x] Added lower text/JSON conversion marker coverage for dynamic assignment and return paths.
- [x] Added C codegen and runtime regressions for dynamic assignment and return paths.

## 2026-04-30 dynamic default-parameter progress

- [x] Added default-parameter coverage for `int -> any` and `int -> var`.
- [x] Verified omitted-argument calls generate boxed dynamic default values.
- [x] Added lower text/JSON conversion marker coverage for dynamic default parameter values.
- [x] Added C codegen and runtime regressions for dynamic default parameter values.

## 2026-04-30 named-argument conversion progress

- [x] Added named-argument coverage for `int -> float`.
- [x] Added named-argument coverage for `int -> var`.
- [x] Added named-call default-argument coverage for `int -> any`.
- [x] Added lower text/JSON conversion marker coverage for named argument conversion paths.
- [x] Added C codegen and runtime regressions for named argument conversion paths.

## 2026-04-30 vararg conversion progress

- [x] Added array vararg pack coverage for `int -> var`.
- [x] Added dict vararg pack coverage for `int -> var`.
- [x] Lower now annotates hidden vararg `array<var>` / `dict<var>` literal elements with conversion markers.
- [x] Added lower text/JSON, C codegen, and runtime regressions for vararg conversion paths.
- [x] Lower now annotates vararg spread arguments from `array<T>` / `dict<T>` to `array<var>` / `dict<var>`.
- [x] Added lower text/JSON regressions for vararg spread container dynamic-element conversion markers.

## 2026-04-30 container dynamic-element conversion progress

- [x] Lower now annotates fixed-argument and return conversions from `array<T>` / `dict<T>` to `array<var>` / `dict<var>`.
- [x] Added sem, lower text/JSON, and runtime regressions for container dynamic-element conversion markers.
- [x] Verified `implicit_` selector after adding container dynamic-element coverage.
- [x] Extended the same coverage to `list<T> -> list<var>` and `set<T> -> set<var>`.
- [x] Added C codegen regressions for container dynamic-element conversion function signatures and call sites.
- [x] Added semantic rejection coverage for dynamic container element downcast such as `array<var> -> array<int>`.
- [x] Added semantic rejection coverage for `array<any>` / `dict<any>` / `list<any>` / `set<any>` downcast to concrete element containers.
