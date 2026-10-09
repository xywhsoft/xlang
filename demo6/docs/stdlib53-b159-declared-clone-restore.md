# 共享默认对象的声明克隆恢复

日期：2026-10-10。私有原生恢复控制器已执行用户批准的规则：共享默认
对象只通过明确声明的 clone/copy 能力隔离，没有能力则 checked 拒绝。
clone 返回值重新验证类型和完整拥有图，所有恢复字段仍统一提交。
这尚不是 Cgen 实际 JSON/XLON 入口、活动模块宿主或完整标准库的验收。

## 声明能力和重新认证

恢复节点保存编译器选择的 clone 回调。回调借入已初始化接收者，返回
一份 owned 结果；不从字段结构推断可复制性，不把默认初始化器当 clone。
XRT 字段事务新增 V2 查询结果：SHARED 时给出首个共享接收者的记录索引，
其他结局不改变索引输出。V1 复用同一实现，不增加第二套提交算法。

接收者按父对象先于子对象的暂存顺序定位，先处理最外层共享对象。
若声明的深 clone 已隔离子对象，子对象不必另行调用 clone；若浅 clone
仍分享待修改子对象，只能使用该子对象自己的声明能力继续隔离。
同一候选只尝试一次语义 clone；返回 self 或重认证后仍共享的候选
checked 拒绝，不能无限 clone、成功 null 或修改原对象来绕过规则。

实际调用前关闭所有模板作用域、退休准备存储并释放旧序列结果。
用户 clone 不在 freeze、mutation 或模板 BUSY fence 中运行。返回值先
通过精确 TypeId、运行时类型标记和 resident Class 族检查，再随整图
进入实际引用槽、拥有策略、共享 backing 和组合容器环的统一认证。
未知策略在 Count/Trace 前拒绝，不能以调用 clone 来规避初始图的准入。

原子性只覆盖恢复器自己的字段发布。用户 clone 显式修改原对象或外部
变量等普通语言副作用不承诺回滚；诊断、无诊断 checked 失败、OOM 和
清理失败仍保持原来的第一错误及结果撤回合同。

## 初始化只执行一次

对象 memo 独立拥有首次初始化对象、源实例和当前候选，并记录准备轮次。
需要隔离时保留实际默认状态，重建字段准备存储；对象默认工厂不会重跑。
序列结果通过现有 native 空容器和准入能力重建，不触发语言初始化器。
每个 source/domain/实际字段实例仍是独立恢复域，原有共享关系继续由
同一 memo 决定，不能把不同父实例的默认对象合并。

已观察的 wire 对象和数组存储按实际 source 身份保留独立 COW 快照。
clone 修改原 wire 后，重准备不能悄然换成另一份恢复输入；未访问的输入
仍按首次观察处理。递归可能重排哈希表，控制器不保留跨递归的 memo 槽
地址。快照、默认对象和实例引用都按真实 multiplicity 进入提交槽，
借用表或观察锚点不会伪造额外引用。

## 本批验收

双端各执行全部 53 个 List/恢复向量，以及全部 21 条计划恢复 OOM 前缀，
均覆盖 O0/O2。共 304 个驱动阶段、212 次组向量执行、84 条关闭的前缀，
合计 12,472 次逐点尝试；新增 clone 前缀占 1,828 次。每次批量尝试
独立关闭真实分配账本，Linux 完整插装所选原生闭包的 ASan/UBSan 通过。
旧 List 的计划恢复以外 OOM 操作未在本批重复，不能扩大范围。

clone 组包含 34 种正常和拒绝场景：子对象/根隔离、浅与深声明能力、
缺少子能力、self、共享全局结果、普通伪对象、错误类型标记/身份、null、
无诊断失败、显式错误、共享析构 COW backing、未知原生句柄、wire 修改、
原对象显式副作用、晚发 wire 拒绝，以及含 List 的工厂和共享结果重准备。
晚发 wire 拒绝不调用 clone；含 List 的用例证明两个默认工厂各只执行一次。

恢复报告为 Windows
`E:/xlang-validation/stdlib53-b159-plan-win-full3/results.json` 和 Linux
`/var/tmp/xlang-stdlib53-b159-plan-linux-full3/results.json`。
代码来自 `stdlib53-b159-source-dev3` 的独立 Git index 快照，未使用旧
DLL/SO/TCC 或未提交工作区源码。

XRT 字段专项对两个嵌入产品和两种声明顺序分别执行 O0/O2，Linux 另有
四次 ASan/UBSan 执行。双端共 44 个驱动阶段、20 次实际嵌入执行、
1,600 次完整分配失败尝试，另各 25 项模块化原生回归通过。V2 索引场景
覆盖第二接收者共享、整批不写及 BUSY/ERROR/COMMITTED 的输出保护。
向量正文与独立冻结的实际 XRT 源相同，未选头文件段与前批一致。

底层报告为 `stdlib53-b159-fields-win-full3` 和
`/var/tmp/xlang-stdlib53-b159-fields-linux-full3` 下的 `results.json`。
旧 Array import 九种布局的独立闭包另双端各通过 3,609 个阶段和
56 条关闭的失败前缀；Linux ASan/UBSan 通过。报告分别在
`stdlib53-b159-chain-win-full3` 和 `/var/tmp/xlang-stdlib53-b159-chain-linux-full3`。
所有报告均 Complete、InputsUnchanged，无失败；日志、编译器、所选源码
和底层实际产物保留来源与哈希。

只同步 `include/xrt/value.h` 和 `src/value/value_container.c` 差量。
嵌入声明和原生符号表现为 XRT 3,189、xruntime 3,648、自有 459 个 API；
能力矩阵与隔离源码计算一致。没有新增公开语言标准库函数。

## 继续完成语言入口

当前 resident 准入不会把未知或活动宿主 owner 当叶子；实际模块的
可检查活动边界、其他已批准原生资源策略和 Cgen 的精确 clone thunk
仍须接入，之后才能切换实际恢复入口。大规模共享默认图的 clone 冷路径
还须评估重复重准备的成本，不能把本批正确性向量视为性能总验收。

开放对象、泛型 class、List/Set/Dict 语言载体、SlotMap、
WaitQueue/Channel、OrderedMap 和产品总门继续。19 个 XRT 未决策组不变，
CRT 无未决策，对象池最后。
