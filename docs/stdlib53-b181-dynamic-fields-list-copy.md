# 动态字段复制与恢复隔离的 List 接入

DynamicFields 的内部复制及类默认子对象的恢复隔离，现在可以处理
真实原生 List。所有相关入口共享一次 XRT 图克隆和一个 memo，
不再逐字段复制同一子对象，也不修改来自全局变量的原对象。
本批不新增公开语言 API，不代表 List 源载体或三组长期任务完成。

## 复制与发布合同

`toValue`、`copy`、`values`、`items`、`fromValue`、`clone`、`merge`
七个消费端使用 List/Array/wire 混合图提供者。重复字段边仍指向
同一个新子对象，新图与输入图隔离；内嵌 NUL 的键和文本、完整
int64 的 List 键不经过 C 字符串截断或窄化。

上游 XRT 增加 `xrtDynamicFieldsMergeRefV1`：借用字段值并保留实际
边的身份，先建立临时字段表，成功后一次发布。xlang 先对源图
克隆一次，再调用该事务入口，避免字段合并再次独立克隆各条边。
现有 XRT `Merge` 的源字段深复制语义保留，目标快照也保留原边身份。

发布前失败返回 false，目标不变。发布后释放被替换值时，析构可能
产生 checked 错误，此时 XRT 返回 true 表示已提交，并保留错误；
不能把不可逆的析构副作用称为回滚。语言包装捕获这类诊断及无诊断
失败，保证清理不会吞掉主错误，也不会覆盖先前环境中的错误状态。

## XLIR 与恢复隔离

提供者选择统一归于 `xl6IrValueCopyProviderAbiForType`。它检查
验证后的物理存储字段，包括跳过持久化的字段、派生布局与实际
泛型闭包，不根据类名、AST 或序列化字段清单猜测对象图。
遍历有深度和工作量上限；不透明、递归或预算耗尽时采用保守闭包。

Cgen 的调用输出与原生依赖计划消费同一选择函数。封闭且可证明
不含 List 的类，复制与恢复仍排除 List graph/native 组件。
Open 或隐藏开放字段可以携带任意 List，因而必须包含 List 所有权
提供者及它的实际依赖。相邻恢复合同保留封闭类型的负向裁剪断言，
只更新 Open 场景的组件断言。

JSON/XLON 恢复遇到外部共享默认子对象时，只按明确声明的 clone
能力隔离后恢复；没有该能力就 checked 拒绝。测试把真实原生 List
和重复 wire/native 边注入共享默认对象，调用实际生成的恢复函数，
验证新对象隔离、别名保留，以及全局原对象的字段与身份完全不变。
这不是新增隐式 clone，也不是 List 的完整 wire codec 接入。

## 双端验收

生产编译器来自隔离 Git index 的完整源码：Windows 270 个翻译
单元、Linux 269 个翻译单元。只复用厂商 TCC 引擎，不用旧生产
DLL/SO 替换被测实现。

每端新专项通过 104 个阶段、46 条完整故障前缀、820 次逐点尝试。
每次尝试单独关闭分配账本，覆盖七个入口、OOM、提供者拒绝、
无诊断失败、发布后清理错误，以及实际生成恢复函数的默认对象
隔离。两项真实实现负对照先编译成功，再被合同拒绝：遗漏 List
提供者、逐字段建立第二次克隆。类型合同另覆盖重复边 DAG 的
有界扫描、泛型闭包、纯叶类裁剪和开放隐藏字段。

正式专项报告：

- Windows：`E:/xlang-validation/stdlib53-b181-list-win-final1/results.json`
- Linux：`/var/tmp/xlang-stdlib53-b181-list-linux-final1/results.json`

相邻回归全部通过，前两项报告位于双端 `b181-regressions-*-final1`，
后三项位于 `b181-regressions-*-final2`。

| 回归门 | 每端阶段 |
| --- | ---: |
| 开放复制与 List 图克隆 | 167 |
| DynamicFields 驻留接口 | 82 |
| Closed/Open 恢复与完整语言 OOM 前缀 | 96 |
| List 方法与 continuation | 84 |
| List 复制与代数 | 126 |

目录验收通过 231 个组件、1,221 个精确驻留导出及 13 个负对照。
新增 XRT 低层 API 不增加语言驻留导出数。Linux ASan/UBSan 覆盖
选定原生提供者，不认证完整编译器或生成的整个恢复模块。
文档门仍有相同 14 项历史拒绝，本批新增为零，不是全仓文档门通过。

上游 XRT 的三个模块化测试与两个单头测试双端通过，release
maturity 与单头一致性检查通过。消费方只同步 `runtime_field.h`
和 `runtime_dynamic_field.c` 两个模块段：同步工具要求每条非空
基线源码行精确匹配，拒绝推测重基。Core 时间 ABI 与其他模块
实现保持不变；这不是向供应方当前 Core 时间合同的整体迁移。
来源与前后哈希保存在 `lib/xruntime/dynamic_fields.snapshot.json`。

## 仍未完成的边界

List 源工厂、cast/准入及 List 持久化 wire codec 尚待联动迁移。
封闭 `Array<List<T>>` 构造 recipe 未开放；本批原生输入由明确的
测试调用方工厂提供，不能冒充普通源 List 工厂已切换。
Set/Dict、发布 DLL/SO、VFS、打包 AOT 和三组总验收继续待完成。

CRT 未决策为零，XRT 19 个未决策组及对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
