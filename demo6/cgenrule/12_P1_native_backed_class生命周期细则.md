# P1 class/native-backed class 生命周期细则

本文细化 `class`、`native-backed class` 的声明布局、构造、默认初始化、成员访问、方法调用、继承路径、析构和 cleanup 生成规则。主要覆盖 `xl6_cgen_decl_class.c`、`xl6_cgen_decl_methods.c`、`xl6_cgen_decl_proto.c`、`xl6_cgen_stmt_decl.c`、`xl6_cgen_value_object.c`、`xl6_cgen_resolve_call.c`。

## CGEN-CLS-001 非泛型 class 结构体布局

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Dog extends Animal` / `int age` / `string name` |
| 生成规则描述 | 非泛型 class 被生成为一个 C `typedef struct`。父类通过第一个字段 `base` 内嵌到子类结构体中，随后按声明顺序输出当前 class 的字段。class 声明会先递归输出父类和字段类型依赖。泛型 class 定义本身不会在该规则下直接输出。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:319`、`xl6_cgen_decl_class.c:343`、`xl6_cgen_decl_class.c:362` |
| 对应生成的 C 代码 | `typedef struct XL_Class_Dog { XL_Class_Animal base; long long age; char* name; } XL_Class_Dog;` |
| 中间形态伪代码 | `ClassDecl(name, base, fields) -> struct { baseStruct base; fields... }` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式声明父结构体字段，并保证字段顺序、类型依赖和原型声明顺序一致。 |
| 额外依赖的封装代码 | 当需要进入 `xvalue` 时会额外生成 record box runtime；普通 class struct 本身不需要封装。 |

## CGEN-CLS-002 泛型 class 具体实例布局

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Box<T>` / `T value`，使用 `Box::<int>` |
| 生成规则描述 | 泛型 class 只有在出现具体类型实例时才生成 C struct。生成前设置 `currentGenericParams/currentGenericArgs`，把字段里的泛型参数替换为具体类型；同一个具体实例只生成一次。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:1`、`xl6_cgen_decl_class.c:94`、`xl6_cgen_decl_class.c:129` |
| 对应生成的 C 代码 | `typedef struct XL_Class_Box__int { long long value; } XL_Class_Box__int;` |
| 中间形态伪代码 | `Box<T> + [int] -> Box<int> concrete struct` |
| 实现同等功能手写的 C 代码 | 手写 C 通常需要为每个实例写一个独立 struct，例如 `Box_int`、`Box_string`。 |
| 额外依赖的封装代码 | 泛型实例表、泛型类型替换状态；需要进入 `xvalue` 时同样生成 record box runtime。 |

## CGEN-CLS-003 class 方法原型与 self 参数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Counter` / `func add(int n) int` |
| 生成规则描述 | class 方法被生成为普通 C 函数，首参固定为 `XL_Class_Name* self`，后续参数按 xlang 参数列表输出。多返回值方法当前不允许进入 class C 原型，会明确诊断为 `class method C prototype does not support multiple return values`；可变参数方法也被拒绝。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:215`、`xl6_cgen_decl_proto.c:239`、`xl6_cgen_decl_proto.c:248`、`xl6_cgen_decl_class.c:409` |
| 对应生成的 C 代码 | `long long xl_class_Counter__add(XL_Class_Counter* self, long long n);` |
| 中间形态伪代码 | `method(self: Counter*, params...) -> returnType` |
| 实现同等功能手写的 C 代码 | `long long Counter_add(Counter* self, long long n);` |
| 额外依赖的封装代码 | class 方法符号名编码；重载时追加参数类型后缀。 |

## CGEN-CLS-004 create/destroy 原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `create(int n)` / `destroy()` |
| 生成规则描述 | `create` 和 `destroy` 都生成 `void` 返回值。`create` 使用可重载的 callable 符号，`destroy` 使用固定成员符号。若没有用户声明 `destroy`，但 class 字段或父类需要销毁，会额外生成自动 `destroy` 原型。 |
| 生成器文件名和行号 | `xl6_cgen_decl_proto.c:263`、`xl6_cgen_decl_proto.c:279`、`xl6_cgen_decl_class.c:413`、`xl6_cgen_decl_class.c:420` |
| 对应生成的 C 代码 | `void xl_class_Counter__create(XL_Class_Counter* self, long long n);` / `void xl_class_Counter__destroy(XL_Class_Counter* self);` |
| 中间形态伪代码 | `special create/destroy -> void C func(self, params...)` |
| 实现同等功能手写的 C 代码 | C 中通常写 `Counter_init(&obj, n)` 和 `Counter_deinit(&obj)`。 |
| 额外依赖的封装代码 | 自动析构原型依赖 `xl6CgenClassNeedsDestroy` 判断。 |

## CGEN-CLS-005 泛型 class 方法与特殊方法原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Box<T>` / `func get() T` / `create(T value)` |
| 生成规则描述 | 泛型 class 的方法原型按具体实例生成。符号名包含 class 名和所有类型实参；重载时再追加参数类型后缀。`self` 参数类型是具体实例 struct 指针。当前 C 原型路径只允许 0 或 1 个返回值，多返回会明确诊断为 `generic class method C prototype does not support multiple return values`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:148`、`xl6_cgen_decl_class.c:183`、`xl6_cgen_decl_class.c:207`、`xl6_cgen_decl_class.c:221`、`xl6_cgen_decl_class.c:239`、`xl6_cgen_decl_class.c:261` |
| 对应生成的 C 代码 | `long long xl_class_Box__int__get(XL_Class_Box__int* self);` |
| 中间形态伪代码 | `Box<T>.get for Box<int> -> Box_int_get(Box_int* self)` |
| 实现同等功能手写的 C 代码 | 每个泛型实例手写一组函数，例如 `Box_int_get`、`Box_string_get`。 |
| 额外依赖的封装代码 | 泛型实例收集表、类型名编码、重载后缀编码。 |

## CGEN-CLS-006 class 方法体上下文

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func inc(int n)` / `self.value += n` |
| 生成规则描述 | 方法体生成前设置 `currentClass`、`currentReturnType`，并把 `self` 注册为当前 class 类型。参数类型进入变量类型表；普通 class 值参数若包含 owned 字段，会从传入的浅 C 值建立独立 owned copy 并注册作用域清理，避免被调函数修改或释放调用方字段；native-backed class 不进入这条复制路径。方法体结束后恢复上下文并释放本方法作用域的变量类型记录。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:41`、`xl6_cgen_decl_methods.c:339`、`xl6_cgen_decl_methods.c:398` |
| 对应生成的 C 代码 | `long long xl_class_Counter__inc(XL_Class_Counter* self, long long n) { self->value += n; return self->value; }` |
| 中间形态伪代码 | `enterMethod(class); addVar("self", class); emitBlock(body); leaveMethod()` |
| 实现同等功能手写的 C 代码 | 手写 C 直接使用 `self->field`，但需要人工维护返回类型和 cleanup。 |
| 额外依赖的封装代码 | 变量类型表、cleanup 栈；拥有字段的 class 值参数依赖 class copy/destroy helper。 |

## CGEN-CLS-007 泛型 class 方法体上下文

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Box<T>` / `func set(T value)` |
| 生成规则描述 | 泛型方法体生成时同时设置当前 class 和当前泛型参数/实参映射，`self` 注册为具体实例类型。方法体内所有 `T` 类型都会通过当前泛型上下文替换为实际类型；泛型 class 值参数的 owned 字段也按具体类型实参复制并在作用域结束时清理。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:227`、`xl6_cgen_decl_methods.c:235`、`xl6_cgen_decl_methods.c:249` |
| 对应生成的 C 代码 | `void xl_class_Box__int__set(XL_Class_Box__int* self, long long value) { self->value = value; }` |
| 中间形态伪代码 | `enterGeneric(Box<T>, [int]); self: Box<int>; emitBlock()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要提前把所有 `T` 替换为具体 C 类型。 |
| 额外依赖的封装代码 | 泛型上下文保存/恢复。 |

## CGEN-CLS-008 create 中自动调用无参 base create

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Child extends Parent` / `create()`，函数体里没有 `base(...)` |
| 生成规则描述 | 子类 `create` 生成时会递归扫描函数体，如果没有显式 `base(...)`，且父类存在无参 `create`，则在子类 create 体开头自动生成父类 create 调用。若父类只有带参 create，则报错要求显式 `base(...)`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:23`、`xl6_cgen_decl_methods.c:43`、`xl6_cgen_decl_methods.c:337` |
| 对应生成的 C 代码 | `xl_class_Parent__create(&self->base);` |
| 中间形态伪代码 | `if createBody lacks base(...) then auto Parent.create(&self.base)` |
| 实现同等功能手写的 C 代码 | `Parent_create(&child->base);` 必须手写在子类初始化开头。 |
| 额外依赖的封装代码 | 父类 create 解析；无额外运行时 helper。 |

## CGEN-CLS-009 显式 base(...) 构造调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `create(int n)` / `base(n)` |
| 生成规则描述 | 普通 call 表达式如果 callee 是上下文关键字 `base`，会被 class 构造路径拦截。当前 class 必须有父类；父类有匹配 create 时生成 `&self->base` 调用；父类没有 create 时只允许无参 `base()`，并生成空语句。若该路径在没有当前 class 的上下文中被触发，兜底诊断为 `base constructor call requires current class context`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:205`、`xl6_cgen_value_object.c:646`、`xl6_cgen_value_object.c:631`、`xl6_cgen_resolve_call.c:373` |
| 对应生成的 C 代码 | `xl_class_Parent__create(&self->base, n);` 或 `((void)0)` |
| 中间形态伪代码 | `base(args) -> Parent.create(&self.base, args)` |
| 实现同等功能手写的 C 代码 | `Parent_create(&self->base, n);` |
| 额外依赖的封装代码 | class create 重载解析。 |

## CGEN-CLS-010 class 变量声明、零初始化与 cleanup 注册

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `Counter c` / `Counter c = Counter(1)` |
| 生成规则描述 | class 局部变量总是先生成 `{0}` 零初始化，并注册变量类型与 class cleanup。这样即使后续字段初始化或 create 失败，也能在作用域关闭时按统一 cleanup 机制处理。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:18`、`xl6_cgen_stmt_decl_class_temp.c:47`、`xl6_cgen_stmt_decl_class_temp.c:66` |
| 对应生成的 C 代码 | `XL_Class_Counter c = {0};` |
| 中间形态伪代码 | `declare class var -> zero init; add cleanup(name, class)` |
| 实现同等功能手写的 C 代码 | `Counter c = {0};` 并在离开作用域前手写 `Counter_destroy(&c)`。 |
| 额外依赖的封装代码 | cleanup 栈；若 class 不需要 destroy，cleanup 判断会避免无意义调用。 |

## CGEN-CLS-011 class 字段默认初始化

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Config` / `int port = 8080` / `File f = File(path)` |
| 生成规则描述 | class 变量声明后，会按父类到子类、字段声明顺序输出字段默认初始化。普通字段生成赋值；native-backed class 字段如果初始化表达式是同类型构造，则写入 `.handle` 而不是整体赋值；如果 native-backed class 字段初始化使用了其他 native-backed class 的构造表达式，则明确诊断为 `native-backed class field initializer constructor type does not match field type`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:426`、`xl6_cgen_decl_class.c:456`、`xl6_cgen_decl_class.c:467`、`xl6_cgen_decl_class.c:475` |
| 对应生成的 C 代码 | `c.port = 8080; c.f.handle = file_open(path);` |
| 中间形态伪代码 | `initBaseFields(var.base); for field in fields: var.field = initExpr` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在对象声明后逐字段赋默认值，并特别处理句柄字段。 |
| 额外依赖的封装代码 | native-backed class create 解析。 |

## CGEN-CLS-012 class 构造表达式初始化变量

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `Counter c = Counter(3)` / `auto c = Counter(3)` |
| 生成规则描述 | 初始化表达式是同类型 class 构造时，声明规则先零初始化和字段默认初始化，再调用匹配的 `create`。如果 class 没有 create 且传入了参数，生成具体诊断 `class constructor has arguments but no matching create method`；如果没有 create 且无参数，会尝试自动调用父类无参 create。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl.c:45`、`xl6_cgen_stmt_decl_class_temp.c:69`、`xl6_cgen_stmt_decl_class_temp.c:104`、`xl6_cgen_stmt_decl_class_temp.c:123`、`xl6_cgen_decl_methods.c:71` |
| 对应生成的 C 代码 | `XL_Class_Counter c = {0}; xl_class_Counter__create(&c, 3);` |
| 中间形态伪代码 | `var = zero; initFields(var); if ctor then Class.create(&var,args)` |
| 实现同等功能手写的 C 代码 | `Counter c = {0}; Counter_create(&c, 3);` |
| 额外依赖的封装代码 | create 重载解析、默认字段初始化、cleanup 栈。 |

## CGEN-CLS-013 class 同类型复制初始化

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `Counter b = a` |
| 生成规则描述 | 初始化表达式不是构造调用时，会推断表达式类型；若与声明 class 类型一致，则允许整体赋值。赋值后如果 class 中存在 function 字段且来源是借用表达式，会保留 function 字段引用，避免闭包环境过早释放。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:75`、`xl6_cgen_stmt_decl_class_temp.c:90`、`xl6_cgen_stmt_decl_class_temp.c:98` |
| 对应生成的 C 代码 | `XL_Class_Counter b = {0}; b = a; /* retain function fields when needed */` |
| 中间形态伪代码 | `if type(init) == declaredClass: copy struct; retain borrowed function fields` |
| 实现同等功能手写的 C 代码 | `Counter b = a;`，但需要人工处理函数/闭包字段的引用计数。 |
| 额外依赖的封装代码 | function 字段 retain helper；cleanup 栈。 |

## CGEN-CLS-014 auto 接收 class 表达式

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `auto b = makeCounter()` |
| 生成规则描述 | `auto` 推断结果是 class 时，生成一个具体 class 类型变量，先 `{0}`，注册 class cleanup，然后整体接收表达式结果。泛型 class 会先克隆并持有具体类型节点，再按具体类型输出变量声明。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:167`、`xl6_cgen_stmt_decl_class_temp.c:179`、`xl6_cgen_stmt_decl_class_temp.c:186` |
| 对应生成的 C 代码 | `XL_Class_Counter b = {0}; b = makeCounter();` |
| 中间形态伪代码 | `auto type = infer(init); if class: declare concrete class var; assign init` |
| 实现同等功能手写的 C 代码 | 需要手动知道 `makeCounter()` 的具体返回结构体类型。 |
| 额外依赖的封装代码 | 类型推断、function 字段 retain、cleanup 栈。 |

## CGEN-CLS-015 class 方法调用

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `c.add(1)` / `self.add(1)` / `child.parentMethod()` |
| 生成规则描述 | 成员 call 优先尝试 function 字段、线程/同步/channel、隐式 `toString`，然后解析 class 方法。receiver 必须是变量名；`self` 在 class 方法内直接使用当前 class。若方法定义在父类，会计算 `.base` 路径并把父类子对象地址作为 self 传入。泛型 receiver 会设置泛型上下文并调用具体实例符号。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:185`、`xl6_cgen_value_object.c:527`、`xl6_cgen_resolve_call.c:378`、`xl6_cgen_resolve_call.c:503` |
| 对应生成的 C 代码 | `xl_class_Counter__add(&c, 1);` / `xl_class_Parent__foo(&child.base);` |
| 中间形态伪代码 | `receiver.method(args) -> Owner_method(&receiver.basePath, args)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要知道方法所属 class，并手动写 `&obj.base.base` 之类继承路径。 |
| 额外依赖的封装代码 | class 方法解析、继承路径计算、泛型符号选择。 |

## CGEN-CLS-016 class 字段访问

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `c.name` / `self.name` / `child.baseField` |
| 生成规则描述 | 成员访问在排除 error、xvalue、union 后进入 class 字段访问。`self.field` 使用 `self->`；普通变量使用 `var.`；嵌套成员或索引表达式会先推断 receiver 类型，再输出表达式和字段后缀。继承字段通过递归追加 `.base` 路径定位。receiver 形态非法时报 `class field receiver must be name, member or index`；receiver 不是 class 值时报 `class field receiver is not a class value`。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:68`、`xl6_cgen_value_object.c:692`、`xl6_cgen_value_object.c:824` |
| 对应生成的 C 代码 | `self->name` / `child.base.id` / `items[i].value` |
| 中间形态伪代码 | `fieldSuffix(class, name) -> ".field" or ".base.field"` |
| 实现同等功能手写的 C 代码 | `self->name`、`child.base.id`。 |
| 额外依赖的封装代码 | 字段查找、成员表达式类型推断。 |

## CGEN-CLS-017 隐式类方法 toString

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `a.toString()` |
| 生成规则描述 | 成员 call 若匹配隐式 `toString`，会在 class/native-backed class 方法解析前生成 xvalue 转字符串调用。receiver 先被装箱为 xvalue，再由 runtime 生成释放责任明确的字符串。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:196`、`xl6_cgen_value_object.c:516` |
| 对应生成的 C 代码 | `xl6_xrt_value_to_string_release(xl6_xvalue_from_expr(a))` |
| 中间形态伪代码 | `receiver.toString() -> xvalue(receiver).toStringRelease()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要先按实际类型装箱，再调用 xvalue 字符串转换并处理释放。 |
| 额外依赖的封装代码 | xvalue 装箱与字符串转换 helper。 |

## CGEN-CLS-018 class destroy 尾部顺序

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `destroy()` |
| 生成规则描述 | 用户 destroy 方法体执行后，生成器会追加统一 destroy tail。tail 先按字段声明的逆序清理当前 class 直接字段，再调用父类 destroy。这样模拟 C++/Rust 常见的“先子类资源，后父类资源”的析构顺序；`test.bat` 对 `cgen_class_inherit_destroy.c` 检查用户 destroy body 位于 base destroy tail 之前。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:107`、`xl6_cgen_decl_methods.c:168`、`xl6_cgen_decl_methods.c:191`、`xl6_cgen_decl_methods.c:337` |
| 对应生成的 C 代码 | `/* user body */ cleanup_direct_fields(self); xl_class_Base__destroy(&self->base);` |
| 中间形态伪代码 | `destroyBody(); for field in reverse(fields): cleanup(field); cleanup(base)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在 destroy 末尾按同样顺序调用字段释放和父类释放。 |
| 额外依赖的封装代码 | 字段类型识别、class/native-backed class/function cleanup helper。 |

## CGEN-CLS-019 function 字段清理

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Holder` / `func(int) as int fn` |
| 生成规则描述 | class 字段如果是 func 类型，析构时检查 `release` 和 `env`，调用 release 后把 `env` 与 `release` 置零。该规则只处理 class 字段里的 function，普通局部 function 由变量 cleanup 规则处理。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:107`、`xl6_cgen_decl_methods.c:124` |
| 对应生成的 C 代码 | `if (self->fn.release != 0 && self->fn.env != 0) { self->fn.release(self->fn.env); self->fn.env = 0; self->fn.release = 0; }` |
| 中间形态伪代码 | `if functionField owns env: release(env); clear` |
| 实现同等功能手写的 C 代码 | 手写 C 需要知道 function value 的 env/release 协议。 |
| 额外依赖的封装代码 | function value ABI。 |

## CGEN-CLS-020 native-backed class 字段清理

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Holder` / `File f`，其中 `File` 是有 `destroy` 的 native-backed class |
| 生成规则描述 | class 字段如果是 native-backed class 且该 native-backed class 声明了 `destroy`，析构时检查 `self->field.handle != 0`，调用 native-backed class destroy 对应的 C 函数，然后将 handle 置零。没有 destroy 的 native-backed class 字段不生成自动释放；多个 native-backed class 字段按字段声明逆序释放，`test.bat` 对 `cgen_class_native_field.c` 检查 `second` 字段释放位于 `first` 字段之前。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:82`、`xl6_cgen_decl_methods.c:139` |
| 对应生成的 C 代码 | `if (self->f.handle != 0) { file_close(self->f.handle); self->f.handle = 0; }` |
| 中间形态伪代码 | `if native-backed class.hasDestroy and handle: destroy(handle); handle = 0` |
| 实现同等功能手写的 C 代码 | `if (holder->f.handle) { file_close(holder->f.handle); holder->f.handle = 0; }` |
| 额外依赖的封装代码 | native-backed class destroy 声明解析。 |

## CGEN-CLS-021 嵌套 class 字段清理

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Outer` / `Inner inner` |
| 生成规则描述 | class 字段如果本身是 class，且该 class 需要 destroy，则析构时调用嵌套 class 的 destroy。泛型 class 字段会根据具体类型调用对应泛型实例 destroy 符号。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:148`、`xl6_cgen_decl_methods.c:154` |
| 对应生成的 C 代码 | `xl_class_Inner__destroy(&self->inner);` / `xl_class_Box__int__destroy(&self->box);` |
| 中间形态伪代码 | `if fieldType is class and needsDestroy: field.destroy(&self.field)` |
| 实现同等功能手写的 C 代码 | `Inner_destroy(&outer->inner);` |
| 额外依赖的封装代码 | class destroy 需求判断、泛型 class 符号选择。 |

## CGEN-CLS-022 父类清理

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Child extends Parent` |
| 生成规则描述 | destroy tail 处理完当前 class 直接字段后，如果父类存在且需要 destroy，则生成父类 destroy 调用，参数为 `&self->base`。泛型父类当前不在该路径中单独建模。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:168`、`xl6_cgen_decl_methods.c:185` |
| 对应生成的 C 代码 | `xl_class_Parent__destroy(&self->base);` |
| 中间形态伪代码 | `after child fields cleanup: if base.needsDestroy: base.destroy(&self.base)` |
| 实现同等功能手写的 C 代码 | `Parent_destroy(&child->base);` |
| 额外依赖的封装代码 | 父类查找、destroy 需求判断。 |

## CGEN-CLS-023 自动 destroy 生成

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class Holder` 有需要释放的字段，但没有显式 `destroy()` |
| 生成规则描述 | class 方法生成阶段会扫描是否存在用户 destroy。若不存在且 class 需要 destroy，则生成一个自动 destroy 函数，函数体只有统一 destroy tail。泛型 class 的每个具体实例也按同样规则生成自动 destroy。 |
| 生成器文件名和行号 | `xl6_cgen_decl_methods.c:372`、`xl6_cgen_decl_methods.c:391`、`xl6_cgen_decl_methods.c:421`、`xl6_cgen_decl_methods.c:308` |
| 对应生成的 C 代码 | `void xl_class_Holder__destroy(XL_Class_Holder* self) { cleanup_fields_and_base(self); }` |
| 中间形态伪代码 | `if !hasUserDestroy && needsDestroy: emitAutoDestroy()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要即使没有业务析构逻辑，也补一个释放字段的析构函数。 |
| 额外依赖的封装代码 | class needs-destroy 分析、cleanup tail。 |

## CGEN-CLS-024 native-backed class 结构体布局

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class File ... native point handle ... end` |
| 生成规则描述 | native-backed class 被生成为只包含 `void* handle` 的轻量结构体，用于把 C 黑盒资源映射成 xlang 的类式对象。native-backed class 不展开 C struct 内部布局。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:612`、`xl6_cgen_decl_class.c:618` |
| 对应生成的 C 代码 | `typedef struct XL_Class_File { void* handle; } XL_Class_File;` |
| 中间形态伪代码 | `native-backed class Name -> struct { void* handle }` |
| 实现同等功能手写的 C 代码 | `typedef struct FileRef { void* handle; } FileRef;` |
| 额外依赖的封装代码 | 需要进入 `xvalue` 时生成 handle box runtime。 |

## CGEN-CLS-025 native-backed class extern 原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `extern create(...) = c_create` / `extern destroy = c_destroy` / `extern func read(int n) string = c_read` |
| 生成规则描述 | native-backed class 的 extern 成员直接声明对应 C 函数原型。`create` 返回 `void*` 且不带 self；`destroy` 固定为 `long long func(void* self)`；普通 extern 方法首参固定 `void* self`，返回值最多一个。参数使用 C ABI 类型名，`...`/`***` 打包参数不能直接进入 C ABI 原型，会明确诊断为 `native-backed class extern parameter must have a non-vararg C ABI type`；多返回会明确诊断为 `native-backed class extern method cannot return multiple values`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:489`、`xl6_cgen_decl_class.c:507`、`xl6_cgen_decl_class.c:522`、`xl6_cgen_decl_class.c:551`、`xl6_cgen_decl_class.c:632` |
| 对应生成的 C 代码 | `void* file_open(char* path);` / `long long file_close(void* self);` / `long long file_read(void* self, long long n);` |
| 中间形态伪代码 | `native-backed class extern method -> C ABI prototype(handle, params...)` |
| 实现同等功能手写的 C 代码 | 手写 C 就是声明原始库函数原型。 |
| 额外依赖的封装代码 | 无 helper；调用时直接调用 extern C 符号。 |

## CGEN-CLS-026 native-backed class helper 方法原型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `class File` with `native point handle` / `func read(int n) string ... end` |
| 生成规则描述 | native-backed class 内部用 xlang 实现的 helper 方法生成 `xl_class_Name__method` 符号，首参是 `XL_Class_Name* self`，而不是原始 `void* handle`。方法体内部可通过 `self.handle` 调用 extern 方法。当前 helper C 原型只允许 0 或 1 个返回值，多返回会明确诊断为 `native-backed class helper method cannot return multiple values`。 |
| 生成器文件名和行号 | `xl6_cgen_decl_class.c:565`、`xl6_cgen_decl_class.c:590`、`xl6_cgen_decl_class.c:597`、`xl6_cgen_decl_methods.c:467` |
| 对应生成的 C 代码 | `char* xl_class_File__read(XL_Class_File* self, long long n);` |
| 中间形态伪代码 | `native-backed helper(self: Class*, params...) -> body` |
| 实现同等功能手写的 C 代码 | `char* File_read_helper(FileRef* self, long long n) { ... }` |
| 额外依赖的封装代码 | native-backed class helper 符号名；helper 体内仍可使用普通 xlang cleanup。 |

## CGEN-CLS-027 native-backed class 声明初始化与 create/pointer 绑定

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `File f = File(path)` / `File f = File(ptr)` |
| 生成规则描述 | native-backed class 变量声明先生成 `{0}`，注册 native-backed class cleanup。若初始化是 native-backed class 构造表达式，`.handle` 接收 create 返回值；若 native-backed class 没有 create，只允许一个 point/指针参数绑定已有句柄。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:16`、`xl6_cgen_stmt_decl_class_temp.c:23`、`xl6_cgen_resolve_call.c:453`、`xl6_cgen_resolve_call.c:525`、`xl6_cgen_resolve_call.c:550` |
| 对应生成的 C 代码 | `XL_Class_File f = {0}; f.handle = file_open(path);` 或 `f.handle = ptr;` |
| 中间形态伪代码 | `native-backed class var -> zero; if create exists: handle=create(args); else handle=args[0]` |
| 实现同等功能手写的 C 代码 | `FileRef f = {0}; f.handle = file_open(path);` |
| 额外依赖的封装代码 | native-backed class create 解析、native-backed class cleanup 栈。 |

## CGEN-CLS-028 native-backed class 同类型复制限制

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `File b = a` |
| 生成规则描述 | 如果 native-backed class 没有 `destroy`，它被视为普通句柄值，可进行同类型整体复制；如果 native-backed class 有 `destroy`，声明初始化必须使用同类型构造表达式，避免两个变量持有同一资源并重复 destroy。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_decl_class_temp.c:27`、`xl6_cgen_stmt_decl_class_temp.c:35`、`xl6_cgen_stmt_decl_class_temp.c:42` |
| 对应生成的 C 代码 | `XL_Class_File b = {0}; b = a;` 仅在无 destroy 的 native-backed class 上允许。 |
| 中间形态伪代码 | `if native-backed class.hasDestroy: forbid copy; else allow struct copy` |
| 实现同等功能手写的 C 代码 | 手写 C 需要靠约定避免双重释放；xlang 在生成阶段限制。 |
| 额外依赖的封装代码 | native-backed class destroy 存在性判断。 |

## CGEN-CLS-029 native-backed class 方法调用 helper/extern 分流

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `f.read(10)` |
| 生成规则描述 | native-backed class 方法调用先解析 receiver 类型，再分别查找 helper 方法和 extern 方法。二者同时存在时报错。helper 方法首参传 native-backed class 对象指针；extern 方法首参传 `handle`。当前 native-backed class helper 内的 `self` receiver 会特殊处理：helper 调用传 `self`，extern 调用传 `self->handle`。receiver 不能解析为 native-backed class 值时报 `native-backed class method receiver is not a native-backed class value`。 |
| 生成器文件名和行号 | `xl6_cgen_value_object.c:397`、`xl6_cgen_value_object.c:422`、`xl6_cgen_value_object.c:438`、`xl6_cgen_value_object.c:453` |
| 对应生成的 C 代码 | `xl_class_File__read(&f, 10);` 或 `file_read(f.handle, 10);` |
| 中间形态伪代码 | `if helper exists: helper(&receiver,args); else extern(receiver.handle,args)` |
| 实现同等功能手写的 C 代码 | `File_read_helper(&f, 10);` 或 `file_read(f.handle, 10);` |
| 额外依赖的封装代码 | native-backed class 方法解析、receiver handle/pointer 表达式生成。 |

## CGEN-CLS-030 成员访问和成员调用分派优先级

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `obj.name` / `obj.length` / `obj.method()` |
| 生成规则描述 | 点号成员访问不是单一路径。成员访问优先级为：error 专用字段、xvalue 成员、union 字段、class 字段。成员调用优先级为：function 字段调用、thread/sync/channel 方法、隐式 `toString`、class 方法、native-backed class 方法。这个顺序决定同样的 `.` 写法最终生成哪类 C 代码。 |
| 生成器文件名和行号 | `xl6_cgen_expr.c:229`、`xl6_cgen_expr.c:386`、`xl6_cgen_value_object.c:460`、`xl6_cgen_value_object.c:624`、`xl6_cgen_value_object_fields.c:95`、`xl6_cgen_value_object_fields.c:145` |
| 对应生成的 C 代码 | `xl6_error_message(e)` / `xvoArrayItemCount(a)` / `u.field` / `obj.field` / `xl_class_A__m(&obj)` / `c_func(obj.handle)` |
| 中间形态伪代码 | `dispatch(member): error -> xvalue -> union -> class; dispatch(call): functionField -> builtins -> class -> native-backed class` |
| 实现同等功能手写的 C 代码 | 手写 C 需要直接选择具体 API；xlang 通过类型推断和注册表选择。 |
| 额外依赖的封装代码 | 类型推断、class/native-backed class/union 查找、标准并发类型方法分派。 |
