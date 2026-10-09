# 数据恢复控制器的整图字段事务

日期：2026-10-10。私有恢复控制器已将逐字段修改默认对象改为整图暂存、
一次认证和统一提交。外部共享默认对象目前 checked 拒绝，不留下部分写入。
用户批准的声明 clone 隔离仍须接入执行和结果重认证；本批没有切换 Cgen
的实际 JSON/XLON 入口，也不代表发布产物或完整 53 模块验收。

## 暂存和提交

每个实际接收者拥有一份 XRT 字段事务记录。递归恢复只写准备存储；
晚发必需字段缺失、类型错误、工厂或提供器失败都不能修改原默认对象。
同一接收者的重复转换须命中明确 memo，冲突配方不能形成第二份事务。

提交之前关闭所有 Array/List 模板作用域，释放其实际 BUSY pin 和准入
元数据。任一模板 End 失败阻止提交，已关闭的模板不会再次 End。
提交槽逐一对应结果、观察输入、消耗的模板和 memo 的真实拥有引用；
重复指针按实际槽数计入，借用表和观察锚点不冒充引用。

实例 memo 独立拥有源默认子对象，即使它当前与结果同指针。这为未来
clone 产生不同结果保留稳定实例身份，不再依赖结果引用偶然保护源实例。
事务记录释放、模板和 memo 清理继续保留第一个诊断或无诊断 checked 失败。

对象提供器增加只读 resident 拥有图准入。在 Count/Trace 前分别认证
实际 Value、Class、CodeLease、Sequence、List 和游标策略。未知原生句柄
不调用外来 Trace；CodeLease 背后的未知或活动宿主 owner 不被视为叶子。
尚无显式 allowlist 的 Future/native-reference 策略保持拒绝。

外部强引用、同一析构 backing 的 COW 别名或游标导致 checked STATE，
不会退化为成功 null。弱引用不制造强拥有槽，提交后提升看到完整新状态。
只有已证明的 wire 拒绝能在 checked 清理成功后得到 canonical null。

## 批量验证

原生驱动支持从已登记向量和 OOM 操作选择专项子集，报告明确记录
FocusedValidation，不把子集当作完整原生总门。每条计划恢复 OOM 前缀
可在一个进程内连续执行，但每次尝试仍独立开始和关闭分配账本，核对
存活数、字节数、分配释放平衡及非法释放、重复释放和释放后访问。
检查器核对逐点连续命中，直到首个无命中成功；减少的是进程启动次数。

本批每个平台执行全部 52 个 List/恢复向量及全部 16 条计划恢复 OOM
前缀，均覆盖 O0/O2。双端合计 280 个驱动阶段、208 次向量执行和
64 条关闭的前缀，共 10,632 次逐点尝试。新增 16 种事务反例覆盖
外部强引用、析构 COW 别名、游标、弱提升、未知句柄和晚发字段错误；
观察者在正常拒绝和 OOM 路径上都保留完整原始数据。

Windows 报告：
`E:/xlang-validation/stdlib53-b158-plan-win-full2/results.json`。
Linux 报告：
`/var/tmp/xlang-stdlib53-b158-plan-linux-full2/results.json`。
两份报告均 Complete、InputsUnchanged，严格 C11 Wall/Wextra/Werror；
Linux 完整插装本次所选实际原生闭包的 ASan/UBSan。
每份保留 201 个输入、两个可执行产物的哈希、全部命令和日志。
本次没有重跑计划恢复以外的旧 List OOM 前缀，不能将此范围扩大。

旧 Array import 的九种布局另以不链接 RestoreClass 的独立实际闭包
回归，检查通用 import 未意外增加 Class/List 的直接链接依赖。
双端各 3,609 个检查阶段和 56 条关闭的连续分配失败前缀通过；Linux
所选闭包 ASan/UBSan 通过。报告为 Windows
`E:/xlang-validation/stdlib53-b158-chain-win-dev2/results.json` 和 Linux
`/var/tmp/xlang-stdlib53-b158-chain-linux-dev2/results.json`。
代码来自 `stdlib53-b158-source-dev2` 的独立 Git index 快照；Linux 使用
独立复制目录，未借用旧 DLL/SO、TCC 或未提交工作区源码。

## 后续语言接入

声明 clone 必须在 freeze 外运行，随后认证返回对象的精确类型、
身份、完整拥有图及所有待修改子对象；返回 self、其他共享对象或仍
共享的子对象不能提交。不能用重跑默认初始化器或结构复制代替声明能力。
活动模块 owner 的可检查边界也仍须落实，不能绕过未知 owner 准入。

之后才能切换 Cgen 整图恢复入口，并继续开放对象、泛型 class 持久化、
List/Set/Dict 语言载体、SlotMap、WaitQueue/Channel、OrderedMap 和
产品总门。19 个未决策 XRT 组不变，CRT 无未决策，对象池最后。
