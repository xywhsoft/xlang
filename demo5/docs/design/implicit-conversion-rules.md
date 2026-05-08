# xlang 隐式类型转换规则设计

## 目标

隐式类型转换用于让 xlang 在函数调用、赋值、返回值、默认参数、基础表达式和 XLE/native bridge 中处理安全的类型适配。

本设计的核心目标是：

- 只允许安全、单向、低歧义转换。
- 不允许会丢信息、改变语义或隐藏运行时失败的转换。
- 所有转换都必须可诊断、可排序、可在重载决议中解释。
- 函数签名保持声明语义，不因调用点隐式转换而改变 ABI。

## 基本原则

1. 隐式转换不是全局自动改写，只在明确类型期望的上下文中发生。
2. `int -> float` 是安全扩大，允许。
3. `float -> int` 是有损转换，禁止隐式，必须显式。
4. `string <-> number`、`number <-> bool` 禁止隐式。
5. `var` / `any` 是动态接收目标，不应优先于更具体的静态类型。
6. `null` 只能隐式转换到可空引用、对象和容器类类型，不能转换到 `int`、`float`、`bool`。
7. 用户自定义转换不进入 V1，避免一开始引入副作用和不可预测的重载结果。

## 转换等级

隐式转换使用 cost 表示优先级。cost 越低越优先。

| cost | 名称 | 说明 |
| ---: | --- | --- |
| 0 | exact | 源类型和目标类型完全一致 |
| 1 | alias | 类型别名或规范化后的同一类型 |
| 2 | numeric widening | 安全数值扩大，例如 `int -> float` |
| 3 | box / erase | 具体类型转换到 `var` / `any` |
| 4 | null literal | `null` 转换到可空引用、对象或容器类型 |
| 5 | dynamic fallback | 保留给后续动态兜底，V1 默认不启用 |
| 禁止 | lossy / semantic | 有损或语义转换，例如 `float -> int`、`string -> int` |

## V1 转换表

| from | to | 隐式 | cost | 说明 |
| --- | --- | --- | ---: | --- |
| `T` | `T` | 是 | 0 | 精确匹配 |
| alias | canonical | 是 | 1 | 类型别名归一 |
| `int` | `float` | 是 | 2 | 安全扩大 |
| concrete | `var` | 是 | 3 | 装箱为动态值 |
| concrete | `any` | 是 | 3 | 如果 `any` 已启用，按动态接收处理 |
| class/object instance | `object` | 是 | 3 | 对象基类或元对象 |
| `null` | object/container/reference | 是 | 4 | 不能转到数值和 bool |
| `float` | `int` | 否 | - | 必须显式 `int(x)`、`floor(x)`、`round(x)` |
| `string` | number/bool | 否 | - | 必须显式 parse |
| number/bool | `string` | 否 | - | 必须显式 `to_string` |
| number | `bool` | 否 | - | 条件判断必须写比较表达式 |
| `bool` | number | 否 | - | 必须显式 |
| container covariance | container | 否 | - | V1 不做容器协变 |
| function variance | function | 否 | - | V1 函数类型只做 exact |

## 启用上下文

V1 只在这些上下文启用隐式转换：

1. 函数调用实参匹配形参。
2. 变量声明和赋值。
3. `return` 值匹配声明返回类型。
4. 默认参数表达式匹配参数类型。
5. 二元数值运算中的类型提升。
6. 条件表达式两端的共同类型计算。
7. 容器字面量元素共同类型计算。
8. XLE/native bridge 按声明签名适配调用边界。

V1 不在这些上下文启用：

1. `if` / `while` 条件中的 number-to-bool。
2. 重载决议中的 `float -> int`。
3. `var -> concrete` 的 checked cast。
4. 任意用户自定义转换。

## 重载决议

函数签名已经完整后，调用匹配按以下流程：

1. 根据名称、receiver/method、arity、默认参数、vararg 做初筛。
2. 对泛型候选先进行类型推导。
3. 对每个候选逐参数计算 `XL_Conversion`。
4. 如果任一参数不可隐式转换，候选失败。
5. 计算候选总分。
6. 选择总分最低者。
7. 若总分相同，使用更具体规则比较。
8. 仍无法决出时报 ambiguous overload，并输出候选转换明细。

候选总分：

```text
 sum(parameter conversion cost)
 default argument penalty
 vararg penalty
 dynamic receiver penalty
```

建议惩罚值：

| 情况 | penalty |
| --- | ---: |
| 每使用一个默认参数 | 1 |
| 使用 vararg | 2 |
| 目标参数为 `var` / `any` | 3 |
| dynamic fallback | 5 |

示例：

```xlang
func f(float x)
func f(var x)

f(1)
```

`int -> float` cost 2，`int -> var` cost 3，所以选择 `f(float)`。

## 泛型规则

泛型推导分两阶段：

1. 严格推导：只用 exact、alias 和结构匹配推导类型参数。
2. 转换检查：推导完成后，再检查实参能否隐式转换到实例化后的形参类型。

不允许用 `int -> float` 反向推导 `T=float`。例如：

```xlang
func id<T>(T value) as T
id(1)
```

推导结果是 `T=int`，不是 `T=float`。

## 诊断格式

普通类型不匹配：

```text
cannot implicitly convert '<source>' to '<target>'; use explicit conversion
```

有损转换：

```text
implicit conversion from 'float' to 'int' is not allowed; use int(x), floor(x), round(x), or ceil(x)
```

重载歧义：

```text
ambiguous overload for 'f(...)'
candidate 1: f(float) conversions: arg0 int -> float cost=2
candidate 2: f(var) conversions: arg0 int -> var cost=3
```

条件表达式错误：

```text
condition must be bool; compare explicitly, for example x != 0
```

## 编译器内部模型

建议新增统一转换判定 API，避免在 resolver、lower、codegen 中散落转换规则。

```c
typedef enum XL_ConvKind {
	XL_CONV_NONE = 0,
	XL_CONV_EXACT,
	XL_CONV_ALIAS,
	XL_CONV_NUMERIC_WIDEN,
	XL_CONV_BOX,
	XL_CONV_NULL,
	XL_CONV_DYNAMIC
} XL_ConvKind;

typedef struct XL_Conversion {
	XL_ConvKind eKind;
	int iCost;
	int bRequiresRuntime;
	char sSourceType[XL_TYPE_NAME_MAX];
	char sTargetType[XL_TYPE_NAME_MAX];
} XL_Conversion;
```

核心 API：

```c
int xlTypeCanConvertImplicit(
	const XL_TypeContext* pTypes,
	const char* sSourceType,
	const char* sTargetType,
	XL_Conversion* pOut
);
```

所有调用匹配、赋值检查、return 检查、默认参数检查、表达式共同类型计算都应调用这套 API。

## Codegen 要求

V1 需要的实际代码生成转换：

| 转换 | 生成方式 |
| --- | --- |
| exact / alias | 不生成额外代码 |
| `int -> float` | 生成数值 cast |
| concrete -> `var` / `any` | 调用 box helper |
| `null -> reference` | 按目标类型生成 null 值 |

禁止生成：

| 转换 | 原因 |
| --- | --- |
| `float -> int` | 有损 |
| `string -> number` | 可能失败 |
| number -> bool | 语义不清 |
| `var -> concrete` | 需要 checked cast，V1 不做 |

## XLE / ABI 规则

1. 函数签名记录声明类型，不记录调用点转换。
2. `languageAbiId` 使用声明签名，不能因为隐式转换改变。
3. XLE bridge 生成调用适配时可以使用转换 API。
4. XLE 元数据可选记录转换需求，但不应把转换写进 ABI identity。

## 后续版本

V2 可考虑：

- 容器字面量共同类型提升。
- 只读容器协变。
- 函数类型 variance。
- `var -> T` checked cast。
- 用户自定义显式转换函数。

V3 可考虑：

- 标记为 `implicit` 的用户自定义转换。
- 转换 purity 检查。
- 跨包转换规则导出。

用户自定义隐式转换必须非常谨慎，默认不建议早期启用。
