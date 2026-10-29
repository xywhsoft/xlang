# 现代类型语义与 HIR 改造

本文是 DEMO6 类型与语义地基的权威设计和实现状态文档。实现只参考
`demo6/docs` 当前文档与本文，不从 DEMO5 或历史阶段文档恢复兼容语义。

## 1. 目标

本阶段完成四项相互依赖的改造：

1. 现有语言能力通过统一调用、左值和退出计划实现正交组合。
2. 建立独立于 AST 和 C 后端的规范类型模型与真实 typed HIR。
3. 建立非空默认、显式 nullable、确定赋值、流类型收窄和 weak reference。
4. 实现用户 protocol、numeric/tagged enum 和穷尽 match。

本阶段不是继续堆叠语法。核心目标是让语义阶段成为唯一事实来源，让
`xl check` 成功的程序必然能够进入后端。

## 2. 硬约束

- 不保留旧语法、兼容别名或用户可见的回退路径。
- 源码语义只由 HIR 提供；C 生成器不得重新选择重载、绑定参数或改变源码值的
  类型与存储类别。AST 只保留源码顺序和语法形状，后端合成节点才允许局部推断。
- XRT 能表达的运行时能力必须先在 XRT 中建立，并能独立供 C 使用。
- XRT 不依赖 xlang 语法；xlang 只映射 XRT 的通用类型、对象和调用模型。
- 规范类型不使用字符串判断相等。类型字符串只用于诊断、dump 和 ABI 名称。
- 新增源码文件按职责拆分，避免继续扩大 `xl6_sem.c` 和大型 cgen 模块。
- 所有新增注释使用中文，代码段之间使用清晰分区。

## 3. XRT 事实来源

DEMO6 的构建只包含 `demo6/lib/xrt/xrt.h` 这一份 XRT。该文件由
`D:/GIT/xrt/dev/ver1/singlehead/xrt.h` 生成并同步；两者在本次收口时的
SHA-256 完全一致。编译器、生成程序、`xl.dll` 和 `xlc.dll` 都不得再混入
第二份 XRT 实现或另一套对象生命周期。

这里固定的是 `dev/ver1` 生成的约 4.7 MB 发布候选快照，不是 XRT 根目录
`single/xrt.h` 中约 8.9 MB、仍在重构的开发主线。`tools/build_xrt_obj.bat`
只能编译 `tools/xrt_single_header_impl.c`，后者只能包含仓库内
`lib/xrt/xrt.h`；DLL API 边界守门会拒绝外部 XRT 绝对路径、`XRT_ROOT` 类
环境变量和上级目录回读。

XRT 根目录的新 `src/include/single` 架构是后续上游演进线，已经拥有通用的
TypeId、weak、protocol 和 owned promise 基础，但它与 DEMO6 使用的标准库 API
尚未完成整体收敛，不能通过直接替换头文件迁移。后续迁移必须作为一次显式的
API 汇流完成，而不是让两个运行时在同一进程中并存。

DEMO6 当前固定同步流程：

1. 在 `D:/GIT/xrt/dev/ver1/lib` 中实现或修复 DEMO6 依赖的 XRT 能力。
2. 在 `D:/GIT/xrt/dev/ver1/test` 中补独立 C 测试。
3. 运行 `D:/GIT/xrt/dev/ver1/singlehead/single_head_maker.exe`。
4. 将生成的 `singlehead/xrt.h` 同步到 `demo6/lib/xrt/xrt.h`。
5. 运行 XRT C 回归、xlang 回归和发布守门。

`demo6/lib/xrt/xrt.h` 是 DEMO6 构建时唯一事实来源；XRT 新主线是待汇流的上游，
不是可同时链接的兼容回退。

## 4. 规范类型模型

编译期使用驻留类型池。每个规范类型拥有稳定的 `XL6_HirTypeId`。

类型种类至少包括：

- builtin
- class
- union
- function
- generic instance
- type parameter
- nullable
- protocol
- numeric enum
- tagged enum

函数类型保存参数类型、参数模式、返回值列表、async 和可变参数标记。
泛型实例保存开放声明和规范类型实参。nullable 保存唯一的底层类型。

类型别名在进入类型池前展开。类型名称和 ABI 名称不参与同一编译单元内的
类型身份判断。

## 5. Typed HIR

DEMO6 当前 HIR 是一棵独立于 AST 的紧凑 typed semantic graph，不是 SSA，
也不伪装成尚未实现的三地址基本块 IR。每个 HIR 节点通过 `source` 指回源码
AST，并拥有独立的子节点索引、稳定符号身份和规范类型身份。

核心身份：

- `XL6_HirTypeId`：驻留类型池中的规范语义类型。
- `XL6_SymbolId`：稳定符号身份；不以名称字符串代替绑定结果。
- `XL6_HirNodeId`：HIR 图中的节点身份。

每个相关节点保存：

- `type`：当前控制流位置上的语义类型。
- `storageType`：值在 C ABI/运行时中的真实存储类型。
- `symbol` / `targetSymbol`：声明与最终绑定目标。
- `callKind`、`lvalueKind`、`exitKind`：调用、写回和退出计划。
- scope/region、value category 与 ownership：清理和所有权计划。

例如 `var` 在 `is bool` 成立的分支中，`type` 可以收窄为 `bool`，但
`storageType` 仍是 `var`。后端必须同时尊重两者，不能把控制流收窄错误地
当作 C 存储布局变化。

运行时桥接只在表达式仍以 `xvalue` 存储形态输出时执行一次。若名称表达式已
根据 HIR 收窄并生成标量 C 值，外层赋值、参数或返回 lowering 不得再次解包；
否则会形成双重转换，造成非法指针读取和进程崩溃。该边界由声明、赋值、复合
赋值、位置/命名参数、返回、字段写入和字符串转换矩阵共同守门。

C 后端仍遍历 AST 以保持源码声明顺序、语句形状和诊断位置，但所有源码节点
的类型、符号、调用、左值、所有权和退出决策必须读取 HIR。仅后端为 lowering
临时构造的节点、循环目标和表达式替换节点允许使用局部推断；这些结果不得
反向改变源码 HIR。未来如果引入 SSA/MIR，应在 HIR 之后增加独立阶段。

### 5.1 调用计划

语义阶段为所有调用生成统一 `CallPlan`：

- 最终 callee 和完整签名
- receiver 与派发方式
- 参数到形参的绑定结果
- 默认参数
- `...` / `***`
- byval / byref / out
- 返回值列表
- 泛型实例
- 所有权效果

普通函数、成员函数、虚方法、协议方法、函数值、extern 和 native-backed
成员函数不得再走互不相干的参数生成路径。

### 5.2 左值计划

所有可写目标归一为 `LValuePlan`：

- 局部与全局变量
- class 字段
- property
- index
- 闭包捕获槽
- byref / out 槽

复合赋值统一降低为读取、运算和写回。协议运算符只参与运算步骤，写回规则
由左值计划决定。

### 5.3 退出与清理计划

return、break、continue、throw 和异常展开都通过 `ExitPlan` 离开作用域。
退出计划显式执行局部值释放、finally、生成器关闭和任务资源清理。

`try/catch/finally` 内不再因为后端缺乏清理路径而禁止 return、break 或
continue。

## 6. Nullable 与生命周期

`T` 默认非空，`T?` 表示可空：

```xlang
Node node = Node()
Node? parent = null
```

规则：

- `null` 只能赋给 nullable、`var` 或明确允许空值的 C ABI 类型。
- 非空局部变量读取前必须完成赋值。
- 非空字段必须有初始化器，或由每个构造函数完整初始化。
- `if value != null`、`value is Type` 和 match 分支支持流类型收窄。
- byref/out 写入、闭包写入和不稳定成员读取会使相关收窄失效。
- `value as T` 失败抛出可捕获的 `TypeError`。
- `value as T?` 失败返回 `null`。

nullable 引用使用原有空句柄表示，不增加尺寸。nullable 标量、union 和
numeric enum 使用带 `hasValue` 的静态值布局。

### 6.1 Weak reference

XRT 提供与语言无关的受管理弱引用，第一阶段覆盖 class 和具备完整 XRT
生命周期的 native-backed class。

```xlang
weak<Node> parent = node
Node? value = parent.lock()
```

最后一个强引用释放时立即析构。弱引用只保留控制信息，不延长 payload 和
外部资源生命周期。借用型 native handle 在没有 retain/release/liveness
能力时不能创建弱引用。

本阶段不引入完整 GC。对象图中的反向边和观察者关系使用 `weak<T>` 打破强
引用环。

## 7. Protocol

只提供 `protocol`，不提供 `interface` 兼容别名。

```xlang
protocol Writer
    func write(Buffer data) as int
    get closed() as bool
end
```

规则：

- class 按公开成员的完整签名进行结构化满足，不需要 `implements`。
- protocol 支持普通方法、重载、property、async 签名和泛型参数。
- 第一阶段不支持字段、构造函数、默认实现和 associated type。
- protocol 可用于变量、参数、返回值、容器和泛型约束。
- 具体类型已知时静态派发；protocol 类型值使用 witness table 派发。
- native-backed class 的 extern 成员可以直接满足 protocol。

XRT 提供通用 protocol descriptor、method requirement、witness table 和
类型查询 API。XRT 结构中不出现 xlang 语法名称。

## 8. Enum

同一个 `enum` 语句覆盖 numeric enum 和 tagged enum：

```xlang
enum HttpStatus as int32
    ok = 200
    notFound = 404
end

enum Result<T>
    ok(T value)
    error(string message)
end
```

numeric enum 采用指定整数底层类型，在 xlang 中不隐式参与整数运算。
`int(value)` 和 `HttpStatus(value)` 承担显式转换和检查。

tagged enum 使用 `tag + union payload` 的静态布局，默认不堆分配。payload
中的受管理成员由 HIR 显式执行初始化、保留、移动和释放。

enum 必须进入 `type()`、`var`、泛型、容器、XAPI、C 头文件、ABI hash 和
文档生成。

## 9. Match

语句形式：

```xlang
match result
case ok(value)
    println(value)
case error(message)
    println(message)
end
```

表达式形式：

```xlang
string text = match result
case ok(value) then value
case error(message) then message
end
```

第一阶段支持：

- enum variant
- 字面量
- `null`
- `else`

enum、bool 和 nullable match 执行穷尽检查。禁止重复分支、隐式 fallthrough
和永远不可达的 case。第一阶段不实现数组/字典解构、guard 和用户 extractor。

numeric enum 和 tagged enum match 都由 HIR 固定判别类型、variant 绑定和
穷尽结果，再由 C 后端降低为 `switch`。

## 10. 当前实现状态

- 规范类型池、`XL6_HirTypeId`、`XL6_SymbolId` 和独立 HIR module 已落地。
- CallPlan、LValuePlan、ExitPlan、语义类型/存储类型和所有权计划已进入 HIR。
- nullable、确定赋值、流收窄、运行时 cast 与 `weak<T>` 已实现。
- 用户 protocol、结构化满足和 XRT witness registry 已实现。
- numeric/tagged enum、语句/表达式 match 与穷尽检查已实现。
- enum/protocol/nullable 已进入泛型、容器、XAPI、C 头和 ABI 元数据路径。
- 后端对源码语义使用 HIR；保留的局部推断仅服务于后端合成节点，不是旧语义
  fallback。

当前 HIR 没有声称提供 SSA 或独立三地址 CFG。若未来优化器需要这些能力，
应从 typed semantic HIR 降低到新的 MIR，而不是重新把语义判断塞回 cgen。

## 11. 发布级验收

- `xl check` 成功后，cgen 不得因为语言组合形态失败。
- cgen 不包含面向合法源码的“当前暂不支持”诊断。
- HIR module 通过结构、类型、符号、调用、左值、所有权和退出计划 verifier。
- 现有完整回归继续通过。
- 新增 nullable、weak、protocol、enum、match 的正例、负例和组合矩阵。
- C 与 xlang 双语覆盖 XRT weak、protocol descriptor 和 enum descriptor。
- export/import/package/XAPI 保留完整类型身份和 ABI hash。
- Windows 本地构建、运行、fuzz 和并发压力测试通过；Linux 原生构建与
  ASan/UBSan 由 CI 守门，本机没有 WSL 时不伪造本地 Linux 结论。
- 文档、功能清单、范例和实际实现保持一致。
