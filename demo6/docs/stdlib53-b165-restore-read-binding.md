# 数据恢复的宿主只读能力绑定

JSON/XLON 恢复需要检查真实模块拥有图，不能把模块或未知资源视为
叶节点。本批接通生成产物与 resident 宿主之间的独立只读能力绑定，
并完成双端验收。外部共享默认子对象仍只能按声明 clone 能力隔离，
否则 checked 拒绝；实际语言恢复入口与 clone thunk 尚未切换。

## 绑定合同

Lowered XLIR 新增 `ModuleAbi.readBoundaryAbi`。可达的四种类恢复
操作需要 `RESIDENT_HOST_BIND_V1`；Typed IR 和无可达恢复操作的模块
为 NONE。验证器从可达函数及运行时操作独立重算，不信任根表或标志。
`xl_script_bind_ownership_read_v1` 保留为编译器私有名字。

生成产物一次性复制 `XL6_OwnershipReadBoundaryV1`。畸形大小或缺失
Begin/Admits/End 拒绝，失败绑定可重试；成功后不可替换。第一次模块
初始化即封闭绑定，包括失败初始化及后续 unit/init。绑定只复制数据，
不分配、不执行能力回调，不把 CodeOwner 扩展尾部解释为另一种能力。
独立原生宿主负责在任何入口前串行绑定，并保证回调代码始终驻留。

真实 API 在拥有图证书、生命周期证书及 CodeOwner 绑定入口验收后，
先绑定 CodeOwner，再绑定自身 resident 只读能力，最后才允许代码
获取与模块执行。拒绝缺少这些前置合同或拒绝绑定的产物，不接受语言
源码提供的同名查找作为宿主能力。无恢复调用的产物不生成绑定和存储。

Scope/Boundary 声明迁入最小 `xl6_ownership_read_abi.h`，允许 ownership
reference 为不完整类型，不引入完整 XRT。私有声明和两种实际生成头均
独立通过严格 C11 与 C++17 检查。证书与能力仍分开：只读证书不能自动
授权任意跨线程访问，宿主 Begin/Admits/End 继续拒绝外线程及未知策略。

## 验收结果

Windows 与 Linux 完整源码构建分别为 251、250 个翻译单元。
编译器/runtime 对象为 O0，实际生成 C 和宿主分别执行 O0/O2。
仅动态加载已有 TCC 工具核心，不以发布 DLL/SO 代替本批实现。

- 新绑定专项每端 33 阶段，含一次预期旧宿主断言。旧宿主处理同一
  新生成产物时未绑定能力；当前宿主自动绑定通过。
- 每端检查两种模块共 10 项元数据变异，C/头输出前拒绝；C、头和
  IR 重复输出一致。恢复缺席及不可达调用都不带出能力存储与绑定。
- 六种畸形输入和重复绑定在 fail-after-zero 下不触发分配；绑定前
  变量改写不影响已复制能力。未绑定、已绑定、初始化失败和 unit/init
  循环分别验证封闭行为，回调不会在绑定期间执行。
- 真实 TCC 模块读取经过复制后的宿主能力：当前线程计数/追踪保留
  所有真实入口和代码引用；外线程活动拒绝，Scope 副本拒绝，嵌套
  Begin 返回 BUSY。五个原生绑定向量检查拒绝与接受的时序。
- 相邻绑定方法、raw callback、旧恢复语言门每端分别 35、22、33
  阶段。原生读取及声明 clone 的集中回归每端 12 阶段，4 条完整
  分配失败前缀各 166 次尝试，Linux 所选原生闭包 O0/O2 ASan/UBSan
  通过。后者为实际宿主读取实现的 white-box，不算语言恢复切换。

合计 270 个驱动阶段，含两次预期断言；8 条关闭前缀共 332 次尝试。
实际 TCC 测试最终核对工具缓存分配的精确地址和大小，其余增量分配
全部释放，账本无 double/invalid free 或 use-after-free 事件。

## 证据和剩余工作

源码快照为 Windows
`E:/xlang-validation/stdlib53-b165-source-dev2/frozen/demo6` 和 Linux
`/var/tmp/xlang-stdlib53-b165-source-dev2/demo6`。完整构建分别在
`stdlib53-b165-build-win-dev2/build.json` 与 Linux 对应目录。
绑定最终报告为 `stdlib53-b165-binding-*-dev6/results.json`；相邻门为
`stdlib53-b165-{bound,raw,language,native-read}-*-final1/results.json`。
Linux 目录使用 `/var/tmp/xlang-` 前缀。测试修正使用显式 test-root 并
记录 SHA，编译源码和封存快照不修改；早期候选不计入通过数。

本批只完成能力绑定。实际恢复调用仍借用已构造目标；迁移到 Take
控制器前，XLIR 必须明确其所有权转移与失败清理，然后生成声明 clone
适配器和节点/边表，切换实际 JSON/XLON 入口，不能直接 retain 目标
制造额外别名或在 Cgen 隐式偷走语言拥有槽。

其他容器载体、开放/泛型恢复、未知资源策略、冷路径性能、发布
DLL/SO/VFS、独立 JIT/AOT 和 53 模块总验收继续。没有新增公开模块
决策；CRT 无未决策，19 个 XRT 未决策组和对象池最后不变。
本批不修改 canonical XRT，不纳入无关工作区改动。
