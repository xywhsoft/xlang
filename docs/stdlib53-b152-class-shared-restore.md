# 类字段与原生容器共享恢复验收

日期：2026-10-09。本批将私有恢复底座扩展到显式类字段和集合节点。
所有节点复用既有 Array 导入事务的 memo、模板及标量恢复，不为每个
类字段启动另一个导入事务。尚未登记 XLIR 构造域、迁移生成的恢复
入口或切换语言 List 载体；53 模块整体发布验收仍在进行。

## 显式构造域

`XL6_RestorePlanV1` 提供标量、集合和对象三种节点，字段记录包含精确
字节键、目标节点、required 和私有默认对象的 in-place 能力。
`xl6PersistenceRestorePlanTakeV1` 的 memo 键是源 Value 与节点索引。
同一个源进入同一个构造域时共享转换结果，即使它来自不同字段或
集合元素；相同名义类型进入不同构造域时则独立恢复。

节点不能仅按 TypeId 合并。不同字段初始化上下文可能拥有不同默认
值，必须由已验证 IR 明确区分；运行时不从 AST、类型名称或标记字段
猜测这个关系。计划和构造函数只在本次调用中借用，不保存在结果里。

对象节点调用真实声明默认构造函数，或消费调用者提供的私有默认根。
in-place 使用已经初始化的私有子对象，不重复运行初始化函数。
缺少非 required 字段保留默认状态，显式 null 按目标 nullable 域判断。
这是构造能力，不是允许任意修改外部共享对象；构造函数和析构函数
的普通语言副作用不属于可回滚事务。

## 真实类与集合准入

可选 `xl6RestoreClassProviderV1` 先通过既有驻留 Class 生命周期族检查，
再核对名义身份和 runtime tag。给普通 Object 填入相同 TypeId 和标记
不能伪造目标类；适配器不另建类存储、finalizer duty 或 code lease。
生命周期族查询也不代表完整拥有图已经获得回收或模块退役授权。

原生类不是 wire Object。是否原生由真实生命周期决定，不凭同名键
猜测；普通数据可以把 `$xlir.class.runtime-type` 当作显式重命名源键。
类的内部目标标记不可由字段计划覆盖。字段源键和目标键均为 byte view，
包含 NUL 时仍按精确长度操作。

集合节点消费真实空模板。可选 List 和默认 Array 使用既有完整驻留
策略与实际 codec，所有实际写入重新准入；集合之间的模板边也检查
完整 schema。类、Array、List 和标量在同一事务内恢复，不先生成一个
递归 wire 副本再重新导入。标量复用已验证的严格规则及身份保护。

构造域和字段数量不受 65 限制；65 限制的是实际对象/集合递归路径。
memo 保留源与结果，扩容后仍以完整键判断身份；活动路径拒绝遍历环。
valid pointer/count 的模板和一个 owned 默认根在成功、错误计划、
上游工厂失败及部分转换失败时都消费，失败结果不会向调用者发布。

## 验证范围

专项使用真实 typed List/Array、JSON/XLON 读写器及 resident Class
生命周期，夹具构造函数为外部驻留代码，不替代存储或 finalizer。
这不是实际源语言声明默认函数、JIT 镜像卸载或宿主模块退役验收。

向量覆盖跨字段别名、相同类型的不同构造域、JSON/XLON 格式往返、
私有嵌套默认对象只初始化一次、List 内类对象别名、类/List/Array
混合路径、65 层成功和 66 层拒绝、70 个独立构造域及 memo 扩容，
以及 missing/null、NUL 键、错误计划、名义身份和伪造普通 Object。

GNU ld wrap 明确注入入口及清理边界失败；provider delegate 先调用
实际准入后注入失败。真实类 finalizer 的 false/NULL failure 或诊断
不能覆盖之前的首错。除明确武装的边界外均委托实际函数，不把这
解释为真实 OS/FLS 耗尽。Linux 插桩范围为被编译的选择原生闭包。

Windows/Linux 同一最终冻结输入、严格 C11/Wall/Wextra/Werror、O0/O2，
六份报告均 Complete/InputsUnchanged，运行行全部成功。

| 专项及计数范围 | 阶段 | 闭合前缀 | 前缀运行 | 实际命中 |
| --- | ---: | ---: | ---: | ---: |
| 类字段与集合共享恢复 每端 | 6038 | 74 | 5948 | 5874 |
| 九种实际 Array 导入链父门 每端 | 3609 | 56 | 3508 | 3452 |
| 未启用恢复适配的 List 父门 每端 | 1144 | 54 | 1090 | 1036 |
| 双端总计 | 21582 | 368 | 21092 | 20724 |

阶段是平台/优化级别/操作的执行次数，不是独立 API 或缺陷数。五个
新增操作每 lane 的首次分配未命中点依次为普通恢复 27、已拥有根 18、
私有嵌套根 39、70 构造域 999、类/List/Array 混合恢复 32；所有前缀
从 0 连续覆盖到该未命中点，没有跳过中间点。既有父门完整前缀保留。
实际内存检测及 Linux ASan/UBSan、泄漏和栈生命周期检查均通过。

两端各 18 份 Array 父门符号记录均无新的 Class/List provider 或
ClassRuntimeV2Get；两份 bare List 记录还没有新恢复入口。
这只证明选择的原生适配依赖没有被无关父门带入，不代表真实产品、
完整 XRT 图模块或 SDK/VFS 裁剪已经完成。

最终源码为 `E:/xlang-validation/stdlib53-b152-source-final-dev8/frozen/demo6`
及 `/var/tmp/xlang-stdlib53-b152-final-dev8`。Windows
`E:/xlang-validation/` 下 `stdlib53-b152-plan-win-final-dev8`、
`stdlib53-b152-array-chain-win-final-dev8`、`stdlib53-b152-list-bare-win-final-dev8`
各含 `results.json`；Linux `/var/tmp/` 下对应
`xlang-stdlib53-b152-plan-linux-final-dev8`、
`xlang-stdlib53-b152-array-chain-linux-final-dev8`、
`xlang-stdlib53-b152-list-bare-linux-final-dev8` 各含 `results.json`。
最终暂存树审计为 `E:/xlang-validation/stdlib53-b152-index-win-final.json`
和 `E:/xlang-validation/stdlib53-b152-index-linux-final.json`。
早期阳性保留，但不替代这一最终输入的六项完整重跑。

## 后续语言切换

下一步从 XLIR 的闭合字段/默认初始化合同生成构造域、数据计划和
固定驻留调用，清理逐类型递归恢复函数。默认函数的 checked outcome
包括没有诊断的失败，不能被简单的 value/NULL 转接吞掉。

开放 DynamicFields 恢复、完整 nullable/默认上下文与真实模块 owner
仍需接入和验证。当前私有入口对所有拒绝发布 checked failure；接入
nullable reader 必须明确区分纯 wire 拒绝与语言构造/清理失败，不能
仅凭错误类型洗掉失败。List 工厂当前仍映射到 `XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW`；
其工厂、producer、索引/迭代、clone、var、泛型、Future/类字段及格式
必须协调切换，再验证按需组件、宿主、SDK/VFS、严格 C/JIT/AOT。

Dict/Set typed 迁移、SlotMap、WaitQueue/Channel、OrderedMap 合同和
53 模块总门继续。CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
