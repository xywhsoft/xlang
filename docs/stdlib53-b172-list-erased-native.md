# List 原生动态方法与快照迭代

List 原生底座增加 checked 动态方法和单步拥有投影，供语言载体切换使用。
它复用现有 XRT typed List 存储、准入策略与游标，不增加第二套容器算法。
目前语言 `List<T>` 仍使用旧 IntMap；本批不解除其恢复 pending，也不声称
通用 `var` 分发、工厂、索引、生产者和语言迭代已完成切换。

## 接口与所有权

| 私有驻留接口 | 合同 |
|---|---|
| `xl6ListErasedCountV1` | 零元素正常；成功才发布完整 int64 数量 |
| `xl6ListErasedKeyV1` | 完整 int64 键查询或删除；missing 与失败分离 |
| `xl6ListErasedClearV1` | 清空真实 typed 存储，复用既有销毁规则 |
| `xl6ListIteratorCreateErasedV1` | 0 表示无关载体，-1 表示失败，1 发布独立 typed 快照及完整布局 |
| `xl6ListIteratorAdvanceValueV1` | 每步发布完整键与一个拥有的 Value；正常结束与失败分离 |

接口先认证私有 handle ops，再读取 owner。相同 TypeId、外部 handle 或旧
IntMap 都不能冒充原生 List；未绑定投影、Cursor、BUSY 和退役 owner 拒绝
作为普通 List。布局、准入和 resident codec 随快照独立保留，不能使用
生成模块的回调或上下文替换。私有输出槽不是语言 byref 参数。

创建快照的成本是一次 O(n) typed 存储复制，不先把整个容器转换成 boxed
wire。每步沿同一 XRT 游标推进并投影当前元素，保持完整有符号稀疏键。
源容器修改或销毁不改变快照。EOF 成功发布 `found=false`、NULL item 和
零 key；投影失败保留输出并使已开始推进的快照永久失效，不伪装成 EOF。
入口无法安装错误作用域时仍返回显式失败，调用者必须检查状态，不能只
检查是否成功分配了诊断对象。

重复引用按真实物理槽逐个 trace；Future 仍要求精确 resident admission
ops 的独立授权。当前 Value 投影由调用者拥有，不是快照中额外缓存的
拥有边。typed 与 erased 路径共用克隆、键操作和推进 helper。

## 按需组件

五个符号归属现有 `LIST_NATIVE` 组件，无新增组件或依赖边。Array、Open
恢复等不使用 List 的闭包不应因此引入 List。原生投影不是公开标准库 API
决策，也不是将两种 List 表示同时开放给语言。

## 验证范围

专项门覆盖完整 int64 键、反向插入的大快照、UTF-8 与内嵌 NUL、bytes、
完整 uint64、char、nullable string 和 Future，以及伪造载体、错误布局、
输出保留、无诊断错误和清除重入诊断后的失败状态。四个故意损坏的真实
实现必须先编译成功，再被相同运行合同拒绝：截断键、非永久失败、源别名
快照和失败时发布输出。

Windows 与 Linux 冻结源码专项门各通过 4,110 阶段：66 组向量、65 项操作在 O0/O2
各执行完整分配失败前缀，合计 130 条闭合前缀、10,182 次逐点账本检查。
六个新增操作使用每点独立账本的批处理；每个优化档的快照创建覆盖 11 次
尝试、单步投影覆盖 2 次，均执行到首个无故障点。count/key/remove/clear
无需分配，首个无故障点为零。图复制与既有混合 Array/List、Closed 类
恢复回归同时执行。

Linux 所选 XRT/xruntime、List 及图复制与恢复 provider 闭包同时通过
ASan/UBSan；这不是完整编译器或运行时的 sanitizer 验收。

冻结输入位于 `E:/xlang-validation/stdlib53-b172-source-final1/frozen/demo6`，
Linux 镜像位于 `/var/tmp/xlang-stdlib53-b172-source-final1/demo6`，5,654
个文件哈希一致。Windows/Linux 分别无缓存重建 257/256 个翻译单元。
双端组件目录均验证 218 组件、1,184 精确导出和 13 个负控；组件数不变，
增加的五个导出仅归属 `LIST_NATIVE`。双端恢复回归各通过 92 阶段，包含
8 个语言用例、22 项 IR 损坏拒绝、严格生成 C O0/O2 和实际 TCC JIT。
本批恢复回归不重跑语言完整 OOM 前缀；该前缀的上一批证据仍见
[Open 语言恢复](stdlib53-b171-open-restore-language.md)。

Windows 报告使用 `E:/xlang-validation/stdlib53-b172-` 前缀，Linux 使用
`/var/tmp/xlang-stdlib53-b172-` 前缀，后缀为
`native-*-final1/results.json`、`catalog-*-final1.json` 和
`restore-*-final1/results.json`。最终 index 与这些报告按输入哈希核验，
记录为 `index-*-final1.json`。本批不提升发布 DLL/SO 或 VFS，不代表语言
List 切换、AOT 或全仓 RC0。

## 下一步

联动切换 List 的类型准入与布局、工厂、全部生产者、索引读写、语言
迭代和 var 操作，再接通持久化恢复；不能只移除 pending 或只替换工厂。
Set/Dict 及其他已批准容器和发布验收继续。CRT 无未决策，19 个 XRT
未决策组不变，对象池仍最后，见[标准库决策清单](标准库决策清单.md)。

文档总索引仍存在上一批记录的 14 项基线失败，本批不把无关扩展库和
IDE 的工作区修改纳入提交，不将基线失败宣称为通过。
