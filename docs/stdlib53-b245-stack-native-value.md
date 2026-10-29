# Stack 中的原生拥有值

Stack 已接通封闭登记的原生拥有值槽，当前语言类型为 SlotMapKey。
构造、单项存取、查询、clone／equals、批量存取和退出清理已完成
双平台专项。复用原生 Sequence 配方与 XRT typed Stack，不增加
运行时组件、库算法或源名称识别。nullable 原生值及相应 try 接口、
Future 和函数值域仍未完成，本批不代表 Stack 全部接入完成。

## 统一构造与输送合同

原实现有三处不一致：完整原生 Sequence 配方已存在，Stack 的构造
准入却遗漏 native-value；指令输送上限只允许 SlotMap 使用原生值；
Stack 生成调用又借用只识别三个旧编号的 Channel 映射。

现在构造仍先核对封闭配方，再接受 native-value 域；指令验证对
Stack 开放原生复制输送，但保留精确元素身份、槽、效果和结果检查。
Stack 调用改用既有 Sequence 输送映射，不把原生记录转成指针或
xvalue。Cgen 只换一处机械编号选择，没有新增算法或失败路径。

语言值、xrttype 描述和实际槽保持同一个原生记录 ABI。单项结果
取得独立拥有权，批量结果复用原生 Array；类型登记提供复制、释放、
比较、装箱及投影。没有通过 POD、相同 sizeof 或借入指针伪装拥有值。

## 实际运行与失败清理

用例覆盖类字段默认 Stack、预留、push／peek／peekAt／pop、clone、
equals、清空和 trim，以及 pushBatch／peekBatch／peekBatchAt／popBatch。
源 Array 清空、原 SlotMap 清空、局部容器释放和返回后的 finally
都不破坏已取得的键值。空与负索引分别保持 checked RangeError。

Windows 317、Linux 316 个完整源码编译单元通过。专项分别通过
243、274 个阶段，每端 O0／O2 共 160 次完整调用线程分配位置检查。
每次失败后完整清理并成功重试；分配、释放、存活数量和字节平衡。
每个优化等级各执行 100 次普通模块生命周期，不外推为工作线程 OOM。

一个原生 Stack 域、27 个实际调用及 86 项 IR 篡改通过核对。错误
域、槽、元素身份和输送方式被 IR、C／header 及原生计划拒绝。
三个实际坏实现经编译链接后均被拒绝：遗漏构造域、恢复旧 ABI 上限，
以及恢复旧 Channel 编号选择；第三项检查实际成功 C／header 输出，
不能只用 IR 验证代替生成器证明。

独立 C、公共 JIT、module 与实际 AOT 通过。原生闭包选择 27 个
供应方，不链接编译器或 MODULE_ALL；Linux ASan／UBSan 覆盖全部
选定供应方。此前原生 Array／Dict 子容器回归每端通过 500 阶段和
418 次分配检查。统一声明检测每端 289 个夹具／577 阶段通过，其中
288 个正例、一个明确负例；该检测仍不是全函数运行覆盖。
常驻目录保持 278 个组件、1437 个真实导出及 13 个拒绝对照。
专项入口为 tools/check_stdlib_stack_native_value.py。

## 剩余语言核心与发布工作

实际探针确认，原生键 tryPop 所需的 nullable 拥有记录布局仍未接通；
不能改成返回伪造 T 默认值来替代已定的可空合同。Future 和函数值的
Stack 构造也未具备完整的调用与槽输送合同，继续作为核心接入缺口。
WaitQueue 拓扑仍待确认；独立 borrowed 原生 SSA spill、并发与拥有图
压力，以及三组与正式 DLL／SDK／VFS 总门继续。CRT 0、XRT 19 组
未决策范围不变，对象池最后。长期任务尚未完成。
