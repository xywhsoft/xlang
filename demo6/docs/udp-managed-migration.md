# UDP 驻留接入进度与依赖

这是既有 P3 范围的实现记录，不替换或缩小完整 UDP／网络目标。

## 已完成：原生结果拥有权前置

XRT 提交 `d0bf1863459628c5d4419f8df25c134071f20ab4` 新增六个入口：

| 对象 | 原生拥有关系与适配 |
| --- | --- |
| Packet | `xrtNetUdpPacketOwnership`、`xrtNetUdpPacketOwnershipAdapterV1` |
| ErrorPacket | `xrtNetUdpErrorPacketOwnership`、`xrtNetUdpErrorPacketOwnershipAdapterV1` |
| Batch | `xrtNetUdpBatchOwnership`、`xrtNetUdpBatchOwnershipAdapterV1` |

普通包和错误包的数据／元数据独立拥有，不借用生产者 UDP、Engine 或 Future。
Next 只是队列结构，不是包对后继的拥有边。Batch 为每个未取走槽持有实际包；
Take 后的空槽不再枚举。没有伪造引用、复制包包装或猜测用户回调身份。

Ref／Destroy／Take 纳入统一 mutation。BatchTake 使用原子交换，不分配、不复制
载荷；并发取同一槽只交付一次。Count 保留原索引范围，空槽返回 NULL。借用
BatchPacket 仍须排除同槽 Take 并保证批次存活，原子读取不等于取得拥有权。
Freeze 排除其他线程的修改；持有线程可做受控且平衡的嵌套游标持有。

## 验收证据

证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-values`，提交封条为
`native-verified-commit.json`。共 34 个阶段：Windows 16、Linux 18，包含单头
GCC／TCC O0／O2、Linux ASan／UBSan，以及各自最小模块的严格独立源码编译。

每个普通运行进程验证 48 次图关系、9 个离线分配预算、64 次并发 Take（8 次唯一
交付）、8192 次冻结图检查；真实四包 Future 批次含 NUL／零长度，检查空槽、
空批次、单包／批量接收取消、零分配接管及生产者销毁后的载荷存活。错误包只用
实际内部构造器验证，无 ICMP／错误队列探测。漏批次边、漏 Take mutation 的两个
独立对照均成功编译，并被真实图断言拒绝。输入／日志／产物哈希提交前重新校验。

本批仅修改独立 XRT 的 UDP 结果代码和相应生成段；未同步语言固定头，未修改
Cgen、公开标准库或发布 DLL／SO／VFS，不冒称整个 UDP 接入通过。

## 已完成：基础 UDP 对象图及关闭尾部

XRT 提交 `a6980d1b2230de5a50b044272d2a6cb2c126a534` 增加 UDP 的 Ownership、
Quiescence、Adapter 与 RuntimeOwnership 四个入口。Engine 的真实运行时槽与 UDP
相互持有，UDP 继续枚举池租约、普通／错误包队列、复制／Take 发送、completion
记录和不可变诊断；不把 Active、唯一分配记录或借用 completion 指针算成额外 RC。
来源标签由构造入口决定，外部释放回调不用于推断拥有含义。

短元数据门覆盖真实调用／Worker 帧尾部；operational LiveObjects 与关闭后的物理
存储保活分离，保持 CLOSED 后可以销毁 Engine 的既有合同。关闭尾部可在 Worker
已退役后解绑，无需向失效 Worker 补发命令。Claim 后隔离新工作，Prepare／Ready／
Clear 分阶段推进，无分配的生命周期投递只借用原本存在的 runtime 引用。

本次 86 个阶段（Windows 41、Linux 45）包括基础图完整退役、Creator／计划 pin
精确计数、仍有原生调用时销毁 Engine、回调重入、普通与错误包漏边拒绝、来源
标签、16 个构造预算、结果对象回归、两端最小模块及六个 TU 的严格独立编译。
错误队列只做真实未发布构造器／队列的纯内存检查；IOCP 不支持 DGRAM_ERROR 时
核对 checked Unsupported，不制造网络错误。证据根是
`F:/xlang-validation/five-groups-20261022/p3-udp-owner`，封条 `native-verified-commit.json`。

基础 V1 明确拒绝尚无已注册图策略的 Future 等待者、未知事件／Data 和裸 socket
逃逸；这不是“已经完整接入所有 UDP 异步能力”的声明。语言固定头和 Cgen 尚未
同步，新标准库还不能使用本批接口；没有重建发布库或 VFS。

## 已完成：原生 UDP Future 拥有图

XRT 提交 `34d4721539a9b110017f2fb288c5b092cab2c2d4` 补齐等待者真实 RC、Future
producer、owned Cancel Watch、Packet／ErrorPacket／Batch payload，以及静止检查和
分阶段退役的七个策略入口。UDP 的三类真实等待链现在枚举原来源槽；等待者枚举
实际 UDP、缓存 Engine 租约、Promise、Watch 和预分配空 Batch，不制造 Active 或
结构链接的拥有边。固定策略身份需要独立准入，副本不能代替注册合同。

唯一摘链胜者完成交付；Unwatch 同步之后才移除来源持有，结果拒收也正确释放。
正常 Worker 接收仍复用原小节点缓存；关闭来源或外线程完成／取消在发布 Future
终态之前先分离 operational 缓存租约，保留“完成后可销毁 Engine”的既有合同。
迟到的关闭命令、readiness 和发送挂接的既有引用贯穿帧尾，不在活动帧内释放最后
引用。节点保持 128 字节以内，未引入逐等待线程或协程栈。

最终 161 个阶段（Windows 75、Linux 86）全部通过，覆盖 Select／IOCP／epoll、
GCC／TCC O0／O2、Linux ASan／UBSan、精确整图及外部根、24 种拥有准备／完成／
释放组合、真实竞争 pin、36 个构造预算、48 轮取消／交付／关闭、模式冲突、拒收、
三种结果的离线拥有合同、晚建 Future 和唯一引用的迟到命令。基础图、包／批次和
五个 TU 的最小模块独立编译一并回归。四个新漏边／提前释放对照均成功编译且被
拒绝；提交前复核输入、日志和产物哈希。

错误等待仅使用真实未发布构造器及纯内存队列；未发布 START 状态仍被整图认证
拒绝，不伪造已启动对象，不进行 OS 错误队列操作。没有运行含 ICMP 测试的旧通用
UDP Future／Sync 夹具，没有外网或网络安全性质专项。证据根为
`F:/xlang-validation/five-groups-20261022/p3-udp-future`，封条 `native-verified-commit.json`。

本批没有同步语言固定头，没有迁移 xlang 驻留 UDP 或旧 Cgen，也没有提升发布
DLL／SO／VFS；不能把原生前置通过算作整个 UDP 或五组长期任务已经完成。

## 已完成：原生 typed 引用、聚集与混合批量发送

XRT `b893607f81cceadf52010610aa95781fbc4bf59f` 增加 `SendMsgBytes`、`SendMsgPacket`、
`SendMsgBytesVec`、`SendOwnedBatch` 四个入口。前两项支持严格的不可变字节片段；
向量经真实 Socket／Port 聚集为单个数据报，不拼接复制载荷；混合批次用明确
Kind／联合体选择 Bytes／Packet／Vector，所有者及元数据在调用返回前落实。
空片段不增加载荷持有，但完整空输入仍发送一个空数据报，不是空操作。

每个非空片段拥有真实引用，重复 owner 保留重复边。前缀构造只投递一次；后项
失败可以返回 ERROR／AGAIN 加非零 accepted，投递失败则 accepted=0 且内部持有
全部回滚，调用方原引用始终不转移。来源使用明确标签并验证真实槽，不由用户
释放回调反推所有权。Vector 的 Count／Capacity、Span 和 owner 数组仅占节点
尾部，控制记录之后按真实对齐补齐；普通节点 x64 sizeof=104、Tail=96，与父版本相同。

最终 212 个阶段（Windows 99、Linux 113）通过：Select／IOCP／epoll、GCC／TCC
O0／O2、Linux ASan／UBSan、静止整图与外部 payload 根、重复拥有边、8 类图、
9 类正常收发、17 轮前缀／15 个分配预算、24 轮 Close 竞态；均为每个测试产品的
实际计数，不拿阶段数冒充语言端用例数。三种漏载荷边及漏投递失败回滚的独立
对照都成功编译且被拒绝；基础／Future、TCP 字节向量、六个最小 TU 和两种独立
声明根一并回归。共享 xnetbytesref 移到 net.h，TCP 类型与布局不变，不给 UDP
引入 TCP 实现依赖。输入、日志及产物哈希在提交前再次核对。

旧 Future 夹具的未发布构造器回滚先与真实内嵌等待命令汇合，不放宽生产静止
断言。没有错误队列、ICMP、外网或网络安全专项；证据根为
`F:/xlang-validation/five-groups-20261022/p3-udp-refs`。没有同步语言固定头、迁移
驻留 UDP 或旧 Cgen，没有重建发布库／VFS；完整 UDP 与五组目标仍未完成。

## 已完成：固定快照与驻留 Execution UDP 基础

只同步 `net.h`／`tcp.h`／`udp.h` 与 Engine、UDP 的必要内部／源码段，保持固定
微秒时间 ABI、标量域合同及其他未选择差异。两套实现头、两套声明头、快照元数据
和 TCC 符号面一致增加 21 个入口：XRT 3342、xruntime 3807（自有 465）。链接归属
目录从冻结上游重新预处理，3807 个符号均有实际定义和模块归属，不按名称猜测。

新增永久驻留 `ExecutionUdp` family；具体记录只持有实际 UDP 与 Engine 父引用，
不在通用 Resource 中增加不透明载荷。Source 负责 last-owner 自动退役和 checked
重试，显式 Close 不消耗语言 owner。每次原生调用保留物理 receiver scope；
发布失败回滚不把 resident 指针交给原生 callback。图 Finish 只消耗已准备／清除
子节点的真实 creator 和 parent，不再次调用已清除对象的 Abort／Close。

BaseNative 精确登记 Socket／Packet／ErrorPacket 与 runtime／静止策略；Future
域独立登记等待者、producer、owned Cancel Watch、三种 payload 和 Batch。复制、
Take、不可变字节／Packet 引用、字节向量和两种批量发送均直接复用 XRT。批量在
作用域准入失败时也保证 accepted=0；Take 失败不转移。原始 Future 的调用者必须
独立持有 receiver，后续语言 continuation 仍须捕获它并进行 typed 结果投影，
不得把原生 Packet／Batch 指针当成语言类指针。

基础 closure 不选择 UDP Future，异步 closure 才加入相应策略；两者均不引入 TCP、
Resolver、TaskPool 或语言 Future 回调队列。构建响应模板、公开源码驱动和精确
symbol／domain 计划已同步，原有模板无关改动未纳入本批。

最终 221 个阶段（Windows 106、Linux 115）通过，包括 GCC／TCC O0／O2，
Select／IOCP／epoll，Linux ASan／UBSan、真实图的 8 种准备／完成／释放顺序、
普通退役、待收单包／批次／Close Future、取消与完成结果、关闭后拒绝发送、
调用中释放最后 owner、Packet 越过 Engine 的生命周期、24／48 个构造预算，
以及 TCP 基础整图与双声明根标量域回归。两种漏 family／漏 parent 边对照成功
编译并被拒绝，TCP 的三种原负对照也继续拒绝。这里的阶段包含编译／计划检查，
不等于语言用例数量；最终报告保持输入、日志及已记录产物哈希。

证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-resident`，封条
`verified-commit.json`。没有 OS 错误队列／ICMP、外网或网络安全专项，没有提升
发布库／VFS。**本批只是驻留资源服务，不是公开 UDP 标准库迁移完成。**

## 已完成：公开不可变 UdpConfig

原生 `4b82b0cfcc802fbef45b59d7c69ba56b0825fb1c` 将既有校验公开为
`xrtNetUdpConfigValid`，`Open` 与语言 builder 共用唯一实现。有效输入不分配，
错误诊断可分配；不创建 Engine／Socket、不触及 I/O。通过只代表结构合法，
不代表平台支持已请求选项。固定快照同步新增 1 个入口，XRT 3343／xruntime 3808。

`UdpConfig()`／`udp.config()` 现在创建独立不可变配置；私有 native 字段不能被
用户读取、设置或用 point 构造。移除旧 host／port 构造、原地 mutator 和配置
对象的旧 `bind()`，不保留兼容包装。地址／Engine 属于后续受管工厂，而非配置。
因此本批不宣称旧 UDP Socket 已能使用这份新配置，工厂接通仍在下一阶段。

全部 28 个原生字段有直接 getter，使用 `pure + no-raise + nonblocking`；12 类
`with…` 返回新的独立值，创建／复制失败使用 checked 错误，不能误标 no-raise。

| Builder | 覆盖配置 |
| --- | --- |
| `withReceive(size, concurrency, batch)` | 接收大小 1—65535、并发 1—64、readiness 每轮 1—1024 包 |
| `withReceiveQueue(packets, bytes, overflow)` | 接收队列双上限与 UdpOverflow |
| `withReceiveMetadata(flags)`／`withTruncation(mode)` | 请求 NetDatagramMetadata、UdpTruncation |
| `withSendQueue(high, low, limit, packets)`／`withSendConcurrency(count)` | 发送水位、硬上限、包数及 1—64 并发 |
| `withSocketBuffers(receive, send)`／`withTraffic(hopLimit, trafficClass)` | 原生整数宽度、-1 系统默认、跳数及业务类别 |
| `withPathMtuMode(mode)` | 既有 NetPathMtuMode；不另造枚举 |
| `withBinding(reuseAddress, reusePort, exclusiveAddress, ipv6Only)`／`withBroadcast(enabled)` | 绑定组合与广播；拒绝独占／复用冲突 |
| `withErrorQueue(enabled, size, packets, bytes, overflow)` | 显式错误接收、捕获前缀、独立队列和策略 |

`UdpOverflow` 为 dropNewest／dropOldest／error，`UdpTruncation` 为 deliver／drop／
error；不同枚举不能混用。接收／错误队列的任一上限为零表示不允许排队，等待中的
消费 Future 仍能直接接收；不擅自把零改成默认值。`receiveBatch` 是 readiness
轮次上限，不是异步结果 Batch 的容量。所有 uint64→size_t／uint32、int64→int／
enum 缩窄先检查，返回项覆盖 flags、策略及全部 bool，不把结构体当整数数组。

驻留配置拥有唯一 leaf policy；不持有 Source／Engine／用户 Data。真实 artifact
计划验证配置不选择 Execution UDP、TCP、Resolver 或 UDP Future，单独用枚举不
选择 NET_UDP。字段词表作为 SDK `.h` 随两端源码与独立 AOT 分发，消除仅在宿主
include 路径可用的依赖。Cgen 只调用登记函数并管理所有权，不生成配置实现体。

最终 224 个检查阶段（原生 44、语言运行时 32、语言程序 148）通过；完整源码
编译的 TU 数单独记录，不拿编译次数冒充语言测试数。验收涵盖两端 GCC／TCC
O0／O2、Linux ASan／UBSan、28 个 getter、12 类 builder、
18 组无效参数和 36 个构造分配预算；完整源码驱动分别编译 799／798 个 TU。
真实 xlang 的普通／最小／枚举／拥有返回／错误程序接受 JIT、module、独立 AOT、
typed／lowered IR 与裁剪检查，AOT 不依赖 xl／xlc 共享库。42 个入口的 10 个枚举
位置、13 个拥有结果及 375 个被破坏的合同在 O0／O2 拒绝生成 C／声明头；拥有
程序在每端／每优化级别各遍历 59 个实际分配前缀到首个未命中，检查每轮模块清理。

证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-values-language`，封条
`native-verified-commit.json`／`verified-commit.json`。只做离线配置／ABI／内存
验收；没有错误队列、ICMP、外网或安全专项，没有更新发布库／VFS。

## 已接入：受管工厂、拥有结果与同步数据通路（2026-10-22）

`UdpSocket` 的唯一载荷是驻留 `ExecutionUdp`，持有真实 Source／Engine parent；
不是裸 Socket 或借用地址。`udp.bind/connect/open` 与同名类构造／静态方法接入
不可变配置、`NetAddress` 和完整 uint64 affinity。Open 同时支持指定 local 与 peer，
不经过隐式全局 Engine／Resolver。没有 point 构造或公开 nativeHandle。

| 公开面 | 已实现的语义 |
| --- | --- |
| `close/abort/state/connected/sendControlAvailable` | checked 物理 receiver scope；Close 不消耗语言引用 |
| `localAddress/peerAddress/snapshot` | 独立地址／不可变 34 字段统计，不借用 Socket 内存 |
| `send/sendTo/sendMessage` | string／bytes／Buffer 复制，`size=-1` 全长，超长截断，小于 -1 拒绝 |
| `sendRef/sendRefTo/sendRefMessage` | NetBytes／UdpPacket 的原生拥有引用，size／offset 检查后发送，不复制载荷 |
| `sendMove/sendMoveTo/sendMoveMessage` | Buffer 仅接受成功后清空；失败保留原对象；空 Buffer 仍发送一个真实空数据报 |
| `tryRecv/tryRecvError` | 返回可空拥有对象；无数据与零字节数据报不同；禁用错误捕获仍为 checked 配置错误 |
| `wait/waitWritable/recv/recvError/recvBatch` | 同步期限及 CancellationToken 变体；共享微秒 deadline，不逐调用建线程／协程栈 |

同步 wait 返回精确 NetStatus；recv 的超时、取消及资源错误抛 checked 错误，不
生成空 Packet 冒充成功。正常 Close 的 close wait 返回 ok；Abort 的取消终态保持
cancelled。资源已经进入退役时的新发送拒绝为 checked StateError。

Packet／ErrorPacket 的句柄拥有实际 XRT 结果，Trace 指向真实 native 节点；不能
当 leaf 或从默认构造伪造。`bytes(size,offset)` 保留 NUL 和精确长度；地址及信息
返回独立快照。Batch 只公开原生原子的 `take`，不暴露存在并发 Take 竞态的借用
Get+Ref。count 保持原索引空间；重复提取合法槽返回 null，越界 checked 拒绝。
独立结果的拥有权域不依赖 Source／Engine／Buffer；Socket、同步等待与 native
Future 的闭包分别登记。所有 policy 均核对真实 Ops 与 Trace 身份。

适配代码全部在 resident runtime；XLIR 登记物理 ABI、名义 owner、效果／错误、
可空结果、状态枚举和 ExecutionSource 服务。旧 UDP Cgen 函数体及裸 point 辅助
退出；剩余旧 Engine snapshot 生成器不属于本批 UDP API。新字段词表使用可随
SDK 分发的 `.h`，不依赖驻留实现侧的 `.inc` 搜索路径。

验收入口为 `check_stdlib_udp_language_runtime.py`、
`check_stdlib_udp_language_source.py` 与原有 UDP resource 回归；新语言夹具位于
`test/cases/stdlib_udp_managed.xl`、`stdlib_udp_batch_cancel.xl`、
`stdlib_udp_surface.xl`、`stdlib_udp_enum_only.xl`。旧全局网络 fixture／suite 和
发布库不能视为新版 UDP 已通过验收，后续联合门禁仍需迁移。证据根为
`F:/xlang-validation/five-groups-20261022/p3-udp-language`。

最终选择的验收轮次为 Windows runtime-r6／source-accept-r3、Linux runtime-r3／
source-accept-r3，以及两端 resource-regression-r1。合计 279 个检查阶段，包含
GCC／TCC O0／O2、Linux ASan／UBSan、select／IOCP／epoll、真实图八种清理顺序、
离线分配失败预算、四个语言程序的 JIT／module／独立 AOT／IR／裁剪和 11 类
错误声明拒绝。113 个入口的 39 个枚举位置、31 个拥有结果接受 1247 种严格合同
变异检查，每个入口另做一次完整模块及 C／声明头拒绝验证。阶段含构建和负例，
不是 279 个独立语言程序；源码 TU 数也不计入该阶段数。AOT 不导入 xl／xlc 库。

## 已接入：受管异步结果、取消与期限（2026-10-22）

`recvAsync`／`recvErrorAsync`／`recvBatchAsync` 返回实际拥有的 Packet／ErrorPacket／
Batch Future；`waitAsync`／`waitWritableAsync` 返回 `future<bool>`。五族均提供普通、
Cancel、Until、UntilCancel、Timeout、TimeoutCancel 六种形式，共 30 个驻留入口。
接收沿用 Future 的 resolved／failed／cancelled／closed 四态；bool 等待在 resolved
时返回 true，正常 closed 返回 false，取消仍是 cancelled，超时仍是 checked 错误。
Batch 容量为 1—256；长度、期限和枚举在提交前检查。预取消／已过期时不向原生
接收队列提交操作，不因等待而新建系统线程或协程栈。

类结果使用通用且封闭的 `CLASS_RESULT_V1` IR 合同：严格认证 Future<T>、唯一私有
native 字段、名义 owner、reference identity、载荷 ABI、服务与效果。不允许自定义
析构、继承或持久化逻辑夹入这条专用结果通路。Cgen 只输出不可变 schema 数据、
精确 policy getter 声明、隐藏 context、调用与拥有清理，不生成 factory／mapper。
导出函数可以仅引用异步接口，不能依赖存在同步构造指令才能声明 policy getter。

等待期间只有真实 receiver、复制的字段名、受帧 CodeLease 保护的静态 schema；
不预造假 native Packet，也不把未提交类构造器放入拥有图。仅在原生成功后同步
建立类、接管实际结果引用、提交并发布。通用 `xl6ClassCreateFromLeaseV3` 从仍存活
的帧租约派生结果租约，宿主已禁止新 root acquire 时也能完成已受理的操作；既有
ClassRuntimeV3 表、对象 lifetime 布局和 XRT 构造器收集规则均不改变。

源 Future 使用独占取消传递，receiver 的 private handle Trace 指向真实拥有图。
回调先完成捕获清理再发布，失败保留首个错误；丢失最后语言引用和模块退出均走
checked 收集／退役。原生同步停止不承诺等待语言拥有者的异步退役；最终退出用
`engine.close()` 后 checked `engine.wait(timeout)`，不用 sleep 或无条件重试掩盖竞态。

验收入口为 `check_stdlib_udp_async_runtime.py` 与 `check_stdlib_udp_async_source.py`，
证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-async`。六个新增语言程序覆盖
实际结果、取消／期限、父对象失去变量、导出函数全表面、悬空等待的模块清理和
128 个同时待接收结果；同时保留同步数据通路与 enum-only 裁剪回归。原生用实际
离线 ErrorPacket，不提交 OS 错误接收操作；检查 7 类整图的 8 种清理顺序、完整
构造分配前缀以及宿主 Close 后的已有租约派生。正式通过轮次与哈希以提交回执为准。
完整源码驱动为 Windows 817／Linux 816 TU；这不是发布 DLL／SO／VFS 验收。

最终验收发现并修复共享队列的通知中间态问题：Committed+Ready（尚未释放通知
尾部）和 Committed+TransportReleased（等待 Ready）均有完整真实拥有引用，不能
误报图损坏。队列、旧 ticket、语言 activation 与 resident callback frame 共用
同一可检查状态谓词；运行中、发布中、前后继 handoff 及未知 flag 仍严格拒绝。
原生夹具在实际 Watch Notify 内、释放 transport credit 前确定性读取队列／快照，
反向事件顺序另验；不改计数或添加假根。pending 程序的独立 AOT 另做 32 次短进程
退出回归，重复运行次数单列，不冒充新增语言程序或构建次数。

最终选用 Windows runtime-win-r7／source-win-r4／source-accept-win-r4 与 Linux
runtime-linux-r6／source-linux-r4／source-accept-linux-r4，以及两端 shared-regression-r1。
输入、日志和已记录产物 SHA 全部重新核验。合计 462 阶段（Windows 224／Linux 238）：
UDP 原生 31／36、语言源码各 149、共享 TCP／回调回归 44／53。阶段含构建、拒绝
控制及每端 32 次重复退出，不是 462 个独立语言用例；817／816 TU 编译数另计。
30 个异步入口、6 个枚举位置和 18 个拥有结果接受 368 种坏合同检查；每个入口
还做完整模块及 C／头拒绝输出。旧队列反证成功编译后在实际 Notify 中被新夹具
确定性拒绝，说明测试不依靠碰巧撞到时序。提交封条为证据根的 `verified-commit.json`。

## 仍必须依次完成

1. 补齐受管向量／批量、引用／接管、元数据、能力协商、多播和普通错误通路；
   同步和异步接口都复用 XRT。异步等待使用固定工作线程／就绪队列，不逐等待建立
   线程或协程栈，不因封装而遗漏已批准的底层能力。
2. 新增接口持续保持 Cgen 仅声明／调用／拥有清理；失效的 `__xrt_udp.bind` 静态依赖
   和旧 UDP mapper 注释已清理。继续迁移旧全局网络
   fixture、suite 与文档，不能把旧发布驱动的通过状态套用到新接口。
3. 双端源码 JIT／module／独立 AOT／IR／裁剪／生命周期验收，最终再做发布联合
   验收。检查仅使用离线合同／固定数据及自有字面量回环正常通信；网络安全性质
   专项由用户在 XRT 层完成，不在此处扩大测试范围。

完整 UDP 仍开放，之后继续网络文件／任务组合、剩余并发、非对称密钥、X.509、
TLS 和发布验收。未决策 CRT 0／XRT 11，对象池最后不变。
