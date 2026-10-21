# TCP 原生拥有图前置

本批是 P3 真正传输接入的生命周期前置，不是 TCP／Dial／Server／UDP 标准库完成声明。
原生实现来自 XRT `5a12e8f3`、`184edd63`、`67f4f38b` 与 `40704d47`，只同步明确选定的网络段。
未同步 XRT 工作区的文件／内存库改动，未替换发布 DLL／VFS。

## 已落实

- Engine 的侵入式运行时槽真正接管构造时已有的 TCP 引用；摘除时消费同一槽，
  不增加第二份引用，不以计数器推测引用关系。
- Stream／Listener 对 Engine 的真实占用、Listener 接受队列每个位置的 Stream 引用、
  连接超时和接受重试计时器已有的独立引用全部入图。队列／哈希链接不是额外拥有边。
- Stream 的 Error／Cause 诊断链按真实引用导出；不可变错误 DAG 不会反向引用 Stream，
  因而和原生 Error 的不可变尾部一样保留到最后 Drop。静态错误不伪造图节点。
- 原生任务实际排队、活动 worker、构造发布、发送提交、Future 创建／取消等不稳定区间
  明确报告忙碌；不在 worker 仍执行时遍历可被其释放的队列或非原子字段。
- Hold／Drop、Claim／Restore 和 Clear 使用真实引用及唯一令牌。新准入受隔离，
  已有强拥有者不会被伪造引用或暗中解锁替代。
- 非等待 Prepare 使用嵌入式命令借用原运行时槽，不新建根引用；排队成功不是 Ready。
  在命令开始前保留实际运行时引用及必要 Engine 占用，发布失败在冻结内回滚。
  端口完成、计时器终态、缓冲归还、通知和槽摘除完成后才报告 Ready。
- 准备可能使普通关闭产生新的原生错误。验证程序重新冻结并发现、准入、保活新 Error
  节点，不沿用旧快照，也不忽略新增拥有边。
- 实际 Future 激活、生产者、取消 Watch、等待链及接收／接受结果已导入固定快照，并有
  独立驻留拥有域；只有显式选入该域，才向目录授予对应精确运行时／载荷合同。

## 低层接口与准入边界

首批低层入口共九个：

| 原生入口 | 作用 |
|---|---|
| `xrtNetEngineOwnershipAdapterV3` | 在原任务／计时器／线程退出策略之外，精确认证原生运行时族 |
| `xrtNetStreamOwnership`／`xrtNetListenerOwnership` | 借用物理拥有视图 |
| 两种 `RuntimeOwnershipV1Get` | 常驻、不可变的运行时槽与原有计时器合同 |
| 两种 `OwnershipQuiescenceV1Get` | 冻结下只读报告瞬时忙碌 |
| 两种 `OwnershipAdapterV1` | 实际保活、隔离与非等待准备合同 |

旧 Engine V1／V2 不会自动认可新的运行时族。当前语言 Engine 拥有域使用 V3 及目录中
显式授予的运行时合同；只选 Engine 时允许集仍为空，不提前承认 TCP 语言资源。九个入口进入快照／TCC 低层符号面，
不是新增九个面向用户的标准库方法；语言 Cgen 没有增加临时包装函数体。

当前 xlang 固定快照中的原生 V1 接受能够完整证明的无任意回调／无 Data、已认证读写缓冲、
已认证 Future 等待及结果。未知引用载荷、文件块、拨号控制器和回调上下文仍明确拒绝，不能套用叶子策略。
借用视图不是保活引用；任何协调器仍须遵守冻结、精确准入、真实 pin、重新证明不可达、
Clear 和最后 Drop 的完整顺序。普通 C 使用者仍须确保 Engine 销毁时外部使用者静止。

## 保留 xlang 固定 ABI

xlang 现有原生快照仍使用 `uint64` 微秒／`xdeadline`；上游 XRT 新的毫秒／实数期限
接口没有整库导入。原有时间字段、零／无限期限规则和转换代码保持原样。
公共 Net 新声明的上下文与上游期限签名相邻，故以精确唯一上下文单独插入，未放宽同步
工具的拒绝规则。TCP 两个相同回滚分支按冻结上游的精确出现次数同步。
其他修改使用现有具备旧字节检查的分段同步工具；Error 边也是独立的限定源段更新。

## 验证

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-ownership`。

- 上游最终 `windows-r5`／`linux-r5`：184／230 阶段。模块化／单头、GCC／TCC
  O0／O2，Linux 另含实际选定原生闭包 ASan／UBSan。保留原 Engine 拥有图、绑定、
  连接计时器、关闭回调与回环接受回归。
- 每次新图执行包含 12 个生命周期图、3 个原有计时器引用场景、6 个 Future 取消后
  重新准入、3 个显式 Error／Cause DAG、12 个策略身份拒绝及 8 个构造 OOM 前缀。
  两端 select 与各自 IOCP／epoll 均执行，全部分配账本平衡。
- `pinned-win-r2`／`pinned-linux-r2`：12／15 阶段。同一真实图夹具只转换 include 和
  微秒／`xdeadline` 调用，直接编译 xlang 当前单头的实际选定闭包，包含 Linux sanitizers。
- `source-win-r2`／`source-linux-r2`：实际完整源码驱动，722／721 个翻译单元；
  `language-win-r1`／`language-linux-r1` 继续检验既有配置、代理值、分帧值和 signal
  的语言／JIT／模块／严格 C／独立 AOT、合同拒绝、裁剪与拥有分配失败路径。

上述源码驱动和专项通过不等于重新签发 shipping DLL／VFS 验收。只使用离线图与
自建的字面回环、动态端口、普通连接／关闭，无公网、外部 DNS、扫描、畸形流量或
网络安全性质专项测试。早期 r1／r2／r4 发现记录保留，不能计成最终通过证据。

## 后续依赖

下一步仍是 Future／文件／回调与组合状态的完整原生传输拥有图、受管理 Stream／Listener／Dial／
Server／UDP、取消／期限／半关闭／背压、代理及文件／任务组合；随后继续非对称密钥、
X.509、TLS 和双平台联合验收。P3 与总长期任务保持 active。
标准库未决策分类没有变化；参见标准库决策清单，CRT 0 项、XRT 11 组。

## 2026-10-21 上游缓冲拥有图批次（历史阶段；导入见下节）

XRT 已提交 `67f4f38b08f1c118dcfb4866d55223459bd04c23`，本次没有修改 xlang
固定快照、编译器／Cgen、运行时拥有域或发布产物，不能把上游通过视为语言接入已完成。

- 每个原生池块持有真实池租约；租约释放覆盖全部回调和最后一次池访问。
  Engine 拥有 Worker 池创建者，首个租约保留一份真实 Engine 引用，最后归还时释放。
  独占块链不额外建立每块 RC、影子注册表或凭统计推算的引用，Move 仍是常数时间转移。
- 不可变原生网络字节具有实际 Ref／Destroy 拥有图；新增复制创建、保留片段追加与发送。
  读写缓冲枚举真实池租约；复制／Take／Bytes 发送帧枚举原有 Stream 与载荷引用。
  SendBuffer 多视图共同拥有原块链，唯一批次 Stream 引用只由首个尚存视图枚举一次。
- 新增低层公共入口八个，语言公共入口零个；未知 SendRef／SendRefs 上下文、文件块、
  Future 等待和组合控制器仍明确拒绝接纳。后续必须闭合其真实合同，不能套用叶子策略。

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-buffer-ownership`。

- 最终 `windows-r6`／`linux-r6` 分别 252／315 阶段全部通过，包含 GCC／TCC O0／O2、
  模块化／单头和 Linux 实际选定闭包 ASan／UBSan。独立最小 net_buffer 构建不依赖
  Atomic 或 Engine 功能，避免低层模块意外拉入并发／网络执行器实现。
- 每次 TCP 图夹具含 18 个生命周期图、6 种带 NUL 的正常二进制发送方式、原有计时器／
  Future 取消／Error DAG／策略拒绝，以及 8 个构造 OOM 前缀；离线夹具另验池租约、
  字节引用、释放回调重入、外部块保活 Engine、checked 退休与 4 个追加 OOM 前缀。
- Linux 的六种发送方式全部留下实际写队列并完成闭合回收。Windows 回环可立即复制
  至内核，实际留存数为零；只计入接收缓冲及完成释放验证，不冒充留存写队列的证明。
- 提交前重新校验输入、日志、产物摘要，并将 649 个候选输入逐项对应暂存区；未提交的
  XRT 文件／内存改动及其单头生成段保持原样。所有进程已结束，无公网／扫描／安全测试。

该上游批次完成时仍要求按精确命名段同步，保留 xlang 的微秒／`xdeadline` ABI，并补上真实
Pool／Bytes 拥有域准入。下节记录实际导入与验收；之后继续 Future／文件／组合传输、
托管 Stream／Listener／Dial／Server／UDP 及 P4—P7。
本批不改标准库未决策分类，总长期任务与 P3 继续 active。

## 2026-10-21 xlang 缓冲拥有域接入

本批将 `67f4f38b` 的两个公共、两个内部、五个源代码命名段以旧字节精确检查同步到四份
固定快照。未导入上游工作区的文件／内存改动，也未导入毫秒／实数期限 ABI；TCP 配置布局、
原有 `xdeadline` 和期限函数签名保持原样。新增原生公共入口八个，语言公开入口零个。

新增 `NetBufferNative` 驻留拥有域，明确接纳实际池引用和不可变网络字节；池的活跃工作用
独立 quiescence 能力报告 BUSY。复制 Ops、缺少目录能力或未知节点仍拒绝，不能自动当成叶子。
Engine 原生拥有域在组件依赖中明确包含该域；既有 Engine V2 仍不准入未知 TCP 运行时族。
独立域本身不依赖 Engine，未来传输组件可组合使用，不在 Cgen 中增加对象特判或包装函数体。

同时从实际展开的定义和最小模块闭包重新生成原生链接目录。共补登记 45 个已实现入口，
其中八个属于本批，37 个是此前并发、标量域、配置与原生 TCP 拥有权的遗漏；没有重写算法。
Windows／Linux 各自独立检查 3,731 个符号与 423 个候选模块根，得到一致目录。
两份未提交重排的构建响应模板只提交新增源文件这一行，保留其他格式改动。

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-buffer-import`。

- `source-win-r1`／`source-linux-r1`：723／722 个实际翻译单元，含新拥有域和当前原生实现。
- `pinned-win-r1`／`pinned-linux-r1`：12／15 阶段，同一 XRT 图夹具只适配 include 与微秒期限；
  原生引用、载荷、准备和 OOM 语义没有调整，包含 Linux 实际选定闭包 ASan／UBSan。
- `runtime-win-r2`／`runtime-linux-r2`：46／48 阶段，实际 Pool／Bytes 准入、释放回调忙碌、
  真实持有与隔离恢复、Engine 新池子边、多顺序清理及七种预期拒绝对照通过。
- `minimal-win-r1`／`minimal-linux-r1`：8／10 阶段。域仅需错误边界、拥有目录和域本身三个
  驻留源文件，不拉入 Engine；实际最小编译、池／字节回收及五个目录 OOM 前缀均通过。
- `engine-language-win-r1`／`engine-language-linux-r1`：各 73 阶段，覆盖 Engine、计时器循环、
  错误、未观察错误、JIT／模块／独立 AOT、严格 C、IR 合同和不含 TCP／Resolver 等无关模块的裁剪。
- `options-language-win-r1`／`options-language-linux-r1`：各 148 阶段，配置／代理／分帧／signal
  回归、合同拒绝及四种独立裁剪计划通过；每端 O0／O2 各 359 个分配失败前缀均平衡。

前期 `runtime-*-r1` 的运行检查虽通过，但链接目录／夹具仍在更新，输入冻结校验拒绝了该轮；
它们不计入最终通过证据。最终报告重新校验输入、产物和日志，并核对编译／镜像路径与暂存区；
保留用户的 IR 空行修改和所有无关工作区内容。仅原生夹具使用自建普通回环，其余均离线，
没有公网、扫描、攻击或网络安全性质测试，也没有重新签发 shipping DLL／SO／VFS 验收。

下一步继续闭合真实 Future 等待／结果／取消、文件和回调组合状态，再实现完整受管理传输；
本批不是 Stream／Listener／Dial／Server／UDP 语言标准库完成声明。P3 与总长期任务保持 active；
标准库未决策分类未变化，仍为 CRT 0 项、XRT 11 组。

## 2026-10-21：TCP Future 原生前置完成，尚未导入 xlang

XRT 独立提交 `40704d476a40fce643ee58d34d82f796d8e0410e`，父提交为 `67f4f38b`。
本节只记录上游原生实现和验收，不改变前文 xlang 固定快照／驻留拥有域的完成范围。

- 一个物理等待激活由源等待链、Future 生产者槽、取消 Watch.Data 各拥有一份真实引用。
  激活保留源对象、实际缓存分配器租约、Promise 和 Watch；Next 和取消链接不伪造拥有边。
- Stream／Listener 导出真实等待链；接收结果认证为不可变 NetBytes，接受结果认证为真实
  Stream。Future／Cancel／Watch 必须各自显式认可精确合同身份，不能复制描述符冒充能力。
- Prepare 不等待、不取消已受理工作；完整图中的源对象按正常关闭／通知推进，协调器重新
  冻结、发现及保活新节点。终态清空原生槽后才 Ready，生产者 Drop 只释放其真实引用。
- 普通完成沿用 64／128 字节小节点缓存；确有收集器引用保活终态节点时，立即归还 Engine
  缓存租约，留下不再拥有源／Promise／Watch 的外壳，避免 Engine 退役等待协调器销毁。
- 终态使用 release／acquire 原子发布；非最后一次 Drop 后不再读取节点，避免与别的线程
  最终释放竞争。旧缓存测试增加同 Worker 的 FIFO 屏障，不把 Future 终态误当生产者已返回。

最终证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-future-ownership`。

- `windows-r6/results.json`：244 阶段；`linux-r6/results.json`：305 阶段，全部通过。
- GCC／TCC、O0／O2、模块化／单头、select 与 Windows IOCP／Linux epoll；Linux 另验
  实际选定原生闭包的 ASan／UBSan。只使用离线检查和自建动态端口的普通字面回环。
- 图夹具每次覆盖 24 种终态／准备／清理顺序、24 次真实外部根保活、24 次三线程最终 Drop
  竞争和 16 个构造 OOM 前缀（六个实际失败点），均内存平衡；普通收发、Accept、四线程
  取消／关闭和接收结果分配失败恢复也通过。阶段数含编译，不等同独立测试数量。
- `native-verified-commit.json` 复核所有输入、日志和产物摘要，并证明 2,116 个候选路径与
  实际提交索引一致。XRT 用户的文件／内存改动未提交，生成单头中的原有改动完整保留。
- 前期 r4 含严格警告、响应文件转义和缓存检查时序问题，不作为最终通过证据；r5 全通过，
  r6 在同一核心实现上增加真实并发最终释放测试后重新冻结、统一验收并提交。

下一批先将这六个低层入口及明确选定的网络源码段导入 xlang，保持现有微秒／xdeadline ABI，
补充真实 TCP Future 驻留拥有域和精确策略准入，再继续文件载荷、回调与上层组合控制器。
未知 SendRef／SendRefs 上下文、文件块及 Dial／Server／UDP 的完整受管理传输仍未完成。
本批没有变更 xlang 标准库公开 API、Cgen、发布 DLL／SO／VFS，也未进行公网或网络安全专项测试。
P3 和总长期任务保持 active；CRT 0 项、XRT 11 组未决策分类不变。

## 2026-10-21：xlang 固定快照导入与驻留 TCP Future 准入

本批导入 XRT `40704d47` 的四个明确网络段：公共 `tcp.h`、内部 `xrt_tcp.h`、
`tcp_future.c`、`tcp_ownership.c`。提交检查从此前固定产品重新执行同一守卫差分，
不导入其他段、特性选择重排、文件／内存工作区修改或新的实数期限 ABI。
两字段字符串、微秒 `uint64`／`xdeadline` 及三个 TCP 配置结构保持现有合同。

新增六个低层原生能力进入两个快照、两份 SDK 声明及 TCC 符号表；Windows／Linux
分别独立预处理 3,737 个符号、423 个模块根，确认六个入口的实际 provider 均为
`XRT_MODULE_NET_TCP_FUTURE`，没有按名称猜测最小依赖。

驻留目录新增强类型 `NetRuntime` 能力组和 `NetTcpNative` 域。该域装配实际 Stream／Listener
运行时、源／激活静止检查、生产者、取消 Watch 和两种结果策略；源、Future、取消及 Pool／Bytes
仍由各自真实物理适配器负责，不新增计数、影子节点、擦除指针表或算法副本。
泛型 Engine 域升级为消费目录显式允许集的 V3：没有选入 TCP 域时，仍拒绝 TCP 运行时槽，
而且不链接 TCP／Future 域。只有该原生 TCP Future 闭包被选择时才引入 23 个必要驻留源文件；
不引入 Resolver、Executor 或 TaskPool。组件 ID 只追加，响应模板只提交新增源文件一行。

验证证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-future-import`。

| 验收 | Windows | Linux |
|---|---:|---:|
| 完整 SourceXrt 编译（真实翻译单元） | 724 | 723 |
| `future-*-r2`：固定 ABI 原生图＋实际驻留目录图 | 43 | 49 |
| `runtime-*-r1`：原有 Engine／Pool／Bytes 合同及拒绝对照 | 46 | 48 |
| `pinned-*-r1`：原有 TCP 拥有图回归 | 12 | 15 |
| `engine-language-*-r1`：JIT／模块／独立 AOT／严格 C／IR／裁剪 | 73 | 73 |
| `options-language-*-r1`：配置及代理／分帧／signal 回归 | 148 | 148 |

源码编译之外共 655 阶段，全部通过；阶段数包含构建和预期拒绝，不等同独立测试数量。
原生与驻留图每次各验证 24 种终态／准备／清理顺序、24 次外部根、24 次并发最终 Drop
及 16 个构造 OOM 前缀。驻留图只经真实目录查询，使用实际现代 Cancel 完成合同，且复制的
生产者能力仍被拒绝；每次另扫 32 个目录 OOM 前缀，10 个实际失败点全部回滚并保持内存平衡。
六种缺失运行时／生产者／取消／结果／激活合同的对照均以明确拒绝结束，不伪装成通过或忙碌重试。
Linux 同时覆盖实际选定闭包的 ASan／UBSan，两个后端均真实自建普通回环。
配置回归每端 O0／O2 各 359 个分配失败前缀保持平衡，并保留四种独立裁剪计划。

提交前复核全部输入、产物、日志和原生提交证书，并将实际编译／镜像路径与暂存区比较。
`future-*-r1` 同样通过；提交卫生检查清理夹具中的一处尾随空白后，重新冻结并验证受影响的专项 r2，
不重跑输入未变的完整源码和其他回归，也不把旧输入摘要当成最终源码证据。
保留用户 IR 空行、响应模板重排和其他无关改动。未更改语言公开 API、Cgen 或发布 DLL／SO／VFS，
没有公网、DNS、扫描、攻击模拟或网络安全专项测试。

下一步将这套实际拥有域用于受管理 Stream／Listener 的语言资源与 Future 结果投影，
再闭合文件／用户引用载荷、Dial／Server／UDP 组合状态和联合验收。本批不是完整 TCP
语言标准库完成声明；P3、其余已批准分类和总长期目标均保持 active，未决策分类未变化。

## 2026-10-21：托管 Stream／Listener 资源和私有 Accept 结果载体

驻留层新增 `xl6_execution_tcp.c` 及精确 `ExecutionTcp` 拥有域。Stream／Listener 是
既有 ExecutionSource 管辖的强类型资源，复用同一个退役控制线程，不为连接或等待创建线程。
具体结构以共有资源为首成员；原有 Executor／TaskPool／Engine／Resolver 的基础实例不增加
TCP 指针。内部 family 只增加纯槽位存在性查询，旧四个 family 使用各自真实字段。

- 每个传输资源拥有一份真实 Engine 语言资源引用。释放 Engine 原变量不会停止仍有子资源
  使用的引擎。私有 Accept 载体还拥有真实 Listener 语言引用，不能只靠原生句柄保活来掩盖
  Listener 语言层的提前自动关闭。所有槽位由其真实物理节点枚举，没有估算拥有边。
- 原生 callback／Data 保持空；原生指针只经精确 family 验证的同线程调用域借出，不跨 Leave
  保存。复用既有资源的独占控制域来完成结果提升，与关闭、退役和图收集互斥。
- 构造及类包装前的 `PrepareAccept` 只保留 Engine／Listener，不提交 Accept。这样构造失败
  或模块关闭拒绝发布时，回滚不可能遗弃已在执行的接受操作。`AcceptSource` 才创建唯一的
  原生 Future；重取只是增加同一 Future 的真实引用，不另排等待。成功提升只保留真实 Stream，
  然后归还 Future／Listener 槽。这个载体必须由后续语言包装保持私有，完成后才交给用户。
- 协作取消不伪造 Future 终态。放弃结果时，控制器请求取消，但保留实际 Future 到生产端
  真正完成；若 native unlink 已先取得连接而随后正常交付，先 Abort 未交付的连接再归还引用。
  不用“反正 Engine 最终会关闭”掩盖仍在运行的 Engine 上的孤立连接。
- 显式 Close 遵循原生优雅关闭语义；丢失最后一个拥有者则中止剩余工作。需要保证发送交付
  的调用者必须显式等待相应完成边界，不能把析构当隐式无限刷新。清理仍走可检查的共有退役
  机制，Busy 保留资源，失败保留原错误和资源，不在释放路径自动吞掉失败或直接 abort。
- 配置按值复制且保留调用者选定的 ReadMode；不为了适配新接口偷偷强制改变读取模式。
  已有状态、端点、半关闭、暂停／恢复和独立统计快照底座通过精确调用域复用原生 API。

新增组件只追加 ID；选择 TCP 资源时明确选入其真实原生 Future／源拥有域。只用 Engine、
原生 TCP 或其他资源不会拉入 TCP 托管实现；Resolver／TaskPool 也不是 TCP 资源的隐式依赖。
本批不修改 IR、Cgen 或标准库公开类，后续公开 Future 结果投影复用既有私有类载体机制和
固定工作线程的队列延续，不把语言对象的最终释放放进 I/O Worker 的内联回调。

同时导入 XRT `a68481ce` 的一个明确 `tcp_future.c` 源码段：生产者拒绝 Accept 结果发布时，
先 Abort 未交付连接再 Destroy。它不是“请求取消必定使 Promise 取消”的语义变更。
两份固定产品的声明、微秒期限、TCP 配置布局和原生导出集合均不变。
上游以干净父提交加三份明确任务文件生成候选；文件／内存等用户工作区改动未被混入。

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-managed`。

- 上游 `native-win-r2`／`native-linux-r2` 共 244／305 阶段。移除拒绝交付时 Abort 的独立
  对照按预期在孤立连接检查处失败。r1 的失败来自测试误把协作请求当终态，已修正该假设；
  保留发现证据，不将 r1 列作最终验收，也未以更改取消语义迁就测试。
- `resource-win-r1`／`resource-linux-r1` 共 31／34 阶段：GCC／TCC、O0／O2、select 与
  IOCP／epoll，Linux 含真实选定闭包 ASan／UBSan；每次执行 24 种图清理、三种普通退役、
  未交付结果与取消后仍 Pending 的真实完成边界，以及 80 个构造分配预算（12 个实际失败）。
  字节收发包含 NUL，所有健康路径内存平衡。阶段数包含构建和预期拒绝，不等同独立测试数。
- 缺少 Stream family、缺少 Listener family、过早遗弃 Pending Accept 的三个独立对照，
  均在真实目录／生命周期断言处被明确检出。没有用假计数、模拟传输或替代策略绕过拒绝。

完整收口验证如下，均已通过；公开语言项是原有 Engine／配置回归，不是新 TCP 公开类验收。

| 验收 | Windows | Linux |
|---|---:|---:|
| SourceXrt 完整源码构建（翻译单元；Windows 按摘要复用未变对象） | 726 | 725 |
| `resource-*-r1`：新增托管 TCP 资源／真实 Accept 载体 | 31 | 34 |
| `execution-*-r2`：共有 Source／Executor／TaskPool、13 项对照 | 84 | 86 |
| `engine-runtime-*-r2`：共有 Engine／Pool 拥有域及拒绝对照 | 46 | 48 |
| `resolver-runtime-*-r1`：数字地址及离线 Resolver 生命周期 | 76 | 88 |
| `future-*-r1`：固定 ABI 原生图及真实目录 Future 图 | 43 | 49 |
| `engine-language-win-r1`／`engine-language-linux-r2`：JIT／模块／独立 AOT | 73 | 73 |
| `options-language-*-r1`：TCP 配置／代理／分帧／signal | 148 | 148 |

源码构建之外共 1,027 阶段，包含预期拒绝；配置每端 O0／O2 各 359 个 OOM 前缀均平衡。
独立链接探测仍为每端 3,737 个原生符号／423 个根，公开声明和链接目录未变化。
共有回归的 r1 在最后清单检查处检出 Linux 发布清单缺少四个网络 provider（本批两个、
前批两个）。补齐 `build_linux.sh` 后重跑受影响的共有执行资源和 Engine r2；各端 runtime／
compiler 共四份清单均恰好覆盖 682 个 provider，无缺失或重复。其余验收输入摘要未变，
不重跑已通过的源码构建／专项，也不覆盖原始失败报告。这仍只是发布源码清单证明，
没有把它当成新发布 DLL／SO 的二进制验收。

仍未完成：公开 Stream／Listener Future 结果投影和语言类迁移；文件／引用载荷、Dial／Server／
UDP 及后续 P4—P7。发布 DLL／SO／VFS 未更新，P3 和总长期任务保持 active。全部验证只用
离线检查与普通自建字面回环，不做公网、DNS、扫描、攻击模拟或网络安全专项测试。

公开投影还必须闭合队列延续的独占源取消路由：现有 V5 Start 是共享源语义，不能直接
当成原生操作的独占 Start，否则输出的取消请求可能无法传到仍 Pending 的 Accept。
后续应以显式版本化合同复用既有独占生产协议，不改变普通 Future 延续的共享语义，
也不以 I/O Worker 内联释放语言对象或每次等待新建线程来绕过这一点。
