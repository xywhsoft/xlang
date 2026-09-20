# XLIR 恢复执行与派发合同

## 状态与证据

这是 NW-04 的实现约束与接入清单，不是完成声明。生产 Cgen 仍在 Future Watch
通知线程内执行 resume。`check_xlir_async_resume_isolation.ps1` 的 400 个用例中，
200 个 pending 用例全部违反生产者执行隔离；200 个 ready 用例正常，四种终态
的结果/finally 次数及生产者诊断均正确。该门默认失败，不接受“预期失败即通过”。

`src/runtime/xl6_continuation_queue.c` 是纯 C 运行时传输基础层，没有 AST/IR/Cgen
依赖。新增的 `xl6_continuation_executor.c` 在其上提供独立 worker 执行服务及
阻塞区补位准入。目前仅由独立质量门编译，尚未接入生成程序、运行库导出或发布
构建；不能据此宣称语言恢复已隔离。

## 固定的语义边界

- Future 生产者只发布结果与就绪事件，不能承担语言恢复、catch/finally、拥有值
  销毁等任意语言执行。ready 与 pending 都进入同一明确的执行域，不由竞态决定。
- 启动在调用者上执行到首个挂起点的规则，与 await 之后的执行域分别建模。不得
  根据具体网络函数名选择恢复方式，也不得禁止原来合法的同步语言调用来收口。
- 每个已接受 activation 恰好执行一次结果入口和一次物理释放。取消属于普通
  await 结果，仍走 verified CFG；执行器不能取消或丢弃一个已接受的工作项。
- 注册和派发的准备失败必须发生在拥有权提交之前，进入已有 setup-failure
  清理程序；不能在通知线程临时分配失败后内联执行语言 CFG 作为 fallback。

## 无分配交接基础层（已实现）

先在运行时队列预留一个容量/存活 lease，再将 ticket 嵌入 activation。下列
三个事件可按任意顺序到达，但必须全部到达才能入可执行队列；同一 activation
的后继还必须等待前一 frame 的物理 release 返回：

1. Commit：注册结果与 frame 拥有权交接已提交。
2. Ready：Future 已终态，结果仍由独立 source lease 保活。
3. TransportReleased：Watch 不再访问嵌入 frame 的存储。

最后一个事件在锁内完成发布，发布不分配、不会满队列、不执行语言代码。新工作
在 Reserve 阶段得到 FULL/CLOSED/INVALID 的明确结果。失败注册可在尚未收到
任何事件且未被 Watch 接管时 Abort，容量退回、frame 仍归调用方；Abort 不调用
语言析构。已被 Watch 接管但尚未完成的 ticket 也不能 Abort 或重复注册。

RunOne 由执行器调用，脱锁运行，然后恰好调用一次 release。live lease 延续到
release 返回，即使 release 已释放 ticket 所在 frame 也不能提前销毁队列。
Close 拒绝新的根 activation，但不撤销已接受项或同一 activation 的后继；Destroy 在存活项非零时返回
busy，不释放队列。生命周期入口的外部串行化、运行回调正常返回是明确前提。

独立门覆盖六种事件顺序、注册回退、容量拒绝、关闭后已接受项继续运行、FIFO、
并发发布期间立即执行并释放 frame、真实创建分配失败、发布零分配、首诊断保持
与逐例内存平衡。它不证明工作线程策略、语言效果或 DLL 安全卸载。

### 后继交接与共用 Watch 注册（2026-09-11，已实现运行时）

不能把第二次 await 的 frame 当成新根任务重新 Reserve：这会把容量不足或
Close 转化为对已接受 activation 的隐式中断。ReserveSuccessor 仅由当前
ticket 的 run 调用，至多有一个后继，继承根 activation 的容量，不增加根额度。
前后两个 ticket 各持物理存活 lease；后继的三个事件可并发完成，但前一 frame
释放返回后才发布后继。因此每个 activation 最多同时保活两个 ticket，不会
通过连续 handoff 绕过容量上限，也不会在旧 frame 析构时并行执行后续 CFG。
未交接的后继可 Abort 后重试；release 回调无权产生后继。所有入口继续要求
调用方遵守 ticket 私有拥有权，不允许第三方与 run 并发申请/撤销后继。

新增 `src/runtime/xl6_continuation_future.c` 共用注册层，直接使用公开 XRT
Future Watch：READY 合成 Ready/TransportReleased 两事件，PENDING 由实际
生产者回调发出，仅操作 ticket，不执行语言回调。调用方在注册和 frame 移交
完成后 Commit；源 Future 的强引用仍由调用方负责直到捕获结果。注册失败
不提交拥有权，保留原诊断或 false/NULL，并允许 Abort。transportOwned 与
事件位分开，防止尚未收到事件时错误撤销/重注册已被 XRT 接管的 Watch。

固定门通过 240 条 open/closed 容量一的 32-frame 链，共 7,680 个 frame；
六种事件顺序、后继 Abort/重试、根容量复用、零分配发布均覆盖。另有 300 组
注册/回退（200 次公共 WatchAdd ABI 模拟错误，不是真实 allocator 错误），
以及 300 组真实 Future 四终态乘三种前驱退休时序、双 worker 运行与内存平衡。
生产者诊断保持；待前驱 release 返回的规则不能由“结果已经发布”代替。

这六个 runtime 源/头文件仍独立编译。当前生成语言程序尚未改用共用注册层，
IR admission/派发 lowering、module code lease 与发布接入仍需完成；不能把
这些运行时证据当成生产异步隔离已经通过。

### 宿主停用事务与代码退休（2026-09-11，v87）

宿主 API 原本在调用清理前就将 module 标为未激活，checked finalizer 失败后，
下一次 Deactivate 会直接返回成功而不重试。源码现改为四状态：inactive、active、
deactivating、deactivation-pending。只有清理结果成功才提交 inactive；失败
（包括 busy、false/NULL、资源已清理但发生异常）保留 pending 并允许重试。
pending 期间不能重新 Activate；deactivating 拒绝公开入口重入；仍有另一个
已接受的公开入口时，不执行停用清理。公开 C ABI 不变。

源 API 固定门通过 400 组事务、3,600 次重入拒绝、入口计数恢复与内存平衡；
两种编译配置均严格编译。测试直接包含生产 API 源码，仅替换 TCC 符号查询，
不是通过发布 DLL 测试源码，也不是完整 JIT 卸载证明。

v85 的销毁路径单独红门曾复现：Destroy 走 void unit 包装，checked 清理
返回失败仍继续调用 tcc_delete。使用与生成模块一致的 checked + void 包装
接口，假 TCC 删除记录器观察到一次清理失败后仍请求一次代码删除。该测试不
真的释放 JIT 页，但源 API 的错误销毁顺序有直接证据。仅修复 Deactivate 不
等于修复 Destroy。v87 已补齐下面的宿主/运行时合同；生成器接入仍未完成。

Destroy 现在提交退休请求：仍有公开入口或代码 lease 时不执行清理；优先调用
checked unit，失败则由宿主侵入式待退休队列继续拥有模块、代码、依赖及运行时
引用。宿主 inactive 不代表一次性运行的 artifact 已清理，checked phase guard
仍须确认。清理在 API 全局锁外执行，拒绝重入退休，保留首诊断及 false/NULL。
成功后才删除代码、卸载依赖，最后归还运行时引用。

最后一个入口/代码 lease 归还会尝试退休；正常 RuntimeUnit 还执行有界的待退休
收集。没有后台自动重试承诺。公开 Destroy 是消费句柄的请求，调用者不能拿
可能已释放的原始句柄重复调用；测试中的重复请求仅用于白盒确认仍被队列拥有
的对象，另有不依赖原始句柄的 RuntimeUnit 收集验证。

新增 XL6_CodeOwner 宿主能力及可选 xl_script_bind_code_owner 握手：绑定提交
前不能取得 lease，关闭后拒绝新 lease。ReserveOwned 在接收根 ticket 前取得
代码引用，ReserveSuccessor 转交同一引用，撤销后继不释放前驱的引用。最终
run/release 回调返回、物理 ticket 退休后，常驻运行时才能归还代码引用；归还
之后不得再访问可能已卸载的生成代码或描述符。进入 ReserveOwned 的生成调用
本身须已有外部入口 lease；裸 GetSymbol 指针不是自动保活的调用能力。

固定门覆盖 400 组 checked/legacy、active/inactive 清理重试，100 组绑定/代码
引用寿命与真实依赖 DLL 卸载，200 组 owned ticket 撤销/后继链；另测绑定失败
回滚、锁外清理、首错误与总内存平衡。TCC 删除仍是记录器，不实际释放 JIT 页。
v86 七门六绿一红：销毁门转绿，生产隔离门仍有 200 个 pending 线程违规。
v87 时 Cgen 不发出绑定符号、不取得代码引用，也未使用 owned ticket 派发；逃逸
闭包、回调、Future payload 的完整代码寿命和生产卸载准入不能借此宣称完成。

### 生成模块的绑定入口（2026-09-11，v89）

ModuleAbi.codeOwnerAbi 现在显式区分无生命周期的 IR 和 bind-once-before-init。
lowering 定型，verifier 从 stage/初始化函数独立重算；文本及 JSON 都公开此事实。
固定绑定符号不能被语言 export/import/global 链接占用，C 和头文件发出前拒绝。
运行时宿主和生成 C 共用同一份 CodeOwner 类型 token 定义，不另维护一套布局，
也不要求生成模块包含源码树头文件。

Cgen 已实际发出 xl_script_bind_code_owner，并在首次初始化尝试的任何副作用前
封闭绑定。能力可选、最多绑定一次：校验前缀大小和两个回调，按字段复制已知
前缀，允许扩展结构与 NULL context，不调用宿主回调；拒绝重复绑定，失败请求
不改变状态。初始化失败、unit/init 周期都不得更换代码拥有者。未绑定 artifact
依赖外部保证代码常驻，不能从未绑定推出“无需代码寿命管理”。宿主串行化绑定
和初始化，原始 C 函数指针的并发调用仍需外部入口 lease。

真实生成 C 在独立编译单元中与源码 API 交接：绑定宿主能力后，测试以明确的
外部入口 lease 将能力交给 ReserveOwned；Close 后两代 ticket 共用一个引用，
执行实际语言 export，最后一个 release 返回后才请求 TCC 删除。另测不绑定、
绑定/未绑定的初始化失败、扩展能力前缀及描述符复制；五种模式内存平衡。
这是测试宿主主动预留 ticket 的集成证明，不是生产 async 已自动取得引用。
TCC 查符号与删除仍由记录器提供，未实际释放 JIT 页。

v88 冻结批次十二门十一绿一红；四组模块生命周期严格 C 回归包含 8,800 个
清理用例及 6,800 次真实诊断分配失败，所有权门保留 470 个拒绝变异，模块池
严格 C/JIT/AOT 通过。生产隔离门仍有 200 个 pending 线程违规。下一步仍须将
执行域、预留/提交、阻塞准入及 setup-failure 清理建模为 verified IR，再让
生产 async 使用 owned ticket；完整逃逸代码寿命和发布准入仍开放。

## 执行服务基础层（已实现，尚未接入语言）

队列增加条件变量等待：没有可执行项的 worker 休眠；Close 后已有的 pending
reservation 仍保活 worker；最后一个 live lease 退休时唤醒所有 worker 退出。
创建条件变量失败回滚队列；发布依旧不分配。Destroy 的 busy 检查不等待尚未
完成的 Future，只有关闭且没有 live lease 时才 join 工作线程并释放运行时状态。
join 覆盖 XRT 线程上下文清理，不能把“语言结果已发布”当成线程代码已退出。

执行服务用独立 worker 执行 run/release，不在生产者回调运行语言工作。初始
worker 数、线程高水位上限、ticket 容量显式配置。进入可能阻塞区时，若本 worker
是最后一个非阻塞 worker，则先创建替补，再标记自己阻塞；线程上限或创建失败
返回明确准入失败，不调用该阻塞操作。嵌套阻塞区只计一次，离开后恢复容量。
此证明以所有可能阻塞调用正确执行 Enter/Leave 为前提，尚不能套用于当前 Cgen。
额外 50 组测试覆盖两个初始 worker 同时申请阻塞：有额度时替补执行解阻工作，
无额度时一个调用被拒绝进入阻塞区，返回的 worker 仍可执行解阻工作。

固定门新增 300 个真实 Future Watch 交接：ready、注册后先提交再完成、注册后
先完成再提交，乘成功/失败/取消/关闭；全部由不同于生产者的线程执行，并保持
payload/错误、一次释放与生产者诊断。另测 100 次隔离交付、150 个阻塞补位/
上限/分配失败组合（50 次真实补位分配失败），含嵌套保护、关闭后继续交付、
busy/self-destroy 拒绝、释放 frame 后 join 与内存平衡。创建故障按当前实现
覆盖九个真实失败预算点；这不是全部 OS 线程创建错误的穷举证明。

## 语言执行器接入要求（未实现）

### 通用调用/析构的根 activation 进展准入（2026-09-11，v91）

现有补位策略只有在所有可能阻塞调用正确 Enter/Leave 时才能证明进展；把
这套会失败的准入插入任意 NO_RAISE 或析构调用，会制造新的语言失败路径，
更不能在准入失败时跳过析构。因此新增 Guaranteed 策略，将资源失败放在根
activation 被接收之前，而不是在每次 foreign 调用或清理时临时申请能力。

运行时不变量是：可发布的已接收根 activation 数不大于已配置的 worker 数。
根 ReserveOwned 先取得代码引用和私有 ticket，再在执行器锁内检查并补足
worker，成功后才能返回供调用者提交。后继继承根容量；每次 await 不新建
线程。无法分配 worker/线程对象时 Abort 私有 ticket，归还代码引用和容量，
返回明确 RESOURCE_FAILURE，frame 仍由调用者沿 setup-failure 清理。既有诊断
保持，原本无诊断时保留新资源错误；拒绝代码引用的无诊断失败也保留明确状态。
获取/释放代码引用不持执行器锁，避免与宿主退休锁的反向依赖。

这允许已接收任务的普通同步调用、frame 析构乃至宿主退休 hook 阻塞，而其他
已接收任务仍有 worker 可以执行；不保证解决源程序的循环等待、永久不返回的
外部调用或公平调度之外的问题。策略有实在的资源代价：最坏 worker 高水位
达到同时接收的根数，capacity 是显式宿主资源预算，不是无限并发承诺。线程
保留供后续 activation 复用直到 shutdown；旧的补位执行器策略仍可独立使用，
但不能未经完整调用/清理准入证明就作为通用语言后端。

另外修正了根信用提前归还：生成 run/release 返回且 frame 已释放后，宿主
code-owner release 本身仍可能进入模块清理并阻塞。此时不能算成空闲 worker。
队列继续持有常驻执行/队列 lease，直到该 hook 返回才减少 active/live；既
不再访问生成 frame，也不允许 hook 中 Destroy 提前释放队列。根 Abort 同理。
v89 冻结队列加只读计数接口的对照，在新策略的 host-drop 等待场景出现五秒
超时；新队列通过同一检查。这是执行容量缺口的对照，不冒充旧执行器完整回归。

固定门覆盖 100 组四根依赖链（普通调用、析构、host-drop、后继交接），50 组
四线程并发准入后关闭/交付，100 次真实 worker/线程对象分配失败，以及 75 组
容量、关闭、Abort、嵌套 bookkeeping 和无诊断拒绝，全部内存平衡。v90 八门
七绿一红，原队列/补位执行器/交接及源码宿主/真实生成绑定均通过，生产隔离
仍有 200 个 pending 线程违规。

下一步生产接入仍须成套完成：ModuleAbi 的执行服务生命周期与预算来源，
AsyncMachine 的恢复执行域、根预留/后继继承/提交与 setup-failure 合同，
frame 内嵌 ticket 及常驻服务链接、模块关闭/重试。Cgen 只能消费这些验证后的
事实。当前生成 async 尚未选择 Guaranteed 策略，不能把运行时证明算成 NW-04
关闭，也不能绕过仍开放的代码 escape、发布同源和完整语言验收。

调用执行边界的 IR 事实已先行接入：CallTarget 新增 executionBoundaryAbi，
区分 `language-cfg` 和 `foreign-may-block`。前者表示函数体由 IR 表达，绝不
等于传递调用链不阻塞；所有外部 C 目标和 native factory 保守标记后者，不能
从 NO_RAISE、返回 bool、函数名称或实现看起来很短推断不阻塞。构建、验证及
文本/JSON dump 共用该合同，缺失、非法和错配字段均在 C/头文件发出前拒绝。

这不是“执行器准入已降低”的标记。目前没有给调用增加潜在失败，也没有切换
生产恢复路径。准入失败与被调用者自身失败必须保持分离。需要区分调用方尚未
进入 CALL，与 CALL 边界已经接收 consuming 参数但尚未进入 native 函数：后者
由调用边界负责退休输入，不能把已从调用方存储分离的 owner 留给调用方猜测。
native factory 输入及析构责任也必须明确。把已有外部
NO_RAISE 改成 MAY_RAISE，或在发出调用前失败却照搬“调用已消费参数”的清理
路径，都不是完整方案；析构不得因新增准入失败而被静默跳过。

默认执行域应由语言运行时拥有，而不是网络 worker、任意生产者或临时创建的
逐 await 线程。队列传输与执行服务分开，宿主执行器只能通过相同的 lease/发布
协议接入，不能绕过 IR 的拥有权与效果合同。

### 已修复的现有 foreign preparation 边界（2026-09-11）

独立于未来执行器，当前 FOREIGN_SCOPE 的 Begin 失败已经会跳过 native 调用。
原直接调用路径遗漏 OWNED 参数回收；原始三分支测试中，带诊断与 false/NULL
两条失败路径均泄漏一个四字节字符串。公共 ABI 边界注入复现此问题，不冒充
实际 OS/FLS 或内存分配故障。

CallTarget.preparationFailureAbi 明确 FOREIGN_SCOPE 的 retire-owned-reverse
合同；其他物理调用 ABI 不增加此调用方适配器回退，它们保留各自的 callee/
launch 输入责任。lowering 定型、verifier 重算、文本/JSON dump 均显式表达。
直接 SSA 调用及协议 witness 共用释放实现，只从已验证的 parameterAbis/types
和物理参数映射取得释放目标；逆序 detach-before-destroy，先锁存失败位和
首错误，清理错误不能覆盖 false/NULL，不回查 AST 或调用方存储。

固定门覆盖 600 次两拥有输入/一借用输入、后续清理报错、原错误身份与
false/NULL 保持，逐例内存平衡；协议门补充真实类析构异常及 scope 故障验证，
严格 C/JIT/AOT 通过。这修复已有准备失败，不代表 blocking admission 或
continuation 的生产派发已接入。

当前 XRT 的 FutureWait 经 CondWait 阻塞系统线程，不是协程让出。只接固定
线程池、或把工作放进协程调度器，都不能证明阻塞等待的进展性。需要在语言/FFI
调用合同中明确可能阻塞的执行边界，让上述执行服务进入阻塞区前保留其他工作可前进
的能力；通用策略在根 activation 发布前取得该能力，失败走其 setup-failure；
采用调用处补位的策略则须另行证明完整失败及清理合同。不可让
所有 worker 阻塞后再等待同一队列自救。未知外部调用不能按“不阻塞”乐观处理。

后续必须一起实现并验证：

- IR 的执行域、ticket 预留/提交、生产者隔离以及 setup-failure 入口；verifier
  对缺失、错配和非法效果组合拒绝；Cgen 只按这些记录发出机械调用。
- 将运行时服务的激活、唤醒、阻塞进展、再入、并发 handoff 与有界容量准入接入语言；不能
  把“队列已有容量”误解为“有可运行 worker”或“阻塞调用必能完成”。
- callback/frame/source 的存活 lease 与模块代码 lease：关闭、待完成 Future、
  正在执行、物理 release 都必须纳入卸载屏障，busy 不得释放代码。
- 现有 outcome、首错误、false-NULL、取消一次、capture-before-source-release、
  retire-before-publish 合同保持不变；迁移旧宿主对内联完成的隐式时序假定，
  但不能放宽拥有权断言或更改语言期望来通过测试。
- 新隔离门转绿后，统一重验 TCP/TLS 及 owner/terminal/cancellation 门，并补
  满容量、所有 worker 阻塞、派发准备失败、关闭竞态和卸载测试。其前不关闭 NW-04。

### 2026-09-11：生产状态机接入 Guaranteed 派发（4494EF34）

本节更新上文“生产尚未选择”的阶段状态，不改变未完成的整体准入要求。
AsyncMachine.executionAbi 明确 guaranteed-resume，ModuleAbi.continuationAbi
明确 guaranteed-workers 服务；lowering 建立、verifier 重算、文本/JSON 输出，
缺失、非法或模块错配在发出 C/header 前拒绝。首个 AWAIT 在调用者执行的前缀
之后预留 owned root，后继继承 ticket；Watch 注册成功且 owner 转交完成才
Commit。恢复及物理 frame release 均由常驻执行器执行，生产者只发布事件。

模块初始化建立服务，checked 退出先 Close/Destroy；busy 保留模块及资源，
不能当作清理成功。容量由 XLIR_CONTINUATION_CAPACITY 指定，默认 256，
按根 activation 数扩充并复用高水位 worker，不是逐 await 创建线程。JIT
符号注册、DLL/Linux 源清单、AOT 私有运行时源选择及 VFS 头清单已接线；
发布二进制和 VFS 资源尚未重建，不据此宣称发布链接或 Linux 已验收。

实际生成 C 的隔离门 400 个用例通过（pending/ready 各 200），原 200 个
生产者线程违规降至零。新增派发门按容量 1/2 分 lane 收集：两次 await、
关闭后的后继、容量耗尽、带诊断注册失败、普通 foreign CALL 阻塞时其他根
仍能完成，共八个 lane、200 次通过且内存平衡。四个 lane 保留失败，分别
是两种容量下的首次及后继 Watch 注册 false/NULL；不能把 300 个计划用例
全部计为已执行通过，失败 lane 在首个断言停止。

共同根因已定位：旧 xl6IrCgenEmitActivationFailure 在诊断为空时补造
MemoryError，而 setupFailureEntry 仅记录控制入口，ERROR_TEST 仍观察环境
错误指针。它不足以表达“失败位为真且诊断为空”。测试要求失败 Future，
并保持原诊断身份或 NULL；不是要求所有 async 函数体失败都同步返回 NULL。
下一批应把 preparation/await 失败载体及其清理传播放进 IR 并由 verifier
校验，贯通 launch/resume/cleanup/publish，不添加生成器局部状态猜测，也
不能删除无诊断用例或把所有准入拒绝伪装成内存不足。

本批未证明实际生成 async 与宿主退休的完整联合卸载、初始化失败时 pending
服务重试、真实 JIT 页释放、全部 terminal/cancellation/owner 综合门及
TCP/TLS 网络进展。NW-04、逃逸代码寿命和发布同源准入仍开放。

### 2026-09-11：AWAIT 独立状态载体与统一获取事务（875A92E1）

先纠正上一节对 NULL 的边界表述：它必须在语言失败转移及清理期间保留，
但不能直接成为 XRT 失败 Future 的 Error 字段。xrtPromiseReject 明确拒绝
NULL；既有 XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND 传输在最终失败发布时使用
静态 INTERNAL。原测试把内部合同套到了外部传输，遗漏了这一边界；修正为
最终 INTERNAL，同时增加 xrtTakeError 公共调用边界观察器，严格断言注册
false/NULL 之后、finally 之前没有生成任何诊断。带诊断失败仍保留 code 62。
旧 v92 驱动跑加强后的相同 fixture，四个首次/后继失败 lane 均在清理前
观察器失败；新实现全部通过，因此不是靠修正终态断言绕过旧缺陷。

Instruction.awaitOutcomeAbi 新增 status-diagnostic，且只允许用于 AWAIT。
lowering 定型，verifier 拒绝缺失、非法及贴在无关指令上的字段，text/JSON
显式输出。AWAIT 的语言 payload 与物理成功位分离，立即相连的 ERROR_TEST
只观察成功位；setupFailureEntry 把失败位写为 false 后进入原清理 CFG，
不再补造 MemoryError。恢复前 source lease 清理使用独立取得状态和清理
结果合成状态，false/NULL 不再靠 INTERNAL 注入来穿过错误测试。

阻塞 AWAIT、同步 next ABI 的 async generator 和 async machine 共用一个
按已验证 payload ABI 获取结果的实现。缺失 payload 与合法的空指针值分离；
复制/retain 返回失败时，即使没有诊断也不进入成功语言语句。移除原逐类型
await wrapper 及重复获取代码，七个相关 Cgen 源文件净减 8,524 字节；这不是
发布 DLL 体积结论。旧等待包装也会把缺失/复制失败误当成功，新固定门添加
成功语句计数，避免“随后 assert 失败进入 catch”冒充 await 已正确拒绝。

验证：派发十二 lane、300 次通过，含 finally 精确次数及内存平衡；新增
300 次模块事务覆盖阻塞与 generator 两条路径（正常载荷、缺失载荷、公共
复制边界 false/NULL），成功语句、finally 和内存均核对。旧 v92 驱动在新
获取门的成功语句计数断言失败。35 个 IR 变异（含 14 个 await 字段变异）
及两个真实源链接冲突在 C/header 之前拒绝。四状态隔离仍为 400/400 通过。

v96 十五门同输入全绿。它关闭本轮注册/获取状态缺口，不关闭联合代码卸载、
初始化 pending 退出重试、全部原 terminal/cancellation/owner 门、TCP/TLS、
逃逸代码或同源发布验收；这些继续作为下一批真实集成验证范围。

### 2026-09-11：生成 DLL 联合退休及逃逸载荷缺口（96A44564）

新联合门将真实生成 C 编为可卸载 DLL，源码 API 负责 CodeOwner 绑定、激活、
代码引用及退休收集；lookup/delete 接到 GetProcAddress/FreeLibrary，而非
TCC 的编译/重定位。八个 continuation 入口从常驻测试 EXE 导入，XRT 从
未改动的 xl.dll 导入；执行器没有被错误地编入待卸载 DLL。每次 FreeLibrary
后断言 GetModuleHandle 返回 NULL。仍不能把这叫作实际 TCC 页释放验收。

公开语言 C 导出拒绝 Future 参数/返回，extern Future 参数也被拒绝。本门
不放宽语言准入，而是用测试专用入口调用 JSON 给出的规范 async target，
并由源码宿主的真实 activeCalls lease 包住入口。因此测试证明内部生成 ABI
的代码寿命要求，不证明公开 Future C 导出已受支持，也不宣称已复现公开 API
可利用漏洞。失败初始化模式则从真实模块初始化代码发起 pending async。

六模式全收集：正常完成、输入失败、输入取消、恢复 CALL 内请求退休、失败
初始化后 pending 继续完成，五模式各 25 次真实卸载、finally 一次、内存
平衡。宿主只请求一次 Destroy，随后从 resident retirement queue 收集，不
重复触碰可能已释放的模块句柄。pending 期间停用被拒绝；最后 worker 的
退休尝试不能销毁自身执行器，必须保留待收集状态，外部收集最终完成。

第六模式保持已完成 Future 的 payload：宿主试图删除模块时，载荷仍持有
xlir_async_value_free 这个生成代码地址。本门在 FreeLibrary 前断言失败，
没有为了演示 UAF 而实际卸载仍需调用的析构代码。根 frame/ticket 的 lease
只覆盖执行，尚未覆盖结果载荷的代码寿命。这是本轮新增的真实内部 ABI 缺口，
不能把其他五模式的成功当成所有逃逸值已安全。

修复需要明确区分执行寿命和逃逸值代码寿命：最终 payload、closure/native
销毁 glue 及其依赖必须由已验证 IR 给出，不按类型名字猜测。继承已有执行
能力的完成结果不能在 closing 之后重新申请一个“新根”权限。还必须验证
模块全局变量持有自身 Future/closure 时的环，以及 checked unit 对内部根
的释放顺序；直接让所有返回值增加现有 codeLeases、并阻止任何模块清理，
可能把悬空指针换成永不退休。解决方案必须一起覆盖这些条件，不能只特判
本测试的整数 payload。直接一次性编译运行路径的绑定及删除也仍须联合审计。

严格 -O2 编译同时发现准备失败输出未初始化：FOREIGN_SCOPE 未进入 native
函数时，Future 返回值仍被错误 CFG 当作待清理 owner。准备失败 ABI 现明确
每次尝试前输出为空，Cgen 从该合同及已知物理类型发出中性初始化，取代仅对
native class field 的特判；恢复入口 AWAIT 输出也与 launch 一致中性初始化。
新门每轮在同一循环 CALL 成功一次，再分别 false/NULL 和带诊断地拒绝两次，
200 轮通过且内存平衡；旧 v96 驱动在同一 -O2 严格编译中被拒绝。

v98 十八门同输入十七绿一红，原十五门全部保持通过。联合载荷寿命、其余
综合 async/网络/逃逸合同及发布同源仍开放，不以门数替代完整验收。

### 2026-09-11：共享代码能力与常驻销毁载体（0489633D）

新增 resident xl6_code_lease：一个宿主 acquire 支撑一个原子引用计数能力，
已有持有者可以在 closing 后保留子引用，不重新执行宿主新根准入。它只代表
代码驻留，不代表新的可执行任务、执行器容量或模块全局资源的存活许可。
最后释放在常驻代码中复制 hook/context、先释放自己的记录、再调用宿主，
此后不读取生成代码或已释放的能力；宿主 release 的诊断与调用者隔离。
使用 XRT atomic64，不依赖某一 C 编译器对原子内建函数的链接实现。

CodeDropCreate 借用原能力，分配成功后持有一个子引用，不消费 payload/context；
失败不调用析构。发布未接受时 Abort 只释放记录及子引用，输入仍由调用者
回滚，且生成调用栈必须仍有独立入口/父能力。发布成功后，Future 调用常驻
CodeDropInvoke：先 detach/free record，再调用真实生成析构，返回后才释放
代码能力。析构自己的失败诊断保留；此机制不规定析构在哪个线程执行。

固定门通过 350 个真实 DLL 生命周期、20 万对四线程保留/释放（含 worker
最后释放）、100 次实际 capability/record 分配失败，检查 closing 后继承、
禁止新 acquire、发布拒绝/Abort、嵌套继承、析构错误、宿主错误污染隔离和
内存平衡。C-host JIT/AOT 各 10 个生命周期、8,000 对引用操作 smoke 通过；
其运行时已启动，故不切换 debug allocator，不宣称 smoke 的分配故障/平衡
覆盖。测试 DLL 的分配及诊断操作通过宿主 hook 使用同一 XRT 实例，避免把
CLI 内嵌运行时与 xl.dll 的 TLS/allocator 混用。

组件已进入 DLL/Linux 源清单、完整 xlang runtime 源列表、六个 JIT 符号与
VFS 头清单；没有重建发布文件，也未声明 Linux/发布同源验收。生产 Cgen 和
continuation ticket 尚未选择它，原联合模式 3 仍失败；不让运行时单元测试
替代原生产红门。

接入前必须继续验证：代码引用应跟随实际 code-bearing 对象，而非仅跟随
最外层 Future。否则 await 取出并保留 closure/class 后释放 Future，仍可能
丢失其代码引用。模块内部拥有值与外部保留值的关系、模块自持 Future/closure
形成的环、全局资源的销毁依赖及直接 one-shot 运行路径也必须进入 IR/宿主
合同。不能给所有对象盲加现有执行 codeLeases，使 module unit 永远无法
释放自己的根；也不能把这些检查缩减为只替换整数载荷的 free 函数。

### 2026-09-11：Future 载荷容器与对象代码身份分离

普通 Future 载荷不应仅因释放容器而引用生成模块。新增常驻
`xl6_future_payload`，覆盖 trivial/tagged/非拥有指针的外层 box、string、
xvalue、error、嵌套 Future 五组物理释放行为；从既有、经验证的
AsyncResultAbi 统一选取。launch、resume 和 task/continuation 使用同一份
映射，移除旧普通类型的生成 free helper，未增加 AST 追溯或名称推断。
闭包仍使用显式的生成释放合同，不把它错误归类为普通指针。

Windows 下直接传导入函数地址可能传入位于生成 DLL 的导入跳板。第一版
仅移动实现的方案仍在联合模式 3 失败。因此改为调用宿主
`xl6PayloadDropGet(kind)` 获取实际常驻函数地址，不保存生成端的取址结果。
联合门在卸载前比较回调地址，并用 GetModuleHandleEx 确认其属于宿主 EXE；
模式 3 随后先实际卸载 DLL，再读取及销毁返回 Future。原六种模式全部保留。
函数命名独立于 `xl6Future` bridge 前缀，避免私有源选择误带整组 bridge。

这只消除了容器自身不必要的代码依赖：xvalue 可含 class/native finalizer，
Future 可含闭包或其他代码相关对象。它们从容器取出后的身份、全局资源依赖、
模块自持根形成的环、one-shot TCC 删除路径仍须独立压实。共享 CodeLease /
CodeDrop 尚未接入生产对象，不宣称所有逃逸值或完整 async 已通过验收。
将来向生成代码提供 CodeDropInvoke 地址时，同样必须避免导入跳板寿命问题。

运行时门覆盖 1,500 次五类型 accepted/rejected/empty 事务、嵌套 Future
独立保留、精确内存平衡；C-host JIT/AOT 各 1,500 次独立验证，不宣称其
debug allocator 覆盖。私有源裁剪测试检查保留 value/coroutine 依赖，且
不强制开启完整运行时或执行器。DLL/Linux 源清单、JIT 符号、VFS 头清单及
私有源选择已接线；没有重建发布 CLI/DLL/VFS，Linux 和发布同源仍未实测。

### 2026-09-11：一次性运行复用模块执行/退休（6AEB38B9）

直接 `xlRunScript*` 的 xlCompileAndRunUnit 原来单独生成/重定位、调用
xlang_main，随后无条件 tcc_delete：它绕过 code-owner 绑定、checked main
成功位、全局清理及 resident retirement queue。现在删除该重复路径，复用
xlCompileUnitToModuleWithC，再由 xlRunAndRetireModuleUnlocked 消费模块。
入口持有 activeCalls；Destroy 先把拥有权交给退休队列，随后才归还入口引用，
此后不再访问模块句柄。pending continuation 或失败清理不会被裸删除跨越。

执行和参数清理沿用 xlRunModuleMainUnlocked 的同一 checked 合同；主函数
成功但清理 busy 仍返回失败并交给退休拥有者，不另行引入隐式等待或取消。
已有代码 owner 在初始化前绑定，模块 shell 保留运行时及依赖库引用。

统一路径的新测试还发现 xlCheckScriptActiveError 在存在 print hook 时只
打印并清除异常，未给宿主 context 保留错误。现在在打印前保存宿主拥有的
错误文本；复制过程中保留原始 xerror，避免分配失败污染待打印异常。已存在
的 context 错误优先，不被后续报告覆盖。分配失败仍可能无法取得错误文本，
不能据此把失败改为成功。

新门从真实生成 DLL 的已重定位状态进入上述源码执行/退休函数：七种场景
共 175 次实际卸载，覆盖 pending 正常返回、初始化失败、main 失败、ready
输入成功/取消、pending 取消及无 async 的成功退出码。源输入 ready 不表示
恢复内联执行，所以 ready 场景接受精确报告的 busy，而非假定立即成功；
无 async 场景独立要求成功且 exit code=42。检查入口已绑定且 activeCalls=1，
异步 finally 后才销毁全局对象，恢复仍能读取模块字符串，错误文本在 DLL
卸载后可读，并逐事务核验内存平衡。宿主不重复 Destroy 已消费句柄。

这证明共同源码生命周期路径，不替代实际 TCC 编译/重定位/页删除的联合门。
生产入口的静态接线也被检查，但测试未假装执行了整个编译前端。闭包/class/
native 身份、外部保留值语义、模块自持环、Linux/同源发布仍开放。

### 2026-09-11：真实 TCC 重定位/代码页退休联合验证

在上轮共同宿主测试中分离状态提供者，新增 actual-TCC 实现：使用 release
xl.dll 导出的真实 tcc_new/add_file/relocate/get_symbol/delete，注册已有
XRT/error 环境，并显式接入当前源码编译到宿主的 dispatcher/payload 服务。
输入仍由当前源码驱动生成且 C/H/JSON 两次一致；没有替换执行/退休逻辑。

one-shot 七场景 175 次、原 async 六场景 150 次，共 325 个实际执行事务
通过。每次先用 VirtualQuery 确认入口页已提交且可执行，再在真实 tcc_delete
后确认同一地址处于 MEM_FREE。async 模式 3 保留结果直到页释放之后才读取/
销毁，并继续检查返回的 drop 地址属于宿主。模块 finally、全局释放顺序、
绑定和入口引用、失败/取消及宿主错误文本与 DLL 提供者共享同一组断言。

首次所有场景曾在 XRT 内存平衡失败：不是直接忽略首轮，而是增加两个明确
的环境建立事务。第一次留下 16 字节，VisitLive 确认唯一活动分配内容为
`<static libtcc>`，对应已有 API 的常驻 TCC 路径记录；第二次完整编译/删除
分配精确平衡，随后每个执行事务也要求平衡。13 个场景进程各有两次 setup，
合计 26 次未执行代码的编译/删除，单列于 325 个执行事务之外。代码页检查
覆盖 setup 和执行；XRT 记账不等同于 TCC/CRT 全堆泄漏审计，不宣称进程最终
零常驻分配。原两组真实 DLL 门也完整复验通过。

同批补齐 Linux runtime/xlc VFS 中六种 runtime 私有头各一份（原 12 项缺失）。
新固定门验证四份 manifest 共 24 个头映射、两 DLL/Linux/embedded runtime
共 20 处源码清单，并确认文件存在。只证明接线，不证明 Linux 实际构建运行。

本次没有修改生产源码；源码仍为 6AEB38B9，v109 固化的是九个验证/打包输入。
这补足现用 TCC 快照的编译、重定位和页删除联合证据，不是从公开 xlRunScript*
开始的完整前端/编译上下文路径验收，也不是同源 release 重新发布。对象代码
身份、模块自持环、完整 async/网络门及 Linux/发布准入仍然开放。
