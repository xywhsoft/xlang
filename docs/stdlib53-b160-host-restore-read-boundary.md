# 数据恢复的宿主模块读取边界

日期：2026-10-10。为共享默认对象的声明 clone 恢复补上私有宿主读取
能力：当前同步调用可以在精确认证后检查实际 Module 拥有图，其他线程
执行、未知参与者、退役中的模块和未声明能力的产物仍 checked 拒绝。
这不是实际 Cgen JSON/XLON 入口或完整标准库的验收。

## 独立合同，不推断旧产物能力

新增私有 `XL6_ModuleReadContractV1`，产物导出名为
`xl_script_module_ownership_read_v1`，精确标签为 `0x584C364D524431`。
宿主装载时检查尺寸、标签和完整 trace 身份。已有 CodeOwner/退役合同
不能代替它；缺少此导出的旧产物不会自动获得读取能力。

证书要求所有能触及模块 owning slots 的执行均被宿主入口或 resident
CodeScope 跟踪，包括直接导出、外部回调和异步尾部。未经跟踪的原生
写入不能声明证书。本批只实现宿主消费合同，没有让 Cgen 输出证书。

恢复计划可选择独立的 resident 读取能力；NULL 保持原来的关闭准入。
它不是用户回调、退役准备能力或“把宿主当叶子”的白名单。未知策略仍
在 Count/Trace 前拒绝；模块是本批唯一新增的可检查宿主族。

## 保留真实引用，隔离其他执行

公开运行、激活、停用和内部运行后退役入口，使用调用栈上的真实记录
跟踪所属线程；记录不另增引用。嵌套入口逐一计数，Count 保留 handle、
activeCalls、codeLeases、preparationRefs 和 snapshotRefs 的实际数量。
当前调用只是暂停，不从拥有图中减掉；外线程入口或 CodeScope 拒绝。

Begin 在 XRT Freeze 前 try-lock 宿主 API 锁，不等待外部占用，失败保持
空 scope 和原诊断。有效读取 scope 必须同线程、原地址、不可复制；
重复 Begin 拒绝。实际字段提交结束 Freeze 后即 End，随后才清理、
调用声明 clone 或执行生成代码，不在持锁读取边界执行语义回调。

Module 状态、API 锁和模块 Count/Trace/入口跟踪实现从原 API 文件抽成
共享私有头与 `xl_module_read.inc`。白盒夹具直接编译同一实现，不维护
第二套状态或模拟准入算法。完整 API 的 Linux 资源函数局部变量移入
Windows 分支，解决严格编译警告，不增加警告抑制。

## 最终验证

源代码来自 `stdlib53-b160-source-final7` 的独立 Git index 快照；未使用
工作区无关源码或旧 DLL/SO/TCC。Windows/Linux 各执行全部 54 个当前
List/恢复向量和全部 23 条计划恢复 OOM 操作，均覆盖 O0/O2 严格 C11。
双端共 316 个驱动阶段、216 次组向量执行、92 条关闭的失败前缀，
合计 12,804 次逐点尝试，新增模块读取和 clone 操作占 332 次。

每次批量 OOM 尝试独立核对真实分配账本，Linux 所选原生闭包完整插装
ASan/UBSan 通过。旧 List 的计划恢复以外 OOM 操作未在本批重复。
没有使用链接 GC 或警告抑制来绕过缺失符号/编译错误。

新向量包含当前线程读取、嵌套入口、共享默认拒绝/声明 clone 隔离、
clone/析构在边界外、复制 scope、递归读取、缺少证书、未登记 activeCalls、
未知 owner、退役态，以及真实外线程入口、CodeScope 和 API 锁占用。
失败不执行未知 Count/Trace、不改变共享原对象、不遗留模块 pin。

原生报告：`E:/xlang-validation/stdlib53-b160-plan-win-final7/results.json`
和 `/var/tmp/xlang-stdlib53-b160-plan-linux-final7/results.json`。
完整 `xl_api.c` 另在双端各以编译器启用/关闭两种配置执行严格
`-fsyntax-only` 检查，四次通过；这只证明语法集成，不证明链接或运行。
报告为 `stdlib53-b160-api-win-final7` 和
`/var/tmp/xlang-stdlib53-b160-api-linux-final7` 下的 `results.json`。

最终 `stdlib53-b160-index-win-final7.json`、
`stdlib53-b160-index-linux-final7.json` 复核快照与待提交 index、报告
输入和实际原生产物哈希；这些证据保存在 `E:/xlang-validation`。

## 下一步：实际语言入口

白盒模块使用不可执行的外部 image token，不装载 TCC 代码页，也不测试
其解绑/卸载尾部。实际 Cgen 必须先覆盖所有执行入口，再输出读取证书，
绑定独立宿主能力并生成精确 clone thunk，才能切换 JSON/XLON 恢复。
其他已批准原生资源拥有策略、开放对象/泛型类和语言容器载体继续。

大规模共享默认图重准备的冷路径成本、Windows/Linux 实际 JIT/AOT
及完整 53 模块产品总门仍待验收。本批未新增公开标准库函数，不标记
长期任务完成。CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
