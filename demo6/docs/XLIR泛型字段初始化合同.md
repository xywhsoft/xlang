# XLIR 泛型字段初始化合同

泛型字段的类型闭包与其初始化表达式必须同时完成。不能只把
`Completion<T>.producer` 的字段类型改为 `Promise<int>`，却继续执行共享的
`Promise<T>()` 表达式；Cgen 也不能根据字段名或调用上下文修补类型。

## 唯一语义来源

- Typed XLIR 的 `ClassField.declaration` 保留源字段身份，`initializer` 指向源表达式。
- Lowered XLIR 对每个具体 owner 的显式初始化表达式建立独立、完整的表达式图。
  `declaration` 不变，`initializer` 指向 owner-closed 图；JSON 中的
  `initializerTemplate` 同时展示原始表达式身份。
- 类型替换使用声明局部的正式 TypeId 与实参 TypeId，而非参数名称猜测。
  表达式、调用、实参、内部局部初始化、原生句柄投影以及异常退出计划一起克隆。
  节点拥有的文本和解码字面量数据独立复制，失败的部分克隆也不能重复释放源数据。
- 初始化图附着于规范化类体的列表中，但只能通过 ClassField 的初始化程序执行。
  不把它作为独立类语句执行，也不缓存运行时结果。
- 初始化图与普通泛型 callable 共用有限专化预算；依赖图在物理调用 ABI 注册前闭包。

## 局部符号和生命周期

克隆声明、参数、闭包和局部变量具有新的符号身份。稀疏的专化符号索引保存
这些符号到规范声明节点的映射，避免拿接近 UINT32_MAX 的合成符号扩展密集数组。
索引有序、唯一、拥有于模块；所有查询使用统一的 `xl6IrSymbolNodeGet`。

源匿名函数模板不注册运行时调用目标；其具体初始化图中的克隆独立注册。
类中默认闭包的 nominal owner 由同一个 ClassField 合同确定。每次构造仍执行一次
字段表达式，已提交字段在后续初始化抛错时按原有拥有图回滚。

## 生成前验证

验证源表达式、具体 owner、表达式形状、完整类型替换、局部符号索引以及既有
SSA/初始化/清理合同。交换 int/string owner 的图、退回共享模板、破坏类型、
篡改局部绑定等都必须在任何 C 或头文件输出前拒绝。Cgen 无新增特殊路径。

用例：`stdlib_promise_generic_field.xl`、`xlir_generic_field_initializer.xl`、
`xlir_generic_field_initializer_contract.c`。后者执行元数据变异并检查零输出拒绝。

本批不宣称所有泛型语言能力完成。带外层泛型类型形参的匿名函数，另有在
Typed TypePool 验证前就存在的失败用例；它与普通具体签名闭包的图克隆不同，
仍须单独修复和验收。异步多返回值等已决策限制也不在此扩展。
