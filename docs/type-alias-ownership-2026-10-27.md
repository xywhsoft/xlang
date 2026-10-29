# 类型别名的所有权规划

本批修复别名返回容器不能直接迭代的根因。语义检查和 XLIR 已将
`typedef Items = array<int>` 解析为正确的数组类型，但最终执行计划仍按
`Items` 这个名称判断是否受管，将调用结果标为 COPY。循环要求独立拥有的
临时容器，因此在 canonical iterator ABI 检查处拒绝；不能放宽此检查。

## 实现

所有权规划在类型注册表仍存活时，复用已有别名解析，统一取得 nullable
包装内的具体类型。该查询只借入类型，不改写 AST、不分配运行时数据，
并保持已有类型查询的有界行为。

同一查询同时用于参数拥有方式、调用／getter 结果、类型化索引的接收者
和元素，以及 `error.cause` 的独立引用。上下文显式传给这些规划入口和
执行计划遍历，避免只修循环、而把相同的别名误判留在其他入口。

生产改动仅在 sem：50 行增加、35 行删除。没有修改 IR／lowering／verifier／
Cgen／XRT 或 ABI；没有新增别名专用载体、运行时适配或转发函数。生成 C
继续使用具体类型已有的拥有和清理操作，不能据此泛称所有绑定均为零开销。

## 验证

Windows／Linux 各 587 项通过，包含：

- 原 source-driver 确认同一新夹具在循环 ABI 处拒绝，修复后确定性 C
  发射、JIT、module、独立 AOT 全部通过。
- 数组／稀疏 List／Dict／Set 的别名返回、别名链、函数别名、getter、
  nullable 别名、string／bytes 值语义、动态元素别名、类索引／安全索引、
  类析构以及错误引用；所有普通断言和析构数量均执行。
- GCC O0／O2 的实际生成 C 在预热后重复三个完整模块生命周期，存活数量／
  字节、分配／释放次数以及非法释放指标平衡。
- 两个优化配置分别运行分配前缀 0—284：各 284 次真实 OOM 与一个不再
  触发失败的正常终点，共 568 次真实 OOM、570 次前缀运行。每次检查
  checked 失败、首个诊断、重复清理和精确物理内存平衡。

独立 IR 合同在 lowering 前核对 6 个拥有式循环、22 个托管调用及 1 个
拥有式 cause；lowering 后 30 个损坏合同在 C／公共头／原生输入头输出前
拒绝，每平台 90 次空输出检查。没有靠把 COPY 临时值加入循环白名单解决。

两端另外各通过原有 callable 34、枚举 39、属性 60 项；Windows 字符串完整
364 项通过。固定 536 份旧语料仍为 507 份成功，507 份成功 C 逐字节一致。
新别名夹具不在这份旧语料内，不把它算作语料成功数增长；514 下限不变。

首次 r4 的合同 C 文件误用了 `XL6_IR_OWNERSHIP_SHARE` 枚举名，导致合同
编译检查失败，其余语言／内存验证通过。更正为实际的 `SHARED` 后，r5
两端完整重跑，均 587／587；保留 r4，没有修改产品枚举迁就测试。

## 证据与尚未完成项

Windows 根为 `E:/xlang-validation/five-groups-20261027/p7-shipping/`，
Linux 根为 `/home/ubuntu/xlang-validation/20261027-p7-shipping/`。

| 路径 | 内容 |
|---|---|
| alias-plan-cohort-r1 | 单语义单元重编译、同步的编译器源码包 |
| alias-plan-check-r4 | 首次合同拼写错误和其余通过项 |
| alias-plan-check-r5/results.json | 每端 587 项及完整分配前缀 |
| alias-plan-regressions-r6/results.json | 每端 34／39／60 项回归 |
| alias-plan-corpus-r5/results.json | 固定 536 份语料与 507 份相同 C |
| alias-plan-strings-r5/results.json | Windows 字符串完整 364 项 |
| alias-plan-proof-r7.json | 独立 reader 核对源码、对象、日志和产物 |

持久工具为 `tools/check_xlir_type_alias_ownership.py`。这是增量 source
profile，不是发布 DLL／VFS 的全量新构建。

另发现普通容器的 bytes 元素迭代准入仍被限制在 SlotMap 专用分支；
typed point 的完整 Sequence 准入也待处理。本批不把这些算成别名问题。
完整 74-worker 的原冻结候选仍在独立收集，已有真实失败和过时测试前置，
不能用本批独立通过结果回填。长期任务仍为 active，最终联合验收未完成；
仅普通离线语言／ABI／生命周期检查，无网络安全性质专项。
