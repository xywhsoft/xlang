# DEMO5 release/examples 范例移植清单

> 生成时间：2026-06-19
>
> 分析范围：`demo5/release/examples`
>
> 目标：从 DEMO5 的范例库中筛选可迁移到 DEMO6 的素材，区分“当前可移植”“高价值但需等标准库/高级特性”“不建议直接移植”的内容。

## 1. 总体结论

`demo5/release/examples` 更像 DEMO5 的全量回归素材库，而不是单纯用户范例目录。
当前目录中约有：

- `.xl` 文件：942 个
- `.c` 文件：14 个
- `package.json`：39 个
- `.dll`：11 个
- 还包含 OpenCV、ONNX Runtime、RapidOCR 等第三方库素材。

对 DEMO6 当前阶段来说，最适合移植的是：

- `full` 目录中的语言基础、表达式、控制流、函数、闭包、泛型、容器、class/custom 子集。
- `lexer`、`parser`、`sem` 中可作为负例或诊断范例的少量文件。
- `native` 中的 extern C / extern var / primitive width / loadlib 思路，但需要改成 DEMO6 的声明和构建口径。
- `module`、`xle_*` 目录中的包/扩展库结构理念，可作为后续 XAPI/import/package 设计参考，不建议直接迁移 demo5 的 `xle-1` 格式。

不适合当前直接移植的是：

- `async`、`task`、`generator`：DEMO6 尚未实现 async/await/yield/task/generator。
- try/catch/finally/throw 相关范例：DEMO6 当前尚未实现异常系统。
- `object`、`new/delete`、动态元对象、弱引用等范例：DEMO6 当前阶段明确不实现 object。
- `#binding` 自动解析 C 头文件路线：DEMO6 当前方向是 XAPI/声明层，不建议复刻 demo5 的 C 头解析机制。
- `xle_official_cloud`、OpenCV/ONNX/RapidOCR/Redis 等大型扩展库 smoke：价值很高，但应该等 import/package/custom/XAPI 稳定后作为第三阶段素材。

## 2. 目录概览

| 目录 | 文件数 | `.xl` 数 | 判断 |
| --- | ---: | ---: | --- |
| `full` | 396 | 393 | 最大的语言功能素材库，第一批移植重点 |
| `native` | 142 | 123 | 标准库、C 绑定、系统能力素材，需拆分筛选 |
| `sem` | 79 | 79 | 语义负例素材，适合补充 DEMO6 诊断测试 |
| `lexer` | 3 | 3 | 可移植为词法 smoke/负例 |
| `parser` | 1 | 1 | 可移植为语法错误负例 |
| `lower` | 2 | 2 | 可参考中文标识符/基础控制流 |
| `generator` | 2 | 2 | yield/generator，暂缓 |
| `async` | 159 | 159 | async/await/try/yield，暂缓 |
| `task` | 5 | 5 | task runtime，暂缓 |
| `module` | 12 | 12 | 模块/export/import 思路有参考价值，语法需重做 |
| `project` | 21 | 11 | 项目结构/package 搜索路径参考，暂缓 |
| `xle*` 系列 | 多个 | 多个 | DEMO5 扩展包系统素材，作为 XAPI/package 参考 |
| `sqlite` / `redis` / `miniz` | 少量 | 少量 | 适合未来扩展库 smoke，不适合当前第一批 |
| `opencv` / `onnxruntime` / `rapidocr` | 多个 | 多个 | 大型 native extlib 范例，后续阶段参考 |
| `libs` | 401 | 3 | 第三方库头文件/二进制/素材，不直接迁移 |

## 3. DEMO5 到 DEMO6 的通用改写规则

大多数可移植范例仍需语法迁移，不能直接复制。

函数返回类型：

```xlang
// DEMO5
func add(int a, int b) as int
end

// DEMO6
func add(int a, int b) -> int
end
```

多返回值：

```xlang
// DEMO5
func pair() as int, string
end

// DEMO6
func pair() -> (int, string)
end
```

字段声明：

```xlang
// DEMO5
var x as int

// DEMO6
int x
```

function 类型：

```xlang
// DEMO5
function<int(int,int)> fn

// DEMO6
function(int, int) -> int fn
```

显式泛型调用：

```xlang
// DEMO5
id<int>(1)

// DEMO6
id::<int>(1)
```

main 入口：

```xlang
// 建议迁移成 DEMO6 当前入口约定
func main(...) -> int
    return 0
end
```

容器字面量注意点：

- DEMO5 的 `list{1, 2}` 需要按 DEMO6 口径改成 `list[0: 1, 1: 2]` 或 `[0: 1, 1: 2]`。
- DEMO6 中 `{}` 默认是 dict。
- DEMO6 中空 set 必须写 `set{}`。
- DEMO6 中 typed container 推荐显式写 `array<T>`、`list<T>`、`set<T>`、`dict<T>`。

不应直接迁移的语法：

- `#binding`
- demo5 的 `#loadlib` 指令式写法
- `function<T(T)>`
- `extends object`
- `new` / `delete`
- backtick 模板字符串
- `assert`
- `try` / `catch` / `finally` / `throw`
- `async` / `await` / `yield`

## 4. 第一批建议移植：DEMO6 当前已具备基础能力

这一批适合转成 `demo6/examples/feature_suite` 或 `demo6/test/cases`，主要工作是语法改写和少量断言宿主替换。

### 4.1 词法、布局、基础声明

建议移植：

- `lexer/minimal.xl`
- `lexer/keywords.xl`
- `lexer/unterminated_string.xl`
- `parser/syntax_error.xl`
- `lower/basic.xl`
- `full/leading_blank_lines_basic.xl`
- `full/typed_decl_basic.xl`
- `full/auto_decl_basic.xl`
- `full/typedef_basic.xl`
- `full/public_types_basic.xl`
- `full/exact_numeric_types_basic.xl`
- `full/int_base_literals_basic.xl`
- `full/raw_string_basic.xl`
- `full/point_basic.xl`
- `full/run_basic.xl`

迁移价值：

- 补充 UTF-8/中文标识符范例。
- 补充 layout、空行、基础声明范例。
- 补充不同整数进制和精确数值类型示例。
- 词法/语法负例可进入 `test/cases`。

注意：

- `raw_string_basic.xl` 中的 demo5 backtick 字符串需要改成 DEMO6 raw string 或普通转义字符串。

### 4.2 运算符和表达式

建议移植：

- `full/assign_basic.xl`
- `full/assign_plus_basic.xl`
- `full/assign_minus_basic.xl`
- `full/assign_mul_basic.xl`
- `full/assign_div_basic.xl`
- `full/assign_mod_basic.xl`
- `full/assign_pow_basic.xl`
- `full/bit_ops_basic.xl`
- `full/logic_basic.xl`
- `full/power_basic.xl`
- `full/approx_basic.xl`
- `full/ternary_basic.xl`
- `full/if_inline_basic.xl`
- `full/if_inline_else_basic.xl`
- `full/index_assign_basic.xl`

部分可移植但需要重写：

- `full/coalesce_basic.xl`
- `full/null_assign_basic.xl`

原因：

- 原文件使用 demo5 的 `error` / `Error(...)` 对象模型，DEMO6 可改成 `var`、`null`、dict 或 class 来展示 `??` / `?=`。

迁移价值：

- demo6 已有这些运算符，但 demo5 示例更碎片化，适合补成“每个运算符一个小范例”。

### 4.3 控制流

建议移植：

- `full/while_basic.xl`
- `full/do_while_basic.xl`
- `full/for_in_range.xl`
- `full/for_in_containers_basic.xl`
- `full/loop_jump_levels.xl`

迁移价值：

- 补齐 while/do while/for range/多层 break continue 的独立范例。

注意：

- `for_in_range.xl` 中 demo5 的 `for index, value in range(...)` 可保留主题，但需确认 DEMO6 range for-in 的 index/value 语义；若 DEMO6 当前只需要值遍历，应改成更明确的 DEMO6 写法。

### 4.4 函数、多返回值、参数

建议移植：

- `full/function_forward_call_basic.xl`
- `full/multi_return_basic.xl`
- `full/multi_return_call_expand_basic.xl`
- `full/named_args_default.xl`
- `full/vararg_array_basic.xl`
- `full/vararg_dict_basic.xl`
- `full/overload_runtime.xl`

暂缓或改写较多：

- `full/vararg_array_spread_basic.xl`
- `full/vararg_dict_spread_basic.xl`

原因：

- demo5 的 spread 形式和 DEMO6 当前 parser/call arg 口径不完全一致。
- DEMO6 当前已有 `arg = packed` 方向，可以把 spread 示例改成命名参数传入 `arg` 的 demo6 形式。

迁移价值：

- demo5 的参数示例颗粒度好，适合补成 DEMO6 参数系统展示。

### 4.5 function value、匿名函数、闭包

建议移植：

- `full/function_value_basic.xl`
- `full/function_value_flow_basic.xl`
- `full/function_value_container_basic.xl`
- `full/function_value_multi_return_basic.xl`
- `full/function_value_multi_return_flow_basic.xl`
- `full/function_value_multi_return_var_basic.xl`
- `full/function_value_overload_basic.xl`
- `full/function_value_paren_call_basic.xl`
- `full/anonymous_function_basic.xl`
- `full/anonymous_function_arrow_basic.xl`
- `full/anonymous_function_arg_basic.xl`
- `full/anonymous_function_var_basic.xl`
- `full/anonymous_function_multi_return_basic.xl`
- `full/anonymous_function_container_var_basic.xl`
- `full/closure_basic.xl`
- `full/closure_return_basic.xl`
- `full/closure_state_basic.xl`
- `full/closure_nested_state_basic.xl`
- `full/closure_shared_pair_basic.xl`
- `full/closure_container_var_basic.xl`

需要谨慎迁移：

- `full/closure_mutation_basic.xl`

原因：

- DEMO6 当前对捕获变量赋值有限制，需确认例子是否符合当前闭包设计。

迁移价值：

- DEMO6 已经实现 function value/闭包，这一组可以显著增强范例覆盖。

### 4.6 容器基础

建议移植：

- `full/array_basic.xl`
- `full/list_basic.xl`
- `full/set_basic.xl`
- `full/dict_basic.xl`
- `full/bare_container_literals_basic.xl`
- `full/typed_empty_container_decl_basic.xl`
- `full/index_assign_basic.xl`
- `full/for_in_containers_basic.xl`
- `full/generic_array_first.xl`
- `full/generic_nested_container.xl`
- `full/generic_wrap_array.xl`

可移植但需要方法口径重写：

- `full/container_append_basic.xl`
- `full/container_clear_basic.xl`
- `full/container_remove_basic.xl`
- `full/container_chain_append_clear.xl`
- `full/container_methods_basic.xl`
- `full/container_keys_values_items.xl`
- `full/container_take_runtime.xl`
- `full/property_length_basic.xl`

原因：

- DEMO6 当前主要已稳定的是字面量、索引、for-in、`length`、typed container 和 `in`。
- demo5 的 `.append()`、`.clear()`、`.remove()`、`.len()`、`.has()`、`.join()` 等方法并不都属于 DEMO6 当前已确认接口。
- 可改成 DEMO6 当前已有的字面量/索引/for-in/length 风格。

### 4.7 泛型

建议移植：

- `full/generic_function_basic.xl`
- `full/generic_explicit_type_arg.xl`
- `full/generic_multi_type_args.xl`
- `full/generic_same_type.xl`
- `full/generic_array_first.xl`
- `full/generic_nested_container.xl`
- `full/generic_wrap_array.xl`
- `full/generic_function_value.xl`
- `full/generic_list_type_method.xl`

适合作为负例移植：

- `full/generic_infer_conflict_error.xl`
- `full/generic_infer_missing_error.xl`
- `full/generic_instance_assign_error.xl`
- `full/generic_instance_binary_error.xl`
- `full/generic_instance_local_decl_error.xl`
- `full/generic_instance_return_error.xl`
- `full/generic_instance_ternary_error.xl`
- `full/generic_instance_unary_error.xl`

迁移要求：

- `foo<int>()` 必须改成 `foo::<int>()`。
- `function<T(T)>` 必须改成 `function(T) -> T`。

迁移价值：

- demo6 泛型已经比较完整，这批可以补充“成功示例 + 失败示例”的成套展示。

### 4.8 类型值、类型转换、type 相关

建议移植：

- `full/type_value_basic.xl`
- `full/type_ctor_basic.xl`
- `full/type_methods_basic.xl`
- `full/var_type_methods_basic.xl`
- `full/var_container_type_methods_basic.xl`
- `full/typedef_basic.xl`

需要拆分或延后：

- `full/type_declared_meta_basic.xl`
- `full/type_declared_names_basic.xl`
- `full/type_declares_meta_basic.xl`
- `full/type_dynamic_*`
- `full/type_meta_bridge_basic.xl`
- `full/type_reflect_class_runtime.xl`
- `full/type_runtime_api_basic.xl`

原因：

- DEMO6 当前已支持 `type(expr)`、类型字面量、基础 type 比较。
- DEMO5 的动态类型描述、meta API、动态方法绑定属于更高级反射/元编程，不适合当前直接迁移。

### 4.9 class 子集

建议移植或参考：

- `full/class_stage1_create_basic.xl`
- `full/class_stage1_create_runtime.xl`
- `full/class_stage1_method_runtime.xl`
- `full/class_stage1_inherit_layout_basic.xl`
- `full/class_stage1_base_create_chain_runtime.xl`
- `full/class_stage1_base_param_create_runtime.xl`
- `full/class_stage1_base_destroy_runtime.xl`
- `full/class_stage1_field_init_runtime.xl`
- `full/class_stage1_local_default_create_runtime.xl`
- `full/class_stage1_destroy_runtime.xl`
- `full/class_stage1_loop_break_destroy_runtime.xl`
- `full/class_stage1_loop_continue_destroy_runtime.xl`
- `full/class_stage1_return_destroy_runtime.xl`

迁移要求：

- 移除 `extends object`。
- 字段声明改成 DEMO6 口径。
- 构造/析构保留 `create`/`destroy` 主题。
- 对 move/copy/delete/object 相关语义要单独审查。

暂缓：

- `full/class_move_*`
- `full/class_nocopy_decl.xl`
- `full/class_stage1_operator_runtime.xl`
- `full/class_stage1_call_operator_runtime.xl`
- `full/class_stage1_index_operator_runtime.xl`
- `full/class_stage1_union_*`
- `full/class_stage1_generator_*`
- `full/class_stage1_try_*`
- `full/class_stage1_throw_destroy_runtime.xl`
- `full/class_stage1_var_dispatch_basic.xl`
- `full/class_stage1_implicit_object_runtime.xl`

原因：

- 涉及 move 语义、运算符重载、union、generator、异常、object/dynamic dispatch 等 DEMO6 当前未完成或未设计能力。

### 4.10 custom 子集

建议参考移植：

- `full/custom_basic_runtime.xl`
- `full/custom_payload_runtime.xl`
- `full/custom_extern_create_destroy_runtime.xl`
- `full/custom_extern_method_runtime.xl`
- `full/custom_destroy_runtime.xl`
- `full/custom_var_dispatch_runtime.xl`
- `full/custom_xrt_resource_basic.xl`

需要改写或暂缓：

- `full/custom_get_let_dispatch_basic.xl`
- `full/custom_index_dispatch_basic.xl`
- `full/custom_operator_basic.xl`
- `full/custom_var_index_dispatch_basic.xl`
- `full/custom_var_operator_basic.xl`
- `full/custom_xrt_dict_basic.xl`

原因：

- DEMO6 当前 custom 方向是轻量 C 句柄类，不走 demo5 的 get/let/operator/dynamic dispatch 路线。
- extern custom 方法很有迁移价值，但需要改成 DEMO6 的 C 混编声明与 host 文件。

### 4.11 C 混编基础

建议移植或重做：

- `native/extern_atoi_basic.xl`
- `native/extern_var_basic.xl`
- `native/extern_var_bool_float_basic.xl`
- `native/extern_var_exact_numeric_basic.xl`
- `native/extern_var_string_basic.xl`
- `native/extern_var_point_basic.xl`
- `native/extern_type_basic.xl`
- `native/extern_type_function_container_basic.xl`
- `native/extern_type_method_basic.xl`
- `native/extern_func_class_abi_basic.xl`
- `native/extern_func_class_abi_layout_basic.xl`
- `native/extern_func_class_abi_nested_basic.xl`
- `native/extern_same_name_basic.xl`

迁移价值：

- DEMO6 已经支持 extern func/var/export/header/mixed build，这组能补充 C 互操作范例。

迁移要求：

- `#loadlib` 改为 `xl run/build --loadlib` 或 `--link`。
- `#binding` 相关不要直接移植。
- C 头自动解析路线不进入 DEMO6 当前方案。
- class ABI 例子需要按 DEMO6 class/custom 设计重新审查。

## 5. 第二批建议移植：高价值，但依赖标准库/隐性类方法收口

这一批不建议现在马上照搬，但非常适合指导 DEMO6 标准库和隐性类方法建设。

### 5.1 字符串方法

建议参考：

- `full/string_methods_basic.xl`
- `full/string_split_basic.xl`
- `native/str_basic.xl`
- `native/str_plus.xl`
- `native/str_fmt_plus_basic.xl`
- `native/template_render_basic.xl`

涉及能力：

- `trim`
- `trim_left`
- `trim_right`
- `slice`
- `repeat`
- `lower`
- `upper`
- `replace`
- `contains`
- `starts_with`
- `ends_with`
- `index_of`
- `last_index_of`
- `split`
- `split_lines`
- `join`
- 格式化字符串/模板渲染

DEMO6 当前状态：

- 已有 `toString()` 基础隐性类方法。
- 完整字符串方法尚未系统落地。

建议：

- 等 XRT string 能力梳理后再迁移。
- 每个方法做一个小型可运行示例，比 demo5 一坨式测试更适合 demo6 文档。

### 5.2 容器方法

建议参考：

- `full/container_methods_basic.xl`
- `full/container_append_basic.xl`
- `full/container_clear_basic.xl`
- `full/container_remove_basic.xl`
- `full/container_keys_values_items.xl`
- `full/var_container_type_methods_basic.xl`
- `native/container_modules_basic.xl`

涉及能力：

- `append`
- `clear`
- `remove`
- `has`
- `len`
- `length`
- `keys`
- `values`
- `items`
- `join`

DEMO6 当前状态：

- 字面量、索引、for-in、`in`、`length`、typed container 已实现。
- 方法化 API 需要和 XRT 统一命名后再定稿。

### 5.3 标准库模块

建议参考：

- `native/core_len_basic.xl`
- `native/types_basic.xl`
- `native/math_basic.xl`
- `native/rand_basic.xl`
- `native/rand_plus_basic.xl`
- `native/time_basic.xl`
- `native/time_plus_basic.xl`
- `native/time_text.xl`
- `native/fmt_basic.xl`
- `native/hash_basic.xl`
- `native/encode_basic.xl`
- `native/json_basic.xl`
- `native/path_basic.xl`
- `native/path_plus.xl`
- `native/fs_basic.xl`
- `native/fs_plus.xl`
- `native/env_basic.xl`
- `native/os_basic.xl`
- `native/process_basic.xl`
- `native/sys_env_basic.xl`
- `native/sys_file_basic.xl`
- `native/sys_mem_basic.xl`

判断：

- 这些不是语言基础移植，而是未来标准库范例素材。
- 应优先检查 XRT 是否已有对应能力。
- 若 XRT 缺口明显，应先补 XRT，再在 DEMO6 声明/导入。

## 6. 第三批建议移植：XAPI/import/package 成熟后处理

### 6.1 module 目录

建议参考：

- `module/math.xl`
- `module/use_math.xl`
- `module/math_export_func.xl`
- `module/use_math_export_func.xl`
- `module/math_export_values.xl`
- `module/use_math_export_values.xl`
- `module/text_ext_method.xl`
- `module/use_text_ext_method.xl`
- `module/list_string_ext_method.xl`
- `module/use_list_string_ext_method.xl`

迁移价值：

- 展示 import/export、模块成员、扩展方法、导出值。

不直接移植原因：

- DEMO5 的 `export name`、`import math`、`math.name` 模块口径与 DEMO6 当前 XAPI/import 设计不完全一致。
- DEMO6 当前 import 主要自动注入可安全映射的 extern C 函数/变量。

建议：

- 等 DEMO6 XAPI 支持 xlang module export/import 后，重做这一组。

### 6.2 project 目录

建议参考：

- `project/app`
- `project/pkgapp`
- `project/pkgbuild`
- `project/std_priority`
- `project/xlang_home_pkg`
- `project/binding_incpath`
- `project/error_paths`

迁移价值：

- package 搜索路径。
- app/src 布局。
- std 优先级。
- 包内源码。
- 错误路径诊断。

不直接移植原因：

- DEMO6 package resolver 目前只是第一版。
- DEMO5 项目格式与 DEMO6 未来 XAPI/package 格式不同。

### 6.3 xle 系列

建议参考：

- `xle_source`
- `xle_static`
- `xle_native`
- `xle_csource`
- `xle_mixed_source`
- `xle_transitive`
- `xle_versions`
- `xle_official`
- `xle_official_native`

迁移价值：

- source package。
- static/native package。
- C source package。
- mixed source package。
- transitive dependency。
- versioned dependency。
- official package 命名和组织。

不直接移植原因：

- DEMO5 使用 `xle-1` package json。
- DEMO6 当前方向是 `xlang-xapi-1` 接口元数据，不应兼容 DEMO5 包格式。

建议：

- 保留示例目标，不保留格式。
- 未来用 DEMO6 XAPI/package 重新生成 package examples。

## 7. 第四批建议移植：高级语言特性完成后再处理

### 7.1 async / await / task

目录：

- `async`
- `task`

当前状态：

- DEMO6 只保留关键字，未实现 async/await/task runtime。

建议：

- 现在不移植。
- 未来设计 async 时，这批非常有价值，尤其是 await 与 if/while/do while/try/finally 交叉矩阵。

### 7.2 generator / yield

目录/文件：

- `generator/basic.xl`
- `generator/mixed_flow.xl`
- `full/generator_*`

当前状态：

- DEMO6 未实现 `yield`。

建议：

- 等 generator 设计完成后再迁移。

### 7.3 try/catch/finally/throw

建议参考：

- `full/try_catch_basic.xl`
- `full/try_catch_finally.xl`
- `full/try_finally_basic.xl`
- `full/try_finally_escape_basic.xl`
- `full/try_nested_rethrow.xl`
- `full/throw_basic.xl`
- `async` 中大量 try/await 组合

当前状态：

- DEMO6 未实现异常处理。

建议：

- 暂缓。

### 7.4 object / dynamic meta

建议暂缓：

- `full/object_*`
- `full/type_dynamic_*`
- `full/object_meta_*`
- `sem/object_super_without_base_error.xl`

原因：

- DEMO6 当前明确只设计 class/custom，不设计 object。
- demo5 object 体系容易污染 demo6 当前清晰口径。

## 8. 大型扩展库范例

这些不建议现在移植为 demo6 基础范例，但很适合作为未来 extlib/package 的验收素材。

### 8.1 可作为中期目标的扩展库

- `miniz_basic.xl`
- `sqlite/sqlite_basic.xl`
- `md4c/*`
- `stb_truetype/stb_truetype_smoke.xl`

原因：

- 规模相对可控。
- 适合验证 custom、C 声明、package、资源释放。

### 8.2 需要外部服务或较重依赖

- `redis/*`
- `opencv/*`
- `onnxruntime/*`
- `rapidocr/*`
- `dinov3/*`

建议：

- 不进入 DEMO6 第一批 examples。
- 等 package/extlib 机制成熟后，作为官方扩展库 smoke。
- Redis 还依赖本地服务或模拟服务，适合 CI 条件化测试，不适合基础 smoke。

## 9. 语义负例可迁移清单

`sem` 目录中以下负例主题适合迁移到 DEMO6 测试体系，许多 DEMO6 已有类似测试，但仍可查漏补缺。

建议对照迁移：

- `sem/assign_const_error.xl`
- `sem/auto_param_error.xl`
- `sem/auto_return_error.xl`
- `sem/logic_type_error.xl`
- `sem/not_type_error.xl`
- `sem/ternary_type_error.xl`
- `sem/typedef_unknown_error.xl`
- `sem/type_ctor_arity_error.xl`
- `sem/no_overload.xl`
- `sem/overload.xl`
- `sem/multi_return_single_bind_error.xl`
- `sem/vararg_mixed_pack_error.xl`
- `sem/vararg_reserved_arg_error.xl`
- `sem/implicit_float_to_int_assign_error.xl`
- `sem/implicit_float_to_int_return_error.xl`
- `sem/implicit_if_number_condition_error.xl`
- `sem/implicit_while_number_condition_error.xl`
- `sem/implicit_overload_ambiguous_error.xl`
- `sem/index_assign_plus_error.xl`

暂缓负例：

- `sem/await_outside_async.xl`
- `sem/yield_top_level_error.xl`
- `sem/yield_expr_value_error.xl`
- `sem/generator_param_unknown_error.xl`
- `sem/object_super_without_base_error.xl`
- `sem/delete_requires_object_lvalue_error.xl`
- `sem/new_requires_call_error.xl`
- `sem/legacy_link_error.xl`

原因：

- 涉及 DEMO6 尚未实现的 async/generator/object/delete/new/legacy link。

## 10. 建议的移植顺序

### 第一轮：补 DEMO6 feature suite

优先移植：

- 基础声明/字面量/精确数值类型。
- 运算符和控制流。
- 多返回值、命名参数、可选参数、vararg。
- function value、匿名函数、闭包。
- 泛型函数和泛型 class 相关成功例。
- 容器字面量、索引、for-in、typed container。

目标：

- 增强 `demo6/examples/feature_suite` 的展示颗粒度。
- 每个文件控制在一个主题内。

### 第二轮：补 DEMO6 test/cases 负例

优先迁移：

- 语义错误。
- 泛型错误。
- 重载错误。
- 参数错误。
- 类型转换错误。

目标：

- 用 demo5 sem 目录查漏补缺。
- 不照搬 demo5 不一致口径。

### 第三轮：C 混编范例

优先移植：

- extern function。
- extern var。
- primitive width。
- bool/string/point。
- `.xl + .c` mixed build。
- export header host。

目标：

- 形成 demo6 风格的 C 混编范例集。

### 第四轮：XAPI/package 范例

优先重做：

- source package。
- C source package。
- mixed source package。
- transitive dependency。
- versioned dependency。

目标：

- 不兼容 DEMO5 `xle-1`，按 DEMO6 `xlang-xapi-1` 重建。

### 第五轮：高级特性范例

等待特性实现后再迁移：

- async/await。
- task。
- generator/yield。
- try/catch/finally/throw。
- object。
- 反射/meta。
- 大型 extlib。

## 11. 不建议迁移的 DEMO5 路线

以下内容只建议作为历史参考，不建议进入 DEMO6 当前设计：

- demo5 专用标准库 API 名称。
- demo5 `xle-1` package json 格式。
- `#binding` 自动解析 C 头文件。
- `extends object` 作为默认 OOP 基类。
- `new/delete` 对象生命周期语法。
- object 动态字段和 meta 方法体系。
- backtick 模板字符串。
- `func ... as Type` 旧返回类型语法。
- `function<T(T)>` 旧函数类型语法。
- `foo<int>()` 旧泛型调用语法。

## 12. 推荐优先移植文件小清单

如果只先做一批最有性价比的范例，建议从这些开始：

- `full/int_base_literals_basic.xl`
- `full/exact_numeric_types_basic.xl`
- `full/typed_decl_basic.xl`
- `full/typedef_basic.xl`
- `full/type_value_basic.xl`
- `full/assign_plus_basic.xl`
- `full/assign_pow_basic.xl`
- `full/bit_ops_basic.xl`
- `full/ternary_basic.xl`
- `full/if_inline_basic.xl`
- `full/while_basic.xl`
- `full/do_while_basic.xl`
- `full/loop_jump_levels.xl`
- `full/array_basic.xl`
- `full/dict_basic.xl`
- `full/set_basic.xl`
- `full/bare_container_literals_basic.xl`
- `full/for_in_containers_basic.xl`
- `full/multi_return_basic.xl`
- `full/named_args_default.xl`
- `full/vararg_array_basic.xl`
- `full/vararg_dict_basic.xl`
- `full/function_value_basic.xl`
- `full/function_value_multi_return_basic.xl`
- `full/anonymous_function_arrow_basic.xl`
- `full/closure_basic.xl`
- `full/closure_state_basic.xl`
- `full/generic_function_basic.xl`
- `full/generic_explicit_type_arg.xl`
- `full/generic_nested_container.xl`
- `full/generic_function_value.xl`
- `full/class_stage1_create_basic.xl`
- `full/class_stage1_method_runtime.xl`
- `full/class_stage1_base_create_chain_runtime.xl`
- `full/custom_extern_method_runtime.xl`
- `native/extern_atoi_basic.xl`
- `native/extern_var_basic.xl`
- `native/extern_var_exact_numeric_basic.xl`

这批基本覆盖 DEMO6 当前已经实现的语言骨架，也不会把 async/object/package/异常等高级口径提前混入。
