# class、object 与协议方法设计

本文固定 DEMO6 下一轮 class/object 能力的设计口径。它不是 DEMO5 object/meta 机制的兼容说明，而是 DEMO6 的新设计来源。

## 1. 目标

本轮目标是让 xlang 的对象系统同时满足三件事：

- `class` 是唯一的用户对象模型，承担堆对象 payload 布局、继承、方法、生命周期和 C 混编职责。
- `object` 是标准库提供的普通基类，不再是独立于 class 的黑盒动态对象系统。
- 特殊语义通过协议方法和 `derive` 能力开关表达，而不是通过大量硬编码关键字或旧的运行时分支表达。

设计重点：

- 保持零成本普通成员函数调用。
- 让 `class X extends object` 自然具备动态字段能力。
- 允许普通 class 通过实现协议获得索引、字段拦截、调用、运算符、复制、序列化等能力。
- 让基础类型隐性类、class、native-backed class、var、标准库函数共享同一套语义解释。
- 不为 DEMO5 旧语义提供兼容回退。

当前已落地的第一批能力：

- `object` 标准库基类。
- `class X extends object` 动态字段读写。
- `__field__` / `__setField__` 字段协议。
- `__item__` / `__setItem__` 索引协议。
- 协议方法签名校验和未知双下划线协议拒绝。
- `derive` 语法、能力名校验、重复声明校验。
- `derive moveOnly` 与 `derive copy` / `derive clone` 冲突校验。
- `derive copy` / `derive clone` 自动生成 `__copy__` / `__clone__`。
- `copy()` / `clone()` 与 class、var、容器、function 值的生成器融合。
- `derive value` 自动生成 `__toValue__` / `__fromValue__`。
- `derive json` / `derive xlon` 基于 value 能力接入标准库持久化。
- `json.stringify(class)` / `json.parse::<Class>(text)` roundtrip。
- `xlon.stringify(class)` / `xlon.parse::<Class>(text)` roundtrip。
- class 命名变量接收者的运算符协议、一元协议、比较协议。
- class 命名变量接收者的 `__contains__` 与 `in`。
- class 命名变量接收者的 `__call__` 与 `obj(...)`。
- class 临时值接收者的运算符、字段、索引、`__contains__`、`__call__` 协议包装调用。
- class 的 `__len__` 协议与 `.length` 只读属性，支持命名变量、函数返回值和构造表达式临时接收者。
- class 方法多返回值，支持成员调用返回类型推断、解构声明和解构赋值。
- 普通 class 的 `__iter__` / `__next__` 迭代协议，支持 `for value in obj` 与 `for index, value in obj`。
- 协议重载统一：`create(...)`、`__item__`、`__setItem__`、`__contains__`、`__call__` 和运算符协议复用 class 方法重载解析。
- `get` / `let` 属性系统，支持零维属性、多维属性参数、属性读写重载和属性索引回退。
- `public` / `private` / `protected` 访问控制；默认公开，`private` 仅当前 class 内可访问，`protected` 允许当前 class 和子类访问。
- `static` 字段和方法；通过类型名访问，static 方法没有 `self`。
- `override` / `final` / `virtual` 规则检查，以及 virtual 方法和 getter 的 vtable 动态派发。

## 2. 核心分层

### 2.1 class

`class` 是静态类型。

它具备：

- 固定字段布局。
- 固定方法集合。
- 单继承。
- 构造生命周期。
- 析构生命周期。
- C ABI 映射。
- ABI 元数据输出。
- 泛型 class 的后续扩展入口。

普通成员访问：

```xlang
class Point
    int x
    int y

    func sum() as int
        return self.x + self.y
    end
end

Point p = Point(1, 2)
print(p.sum())
```

普通成员函数调用应保持直接静态分发，不经过动态字典。

### 2.2 object

`object` 是标准库 class，不是编译器黑盒对象。

它可以拥有：

- 内部动态字段字典。
- 字段读取协议。
- 字段写入协议。
- 字段调用协议。
- 转换为字典/xvalue 的能力。
- json/xlon 持久化辅助能力。

典型写法：

```xlang
class Entity extends object
    int id
    string name
end

Entity e = Entity()
e["hp"] = 100
e.title = "hero"
```

这里 `e["hp"]` 和动态字段访问不是 object 的特殊语法黑盒，而是由 `object` 基类提供对应协议方法。

### 2.3 native-backed class

DEMO6 后续不再把 `custom` 作为独立的用户侧类型系统扩展方向。

零成本 C opaque handle 抽象融入 `class`，形成 native-backed class：

- 普通 class 负责 xlang 自有字段布局。
- object 只是标准库 class 基类，负责动态字段能力。
- native-backed class 负责把一组 C 函数和 C 资源句柄纳入 class 体系。

native-backed class 同时可以拥有：

- xlang 字段。
- xlang 方法。
- C native 句柄。
- extern 绑定的 C 方法。
- 协议方法。
- 继承、访问控制、属性、virtual 等 class 能力。

示例：

```xlang
class Texture
    native void* handle

    extern "tex_open" func __create__(string path)
    extern "tex_close" func __destroy__()
    extern "tex_width" func width() as int
    extern "tex_height" func height() as int

    func area() as int
        return self.width() * self.height()
    end
end
```

绑定规则：

- `native` 字段表示由 C 管理或 C 暴露的底层资源。
- extern 成员函数的 C 侧第一个参数固定为 native 句柄或实例指针，由编译器自动传入。
- xlang 调用方不显式传入 C receiver 参数。
- `__create__` 可绑定 C 构造函数，也可以由 xlang 代码实现。
- `__destroy__` 可绑定 C 析构函数，也可以由 xlang 代码实现。
- native-backed class 默认不可复制；只有显式提供 `derive copy`、`derive clone` 或对应协议后才允许复制。
- 旧 `custom` 只允许出现在明确标记的历史说明中；当前词法、语法、AST、语义和 cgen 均不存在兼容入口。

## 3. 协议方法

协议方法是运行期语义钩子，使用双下划线命名：

```xlang
func __item__(string key) as var
func __setItem__(string key, var value)
```

选择双下划线的原因：

- 避免占用 `item`、`field`、`copy` 等普通用户命名。
- 让特殊语义在代码中明显可见。
- 和普通方法共用函数声明、重载、ABI、ABI 元数据、文档注释等基础设施。
- 便于编译器在语义阶段识别并建立协议表。

协议方法不是 `meta` 方法。

`meta` 保留给未来编译期元编程；协议方法负责运行时语义。

### 3.1 下一批 OOP 基础能力

下一批 class/OOP 开发固定以下能力，按优先级推进。

P0：协议重载统一。

以下协议允许重载：

- `create(...)`
- `__item__(...)`
- `__setItem__(...)`
- `__contains__(...)`
- `__call__(...)`
- 运算符协议，例如 `__add__`、`__sub__`、`__eq__`、`__lt__` 等。

这些协议调用必须降为隐藏 class 方法调用，并复用普通 class 方法重载解析：

```xlang
a[x]        // a.__item__(x)
a[x] = v    // a.__setItem__(x, v)
x in a      // a.__contains__(x)
a + b       // a.__add__(b)
a(...)      // a.__call__(...)
```

规则：

- 协议重载解析与普通 class 方法重载使用同一套候选选择和歧义诊断。
- 语义阶段必须把调用解析到确定候选；cgen 不再按协议名猜测第一个方法。
- `destroy()`、`__len__()`、`__iter__()`、`__next__()`、`__copy__()`、`__clone__()`、`__toValue__()`、`__fromValue__(var)` 这类固定形态协议不支持重载。
- `create(...)` 是生命周期入口，但明确支持重载；`destroy()` 不支持重载，只允许一个无参数声明。

P1：属性系统。

属性使用 `get` / `let` 上下文声明。为了避免属性语法、容器类型和普通成员命名长期冲突，`get`、`let`、`set` 不允许作为变量名、参数名、字段名、属性名或方法名。

```xlang
class Player
    private array<int> hpValue = [0]

    get hp() as int
        return self.hpValue[0]
    end

    let hp(int value)
        self.hpValue[0] = value
    end

    get hp(int idx) as int
        return self.hpValue[idx]
    end

    let hp(int idx, int value)
        self.hpValue[idx] = value
    end
end
```

属性访问规则：

```xlang
obj.prop              // get prop()
obj.prop = value      // let prop(value)
obj.prop[i]           // get prop(i)
obj.prop[i] = value   // let prop(i, value)
obj.prop[i, j]        // get prop(i, j)
obj.prop[i, j] = val  // let prop(i, j, val)
```

维度参数规则：

- 属性维度参数支持多维。
- `let` 的最后一个参数固定为写入值，前面的参数都是维度参数。
- `get` / `let` 支持维度参数重载。
- 维度参数第一批只支持位置参数。
- 维度参数第一批不支持命名参数、`...` 或 `***`。
- 属性索引中的 `name = value` 不属于当前表达式语法，解析阶段直接拒绝；属性声明中的 `...` / `***` 在语义阶段报告 `property parameters cannot be vararg`。
- 同一属性同一维度签名下，如果同时存在 `get` 和 `let`，`let` 的最后一个参数类型必须与 `get` 返回类型兼容。

索引歧义处理：

- `obj.prop[i]` 优先匹配维度属性 `get prop(i)`。
- 如果没有匹配维度属性，但 `obj.prop` 本身返回 array/list/dict/class，则退回为 `(obj.prop)[i]`。
- 用户可以写 `(obj.prop)[i]` 强制索引属性返回值。

多参数索引同步进入协议设计：

```xlang
matrix[x, y]       // matrix.__item__(x, y)
matrix[x, y] = val // matrix.__setItem__(x, y, val)
```

普通 array/list/dict 第一批仍只支持单索引；多索引主要服务 class 协议和属性系统。

P2：访问控制。

第一批引入：

- `public`
- `private`
- `protected`

规则：

- 默认访问级别为 `public`。
- `private` 只允许当前 class 内访问。
- `protected` 允许当前 class 和任意子类访问。
- 访问控制关键字只在 class 成员声明位置作为上下文关键字。

P3：静态成员。

支持类型级字段、常量和方法：

```xlang
class Color
    static const int Max = 255

    static func rgb(int r, int g, int b) as Color
        ...
    end
end

auto c = Color.rgb(255, 0, 0)
```

规则：

- `static` 只在 class 成员声明位置作为上下文关键字。
- 静态成员通过类型名访问。
- 静态方法没有 `self`。
- 静态成员不参与实例对象布局。

P4：`override` / `final`。

```xlang
class Entity
    virtual func update()
    end
end

class Player extends Entity
    override func update()
    end
end
```

规则：

- 子类覆盖父类方法或属性时必须写 `override`。
- `override` 必须能找到父类同签名成员。
- `override` 目标必须是 `virtual` 或已经处于 override 链上的成员。
- `final` 成员不能被子类覆盖。
- `static` 与 `private` 成员不能参与 `override` / `final` / `virtual`。
- 第一批已完成静态检查和 virtual 动态派发。

P5：`virtual` 动态派发。

规则：

- 只有显式 `virtual` 方法和属性 getter 进入动态派发。
- 普通方法继续保持静态分发和零成本调用。
- `virtual` 成员生成 vtable 调度表；派生类 override 生成对应 vtable 对象。
- `override` 方法必须兼容父类 virtual 方法 ABI。
- 当前实现支持方法、getter 和 setter 的动态派发。
- C ABI 导出默认不暴露 vtable 细节，必要时生成 wrapper。

暂缓能力：

- 用户自定义 `protocol` / `interface`。
- `is` / `as` 运行时类型判断与向下转换。

## 4. 协议表

### 4.1 生命周期协议

| 协议 | 用途 |
| --- | --- |
| `__create__` | 构造对象，可由构造语法触发 |
| `__destroy__` | 析构对象，由生命周期清理触发 |

`create` 和 `destroy` 仍可作为用户层构造/析构入口的上下文名称，但编译器内部应归一到协议语义。

### 4.2 类型与显示协议

| 协议 | 用途 |
| --- | --- |
| `__typeName__` | 返回运行期类型显示名 |
| `__toString__` | 返回字符串表示 |
| `__hash__` | 返回哈希值 |
| `__equals__` | 统一相等语义 |

标准库函数应优先调用这些协议：

```xlang
print(value)
string text = string(value)
```

### 4.3 复制协议

| 协议 | 用途 |
| --- | --- |
| `__copy__` | 浅拷贝 |
| `__clone__` | 深拷贝 |
| `__move__` | 移动或转移所有权 |
| `__copyable__` | 返回是否允许复制 |

标准库函数：

```xlang
auto b = copy(a)
auto c = clone(a)
```

规则：

- 基础值类型默认可复制。
- string 由运行时管理；time/point/point<T> 按值语义处理。
- array/list/set/dict 的 `copy` 复制容器外壳，元素按浅拷贝处理。
- array/list/set/dict 的 `clone` 递归复制元素。
- xlang 中 `clone` 固定表示深复制，不保留 `deepcopy` 公开函数或兼容别名。
- class 默认只允许共享受管理引用，不根据字段形态自动开放对象复制。
- `derive copy` 自动生成 `__copy__`，`derive clone` 自动生成 `__clone__`。
- `derive copy/clone` 的字段和父类必须可复制。
- `derive moveOnly` 明确禁止复制，且不能与 `derive copy/clone` 同时出现。
- native-backed class 默认不可复制，拥有 `__destroy__` 的 native 资源不能通过 class 自动复制绕过。
- 资源型 class 默认不可复制，除非显式声明复制协议或 derive 能力。
- 默认参数、`byval`、赋值、返回、闭包捕获和容器存储不调用复制协议；它们只保留同一对象引用。

### 4.4 索引协议

| 协议 | 用途 |
| --- | --- |
| `__item__` | `a[x]` 读取 |
| `__setItem__` | `a[x] = value` 写入 |

示例：

```xlang
class FloatList
    func __item__(int64 index) as float64
        ...
    end

    func __setItem__(int64 index, float64 value)
        ...
    end
end
```

索引和值类型由协议签名约束。

如果 `__item__` 的参数是 `int64`，则 `a["name"]` 在语义检查阶段报错。

如果 `__setItem__` 的 value 是 `float64`，则 `a[0] = "x"` 在语义检查阶段报错。

下一批索引协议升级为多参数索引和重载解析：

```xlang
class Matrix
    func __item__(int x, int y) as float64
        ...
    end

    func __setItem__(int x, int y, float64 value)
        ...
    end
end

auto v = matrix[10, 20]
matrix[10, 20] = 3.14
```

规则：

- `a[x]`、`a[x, y]`、`a[x, y, z]` 都应先尝试 class 索引协议。
- `__item__` 的参数列表就是索引参数列表。
- `__setItem__` 的最后一个参数是写入值，前面的参数是索引参数列表。
- `__item__` / `__setItem__` 支持重载，并复用普通 class 方法重载解析。
- 普通 array/list/dict 第一批仍只支持单索引；多参数索引只对 class 协议和属性系统开放。

### 4.5 字段协议

| 协议 | 用途 |
| --- | --- |
| `__field__` | 读取不存在的字段 |
| `__setField__` | 写入不存在的字段 |
| `__callField__` | 调用不存在的字段 |

字段协议只处理静态字段表中不存在的字段。

示例：

```xlang
class Entity extends object
    int id
end

Entity e = Entity()
e.hp = 100
print(e.hp)
```

语义：

- `e.id` 命中静态字段，生成直接字段访问。
- `e.hp` 不在静态字段表中，转为 `__field__("hp")` 或 `__setField__("hp", value)`。
- `e.move(1, 2)` 如果静态方法不存在，可转为 `__callField__("move", args)`。

### 4.6 `__field__` 与 `__item__` 的区别

`__item__` 负责用户显式索引：

```xlang
a[0]
a["name"]
```

`__field__` 负责点号字段缺失后的动态访问：

```xlang
a.name
```

二者必须分开，原因是：

- 点号字段名是编译期字符串。
- 索引表达式可以是运行期值。
- 点号优先服务对象模型，索引优先服务容器模型。
- 字段访问需要先检查静态字段和静态方法，索引访问不需要。
- C ABI、ABI 元数据 和错误诊断可以分别表达字段协议与索引协议。

## 5. 运算符协议

算术：

| 运算符 | 协议 |
| --- | --- |
| `+` | `__add__` |
| `-` | `__sub__` |
| `*` | `__mul__` |
| `/` | `__div__` |
| `%` | `__mod__` |
| `**` | `__pow__` |

一元：

| 运算符 | 协议 |
| --- | --- |
| `-a` | `__neg__` |
| `+a` | `__pos__` |
| `not a`、`!a` | `__not__` |
| `~a` | `__bitNot__` |

比较：

| 运算符 | 协议 |
| --- | --- |
| `==` | `__eq__` |
| `!=` | `__ne__` |
| `<` | `__lt__` |
| `<=` | `__le__` |
| `>` | `__gt__` |
| `>=` | `__ge__` |
| `~=` | `__approx__` |

逻辑：

| 运算符 | 协议 |
| --- | --- |
| `and` | `__and__` |
| `or` | `__or__` |

规则：

- 内置数值、字符串、时间等类型优先使用内置规则。
- 用户 class 和 native-backed class 仅在内置规则不可用时进入协议查找。
- 运算符协议参与重载解析。
- 运算符协议的返回类型必须符合对应运算语义。
- `and`、`or` 的短路语义不能被协议破坏；协议重载只能用于非短路对象逻辑场景，具体落地需在语义阶段限制。

## 6. 容器与迭代协议

| 协议 | 用途 |
| --- | --- |
| `__len__` | 长度 |
| `__contains__` | `in` 判断 |
| `__iter__` | 创建迭代器 |
| `__next__` | 迭代下一项 |
| `__call__` | 对象可调用 |

迭代协议固定为两段：

- 可迭代 class 实现 `__iter__() as IteratorClass`。
- 迭代器 class 实现 `__next__() as (bool, T)`。
- `__next__` 的第一个返回值表示是否存在下一项，第二个返回值是本次迭代值。
- `for value in obj` 使用 `T` 作为循环值类型。
- `for index, value in obj` 使用 `int` 作为索引类型，使用 `T` 作为循环值类型。
- 普通 class 和泛型 class 使用同一套迭代协议。
- 泛型 class 的 `__iter__` / `__next__` 必须参与泛型实例化；`T` 由具体实例确定。
- `__next__() as (bool, T)` 是推荐形态，便于 `for` 语句直接获得静态循环变量类型。
- `__next__` 支持重载，但 `for in` 只选择零参数 `__next__()`。

示例：

```xlang
if item in box then
    print("hit")
end

for value in box
    print(value)
end

for index, value in box
    print(index, value)
end

box(1, 2, 3)
```

泛型 class 示例：

```xlang
class Range<T is comparable>
    func __iter__() as RangeIter<T>
    end
end

class RangeIter<T is comparable>
    func __next__() as (bool, T)
    end
end

Range<int> r = Range::<int>()
for value in r
    print(value)     // value 的静态类型是 int
end
```

## 7. derive 能力开关

`derive` 用于声明由编译器生成的标准能力。

推荐写法：

```xlang
class Player
    derive value
    derive copy
    derive json
    derive xlon

    int id
    string name
end
```

能力：

| derive | 生成能力 |
| --- | --- |
| `derive copy` | 生成浅拷贝协议 |
| `derive clone` | 生成深拷贝协议 |
| `derive moveOnly` | 禁止复制，只允许移动 |
| `derive value` | 生成 class 与 xvalue/dict 互转能力 |
| `derive json` | 基于 value 生成 JSON 序列化与反序列化 |
| `derive xlon` | 基于 value 生成 XLON 序列化与反序列化 |
| `derive string` | 生成默认字符串表示 |
| `derive hash` | 生成哈希协议 |
| `derive equals` | 生成相等比较协议 |

约束：

- `derive json` 依赖 `derive value` 能力；可以由编译器自动补齐内部依赖。
- `derive xlon` 依赖 `derive value` 能力。
- `derive copy` 要求所有字段可浅拷贝。
- `derive clone` 要求所有字段可深拷贝。
- `derive moveOnly` 与 `derive copy` 冲突。
- 资源型字段会阻止默认复制生成。

## 8. 字段级持久化注解

字段级 json/xlon 配置采用字段尾部注解，不使用 class 内配置块。

这样字段名、类型、默认值和持久化规则集中在同一行，阅读和工具处理都更直接。

示例：

```xlang
class Player
    derive json
    derive xlon

    int id
    string playerName @json("player_name") @xlon("player_name")
    string nick @json(omit=empty) = ""
    private string password @json(skip) @xlon(skip)
    private int internalId @json(include, required)
end
```

支持的注解：

- `@json(...)`：只影响 JSON 序列化与反序列化。
- `@xlon(...)`：只影响 XLON 序列化与反序列化。
- `@persist(...)`：同时影响 JSON 与 XLON，可作为公共配置。

支持的参数：

- `@json("name")`：将字段序列化为指定名称。
- `skip`：字段不参与序列化，也不参与反序列化。
- `include`：强制字段参与持久化，常用于 `private` 字段。
- `required`：反序列化时该字段必须存在。
- `omit=empty`：字段为空值时不输出。
- `omit=null`：字段为 `null` 时不输出。
- `name="field_name"`：等价于字符串位置参数，适合后续扩展更多参数时保持清晰。

规则：

- 字段默认使用源码字段名。
- `private` 字段默认不参与持久化，除非声明 `include`。
- `skip` 与 `include` 冲突。
- `skip` 与 `required` 冲突。
- `@json(...)` 覆盖 `@persist(...)` 中与 JSON 相关的同名配置。
- `@xlon(...)` 覆盖 `@persist(...)` 中与 XLON 相关的同名配置。
- 注解只描述持久化策略，不改变字段访问控制、不改变内存布局。

后续可扩展：

- 字段版本。
- 只读字段。
- 兼容旧字段名。
- 自定义格式化器。

## 9. 持久化 API

标准库提供统一入口：

```xlang
string text = json.stringify(value)
Player p = json.parse::<Player>(text)
json.write("player.json", value)
Player jp = json.read::<Player>("player.json")

string data = xlon.stringify(value)
Player q = xlon.parse::<Player>(data)
xlon.write("player.xlon", value)
Player r = xlon.read::<Player>("player.xlon")
```

规则：

- 基础类型直接序列化。
- 容器递归序列化。
- class 需要 `derive value`、`derive json` 或 `derive xlon`。
- native-backed class 默认不可序列化，除非显式绑定转换协议或 derive 持久化能力。
- object 动态字段由 object 基类的 value 能力参与序列化。

## 10. 基础类型隐性类

xlang 的基础类型以隐性类方式暴露能力。

示例：

```xlang
auto a = 123.toString()
auto b = "1,2".split(",")
auto c = [1, 2, 3]
c.append(4)
```

原则：

- 基础类型不一定都有真实用户可见 class 声明。
- 使用体验上，基础类型应和 class 实例保持一致。
- int8、int16、int32、int64、uint 系列共享 int 家族能力。
- float32、float64 共享 float 家族能力。
- ABI 精确类型仍保留独立类型身份。
- 标准库 `copy`、`clone`、`string`、`type` 等函数应覆盖基础类型。

## 11. nocopy 设计的保留解释

DEMO5 中 `nocopy` 的核心目的，是避免资源对象被意外按值复制。

典型风险：

- 两个对象持有同一个文件句柄。
- 两个对象持有同一个 C 指针。
- 两个对象重复析构同一块内存。
- 浅拷贝破坏锁、线程、socket、窗口句柄等资源生命周期。

DEMO6 不继续扩大 `nocopy` 关键字路线，而是使用能力系统表达：

- 默认资源型 class 和 native-backed class 不可复制。
- `derive copy` 明确请求生成浅拷贝。
- `derive clone` 明确请求生成深拷贝。
- `derive moveOnly` 明确表达只能移动。
- `__copyable__` 可作为运行期能力查询协议。

这样比单独关键字更容易和标准库、ABI 元数据、文档、C ABI 结合。

## 12. ABI 与 ABI 元数据

协议方法应进入类型元数据，但需要和普通方法区分。

ABI 元数据 建议字段：

```json
{
  "name": "__item__",
  "kind": "method",
  "protocol": "item",
  "params": [
    {"name": "index", "type": "int64"}
  ],
  "returns": [
    {"type": "float64"}
  ]
}
```

要求：

- 协议方法保留普通函数签名。
- ABI 元数据 标记 protocol 名称。
- C 头文件默认不导出隐式协议调用语法，只导出真实函数或 wrapper。
- `derive` 生成的方法要在 ABI 元数据 中标记为 generated。
- 文档生成可以把协议能力展示为类型能力。

## 13. 语义检查规则

语义阶段负责：

- 收集 class 和 native-backed class 协议方法。
- 检查协议方法签名是否符合最小要求。
- 建立类型的协议表。
- 将字段、索引、调用、运算符、复制、序列化等语义解析到协议表。
- 将允许重载的协议调用解析到唯一候选，解析规则和普通 class 方法重载一致。
- 将属性访问、属性写入和属性维度索引解析到唯一 `get` / `let` 候选。
- 对协议缺失给出普通语义错误。
- 对协议签名不匹配给出明确错误。
- 对协议重载歧义给出明确错误，不能在 cgen 阶段才暴露。
- 对访问控制、override/final/virtual 规则给出明确错误。
- 对 `derive` 之间的冲突给出明确错误。
- 对不可复制资源对象进入复制路径给出明确错误。

不做：

- 不为旧 meta 运行时方法做兼容。
- 不为旧 object 黑盒动态对象做兼容。
- 不把普通未知函数名猜测成协议。

## 14. 代码生成规则

代码生成应遵守：

- 静态字段访问生成直接 C 字段访问。
- 静态方法调用生成直接函数调用。
- 协议调用在语义阶段已经解析为确定目标，cgen 不做二次猜测。
- 允许重载的协议调用必须使用语义阶段解析出的候选，cgen 禁止按协议名选择第一个方法。
- 属性 `get` / `let` 生成应复用 class 方法调用生成路径，避免另起一套调用 ABI。
- 普通方法保持静态分发；显式 `virtual` 方法进入 vtable 或等价调度表。
- object 动态字段通过 object 基类真实函数实现。
- copy/clone 通过标准库入口或已解析协议函数生成。
- derive 生成函数应进入普通 cgen 流程，避免特殊分支膨胀。
- json/xlon 持久化优先复用 XRT/xvalue/json/xlon 能力。

## 15. 实现阶段

第一批实现：

- 协议方法识别。
- 协议表数据结构。
- `__item__` / `__setItem__`。
- `__field__` / `__setField__` / `__callField__`。
- 标准库 `object` 基类。
- 标准库 `copy` / `clone`。
- `derive copy` / `derive clone` / `derive moveOnly`。
- ABI 元数据 protocol 标记。
- 对应测试与范例。

第二批实现：

- 运算符协议。
- `__len__` / `__contains__` / `__call__`。
- 普通 class 的 `__iter__` / `__next__`。
- `derive value`。
- `derive json` / `derive xlon`。
- 持久化标准库 API。

第三批实现：

- 基础类型隐性类能力补齐。
- native-backed class 协议能力补齐。
- 泛型 class 的 `__iter__` / `__next__`。
- 字段级持久化注解。
- 协议能力文档自动生成。
- 更多容器协议与泛型约束融合。

下一批 OOP 基础能力：

- 协议重载统一：
  - `create(...)`。
  - `__item__(...)` / `__setItem__(...)`。
  - `__contains__(...)`。
  - `__call__(...)`。
  - 运算符协议。
- 多参数索引：
  - `obj[x, y]`。
  - `obj[x, y] = value`。
- 属性系统：
  - `get` / `let`。
  - 多维维度参数。
  - 维度参数重载。
  - 属性索引优先级与 `(obj.prop)[i]` 显式退回。
- 访问控制：
  - `public`。
  - `private`。
  - `protected`。
- 静态成员：
  - `static const`。
  - `static func`。
  - 类型名访问。
- 覆盖控制：
  - `override`。
  - `final`。
- 动态派发：
  - 显式 `virtual`。
  - vtable 或等价调度表。

### 当前暂不纳入完成口径

- `and` / `or` 短路逻辑协议化方案。
- 用户自定义 `protocol` / `interface`。

### 本批新增开发列表

- 泛型约束系统，语法为 `<T is Constraint>`，多个约束使用 `and`。
- 字段级持久化注解：`@json(...)`、`@xlon(...)`、`@persist(...)`。
- 泛型 class 的 `__iter__` / `__next__` 协议。
- custom 融合到 class，形成 native-backed class。
- class 数据 ABI 与成员函数 C export ABI。

## 16. 验收标准

本轮完成后必须满足：

- 根功能清单更新。
- 本文档与 `README.md`、`文档状态矩阵.md` 同步。
- 新语法均有正例测试。
- 错误路径均有负例诊断测试。
- class/object/protocol 有功能范例。
- copy/clone 有基础类型、容器、class、native-backed class 边界测试。
- json/xlon 持久化有 roundtrip 测试。
- ABI 元数据 输出包含协议与 derive 元数据测试。
- C 混编不被 object/protocol 破坏。
- `test.bat` 通过。
- `rc0_guard.ps1` 通过或明确更新守门基线。
