# xlang 示例脚本生成方案

## 一、现有目录结构

当前示例目录结构如下：
```
demo/release/examples/
├── 01_basics/
├── 02_operators/
├── 03_control_flow/
├── 04_functions/
├── 05_strings/
├── 06_containers/
├── 07_classes/
├── 08_errors/
├── 09_modules/
├── 10_async/
├── 11_threads/
└── 12_io/
```

## 二、示例脚本类型分类

1. **基础演示型**：展示基本语法、语句使用方法
2. **边界测试型**：测试功能的边界情况、异常处理
3. **功能模块型**：完整展示某个功能模块的能力
4. **复杂组合型**：多个功能的综合运用，展示完整应用场景

## 三、详细脚本清单

### 1. 基础语法模块 (01_basics/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_hello_world.xl | 基础演示 | 基本输出语句 |
| 002 | 002_variables_types.xl | 基础演示 | 变量定义和类型系统 |
| 003 | 003_constants.xl | 基础演示 | 常量定义和使用 |
| 004 | 004_number_literals.xl | 基础演示 | 各种数字字面量写法 |
| 005 | 005_type_conversion.xl | 基础演示 | 类型转换功能 |
| 006 | 006_scope.xl | 基础演示 | 作用域规则 |
| 007 | 007_identifiers.xl | 基础演示 | 标识符命名规则（含中文） |
| 008 | 008_default_values.xl | 基础演示 | 变量默认值 |
| 009 | 009_comments.xl | 基础演示 | 注释语法 |
| 010 | 010_multiple_assignment.xl | 基础演示 | 多变量赋值 |
| 011 | 011_type_of.xl | 基础演示 | type() 函数使用 |
| 012 | 012_is_operator.xl | 基础演示 | is 运算符使用 |
| 013 | 013_basic_output.xl | 基础演示 | 输出函数的各种用法 |

### 2. 运算符模块 (02_operators/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_arithmetic.xl | 基础演示 | 算术运算符 |
| 002 | 002_comparison.xl | 基础演示 | 比较运算符 |
| 003 | 003_logical.xl | 基础演示 | 逻辑运算符 |
| 004 | 004_bitwise.xl | 基础演示 | 位运算符 |
| 005 | 005_null_operators.xl | 基础演示 | null 相关运算符（??, ?=） |
| 006 | 006_ternary_special.xl | 基础演示 | 三目运算符 |
| 007 | 007_assignment.xl | 基础演示 | 赋值运算符 |
| 008 | 008_unary.xl | 基础演示 | 一元运算符 |
| 009 | 009_power.xl | 基础演示 | 乘方运算符 |
| 010 | 010_in_operator.xl | 基础演示 | in 运算符 |
| 011 | 011_approximate.xl | 边界测试 | 约等于运算符 ~= |
| 012 | 012_priority.xl | 边界测试 | 运算符优先级测试 |
| 013 | 013_chain_operators.xl | 边界测试 | 运算符链式使用 |

### 3. 控制流模块 (03_control_flow/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_if_else.xl | 基础演示 | if-else 语句 |
| 002 | 002_switch.xl | 基础演示 | switch 替代方案（使用 if-in） |
| 003 | 003_for_loop.xl | 基础演示 | for-in 循环 |
| 004 | 004_while_loop.xl | 基础演示 | while 循环 |
| 005 | 005_break_continue.xl | 基础演示 | break 和 continue 语句 |
| 006 | 006_with_statement.xl | 基础演示 | with 资源管理 |
| 007 | 007_do_while.xl | 基础演示 | do-while 循环 |
| 008 | 008_if_single_line.xl | 基础演示 | 单行 if 语句 |
| 009 | 009_if_in_range.xl | 功能模块 | if-in-range 高级用法 |
| 010 | 010_nested_loops.xl | 边界测试 | 嵌套循环 |
| 011 | 011_break_multiple.xl | 边界测试 | 多层 break |
| 012 | 012_continue_multiple.xl | 边界测试 | 多层 continue |
| 013 | 013_infinite_loop.xl | 边界测试 | 无限循环处理 |

### 4. 函数模块 (04_functions/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_basic_functions.xl | 基础演示 | 函数定义和调用 |
| 002 | 002_parameters.xl | 基础演示 | 函数参数（位置、命名、默认值） |
| 003 | 003_lambda_closure.xl | 基础演示 | 匿名函数和闭包 |
| 004 | 004_calling_convention.xl | 基础演示 | 调用约定（xcall, cdecl 等） |
| 005 | 005_return_values.xl | 基础演示 | 返回值（单值、多值） |
| 006 | 006_varargs.xl | 基础演示 | 可变参数（数组、字典） |
| 007 | 007_recursion.xl | 功能模块 | 递归函数 |
| 008 | 008_main_function.xl | 功能模块 | 主函数入口 |
| 009 | 009_function_overloading.xl | 功能模块 | 函数重载 |
| 010 | 010_function_pointers.xl | 功能模块 | 函数指针和类型别名 |
| 011 | 011_closure_lifetime.xl | 边界测试 | 闭包生命周期 |
| 012 | 012_high_order_functions.xl | 复杂组合 | 高阶函数 |
| 013 | 013_callback_functions.xl | 复杂组合 | 回调函数 |

### 5. 字符串模块 (05_strings/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_string_literals.xl | 基础演示 | 字符串字面量（单引号、双引号、多行） |
| 002 | 002_template_strings.xl | 基础演示 | 模板字符串 |
| 003 | 003_string_methods.xl | 功能模块 | 字符串方法 |
| 004 | 004_regex.xl | 功能模块 | 正则表达式 |
| 005 | 005_escape_sequences.xl | 基础演示 | 转义序列 |
| 006 | 006_string_concatenation.xl | 基础演示 | 字符串连接 |
| 007 | 007_string_indexing.xl | 基础演示 | 字符串索引和切片 |
| 008 | 008_string_operations.xl | 基础演示 | 字符串运算符 |
| 009 | 009_raw_strings.xl | 基础演示 | 原始字符串 |
| 010 | 010_string_interpolation.xl | 基础演示 | 字符串插值 |
| 011 | 011_empty_strings.xl | 边界测试 | 空字符串处理 |
| 012 | 012_long_strings.xl | 边界测试 | 长字符串处理 |
| 013 | 013_unicode_strings.xl | 边界测试 | Unicode 字符串 |

### 6. 容器模块 (06_containers/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_array.xl | 基础演示 | 数组使用 |
| 002 | 002_list.xl | 基础演示 | 列表使用 |
| 003 | 003_dict.xl | 基础演示 | 字典使用 |
| 004 | 004_set.xl | 基础演示 | 集合使用 |
| 005 | 005_array_operations.xl | 功能模块 | 数组操作 |
| 006 | 006_list_operations.xl | 功能模块 | 列表操作 |
| 007 | 007_dict_operations.xl | 功能模块 | 字典操作 |
| 008 | 008_set_operations.xl | 功能模块 | 集合操作 |
| 009 | 009_container_conversion.xl | 功能模块 | 容器类型转换 |
| 010 | 010_range_type.xl | 基础演示 | range 类型使用 |
| 011 | 011_empty_containers.xl | 边界测试 | 空容器处理 |
| 012 | 012_large_containers.xl | 边界测试 | 大容器性能测试 |
| 013 | 013_nested_containers.xl | 复杂组合 | 嵌套容器 |
| 014 | 014_container_iteration.xl | 复杂组合 | 容器遍历技巧 |
| 015 | 015_array_vs_list.xl | 功能模块 | 数组与列表对比 |

### 7. 类和对象模块 (07_classes/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_class_basic.xl | 基础演示 | 类定义和实例化 |
| 002 | 002_inheritance.xl | 基础演示 | 类继承 |
| 003 | 003_special_features.xl | 基础演示 | 类的特殊特性 |
| 004 | 004_constructors.xl | 基础演示 | 构造函数 |
| 005 | 005_destructors.xl | 基础演示 | 析构函数 |
| 006 | 006_properties.xl | 功能模块 | 属性访问器（get/let） |
| 007 | 007_methods.xl | 功能模块 | 方法定义和调用 |
| 008 | 008_visibility.xl | 功能模块 | 成员可见性（public/private/protected） |
| 009 | 009_static_members.xl | 功能模块 | 静态成员 |
| 010 | 010_super_keyword.xl | 功能模块 | super 关键字使用 |
| 011 | 011_object_class.xl | 功能模块 | object 基类 |
| 012 | 012_dynamic_objects.xl | 功能模块 | 动态对象属性 |
| 013 | 013_method_overriding.xl | 功能模块 | 方法覆盖 |
| 014 | 014_multiple_inheritance.xl | 边界测试 | 多重继承（不支持） |
| 015 | 015_complex_inheritance.xl | 复杂组合 | 复杂继承层次 |
| 016 | 016_class_composition.xl | 复杂组合 | 类组合 |

### 8. 元编程模块 (08_metaprogramming/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_meta_methods.xl | 基础演示 | 元方法基础 |
| 002 | 002_operator_overloading.xl | 功能模块 | 运算符重载 |
| 003 | 003_meta_get_set.xl | 功能模块 | 元方法 get/let |
| 004 | 004_meta_call.xl | 功能模块 | 元方法 call/callself |
| 005 | 005_meta_to_from.xl | 功能模块 | 元方法 to/from 类型转换 |
| 006 | 006_meta_iter_next.xl | 功能模块 | 元方法 iter/next（迭代器） |
| 007 | 007_meta_enter_exit.xl | 功能模块 | 元方法 enter/exit（with 语句） |
| 008 | 008_custom_iterator.xl | 复杂组合 | 自定义迭代器 |
| 009 | 009_operator_chaining.xl | 复杂组合 | 运算符链式调用 |
| 010 | 010_dynamic_metaprogramming.xl | 复杂组合 | 动态元编程 |

### 9. 自定义类型模块 (09_custom/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_custom_basic.xl | 基础演示 | custom 类型基础 |
| 002 | 002_custom_constructors.xl | 基础演示 | custom 构造函数 |
| 003 | 003_custom_methods.xl | 功能模块 | custom 方法 |
| 004 | 004_custom_metamethods.xl | 功能模块 | custom 元方法 |
| 005 | 005_custom_integration.xl | 复杂组合 | custom 与 C 语言集成 |

### 10. 枚举和联合体模块 (10_enums_unions/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_enum_basic.xl | 基础演示 | 枚举类型基础 |
| 002 | 002_enum_underlying.xl | 基础演示 | 枚举底层类型 |
| 003 | 003_enum_string.xl | 基础演示 | 字符串枚举 |
| 004 | 004_enum_flags.xl | 功能模块 | 位标志枚举 |
| 005 | 005_union_basic.xl | 基础演示 | 联合体基础 |
| 006 | 006_union_usage.xl | 功能模块 | 联合体使用 |
| 007 | 007_enum_in_switch.xl | 复杂组合 | 枚举在条件中的使用 |

### 11. 命名空间和导入模块 (11_namespaces/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_namespace_basic.xl | 基础演示 | 命名空间基础 |
| 002 | 002_namespace_nested.xl | 基础演示 | 嵌套命名空间 |
| 003 | 003_using_directive.xl | 基础演示 | using 导入 |
| 004 | 004_namespace_alias.xl | 基础演示 | 命名空间别名 |
| 005 | 005_namespace_resolution.xl | 功能模块 | 命名空间解析 |
| 006 | 006_cross_namespace.xl | 复杂组合 | 跨命名空间调用 |

### 12. 类型别名和函数指针模块 (12_type_system/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_typedef_basic.xl | 基础演示 | typedef 基础 |
| 002 | 002_function_signatures.xl | 功能模块 | 函数签名类型 |
| 003 | 003_function_pointers.xl | 功能模块 | 函数指针 |
| 004 | 004_type_aliases.xl | 功能模块 | 复杂类型别名 |
| 005 | 005_pointer_conversion.xl | 边界测试 | 指针类型转换 |

### 13. 错误处理模块 (13_errors/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_try_catch.xl | 基础演示 | try-catch 异常处理 |
| 002 | 002_finally_block.xl | 基础演示 | finally 块 |
| 003 | 003_throw_error.xl | 基础演示 | 抛出异常 |
| 004 | 004_assert.xl | 基础演示 | assert 断言 |
| 005 | 005_error_types.xl | 功能模块 | 错误类型 |
| 006 | 006_custom_exceptions.xl | 功能模块 | 自定义异常 |
| 007 | 007_error_propagation.xl | 复杂组合 | 错误传播 |
| 008 | 008_exception_safety.xl | 复杂组合 | 异常安全性 |

### 14. 资源管理模块 (14_resources/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_with_basic.xl | 基础演示 | with 语句基础 |
| 002 | 002_with_multiple.xl | 基础演示 | 多个资源管理 |
| 003 | 003_with_alias.xl | 基础演示 | with 语句别名 |
| 004 | 004_custom_resources.xl | 功能模块 | 自定义资源类型 |
| 005 | 005_resource_lifecycle.xl | 功能模块 | 资源生命周期 |
| 006 | 006_exception_in_with.xl | 边界测试 | with 语句中的异常 |

### 15. 位运算和内存操作模块 (15_bitwise_memory/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_bitwise_operators.xl | 基础演示 | 位运算符 |
| 002 | 002_bit_manipulation.xl | 功能模块 | 位操作技巧 |
| 003 | 003_bit_flags.xl | 功能模块 | 位标志 |
| 004 | 004_shift_operators.xl | 基础演示 | 位移运算符 |
| 005 | 005_memory_operations.xl | 功能模块 | 内存操作 |
| 006 | 006_pointer_operations.xl | 功能模块 | 指针操作 |

### 16. 高级组合示例模块 (16_advanced/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_data_structures.xl | 复杂组合 | 自定义数据结构 |
| 002 | 002_algorithms.xl | 复杂组合 | 算法实现（排序、搜索） |
| 003 | 003_design_patterns.xl | 复杂组合 | 设计模式 |
| 004 | 004_event_system.xl | 复杂组合 | 事件系统 |
| 005 | 005_state_machine.xl | 复杂组合 | 状态机 |
| 006 | 006_singleton.xl | 复杂组合 | 单例模式 |
| 007 | 007_factory_pattern.xl | 复杂组合 | 工厂模式 |
| 008 | 008_builder_pattern.xl | 复杂组合 | 构建器模式 |
| 009 | 009_iterator_pattern.xl | 复杂组合 | 迭代器模式 |
| 010 | 010_observer_pattern.xl | 复杂组合 | 观察者模式 |

### 17. 边界测试模块 (17_boundary/)

| 序号 | 文件名 | 类型 | 功能说明 |
|------|--------|------|----------|
| 001 | 001_large_numbers.xl | 边界测试 | 大数值处理 |
| 002 | 002_small_numbers.xl | 边界测试 | 小数值处理 |
| 003 | 003_divide_by_zero.xl | 边界测试 | 除零错误 |
| 004 | 004_array_out_of_bounds.xl | 边界测试 | 数组越界 |
| 005 | 005_null_pointer.xl | 边界测试 | 空指针 |
| 006 | 006_infinite_recursion.xl | 边界测试 | 无限递归 |
| 007 | 007_max_call_stack.xl | 边界测试 | 最大调用栈 |
| 008 | 008_undefined_variables.xl | 边界测试 | 未定义变量 |
| 009 | 009_invalid_syntax.xl | 边界测试 | 无效语法 |
| 010 | 010_type_mismatch.xl | 边界测试 | 类型不匹配 |

## 四、脚本生成计划

1. **第一阶段**：生成基础演示型脚本（约 50-60 个）
   - 覆盖所有基础语法和核心功能
   - 每个脚本专注于一个特定功能点
   - 包含清晰的注释和输出

2. **第二阶段**：生成功能模块型脚本（约 30-40 个）
   - 完整展示某个功能模块的能力
   - 包含多种使用场景
   - 提供最佳实践示例

3. **第三阶段**：生成边界测试型脚本（约 20-30 个）
   - 测试功能的边界情况
   - 验证错误处理机制
   - 确保语言的健壮性

4. **第四阶段**：生成复杂组合型脚本（约 15-20 个）
   - 综合运用多个功能模块
   - 展示完整的应用场景
   - 提供实际项目参考

## 五、脚本模板

每个脚本应包含以下结构：

```xlang
// 脚本标题
// 功能说明
// 作者和日期

// 导入必要的模块（如果有）

// 示例代码
function main() {
    // 示例逻辑
    print("示例输出")
    return 0
}
```

## 六、输出要求

1. 每个脚本应包含清晰的注释，说明其目的和功能
2. 脚本应具有可执行性，能够直接运行
3. 包含必要的输出，以便验证功能正确性
4. 遵循语言标准的最佳实践
5. 代码风格统一，使用 tab 缩进

## 七、生成工具

考虑使用脚本生成器自动生成部分示例，特别是基础演示型脚本，然后手动编写复杂组合型脚本。

## 八、验证方法

1. 每个脚本生成后，应使用 xlang 编译器运行验证
2. 确保输出符合预期
3. 检查是否有语法错误或运行时错误
4. 验证边界条件处理

## 九、交付时间

根据脚本复杂度和数量，预计需要：
- 基础演示型脚本：3-5 天
- 功能模块型脚本：2-3 天
- 边界测试型脚本：1-2 天
- 复杂组合型脚本：3-5 天

总计：9-15 天

## 十、后续维护

1. 随着语言标准的更新，定期更新示例脚本
2. 收集用户反馈，优化示例内容
3. 补充新功能的示例脚本
4. 保持示例脚本的可读性和实用性
