# P1 decl_proto 与 ABI 元数据细则

本文细化函数原型、extern/export C ABI、record/handle box runtime、函数签名 ID、多返回值结构、运行时 type desc 和 `type(expr)` 的生成规则。主要覆盖 `xl6_cgen_decl_proto.c`、`xl6_cgen_abi.c`、`xl6_cgen_symbol.c`、`xl6_cgen_module.c` 中与 ABI 元数据直接相关的路径。

## CGEN-ABI-001 普通 xlang 函数原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func add(int a, int b) int` |
| 生成规则描述 | 普通函数原型使用 xlang 内部 C 类型映射。无返回值输出 `void`；单返回输出对应 C 类型；多返回输出签名 ID 派生的返回结构体类型。参数为空输出 `void`，否则逐个输出参数 C 类型和 C 标识符。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:1`、`xl6_cgen_decl_proto.c:8`、`xl6_cgen_decl_proto.c:23` |
| 对应生成的 C 代码 | `long long xl_func_add(long long a, long long b);` |
| 中间形态伪代码 | `FuncProto(name, params, returns) -> returnCType symbol(paramCType name...)` |
| 实现同等功能手写的 C 代码 | `long long add(long long a, long long b);` |
| 额外依赖的封装代码 | 多返回值时依赖 `XL6_Function_Return_<signature>` 结构体声明。 |

## CGEN-ABI-002 export 内部原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func sum(array<int> values) as int` / `export func pair() (int, string)` |
| 生成规则描述 | export 函数会生成一个 xlang 内部函数原型，内部原型保留 xlang 的参数和返回模型：静态泛型容器参数使用 `xtarray` / `xtlist` / `xtset` / `xtdict`，不能退化为 `xvalue`；多返回值仍返回结构体，单返回值返回内部 C 类型。它用于 xlang 代码自身和 package 模块调用，不直接暴露给 C ABI。显式 C export 的类型限制由 wrapper 规则单独检查。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:39`、`xl6_cgen_decl_proto.c:81`、`xl6_cgen_module.c:178` |
| 对应生成的 C 代码 | `long long xl_export_internal_sum(xtarray values);` / `XL6_Function_Return_ABCD xl_export_internal_pair(void);` |
| 中间形态伪代码 | `export func -> internalSymbol(params) returns xlangReturnShape` |
| 实现同等功能手写的 C 代码 | 手写 C 通常会写一个内部实现函数，再由导出 wrapper 转换 ABI。 |
| 额外依赖的封装代码 | export wrapper；多返回结构体；静态泛型容器类型和元素操作 helper。 |

## CGEN-ABI-003 export C ABI wrapper 原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `export func pair() (int, string)` |
| 生成规则描述 | export wrapper 面向 C ABI。单返回值直接作为 C ABI 返回值；无返回值和多返回值返回 `void`。多返回值通过追加 `out0/out1/...` 指针参数传出。可变参数 export wrapper 当前拒绝生成。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:87`、`xl6_cgen_decl_proto.c:95`、`xl6_cgen_decl_proto.c:119`、`xl6_cgen_module.c:181` |
| 对应生成的 C 代码 | `void pair(long long* out0, char** out1);` |
| 中间形态伪代码 | `export wrapper returns void; append out pointers for each return` |
| 实现同等功能手写的 C 代码 | `void pair(long long* out0, char** out1);` 并在实现里调用内部函数后拆字段。 |
| 额外依赖的封装代码 | export wrapper 函数体。 |

## CGEN-ABI-004 extern 函数原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `extern func puts(string s) int` |
| 生成规则描述 | extern 函数声明生成直接 C ABI 原型。返回值最多一个，多返回 extern 当前拒绝。参数使用 C ABI 类型映射；`array<var>` 和 `dict<var>` 形式的可变参数不允许直接出现在 extern C 原型中。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:142`、`xl6_cgen_decl_proto.c:148`、`xl6_cgen_decl_proto.c:169`、`xl6_cgen_module.c:174` |
| 对应生成的 C 代码 | `long long puts(char* s);` |
| 中间形态伪代码 | `extern func -> raw C declaration using ABI types` |
| 实现同等功能手写的 C 代码 | 直接手写外部 C 函数声明。 |
| 额外依赖的封装代码 | 无 wrapper；调用点直接调用 C 符号。 |

## CGEN-ABI-005 extern/export 变量原型与定义

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `extern int errno` / `export const int version = 1` |
| 生成规则描述 | extern 变量输出 `extern CAbiType symbol;`。export 变量/常量输出定义，常量额外加 `const`，初始化表达式直接作为 C 初始化值生成。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:185`、`xl6_cgen_decl_proto.c:197`、`xl6_cgen_module.c:176`、`xl6_cgen_module.c:183` |
| 对应生成的 C 代码 | `extern long long errno;` / `const long long version = 1;` |
| 中间形态伪代码 | `extern var -> declaration; export var -> definition` |
| 实现同等功能手写的 C 代码 | C 中同样分为 `extern` 声明和全局定义。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-ABI-006 函数签名 ID

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int, string) (bool)` |
| 生成规则描述 | 函数签名 ID 使用 FNV-1a 风格哈希，从 `func(` 开始，依次写入参数类型、`) as (`、返回类型、`)`。类型会先做泛型替换和 alias 解析，确保同一语义类型得到稳定签名。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:1`、`xl6_cgen_abi_signature.c:80`、`xl6_cgen_abi_signature.c:115`、`xl6_cgen_abi_signature.c:168` |
| 对应生成的 C 代码 | `0x0123456789ABCDEFULL` |
| 中间形态伪代码 | `hash("func(" + params + ") as (" + returns + ")")` |
| 实现同等功能手写的 C 代码 | 手写 C 可用枚举或手写常量；xlang 自动按签名生成。 |
| 额外依赖的封装代码 | 签名 hash helper；不生成运行时代码。 |

## CGEN-ABI-007 前缀函数签名 ID

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 可选参数 wrapper 或部分参数适配 |
| 生成规则描述 | 前缀签名 ID 只 hash 前 `paramCount` 个参数，并可选择是否把 vararg 参数纳入签名。该规则用于可选参数 adapter、xcall wrapper 等需要“较短调用签名”的场景。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:135`、`xl6_cgen_abi_signature.c:181`、`xl6_cgen_callable_wrappers.c:565` |
| 对应生成的 C 代码 | `XL6_Function_Return_<prefixSignature>` |
| 中间形态伪代码 | `hash(func(param[0:n], maybeVararg) as returns)` |
| 实现同等功能手写的 C 代码 | 手写 C 通常需要为每个可选参数数量写一个 adapter 类型。 |
| 额外依赖的封装代码 | optional adapter / xcall wrapper。 |

## CGEN-ABI-008 多返回值结构体声明

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func pair() (int, string)` |
| 生成规则描述 | 返回值超过一个时生成 `XL6_Function_Return_<signature>` 结构体，每个返回值字段按顺序命名为 `value0/value1/...`。结构体声明带 include guard，避免同一签名在多处重复声明。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:194`、`xl6_cgen_abi_signature.c:209`、`xl6_cgen_abi_signature.c:235` |
| 对应生成的 C 代码 | `typedef struct XL6_Function_Return_ABCD { long long value0; char* value1; } XL6_Function_Return_ABCD;` |
| 中间形态伪代码 | `returns(T0,T1,...) -> struct { T0 value0; T1 value1; ... }` |
| 实现同等功能手写的 C 代码 | 手写 C 通常为每个多返回函数定义一个结果结构体。 |
| 额外依赖的封装代码 | 签名 ID 与结构体去重表。 |

## CGEN-ABI-009 多返回值结构体预扫描

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func(int) (int, string) f` / `func() (int, string) ... end` |
| 生成规则描述 | 除普通函数本身外，func 类型和匿名函数中出现的多返回签名也会被递归扫描并提前生成返回结构体，避免后续 function value 或 wrapper 引用未声明类型。 |
| 生成器文件名和行号 | `xl6_cgen_symbol.c:444`、`xl6_cgen_symbol.c:463`、`xl6_cgen_module.c:172` |
| 对应生成的 C 代码 | `typedef struct XL6_Function_Return_ABCD ...;` 在函数原型前输出。 |
| 中间形态伪代码 | `walk AST; if function type has multi returns: emit return struct` |
| 实现同等功能手写的 C 代码 | 手写 C 需要把所有结果结构体放在使用点之前。 |
| 额外依赖的封装代码 | AST 递归扫描。 |

## CGEN-ABI-010 function signature object

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | function value、xcall、var func 调用相关 wrapper |
| 生成规则描述 | 需要运行时签名检查或 xcall 调用时，生成静态 `xrt_func_sig` 对象，包含签名 ID、名称、名称长度、参数数量、返回数量等字段。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:251`、`xl6_cgen_abi_signature.c:261`、`xl6_cgen_callable_wrappers.c:177`、`xl6_cgen_callable_wrappers.c:486` |
| 对应生成的 C 代码 | `static const xrt_func_sig __xl6_sig = { 0x..., "foo", 3u, 2u, 0, 1u, 0, 0u };` |
| 中间形态伪代码 | `sig = {id, name, paramCount, returnCount}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要人工维护签名元数据结构。 |
| 额外依赖的封装代码 | callable wrapper / xcall frame。 |

## CGEN-ABI-011 type id 生成

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `type(a)` / `int` 作为 type 字面量 |
| 生成规则描述 | 普通类型 ID 使用 `type:` 前缀和类型节点 hash。class/native-backed class 节点会转成普通类型节点后 hash；func 类型直接使用函数签名 ID，使 `func(...) (...)` 的 type id 与 callable 签名保持一致。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:14`、`xl6_cgen_abi_signature.c:283`、`xl6_cgen_abi_signature.c:309`、`xl6_cgen_abi_signature.c:314` |
| 对应生成的 C 代码 | `0x0123456789ABCDEFULL` |
| 中间形态伪代码 | `typeId(typeNode) = hash("type:" + canonicalTypeText)` |
| 实现同等功能手写的 C 代码 | 可手写枚举值，但需要人工保证跨模块稳定。 |
| 额外依赖的封装代码 | 类型 hash helper；不生成运行时代码。 |

## CGEN-ABI-012 类型文本生成

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `array<int>` / `dict<string>` / `Box::<int>` |
| 生成规则描述 | type desc 中的源码类型名通过 `xl6CgenTypeText` 生成。它会先替换泛型和解析 alias，再输出 `name<arg0,arg1>` 形式。该文本用于 type desc 的可读名称和调试/文档信息。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:47`、`xl6_cgen_abi_signature.c:451` |
| 对应生成的 C 代码 | `"array<int>"` / `"dict<string>"` |
| 中间形态伪代码 | `typeText(T<Args...>) -> "T<Args...>"` |
| 实现同等功能手写的 C 代码 | 手写 C 需要为每个类型写字符串常量。 |
| 额外依赖的封装代码 | 无运行时 helper。 |

## CGEN-ABI-013 runtime container type kind

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `array<int>` / `list<string>` / `set<int>` / `dict<string>` |
| 生成规则描述 | 运行时容器 type desc 的 kind 只区分 array/list/set/dict，其他类型在该路径下返回 invalid。该 kind 写入 `xrt_type_desc`，供 xvalue 容器和泛型容器元数据使用。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:328`、`xl6_cgen_abi_signature.c:470` |
| 对应生成的 C 代码 | `XL6_XRT_TYPE_KIND_ARRAY` |
| 中间形态伪代码 | `kind(array)=ARRAY; kind(list)=LIST; kind(set)=SET; kind(dict)=DICT` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护类型枚举映射。 |
| 额外依赖的封装代码 | runtime type desc 函数。 |

## CGEN-ABI-014 XRT type desc 引用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 类型进入 xvalue、typed container、runtime metadata |
| 生成规则描述 | 生成某个类型的 `xrt_type_desc*` 引用时，基础类型映射到 XRT 内置 `xrtType*()`；runtime 容器类型引用生成的 runtime type desc；class/union/native-backed class 引用各自生成的 type desc 函数；未知类型输出 `0`。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:365`、`xl6_cgen_args_value.c:261`、`xl6_cgen_value_object.c:59`、`xl6_cgen_value_index.c:389` |
| 对应生成的 C 代码 | `xrtTypeInt()` / `xl6_type_desc_XL_Class_Person()` / `xl6_runtime_type_desc_ABCD()` |
| 中间形态伪代码 | `typeDescRef(type) -> builtin/ref/generated/0` |
| 实现同等功能手写的 C 代码 | 手写 C 需要知道每种类型的元数据入口函数。 |
| 额外依赖的封装代码 | class/union/native-backed class box runtime 或 runtime container type desc。 |

## CGEN-ABI-015 runtime type desc 依赖原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `array<Box::<int>>` |
| 生成规则描述 | 运行时容器 type desc 生成前，会递归输出其类型参数所依赖的 class/union/native-backed class type desc 原型。容器嵌套时继续递归，确保后续静态初始化代码能引用依赖函数。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:414`、`xl6_cgen_abi_signature.c:427`、`xl6_cgen_abi_signature.c:443` |
| 对应生成的 C 代码 | `static const xrt_type_desc* xl6_type_desc_XL_Class_Box__int(void);` |
| 中间形态伪代码 | `emit dependency prototypes for generic args before container desc` |
| 实现同等功能手写的 C 代码 | 手写 C 需要把被引用的 type desc 函数原型放在前面。 |
| 额外依赖的封装代码 | runtime type desc 批量输出器。 |

## CGEN-ABI-016 runtime type desc 函数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `array<int>` 作为运行时可见类型 |
| 生成规则描述 | 每个需要运行时描述的容器类型生成一个静态函数，返回静态 `xrt_type_desc`。若有泛型参数，函数内有静态 `args[]` 和 `xrt_generic_type`，第一次调用时填充参数 type desc。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:451`、`xl6_cgen_abi_signature.c:460`、`xl6_cgen_abi_signature.c:483` |
| 对应生成的 C 代码 | `static const xrt_type_desc* xl6_runtime_type_desc_ABCD(void) { static const xrt_type_desc desc = {...}; return &desc; }` |
| 中间形态伪代码 | `runtimeTypeDesc(T<Args>) -> static desc + lazy args init` |
| 实现同等功能手写的 C 代码 | 手写 C 需要定义静态 desc 和泛型参数数组。 |
| 额外依赖的封装代码 | 参数类型的 type desc 引用。 |

## CGEN-ABI-017 runtime type desc 批量输出顺序

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 任意触发 runtime type desc 的代码 |
| 生成规则描述 | 批量输出分三段：先输出依赖 type desc 原型，再输出当前 runtime type desc 原型，最后输出函数体。模块生成中该批量输出位于 xvalue runtime 之后、class/native-backed class/union 声明之前。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:498`、`xl6_cgen_module.c:94`、`xl6_cgen_module.c:98` |
| 对应生成的 C 代码 | `dependency prototypes; runtime prototypes; runtime desc bodies;` |
| 中间形态伪代码 | `emitDeps(); emitProtos(); emitBodies();` |
| 实现同等功能手写的 C 代码 | 手写 C 也必须保证被引用 type desc 的声明在使用前。 |
| 额外依赖的封装代码 | runtime type desc 注册/收集表。 |

## CGEN-ABI-018 type(expr) 静态类型值

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `type(123)` / `type("abc")` / `type(obj)` |
| 生成规则描述 | `type(expr)` 被普通 call 分派拦截。若表达式类型不是动态 `var`，编译期推断类型并直接输出对应 type id 常量。容器基础类型在 type value 场景会折叠为基础容器类型，避免把 `array<int>` 当作独立 type 字面量。若无法克隆/推断表达式类型，兜底诊断为 `cannot infer type for type(expr)`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:257`、`xl6_cgen_abi_signature.c:525`、`xl6_cgen_abi_signature.c:548`、`xl6_cgen_abi_signature.c:555` |
| 对应生成的 C 代码 | `0x0123456789ABCDEFULL` |
| 中间形态伪代码 | `type(expr) -> compileTimeTypeId(infer(expr))` |
| 实现同等功能手写的 C 代码 | 手写 C 通常用枚举或常量表达式。 |
| 额外依赖的封装代码 | 类型推断；无运行时 helper。 |

## CGEN-ABI-019 type(var) 动态类型值

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `var v = 1` / `type(v)` |
| 生成规则描述 | 如果 `type(expr)` 的表达式类型是 `var`，生成运行时调用 `xl6_xrt_type_value_id`。表达式如果是借用 xvalue，则传 `false` 避免释放；否则传 `true` 表示 helper 接管临时值释放。 |
| 生成器文件名和行号 | `xl6_cgen_abi_signature.c:548`、`xl6_cgen_abi_signature.c:562` |
| 对应生成的 C 代码 | `xl6_xrt_type_value_id(v, false)` / `xl6_xrt_type_value_id(expr, true)` |
| 中间形态伪代码 | `if expr is var: runtimeTypeId(expr, ownsTemp)` |
| 实现同等功能手写的 C 代码 | C 中需要查看 xvalue 的运行时 type desc 或 tag。 |
| 额外依赖的封装代码 | xvalue runtime 的 type value helper。 |

## CGEN-ABI-020 class record copy 与 function 字段保留

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class 进入 `var` 或被 xvalue 装箱 |
| 生成规则描述 | class record box runtime 生成 `xl6_type_copy_*`。普通情况下 `memcpy`；如果 class 需要 destroy，会在复制后对 function 字段执行 retain，避免装箱副本持有悬空闭包环境。src 为空时用 `memset` 清零。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:288`、`xl6_cgen_decl_proto.c:313`、`xl6_cgen_decl_proto.c:321` |
| 对应生成的 C 代码 | `memcpy(dst, src, sizeof(XL_Class_Person)); xl6_retain_function_fields(dst);` |
| 中间形态伪代码 | `copyRecord(dst, src); if needsDestroy: retain function fields` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式复制并处理 function 字段引用计数。 |
| 额外依赖的封装代码 | record box runtime、function 字段 retain 代码。 |

## CGEN-ABI-021 class record drop

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 被装箱 class 的 xvalue 生命周期结束 |
| 生成规则描述 | 如果 class 需要 destroy，record box runtime 会生成 `xl6_type_drop_*`，它调用对应 class destroy。无需 destroy 的 class 不生成 drop 函数，type ops 中 drop 槽位为 `0`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:329`、`xl6_cgen_decl_proto.c:344` |
| 对应生成的 C 代码 | `static void xl6_type_drop_XL_Class_Person(void* obj) { xl_class_Person__destroy((XL_Class_Person*)obj); }` |
| 中间形态伪代码 | `if needsDestroy: drop(obj) -> Class.destroy(obj)` |
| 实现同等功能手写的 C 代码 | C 手写 xvalue record 时也要提供 drop hook。 |
| 额外依赖的封装代码 | class destroy 函数。 |

## CGEN-ABI-022 record compare/hash

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class/union 进入 set/dict key 或需要比较/hash 的 runtime 路径 |
| 生成规则描述 | record compare 处理指针相等、空指针排序，再对 record 内存做 `memcmp`。hash 对对象内存调用 `xrtHash64`。class 和 union 均采用该基本策略。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:354`、`xl6_cgen_decl_proto.c:373`、`xl6_cgen_decl_proto.c:480` |
| 对应生成的 C 代码 | `return memcmp(a, b, sizeof(T));` / `return xrtHash64((const char*)obj, sizeof(T));` |
| 中间形态伪代码 | `compare raw bytes; hash raw bytes` |
| 实现同等功能手写的 C 代码 | 手写 C 可写 `memcmp` 和 hash，但要注意 padding 带来的稳定性风险。 |
| 额外依赖的封装代码 | type ops compare/hash 函数。 |

## CGEN-ABI-023 record box/unbox

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class/union 值转换为 `var` 或从 `var` 转回具体类型 |
| 生成规则描述 | box 使用 `xvoCreateRecord(type, obj)`。unbox 检查 value、out、type，确认实际 type desc 与期望一致，取 record data 后调用 copy 函数写入输出地址。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:386`、`xl6_cgen_decl_proto.c:400`、`xl6_cgen_decl_proto.c:509` |
| 对应生成的 C 代码 | `xvoCreateRecord(type, obj); xl6_type_copy_T(out, data);` |
| 中间形态伪代码 | `box(record) -> xvalue record; unbox(value,T) -> copy data` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己校验 type desc 并复制 record data。 |
| 额外依赖的封装代码 | record type ops 和 type desc。 |

## CGEN-ABI-024 record type ops 与 type desc

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class/union 成为运行时一等类型 |
| 生成规则描述 | record runtime 生成 `xrt_type_ops`，包含 copy、可选 drop、compare、hash、box、unbox。随后生成静态 `xrt_type_desc` 函数，kind 为 `XL6_XRT_TYPE_KIND_RECORD`，记录 xlang 源名、C 类型名、大小、对齐和 ops 指针。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:415`、`xl6_cgen_decl_proto.c:432`、`xl6_cgen_decl_proto.c:459` |
| 对应生成的 C 代码 | `static const xrt_type_ops xl6_type_ops_T = {...}; static const xrt_type_desc* xl6_type_desc_T(void) { static const xrt_type_desc desc = {...}; return &desc; }` |
| 中间形态伪代码 | `typeOps = {copy,drop,compare,hash,box,unbox}; desc = {kind=record, ops}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要完整填写 `xrt_type_ops` 和 `xrt_type_desc`。 |
| 额外依赖的封装代码 | record box runtime。 |

## CGEN-ABI-025 class 默认值 helper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | class 默认值、class 装箱默认对象 |
| 生成规则描述 | class record runtime 会生成默认值函数，先 `{0}`，再调用 class 字段默认初始化逻辑，最后返回 value。这样 boxed/default class 值与普通声明初始化保持一致。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:446`、`xl6_cgen_decl_class_layout.c:426` |
| 对应生成的 C 代码 | `static XL_Class_Config xl6_default_XL_Class_Config(void) { XL_Class_Config value = {0}; value.port = 8080; return value; }` |
| 中间形态伪代码 | `defaultClass() -> zero + field defaults` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护一个 init-default 函数。 |
| 额外依赖的封装代码 | class 字段默认初始化生成器。 |

## CGEN-ABI-026 plain record box runtime

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `union ... end` 进入 xvalue |
| 生成规则描述 | union 使用 plain record runtime。它生成 copy、compare、hash、box、unbox、type ops、type desc，但没有 class destroy/drop，也没有 class 默认值 helper。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:459`、`xl6_cgen_decl_union.c:100` |
| 对应生成的 C 代码 | `static const xrt_type_desc* xl6_type_desc_XL_Union_U(void) { ... kind RECORD ... }` |
| 中间形态伪代码 | `union -> plain record ops without drop` |
| 实现同等功能手写的 C 代码 | 手写 C 为 union 提供 record type ops，但不需要 destroy hook。 |
| 额外依赖的封装代码 | plain record box runtime。 |

## CGEN-ABI-027 handle box drop 与句柄清理

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | native-backed class 值进入 `var` 后释放 |
| 生成规则描述 | native-backed class handle box runtime 的 copy 只复制 `void*`。若 native-backed class 声明了 destroy，则生成 drop hook：把 obj 看作 `void**`，非空时调用 destroy 并把 handle 清零；无 destroy 时 drop 槽位为 `0`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:563`、`xl6_cgen_decl_proto.c:577`、`xl6_cgen_decl_proto.c:589` |
| 对应生成的 C 代码 | `if (handle != 0 && *handle != 0) { file_close(*handle); *handle = 0; }` |
| 中间形态伪代码 | `drop(handleBox) -> native-backed class.destroy(handle); handle = null` |
| 实现同等功能手写的 C 代码 | 手写 C 要显式区分 borrowed handle 和 owned handle 的释放策略。 |
| 额外依赖的封装代码 | native-backed class destroy C 原型。 |

## CGEN-ABI-028 handle compare/hash/box/unbox/type desc

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | native-backed class 值作为 `var`、set/dict key 或运行时类型值 |
| 生成规则描述 | handle compare 按 `uintptr_t` 比较 handle 地址；hash 对 handle 指针值本身 hash；box 使用 `xvoCreateHandle`，标记 borrowed 和 nullable；unbox 校验 type desc 后把 `xvoGetHandleData` 写回 native-backed class 的 `.handle`。type desc kind 为 `XL6_XRT_TYPE_KIND_HANDLE`，大小和对齐都是 `sizeof(void*)`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:602`、`xl6_cgen_decl_proto.c:616`、`xl6_cgen_decl_proto.c:623`、`xl6_cgen_decl_proto.c:638`、`xl6_cgen_decl_proto.c:667` |
| 对应生成的 C 代码 | `xvoCreateHandle(type, value.handle, XL6_XRT_HANDLE_FLAG_BORROWED | XL6_XRT_HANDLE_FLAG_NULLABLE);` |
| 中间形态伪代码 | `native-backed class <-> xvalue handle with type desc check` |
| 实现同等功能手写的 C 代码 | 手写 C 需要提供 handle 的 compare/hash/box/unbox/type_desc 全套 hooks。 |
| 额外依赖的封装代码 | handle box runtime。 |
