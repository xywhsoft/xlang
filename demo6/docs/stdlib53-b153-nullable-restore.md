# Nullable 数据恢复错误合同

日期：2026-10-09。本批为已有类字段、Array/List 和标量共享恢复事务
增加私有 nullable 读取入口，明确区分数据不匹配与运行失败。没有
迁移 XLIR 构造域、语言载体或生成的持久化入口；整体 53 模块验收继续。

## 数据拒绝与运行失败

`xl6PersistenceRestorePlanReadTakeV1` 与原 strict 入口复用相同模板、
源 Value 与构造域索引 memo，以及 checked 清理过程。运行时只在已经
证明的数据形状、名义输入身份、值域、非 nullable 的 null 或缺少
required 字段处登记数据拒绝，不从错误类型猜测拒绝原因。

数据拒绝先清理全部部分结果和拥有输入；只有清理成功才返回 true
及 owned canonical null。工厂、原生 provider、codec、分配、递归预算
或清理失败均返回 false，不发布输出；没有可分配的错误对象时也保持
失败位。用户构造或清理产生的 TypeError 与数据类型不匹配因此不会
混淆。所有 actual writes 仍经过真实集合准入和 codec。

标量恢复共享新的显式拒绝状态。错误 tag、数值溢出、负数进入无符号
域及非法 Unicode scalar 属于数据拒绝；getter、clone、分配或未知规则
不会设置这个状态。原 strict 标量 ABI 保留，旧调用仍报告 checked
失败，不改变既有宽度归一化规则，也不 retag 原始输入。

错误计划、JSON 不支持的 bytes/time 能力及错误的原生目标仍是 checked
失败。调用者提供的私有默认根先核对实际类族、名义身份和 runtime tag，
即使 wire 同时不匹配也不能掩盖这个错误。output 必须为空、不能指向
消费的 seed 表；非 alias 的已有 output 被保留。NULL source 表示上游
失败，而不是普通解析拒绝。65 层预算仍是运行限制，不能被转换成 null。

活动路径的环保护沿用原事务。普通 XRT wire Object 在存储入口就拒绝
自身引用；本批没有伪造 backing 或绕过这个保护来声称循环 wire 验收。
构造和析构的普通语言副作用依旧不能回滚，返回 null 不意味着没有执行
默认初始化或清理。

## 批量原生验收

最终冻结源码位于
`E:/xlang-validation/stdlib53-b153-source-final-dev4/frozen/demo6` 及
`/var/tmp/xlang-stdlib53-b153-source-final-dev4/demo6`。两端严格
C11/Wall/Wextra/Werror、O0/O2，六份报告 Complete/InputsUnchanged，
全部执行行成功。Linux 本批选择的原生闭包启用 ASan/UBSan、泄漏和
栈生命周期检查，不等于整个运行时或真实产品已经全面插桩。

| 专项及计数范围 | 阶段 | 闭合前缀 | 前缀运行 | 实际命中 |
| --- | ---: | ---: | ---: | ---: |
| 共享恢复和 nullable 读取 每端 | 6170 | 82 | 6070 | 5988 |
| 九种实际 Array 导入链父门 每端 | 3609 | 56 | 3508 | 3452 |
| 未启用恢复适配的 List 父门 每端 | 1144 | 54 | 1090 | 1036 |
| 双端总计 | 21846 | 384 | 21336 | 20952 |

阶段计数包含平台、优化级别及操作重复，不是独立 API 或缺陷数。
四项新增读取操作在每 lane 的首次未命中点分别为成功对象 27、末尾
required 缺失 27、根形状不匹配 2、整数标量恢复 1；前缀均从 0 连续
覆盖到首次未命中点，全部既有父门完整前缀保留。

向量包含自有默认根、跨字段共享、missing/null 区分、名义身份、
全部原始标量规则的错误 tag、整数及 f32 溢出、Unicode 边界、原错误
保存、occupied/NULL 输出、错误目标与 wire 同时存在、真实 JSON/XLON
往返和 66 层 checked 拒绝。实际类 finalizer、委托真实准入/append 的
显式武装边界、入口/清理 GNU ld wrap 覆盖无诊断失败及 TYPE/MEMORY
诊断；没有把这些注入解释为真实 OS/FLS 耗尽。首个无诊断失败不被
随后 VALUE/TYPE 清理诊断覆盖，结果及内存账本平衡。

Windows `E:/xlang-validation/` 下以下目录各有 `results.json`：

- `stdlib53-b153-plan-win-final-dev4`
- `stdlib53-b153-array-chain-win-final-dev4`
- `stdlib53-b153-list-bare-win-final-dev4`

Linux `/var/tmp/` 下对应目录为 `xlang-stdlib53-b153-plan-linux-final-dev4`、
`xlang-stdlib53-b153-array-chain-linux-final-dev4` 和
`xlang-stdlib53-b153-list-bare-linux-final-dev4`。
最终暂存树与报告输入审计分别为
`E:/xlang-validation/stdlib53-b153-index-win-final.json` 和
`E:/xlang-validation/stdlib53-b153-index-linux-final.json`。

最初通过的 dev1 不包含全部补充向量。dev2 的循环夹具失败报告保留，
dev3 通过但早于私有默认根前置验证；均不能替代上述最终六项重跑。

## 宿主持锁恢复决定

继续采用 `xl_sync_retirement.h` 的 Snapshot/Acquire/Unlock/Retry 四项
checked 接口。持锁失去语言引用后，运行时保留真实原生引用及 token，
宿主取得 owned handle，手动解锁后重试；不自动解锁、丢弃资源或因这类
正常清理拒绝直接 abort。mutex 必须由原获取锁的线程解锁。记录修复后
仍须单独重试原来失败的 checked module finalizer。

这项 ABI 在此前批次已实现。本批复用 B144 的真实产品候选，重跑两份
DLL/SO 的 O0/O2、持锁 mutex/read/write、原错误和全局清理情形，共
52 阶段通过，报告 Complete/InputsUnchanged。Windows 证据为
`E:/xlang-validation/stdlib53-b153-sync-decision-win-dev1/sync.shipping.json`，
Linux 为 `/var/tmp/xlang-stdlib53-b153-sync-decision-linux-dev1/sync.shipping.json`。
这是既有宿主合同的复测，不是新增恢复入口或重建本批 nullable 实现的
shipping 产品；不计入上表原生阶段。

## 剩余语言迁移

XLIR 显式构造域、开放 DynamicFields 恢复、完整默认初始化上下文与
真实模块 owner，以及 List 的工厂/producer/索引/迭代/clone/var/
Future/类字段/格式入口仍须协调切换。旧生成式递归恢复及 nullable
finish 不能因私有原生入口通过而视为已经清理；实际 C/JIT/AOT、
SDK/VFS、按需组件和真实产品验收继续。

Dict/Set typed 迁移、SlotMap、WaitQueue/Channel、OrderedMap 公开合同
和 53 模块总门继续。CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
