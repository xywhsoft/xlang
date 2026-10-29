# List 元素准入与目标构造合同

XLIR 为 List 保存独立的元素准入 schema，Cgen 依据已验证的目标合同
生成布局和驻留工厂调用。这个合同用于准备原生载体切换，不授权把
现有语言 List 的 IntMap 解释成原生 handle。语言工厂、生产者、索引、
迭代、var 分发及 List 恢复的联动迁移仍待完成。

## 类型与构造

`listValueSchemaType` 指向规范化的 `Array<T>` 准入 witness。它只描述
元素域，不表示 List 内存储了 Array；原生 List 拥有自己的 typed 存储。
默认 `List` 明确使用 `Array<var>`，不能把不支持的 `List<T>` 降级成 ANY。

AST 类型登记、类字段替换和函数泛型特化均补齐 witness。特化后的
重复类型复用同一条规范记录；可能扩容的 interning 完成后重新取得
TypePool 指针。最终闭包完成时补齐默认 List，随后刷新派生字段。

`xl6IrListConstructionRecipe` 验证实际 List 形状、精确 witness 和
所有嵌套/名义元素合同，成功才发布 schema 与名义集合。模块验证器
独立重推派生字段；物理槽与 transfer 相同不代表逻辑类型相同。
开放 function、point、Set/List 元素以及未闭合类型参数仍拒绝目标
构造，可空 List owner 也不能充当直接构造域。

## 生成 C 与失败处理

私有目标 renderer 复用 Sequence 准入工厂，生成空 witness、完整 List
布局、`xl6ListCreateTakeV1` 和 resident 投影绑定。List 工厂消费临时
witness 的所有路径，并只保留独立拥有的准入 metadata，不保留其存储。
闭包槽明确使用 boxed Value，Future 使用既有精确 resident 策略。

生成代码不实现 List 算法，不遍历 wire，不从类型拼写追溯语义。
拒绝的目标域不写出部分代码。List 和嵌套 Queue schema 工厂在错误
作用域进入失败时传播 checked 失败并保持空结果，不再由该路径 abort。
这不代表其他既有 Cgen 错误作用域的所有 abort 已清理。

## 验证范围

真实源码编译器、真实 XLIR 和 Cgen unity 专项覆盖 59 个完整目标域、
944 项 metadata 损坏拒绝、25 个同物理槽不同逻辑域、3 项嵌套损坏和
默认 erased List。5 个合法但不支持的目标域保留拒绝；无效字段及错误
名义集合在输出前拒绝。

Windows 与 Linux 各通过 21 阶段。生成目标 C 在 O0/O2 各执行到首个
无故障点：439 次分配尝试和 62 次错误作用域进入尝试，每点独立核验
分配账本。作用域故障只替换生成 C 的 Begin 调用，模拟无诊断的入口
拒绝，不替换驻留运行时 internals，也不声称模拟了全部 OS 故障。

旧语言 List 的泛型/erased 用例另通过确定性 C、严格 O0/O2 执行账本
和实际 TCC JIT。三组整体、原生 List 语言切换、发布 DLL/SO、VFS、
打包 AOT 和 sanitizer 验收不由这个专项门证明。

最终候选源码为 `E:/xlang-validation/stdlib53-b173-source-dev5/frozen/demo6`，
Linux 镜像为 `/var/tmp/xlang-stdlib53-b173-source-dev5/demo6`。专项使用
`check_stdlib_list_recipes.py --root <frozen-demo6> --public-directory <fresh-build>
--out <fresh-evidence>`，必须链接双端当前源码编译器与运行时对象，不能
用旧 libxlc 代替。最终报告为 `stdlib53-b173-list-*-final1/results.json`。

## 后续范围

下一步联动完成 List 的源载体与全部使用边界，再接通恢复；Set/Dict、
其余已批准容器、资源策略与跨组发布验收继续。共享默认子对象仍按
声明 clone 隔离，否则 checked 拒绝。不新增公共 API 决策，CRT 无
未决策，19 个 XRT 未决策组及对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
