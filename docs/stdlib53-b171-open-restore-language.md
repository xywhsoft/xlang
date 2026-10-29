# Open 动态字段语言恢复与 clone 隔离

JSON/XLON 的受支持 Open 类、标量、nullable 与原生 Array 恢复图已切换
驻留事务。共享的声明默认子对象只按类声明的 clone 能力隔离；缺少能力、
返回自身或结果被外部共享均为 checked 失败，不能修改原对象。没有出现
在线格式中的字段继续保留原默认值。恢复器不以重新执行初始化器代替
隔离；用户 clone 内的显式构造仍按普通语言规则执行。

## XLIR 与生成代码

Lowered XLIR 保存 Open 存储类身份、声明 clone 的函数/符号/错误 ABI，
以及全部静态字段的运行时键和格式别名。忽略字段和禁用格式的基类字段
仍占用静态键，不能被当作动态额外字段。键采用精确字节跨度，保留 NUL
和 UTF-8。验证器从类型、类布局和调用合同独立重建，拒绝被删改的表项。

DynamicFields 现有公开 `clone()` 保留，并显式声明语言 `__clone__` 协议，
调用已有复制构造能力。恢复描述引用的 clone 是 IR 可达性的一条隐式调用
边；其函数体和依赖按正常可达图保留，不强制链接整个标准库。

Cgen 仅发出描述数据、计长键和声明调用的物理 ABI 适配器，实际恢复使用
RESTORE_OPEN provider 和现有字段事务。无 pending 的受支持根不再生成
旧的 JSON/XLON 逐字段赋值函数。Set/List/Dict 等仍有实际 pending 的混合
根保持原迁移边界，本批没有宣称这些载体已经切换。

## 已验证行为

- JSON parse/read、XLON parse/read 和往返保持 NUL/UTF-8 键和值、静态
  别名、忽略字段、继承字段、动态默认值与原生 Array 中的 Open 元素。
- 父 clone 可以仍引用旧 DynamicFields；恢复只在新隔离存储上整批发布，
  不修改全局共享默认对象。无对应线格式字段时保留原共享值且不调用 clone。
- 无 clone、返回自身、原异常和外泄 clone 分别在真实语言调用中拒绝。
  失败后的原对象、外泄对象和原字段表保持不变；原异常消息不被替换。
- 22 项真实 IR 损坏在 C、头和裁剪组件计划输出前拒绝，覆盖消费所有权、
  Set 屏障、Open 存储布局、clone 函数/符号/效果、计长静态键、调用可达性
  和原生 handle 策略。合法 Open 计划包含 RESTORE_OPEN，不带入 List 或
  Future 准入组件。

## 最终验收

冻结输入位于 `E:/xlang-validation/stdlib53-b171-source-final1/frozen/demo6`；
Linux 镜像位于 `/var/tmp/xlang-stdlib53-b171-source-final1/demo6`，5,652
个文件的哈希一致。Windows/Linux 分别无缓存重建 257/256 个源码翻译单元，
使用真实 XRT/xruntime 实现和现有 TCC 核心。

| 门 | 每个平台的结果 | 证据后缀 |
|---|---|---|
| 语言恢复 | 96 阶段，8 个语言用例，严格 C O0/O2 账本与实际 TCC JIT | restore-*-final1/results.json |
| DynamicFields 回归 | 82 阶段，含三个实际实现变异拒绝 | resident-*-final1/results.json |
| Open 原生事务 | 12 阶段，含三个实际 provider 变异拒绝 | open-native-*-final1/results.json |
| 驻留组件目录 | 218 组件、1,179 精确导出、13 个拒绝负控 | catalog-*-final1 |

以上证据使用 `stdlib53-b171-` 前缀；Windows 在 `E:/xlang-validation`，
Linux 使用 `/var/tmp/xlang-` 前缀。早期开发报告不作为最终通过证书。

语言 OOM 改为一进程内逐点初始化、终结和账本核验的批处理，每个点独立
打开故障作用域，覆盖到首个无故障尝试，不跳号。每端 O0/O2 的 Closed
用例各 257 个前缀，Open 用例各 537 个；原生 Open 各 71 个。全部尝试
内存平衡，重复终结安全。该范围是这两个语言夹具的完整前缀，不是所有
语言用例或整个编译器的 OOM 验收。

Linux 原生 Open 门的所选 XRT/xruntime 与 provider 闭包通过 ASan/UBSan；
语言门不是完整运行时 sanitizer 验收。最终 index 与报告输入经双端哈希
核验。本批不提升发布 DLL/SO、VFS 或 AOT 产物，不代表全仓 RC0。

## 剩余工作

Set/List/Dict 等恢复载体、未知资源策略、性能和发布总门继续。CRT 无
未决策，19 个 XRT 未决策组不变，对象池仍最后，见
[标准库决策清单](标准库决策清单.md)。本批不新增公开标准库 API 决策。

文档总索引仍有 14 项基线失败：六份旧扩展库文档各缺两处索引，另有
两个 README 未列入已提交守门清单。同一守门器在本批前 HEAD 和暂存
文档快照返回相同失败，新文档无新增失败；对照记录为
`E:/xlang-validation/stdlib53-b171-doc-compare-final1.json`。这些问题未被
掩盖或计为通过，也没有把无关扩展库/IDE 的工作区修改纳入本批提交。
