# List 跨容器嵌套类型验收

日期：2026-10-09。属于已批准的容器接入任务。本批完成私有原生 List
嵌套类型策略及 Array/Stack/List 混合元数据链；没有新增语言 API。
递归图转换、完整语言载体与 var/持久化入口、真实产品验收仍待完成。

## 完整类型策略

`xl6ListSchemaCopyV1` 只从真实、存活的 List 捕获完整布局和拥有的准入
元数据，不读取元素、复制存储或保留源 Value。源不必为空，operation
BUSY 也合法。未知 handle 的原始指针在驻留 ops 匹配前不会被访问；
Cursor、已清空退役的 owner 和错误身份均被拒绝。失败不修改两个输出，
准入输出必须初始为空。成功捕获和类型探针不分配内存，保留先前诊断。

可选组件 `xl6_list_schema.c` 的 `xl6SequenceListCreateTakeV1` 消耗真实
子 List，复用现有 resident reference schema envelope 创建 Array/Stack
引用域。List 父对象继续通过既有空 Array 元数据域工厂获得该策略，
实际存储仍是 typed List，不把 Array 保存到 List 内部。

策略拥有子 List 的身份、完整值域布局、元素身份、transfer、物理描述符、
boxed identity 和完整逻辑准入策略。独立构造的等价元数据按结构比较；
相同 TypeId 或相同物理槽不能放宽逻辑域。标量宽度、字符串 boxed
identity、bytes/ANY、Future 基础身份或 nullable、值域布局及更深子策略
不同，均不能进入目标。元素内容、稀疏键和投影 codec 绑定不是类型策略。

混合链共用已有 64 层上限；只比较有限的拥有元数据，不遍历子对象内容。
非 nullable 元素不凭空构造默认子对象，nullable 默认增长为 absent。
Clone、原生引用读写和快照保留完整策略，源包装全部消失后仍可判断新
构造的子对象。元数据没有 Value/storage 边、生成模块代码或借用身份表。

## 消耗与错误

新嵌套工厂及既有 List Create 的入口边界失败时，仍先锁定首错再消耗
输入。GNU ld 显式边界注入同时验证 MemoryError 与没有诊断对象的失败，
后续真实 finalizer 的 ValueError 不覆盖首错；此注入不是实际 OS/FLS 耗尽。

消耗已填充子 List 时，实际存储析构失败会撤回未发布的父结果，并保留
原始失败。上游工厂的 NULL failed 位也不会变成新的 ArgumentError。
已退役 owner 不可重新作为 schema 见证，失败输出和捕获前的元数据仍有效。

## 批量验收

同一最终 Git index 冻结输入，无旧 DLL、外部存储 provider 或替代容器。
两端使用严格 C11、Wall/Wextra/Werror，分别执行 O0/O2。Linux ASan、
UBSan、泄漏及栈生命周期检查覆盖本次选择的原生闭包，不代表整套产品
或运行时均已插桩。

| 专项与计数范围 | 阶段 | 闭合前缀 | 前缀运行 | 实际命中 |
| --- | ---: | ---: | ---: | ---: |
| List 原生及新增嵌套策略 每端 | 1144 | 54 | 1090 | 1036 |
| 既有 Array/Stack 嵌套父门 每端 | 1880 | 370 | 1874 | 1504 |
| 双端总计 | 6048 | 848 | 5928 | 5080 |

阶段是不同平台/优化级别的运行次数，不是独立 API 或缺陷数。每条分配
失败前缀从 0 连续执行到首次实际未命中，包含该未命中点。新增三个
操作的每 lane 首次未命中分别为 schema-create 5、schema-list-create 8、
schema-deep-create 390。所有最终报告 Complete/InputsUnchanged 为 true，
全部运行行成功，分配/释放、live bytes、invalid/double free 与 UAF 计数平衡。

新向量包含三种父容器 × 四种真实叶配置 × 两种 nullable，共 24 个
组合，以及混合 64/65 层、独立等价/最深逻辑域差异、同策略不同值域布局、
Cursor/foreign/wire 拒绝、populated exemplar 消耗、clone 后生命周期、
operation BUSY 捕获、退役拒绝和首错析构。父门保留原有 16 种 Sequence
嵌套配置和全部 370 条每端前缀；其 O0/O2 符号记录不含 List/schema
入口，证明未给该 Array/Stack 专项强行链接可选 List 实现，不是产品裁剪门。

最终证据：

- Windows 冻结输入 `E:/xlang-validation/stdlib53-b149-source-final-dev4/frozen/demo6`。
- Linux 同源目录 `/var/tmp/xlang-stdlib53-b149-final-dev4`。
- Windows 报告 `E:/xlang-validation/stdlib53-b149-list-win-final-dev4/results.json`
  与 `E:/xlang-validation/stdlib53-b149-sequence-win-final-dev4/results.json`。
- Linux 报告 `/var/tmp/xlang-stdlib53-b149-list-linux-final-dev4/results.json`
  与 `/var/tmp/xlang-stdlib53-b149-sequence-linux-final-dev4/results.json`。
- 最终 index 审计 `E:/xlang-validation/stdlib53-b149-index-win-final-v2.json`
  与 `E:/xlang-validation/stdlib53-b149-index-linux-final-v2.json`。

早期验证保留但不计阳性：dev3 专项缺少真实 typed Array value/Stack
组件导致链接失败；final-dev2 的测试给 Stack 绑定了只适用于 Array 的
投影，两端对应 profile 向量失败。修正后最终 final-dev4 统一重跑通过。
没有复用失败轮次的阳性来替代最终输入验证。

## 后续整体迁移

本批只补齐驻留嵌套策略。下一步是让 List 与 Array 共用单个 XRT 图
memo，并按编译器关闭的格式计划完成递归数据转换与恢复。既有语言
List/Set 的数组格式规则不由本批一层 IntMap 边界替换。

语言工厂、所有 producer、索引/迭代器、通用 clone、var 分派、泛型、
Future/类字段和持久化仍需作为一组协调切换。随后登记按需组件、宿主
导出与 VFS/SDK，执行真实产品严格 C/JIT/AOT、Windows/Linux 和跨组总门。
Dict/Set typed 迁移、SlotMap、WaitQueue/Channel、OrderedMap 合同与 53
模块总验收仍在长期任务中，不能用本次原生专项替代。

CRT 无未决策；19 个 XRT 未决策组不变，对象池仍最后开发。
