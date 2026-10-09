# List 与 Array 混合类型链恢复验收

日期：2026-10-09。本批完成已批准容器任务的私有混合类型链递归导入及
JSON/XLON 标量恢复底座，直接扩展既有 Array 导入事务，没有另建 List
恢复 memo 或增加 Cgen 库函数体。类字段共享恢复、语言载体与 var/持久化
整体切换、真实产品和 53 模块总验收仍待完成。

## 共享恢复事务

`xl6CollectionImportChainTakeV1` 与 SourceTake 入口接收编译器提供的
有序类型链及真实空模板。默认使用 Array；可选驻留
`xl6ListImportProviderV1` 使用 actual typed List，不实现替代存储。
所有层复用 `xl6_array_import_chain.c` 的同一事务、memo 和回滚路径。
原有 Array-only 两个入口继续保留其根 seed 返回合同。

模板必须是已投影、空且可 pin 的实际原生对象。各相邻模板在元素进入
存储前，通过父模板的完整 resident 准入策略检查；相同物理槽或 TypeId
不放宽逻辑域。List context pin owner 并拥有完整元数据引用，结束时
先释放所有 pin，再消费模板；结果不保存生成模块回调或借用计划。

混合入口创建独立原生根。所有有效指针/count 的模板在两条路径都被
清空和消耗，包括格式计划错误、上游 NULL 工厂失败与部分转换失败。
SourceTake 另在两条路径消费一个源引用。源内容与身份不由操作修改。

memo 键为源 Value 和类型链深度，同一源在同一类型层共享转换结果，
在另一层则属于另一转换域。memo 保留源外壳和完成结果，避免回调后的
地址重用；类型链支持 1 至 65 个容器，使用原有 64 层嵌套元数据上限。
这不是跨多个类字段的统一恢复域；该部分仍需闭合对象/字段计划。

## 显式 wire 恢复

Array 与批准的紧凑 List 持久化格式都以 wire ARRAY 为输入。List 恢复
按 0 起始索引写入，不把一层稀疏 IntMap 导入当作这一持久化格式。
已有且完全匹配的原生子对象保留其真实身份及引用，不猜测其子类型。

普通导入只接受原生准入；RESTORE 标志才允许按声明的叶域归一化。
复用既有标量规则，拒绝字符串转数字、浮点截断为整数、整数绕回与
bool/数值互换；JSON 字符整数必须是合法 Unicode scalar。
f32 检查有限溢出后才转换；nullable absent 与普通值分别处理。
JSON 的 bytes/time 叶域即使输入为空也拒绝，不能借空容器绕过类型限制。

不匹配的非零 nominal 身份在归一化前拒绝，不能洗掉身份后重新绑定为
基础类型。不可变标量 Clone 可能返回同一外壳，因此字符串、bytes/time
需要另建目标外壳才能绑定目标身份，绝不改写借用源。bool/null 沿用
其 canonical 值；所有归一化结果仍经实际目标准入及 codec 检查。

## 首错与消耗

恢复事务及 Array 输入清理的可检查错误边界失败不再 abort。已拥有资源
仍会释放，首个诊断或独立 NULL failed 位在真实 finalizer 错误之后保留。
Begin/Empty/Append 返回有效对象或成功值却同时发布 NULL failure 的
provider 不能使部分结果逃逸。测试 delegate 只调用真实 List provider
后注入该边界失败，不替代容器或恢复算法。

GNU ld wrap 验证入口及 cleanup 的显式失败；除明确武装向量外均委托
真实函数。这不是实际 OS/FLS 耗尽或整套运行时插桩的验收。

## 批量验收

Windows/Linux 使用同一最终 Git index 冻结源码，严格 C11、
Wall/Wextra/Werror，O0/O2。Linux ASan/UBSan、泄漏及栈生命周期检查
覆盖选择的原生闭包。六份最终报告 Complete/InputsUnchanged 均为 true，
所有运行行成功；存在 Failures 字段的报告均为空。

| 专项与计数范围 | 阶段 | 闭合前缀 | 前缀运行 | 实际命中 |
| --- | ---: | ---: | ---: | ---: |
| List 混合恢复与标量专项 每端 | 3780 | 64 | 3708 | 3644 |
| 既有九类 Array 导入链父门 每端 | 3609 | 56 | 3508 | 3452 |
| 不启用可选恢复适配的 List 父门 每端 | 1144 | 54 | 1090 | 1036 |
| 双端总计 | 17066 | 348 | 16612 | 16264 |

阶段是平台、优化级别和操作的运行次数，不是独立 API 或缺陷数。每条
分配失败前缀从 0 连续运行到首次实际未命中，包含该未命中点。新增五个
操作每 lane 的首次未命中为 native 26、JSON 23、XLON 23、wide 1205、
source take 27；原有 List 和九类 Array 的完整前缀保留。

新增向量包括九种叶配置与 17 个格式/配置组合的真实 JSON/XLON 写出、
解析和恢复，UTF-8/NUL、bytes、char、nullable、i8、u64、f32、bool；
2 至 4 层混合、65 容器类型链、70 个不同子节点各重复两次的 memo 扩容；
真实原生子节点保留与同物理/不同逻辑域拒绝；错误计划、物理格式、
nominal 身份、入口、provider 和清理边界的首错保留。

XRT 的 wire API 本身拒绝直接自环，测试不伪造 backing 绕过此约束。
恢复器另有活动路径检查，但本批不把该 wire 构造拒绝表述成新恢复器
已经实际遍历了一个 wire 自环。

两端的 18 份 Array 父门符号记录均无 List 适配，bare List 的两份记录
均无新的混合恢复入口或 List provider。此证据只证明新增适配的原生
可选依赖，不代表整套 XRT 图模块、真实产品或 SDK/VFS 裁剪完成。

最终证据：

- Windows 源码 `E:/xlang-validation/stdlib53-b151-source-final-dev7/frozen/demo6`。
- Linux 源码 `/var/tmp/xlang-stdlib53-b151-final-dev7`。
- Windows `E:/xlang-validation/` 下的 `stdlib53-b151-restore-win-final-dev7`、
  `stdlib53-b151-array-chain-win-final-dev7`、
  `stdlib53-b151-list-bare-win-final-dev7`，各含 `results.json`。
- Linux `/var/tmp/` 下的 `xlang-stdlib53-b151-restore-linux-final-dev7`、
  `xlang-stdlib53-b151-array-chain-linux-final-dev7`、
  `xlang-stdlib53-b151-list-bare-linux-final-dev7`，各含 `results.json`。
- 最终暂存树审计 `E:/xlang-validation/stdlib53-b151-index-win-final.json`
  与 `E:/xlang-validation/stdlib53-b151-index-linux-final.json`。

早期证据保留但不计最终阳性：dev2 的夹具缺少 float 头和字符串宏的
表达式转换；dev3 的夹具误以为 wire API 允许直接自环。中间完整专项
通过后补入真实格式往返、cleanup 和 nominal 身份检查；所有最终门
统一在 final-dev7 重跑，不用早期阳性替代更新后的源码验收。

## 后续整体切换

当前 IR 仍将语言 List 工厂映射到 `XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW`，
新 provider 尚未登记为语言调用或宿主导出。下一步完成类/字段共享
恢复计划，并协调工厂、所有 producer、索引/迭代、clone、var、泛型、
Future/类字段和持久化的实际载体切换，不能只把普通调用换成新工厂。
随后完成按需组件、宿主、SDK/VFS、真实产品严格 C/JIT/AOT 和跨组门。

Dict/Set typed 迁移、SlotMap、WaitQueue/Channel、OrderedMap 合同和
53 模块总验收继续。CRT 无未决策；19 个 XRT 未决策组不变，对象池最后。
