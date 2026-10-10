# List 原生语言构造与稀疏导入

受支持的 List 字面量、空默认值和目标类型导入现在直接产生原生稀疏
List，不再先构造 wire int-map 后修改类型标记。XLIR 明确区分
`LIST_CREATE` 与 `LIST_IMPORT_CHAIN`，Cgen 只消费已验证的构造和导入合同。
公开 API 与既有共享对象的 clone 授权范围不变。

## 语言入口与类型合同

字面量和默认构造在 checked 工厂调用前绑定完整元素 schema、准入域
与运行时身份。已有的同类型值保留原生身份；擦除到 `list` 或 `var`
不重新标记源容器。初始化、赋值、实参和返回值的目标类型边界使用
明确的导入操作，保留完整 int64 稀疏键。

导入成功返回独立根容器；错误类型、错误容器族和分配失败不发布
部分结果，赋值失败不替换旧目标。普通对象子节点保持浅引用语义，
不是隐式深复制。完整构造 recipe 同时决定物理槽、投影器、转换器
和依赖闭包，不能根据值的内容或源代码类型名称补猜目标。

XLIR verifier 拒绝缺失或不匹配的身份、元素 payload、容器族、
准入域、构造 witness、参数数目和错误分支。List 导入必须具有
`may-raise` 合同及紧随其后的 checked 错误测试；wire int-map
工厂、通用 rebind 和 Array 操作不能冒充 List 的生产者。

## 嵌套 Array 与共享图

显式目标 schema 授权将真实原生 `array<var>` 子节点导入 typed
Array，根 List 和各层 Array 共用一个导入 memo。同一来源在同一
schema 深度只转换一次，转换后的容器与源容器隔离，重复边仍共享
同一结果。

已符合完整目标域的 typed 子节点保持浅引用，即使子节点处于 BUSY
状态也不读取其槽位。只有经原生 handle 与驻留 ANY policy 共同
认证的擦除 Array 才能转换；其他 typed 值或外来 handle 不因其
内容恰好符合目标而获得转换授权。紧凑 JSON/XLON 恢复仍使用独立
输入模式，不继承语言稀疏键规则。

外部共享默认子对象仍只按声明的 clone 能力复制后恢复；没有该
能力时 checked 拒绝，不修改原对象。具体规则见
[List 的 JSON/XLON 投影与恢复](stdlib53-b182-list-persistence-native.md)。

## 生成代码与运行时依赖

Cgen 发出精确的原生 List 工厂、后续 Array 模板和一个
`xl6CollectionImportChainSourceTakeV1` 调用，不生成逐项导入循环、
图 memo 或递归转换算法。导入 seed 和提供者由已验证的 recipe
决定。JIT 注册和独立产物使用同一驻留符号目录，纯 Array 产物
不引入 List 工厂或 List 输入提供者。

wire int-map 继续作为显式 C 输入边界接受，不再是普通语言 List
的内部存储。分配与清理失败保持首次 checked 诊断，所有输入
快照和 seed 都按既有拥有协议退休。

## 验收

Windows 272 个、Linux 271 个编译单元从选定源码新编译，不以
对象覆盖或旧 shipping 实现代替本批代码。新生产者专项双端各
194 阶段通过，覆盖实际的 20 次原生构造与 20 次目标导入；
355 项坏 IR 合同在 C、声明头和产物计划输出前拒绝。

三种实际语言程序在 O0/O2 下各运行完整模块分配前缀，共
1,456 次独立初始化、终结及资源账本检查。包含 char、内嵌 NUL、
UTF-8、bytes、nullable、C 标量、数值枚举、对象、callable、Future、
共享嵌套 Array、INT64 两端键和失败后目标不变。程序均通过真实
裁剪产物执行与 JIT；纯 Array 程序不引入 List。

原生专项双端各通过 1,220 阶段、84 条完整故障前缀与 5,366 次
逐点尝试，包括九种物理域的原生 ANY 子 Array 导入、重复边
隔离与匹配 BUSY 子对象的浅引用。三项真实实现负对照先严格
编译，再由同一组合同拒绝。

十二项语言与相邻回归双端各通过 1,448 阶段。阶段包含编译、
执行与预期拒绝，不是独立语言测试用例的数量：

| 专项 | 双端各通过阶段 |
|---|---:|
| 原生语言生产者 | 194 |
| List 持久化 | 236 |
| Closed/Open 恢复 | 96 |
| 开放复制 | 167 |
| DynamicFields | 104 |
| List 方法 | 84 |
| List 代数 | 134 |
| 动态索引和查询 | 166 |
| 静态索引 | 149 |
| 快照遍历 | 64 |
| 精确键和动态方法 | 33 |
| 构造 recipe | 21 |

驻留目录双端各通过 233 组件、1,223 精确导出与 13 负对照。
公开能力矩阵与 deferred API 清单逐字不变；文档门保留相同
14 项历史拒绝，本批新增为零，不作为全仓文档门通过。

正式报告与提交门共同认证选定源码、编译产品、生成 C、fixture、
runner 和故障负对照。Linux 仪器化范围限于选定原生闭包及专项
裁剪生成程序，不等于完整编译器的 sanitizer 验收。

原生报告：

- Windows：`E:/xlang-validation/stdlib53-b184-native-win-final3/results.json`
- Linux：`/var/tmp/xlang-stdlib53-b184-native-linux-final3/results.json`

语言与相邻回归报告：

- Windows：`E:/xlang-validation/stdlib53-b184-regressions-win-final3`；
  主机提交内存不足的六项另用
  `E:/xlang-validation/stdlib53-b184-regressions-win-memory-retry1` 复测。
- Linux：`/var/tmp/xlang-stdlib53-b184-regressions-linux-final3`

Windows 的原始内存失败记录保留，不计入通过阶段；最终门只采用
新目录中的已完成复测与原先已完成的专项，逐项核对来源和哈希。

## 后续工作

封闭的 `Array<List<T>>`、`List<List<T>>` 构造 recipe、成员表达式
双变量遍历、Set/Dict 迁移、发布 DLL/SO、VFS、打包 AOT 与三组
整体总验收仍待完成。已验收的生产者和导入入口不代表全部 List
类型组合或显式转换语法已经完成。

CRT 未决策为零；XRT 19 组未决策与对象池最后的安排不变，见
[标准库决策清单](标准库决策清单.md)。
