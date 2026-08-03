# Typed HIR 与后端边界

本文档规定 DEMO6 语义阶段与代码生成阶段之间的唯一类型口径。

## 1. 编译阶段

DEMO6 当前编译主线为：

1. Flex 词法分析。
2. Lemon 语法分析，产生 AST。
3. import 与标准库展开。
4. 语义检查、重载解析与泛型推断。
5. typed HIR 收口。
6. C 代码生成与 TCC 后端。

typed HIR 使用独立的紧凑语义图。`XL6_HirModule` 拥有驻留类型池、HIR 节点、
子节点索引、源码索引和符号索引；每个 HIR 节点通过 `source` 指回对应 AST，
因此不复制 token 文本和源码位置信息。

AST 负责语法形状与源码位置，HIR 负责语义事实。两者有明确分工，不再把
`inferredType` 覆盖层称为完整 HIR，也不把当前 HIR 描述成尚未实现的 SSA 或
三地址基本块 IR。

## 2. 完整性约束

`src/ir/xl6_hir.c` 在语义检查成功后构建并验证 HIR。以下确定产生单值的
节点必须具有规范 `XL6_HirTypeId`：

- 整数、浮点、字符串、时间、布尔和 null 字面量。
- 普通字符串插值、匿名函数。
- 索引、安全索引、条件表达式。
- 一元、二元、`is`、运行时 `as` 和 `await` 表达式。
- array、list、set、dict 和带类型容器字面量。

函数调用可能无返回值或产生多返回值，因此不能只用单一值类型表达；返回值
列表由最终解析签名和调用计划共同承担。函数名、成员名和泛型实例也可能只
作为 callee，不强制伪造运行时值类型。

HIR 节点同时保存 `type` 与 `storageType`。前者允许表示 nullable 流收窄和
协议视图，后者固定真实 C/XRT 存储；两者不能相互覆盖。

缺少必需类型会在 `hir` 阶段直接报错，不允许静默进入后端。

## 3. 后端规则

- cgen 通过 `XL6_HirModule`、`xl6HirSourceType()` 和
  `xl6HirSourceStorageType()` 读取语义阶段的权威类型。
- `symbol`、`targetSymbol`、`callKind`、`lvalueKind`、`exitKind` 与
  ownership plan 同样只能来自 HIR。
- 非空 `targetSymbol` 必须能在模块符号索引中解析到真实声明节点；合成测试和后端临时节点也不得伪造悬空的“已解析”目标符号。
- 泛型实例化时，只对 HIR 类型执行当前类型参数替换和别名规范化。
- cgen 自己创建的协议探测节点、重载匹配节点和表达式替换节点没有源码 HIR，此时才允许使用后端局部推断。
- 源码表达式不能因为 cgen 无法处理某种 AST 形状而从静态类型降级为 `var` 或动态容器。
- `auto` 的 C 存储由 HIR 类型决定；例如 `array<int>` 必须使用 `xtarray`，不能再次按初始化表达式形状猜测。
- cgen 可以遍历 AST 保留源码顺序和语句形状，但不能借此重新做源码语义决策。

## 4. 容器类型推断

同质容器字面量在语义阶段形成静态泛型类型：

```xlang
auto a = [1, 2]          // array<int>
auto b = {1, 2}          // set<int>
auto c = {"x": 1}       // dict<int>
```

异质字面量保留动态容器类型。静态泛型容器可以显式或按声明目标装箱为同类动态容器；动态容器导入静态泛型容器时必须逐项验证。不同容器种类、不同静态泛型参数之间不做兼容匹配。

## 5. 验收

- 核心容器、闭包、OOP、泛型、异步、任务和语义负例套件必须通过。
- `xl check` 成功后 `XL6_CompileUnit.hirReady` 必须为真，HIR module 必须通过
  类型、符号、调用、左值、所有权和退出计划验证。
- cgen 不得再次改变源码表达式的静态类型和存储类别。
- `tools/check_typed_hir_contract.ps1` 必须同时接入主回归和 RC0 守门，
  阻止 cgen 重新直读 AST `inferredType`、绕过 HIR 源表达式类型网关，
  或丢失流合并失效与源索引不变式。
- `test/cases/hir_flow_value_ownership_matrix.xl` 作为跨特性组合基线，
  同时覆盖 class、protocol、`var`、`is/as`、泛型 enum、nullable、
  typed array/list/dict、闭包捕获、weak handle 与 async/future 的所有权交叉。
- `test/cases/hir_module.c` 必须用真实符号构造 resolved call，并验证损坏类型、存储类型、子节点、源码身份、符号索引和统计量都会被 verifier 拒绝。
