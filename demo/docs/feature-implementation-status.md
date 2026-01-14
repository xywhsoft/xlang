# xlang 第三版功能实现清单

本文档对比「语言标准第三版方案.txt」与 demo 目录下的源代码，列出已实现和未实现的功能。

*文档生成日期: 2026-01-14*

---

## 一、已实现的功能

### 1. 基础规范

| 功能 | 状态 | demo示例 |
|------|------|----------|
| UTF-8 编码 | ✓ | 全部demo |
| 标识符命名（含中文） | ✓ | 多处测试 |
| 注释（// 和 /* */） | ✓ | 全部demo |

### 2. 数据类型

| 类型 | 状态 | demo示例 |
|------|------|----------|
| null | ✓ | nullish_operators.xl |
| bool (true/false) | ✓ | bool_types.xl |
| int（基本整数） | ✓ | number_types.xl |
| float（基本浮点） | ✓ | number_types.xl |
| string | ✓ | string_types.xl |
| array | ✓ | array_types.xl |
| dict | ✓ | dict_types.xl |
| var（变体类型） | ✓ | 全部demo |
| function | ✓ | 022_closures.xl |
| class | ✓ | test_class_basic.xl |
| range | ✓ | range_types.xl |
| time | ✓ | builtin-functions.md |

### 3. 立即值（字面量）

| 功能 | 状态 | demo示例 |
|------|------|----------|
| 十进制整数 | ✓ | 全部 |
| 十六进制整数 (0x) | ✓ | bitwise_operators.xl |
| 二进制整数 (0b) | ✓ | bitwise_operators.xl |
| 浮点数 | ✓ | 全部 |
| 字符串（双引号） | ✓ | 全部 |
| 字符串（单引号） | ✓ | 部分demo |
| 数组字面量 [] | ✓ | array_types.xl |
| 字典字面量 {} | ✓ | dict_types.xl |
| 空数组 new array() | ✓ | 011_arrays.xl |
| 空字典 {} | ✓ | dict_types.xl |

### 4. 运算符

| 运算符 | 状态 | demo示例 |
|--------|------|----------|
| 比较 (>, <, >=, <=, ==, !=) | ✓ | comparison_operators.xl |
| 算术 (+, -, *, /, %) | ✓ | arithmetic_operators.xl |
| 乘方 (**) | ✓ | power_operators.xl |
| 乘方赋值 (**=) | ✓ | power_operators.xl |
| 位运算 (&, \|, ^, >>, <<) | ✓ | bitwise_operators.xl |
| 位非 (!) | ✓ | bitwise_operators.xl |
| 逻辑 (and, or, not) | ✓ | logical_operators.xl |
| 赋值 (=, +=, -=, *=, /=, %=) | ✓ | 014_assignment.xl |
| 自增自减 (++, --) 前后缀 | ✓ | incdec_test.xl |
| 空值合并 (??) | ✓ | nullish_operators.xl |
| 空值赋值 (?=) | ✓ | nullish_operators.xl |
| is 类型判断 | ✓ | test_is_in.xl |
| in 包含判断 | ✓ | test_is_in.xl |
| 字符串拼接 (+) | ✓ | string_types.xl |
| 数组合并 (+) | ✓ | list_types.xl |
| 数组重复 (*) | ✓ | repeat_operator.xl |
| 负索引 (arr[-1]) | ✓ | index_and_slice.xl |
| 范围切片 (arr[a to b]) | ✓ | index_and_slice.xl |

### 5. 控制流程

| 功能 | 状态 | demo示例 |
|------|------|----------|
| if/elseif/else/end | ✓ | if_else.xl |
| if exp then 单行 | ✓ | test_if_then.xl |
| for v in obj | ✓ | for_loops.xl |
| for i, v in obj | ✓ | dict_types.xl |
| for i in range() | ✓ | range_types.xl |
| while/end | ✓ | while_loops.xl |
| break | ✓ | test_break_continue.xl |
| break n | ✓ | test_break_continue.xl |
| continue | ✓ | test_break_continue.xl |
| continue n | ✓ | test_break_continue.xl |

### 6. 函数

| 功能 | 状态 | demo示例 |
|------|------|----------|
| function 定义 | ✓ | 009_functions.xl |
| return | ✓ | 全部 |
| 多返回值（数组形式） | ✓ | test_multi_return.xl |
| 可选参数 | ✓ | 文档记录 |
| 匿名函数 | ✓ | 022_closures.xl |
| 闭包 | ✓ | 022_closures.xl |
| 递归调用 | ✓ | 010_recursion.xl |

### 7. 类与对象

| 功能 | 状态 | demo示例 |
|------|------|----------|
| class 定义 | ✓ | test_class_basic.xl |
| create 构造函数 | ✓ | test_ctor_dtor.xl |
| destroy 析构函数 | ✓ | test_ctor_dtor.xl |
| extends 继承 | ✓ | test_inheritance.xl |
| super 调用父类 | ✓ | test_super.xl |
| self 引用 | ✓ | 全部class demo |
| public/private/protected | ✓ | test_access.xl |
| 成员函数 | ✓ | test_class_basic.xl |
| 成员变量 | ✓ | test_class_basic.xl |
| 方法覆盖 | ✓ | test_inheritance.xl |

### 8. 常量

| 功能 | 状态 | demo示例 |
|------|------|----------|
| const 常量定义 | ✓ | const_variables.xl |

### 9. 内置函数（部分列举）

| 函数 | 状态 |
|------|------|
| print/println | ✓ |
| len | ✓ |
| type | ✓ |
| range | ✓ |
| abs/min/max | ✓ |
| round/floor/ceil | ✓ |
| chr/ord | ✓ |
| hash/id | ✓ |
| 字符串函数 (trim/upper/lower/split/join等) | ✓ |
| 时间函数 (now/year/month等) | ✓ |
| 文件函数 (readfile/writefile等) | ✓ |
| JSON函数 (json_parse/json_stringify) | ✓ |
| 编码函数 (base64_encode/decode等) | ✓ |

---

## 二、未实现/未在demo中验证的功能

### 1. 数据类型

| 类型 | 状态 | 说明 |
|------|------|------|
| int8/int16/int32/int64 | ✗ | 明确位数的整数类型未见 |
| uint8/uint16/uint32/uint64 | ✗ | 无符号整数未见 |
| float32/float64 | ✗ | 明确位数的浮点数未见 |
| point | ✗ | 内存指针类型未见 |
| list（AVL树实现） | ✗ | 标准定义的键值列表未见 |
| set（集合） | ✗ | 集合类型未见实际使用 |
| auto | ✗ | 类型推断未见（都用var） |
| custom | ✗ | C语言集成类型未见 |
| object（特殊基类） | ✗ | 对象类未见 |

### 2. 立即值（字面量）

| 功能 | 状态 | 说明 |
|------|------|------|
| 八进制整数 (&O) | ✗ | 未见使用 |
| 多行字符串 ("""/''') | ✗ | 未见使用 |
| 模板字符串 (\`{$var}\`) | ✗ | 未见使用 |
| 时间字面量 (#2000-01-01) | ✗ | 未见使用 |
| 列表字面量 [key:value] | ✗ | list类型未实现 |
| 集合字面量 {value} | ✗ | set类型未实现 |

### 3. 运算符

| 运算符 | 状态 | 说明 |
|--------|------|------|
| 约等于 (~=) | ✗ | 未见使用 |
| 三目运算符 (? :) | ✗ | 未见使用 |
| 安全访问 (?.) | ✗ | 未见使用 |
| 安全索引 (?[]) | ✗ | 未见使用 |
| 集合运算 (-差集, &交集, ^对称差) | ✗ | set未实现 |

### 4. 控制流程

| 功能 | 状态 | 说明 |
|------|------|------|
| do...while (后置条件) | ✗ | 未见使用 |
| with 资源管理语句 | ✗ | 未见使用 |
| if exp in [A,B,C] 展开优化 | ? | 有in运算符但展开优化未验证 |

### 5. 函数高级特性

| 功能 | 状态 | 说明 |
|------|------|------|
| 多返回值接收 (a, b) = func() | ✗ | 目前用数组接收 |
| auto (a, b) = func() | ✗ | 自动类型推断多返回值未见 |
| 返回值类型声明 as int | ✗ | 函数返回类型未见 |
| 参数类型声明 | ✗ | 参数类型声明未见 |
| 可变参数 (...) | ✗ | 数组可变参数未见 |
| 字典可变参数 (***) | ✗ | 字典可变参数未见 |
| 调用约定 (xcall/cdecl等) | ✗ | 未见使用 |
| 命名参数调用 | ✗ | 未见使用 |
| main 入口函数 | ✗ | demo直接执行代码 |

### 6. 类型系统

| 功能 | 状态 | 说明 |
|------|------|------|
| typedef 类型别名 | ✗ | 未见使用 |
| enum 枚举类型 | ✗ | 未见使用 |
| union 共用体 | ✗ | 未见使用 |
| 类型转换函数 int()/float() | ? | builtin文档有，demo验证有限 |

### 7. 类高级特性

| 功能 | 状态 | 说明 |
|------|------|------|
| static 静态成员 | ✗ | 未见第三版语法实现 |
| get/let 属性访问器 | ✗ | 未见使用 |
| meta 元编程 | ✗ | 元方法未见 |
| 函数重载 | ✗ | 未见使用 |
| 类外定义成员函数 | ✗ | ClassName.method 未见 |
| super.create() 调用 | ✗ | 子类构造未调用父类构造 |

### 8. 模块与命名空间

| 功能 | 状态 | 说明 |
|------|------|------|
| namespace/using | ✗ | 命名空间未见 |
| import/export | ✗ | 模块导入导出未见 |
| extern 外部函数 | ✗ | 外部函数声明未见 |

### 9. 异常处理

| 功能 | 状态 | 说明 |
|------|------|------|
| try/catch/finally (end风格) | ✗ | demo是花括号风格 |
| throw(type, message) | ✗ | 第三版语法未见 |
| assert | ✗ | 未见使用 |
| Error 内置类 | ✗ | 未见使用 |

---

## 三、备注

### 1. 语法风格差异

部分demo文件使用花括号风格（类似JS），与第三版标准的 `end` 结束块风格不一致。

**第三版标准语法：**
```xlang
function foo()
	// code
end

if condition
	// code
end

class MyClass
	// code
end
```

**部分demo使用的非标准语法：**
```xlang
func foo() {
	// code
}

if condition {
	// code
}
```

真正符合第三版标准的demo使用 `function...end`、`if...end`、`class...end` 语法。

### 2. 内置函数

`builtin-functions.md` 文档记录了大量内置函数，但部分函数在demo中未实际验证。

### 3. list vs array

标准定义：
- `array`: 固定从0开始索引，索引必须连续
- `list`: AVL树实现，int64索引范围，支持负数索引，索引可不连续

但demo中 list 和 array 概念混用，真正的 list 类型未见实现。

### 4. 类型系统

当前实现主要使用 `var` 变体类型，精确类型声明（int32/float64等）未见实现。

---

## 四、功能实现统计

| 分类 | 已实现 | 未实现 | 实现率 |
|------|--------|--------|--------|
| 基础规范 | 3 | 0 | 100% |
| 数据类型 | 12 | 9 | 57% |
| 立即值 | 10 | 6 | 63% |
| 运算符 | 18 | 5 | 78% |
| 控制流程 | 10 | 3 | 77% |
| 函数 | 7 | 9 | 44% |
| 类与对象 | 10 | 6 | 63% |
| 常量 | 1 | 0 | 100% |
| 类型系统 | 0 | 4 | 0% |
| 模块命名空间 | 0 | 3 | 0% |
| 异常处理 | 0 | 4 | 0% |

**整体估算实现率：约 60%**

---

*本文档基于 demo 目录下的源代码分析生成，部分功能可能已在解析器中实现但未在demo中展示。*
