# TLS 恢复票据

`TlsResume` 是受语言生命周期管理的不可变恢复凭据，复用 XRT 的深拷贝、引用计数和最终清零实现。它不是连接，也不会自行发起网络操作。

本阶段只完成凭据对象：角色配置、服务端类型化查找回调、Session/传输层签发与接收仍待接入。不能把下面的对象验收等同于 TLS 恢复握手或发布封板已完成。

这是凭据对象批次的历史边界。后续服务端类型化查找已接入，见下文；客户端恢复配置、票据签发/接收和传输 KeyUpdate 仍未完成。

## 公开合同

高级恢复入口为：

```xlang
TlsResume.fromParts(TlsVersion version, TlsCipherSuite cipher,
    bytes ticket, bytes secret, time issuedAt, int lifetimeSeconds,
    uint64 ageAdd = 0, string serverName = "", bytes protocol = b"",
    bytes peerIdentity = b"", uint64 maxEarlyData = 0) as TlsResume
```

该入口用于恢复调用者已有的完整凭据，不产生随机票据或 PSK，也不完成认证。全部视图在返回前独立深拷贝；凭据输入不做前缀截断，没有裸指针构造、`nativeHandle()` 或可写字段。

数值缩窄前检查范围；XRT 继续负责 TLS 版本、密码套件、完整 PSK 长度、票据长度、生命周期和过期时刻溢出等验证。`ageAdd` 和 `maxEarlyData` 的语言载体是 `uint64`，有效值为 uint32 范围；后者仅保存原生元数据，本阶段不提供 0-RTT 读写能力。

只读属性：

| 属性 | 类型 | 单位/含义 |
| --- | --- | --- |
| version / cipher | TlsVersion / TlsCipherSuite | 精确枚举 |
| ticket / protocol / peerIdentity | bytes | 完整拥有副本，包含 NUL 和任意字节 |
| serverName | string | 拥有副本；原生 SNI 合同禁止内嵌 NUL |
| lifetimeSeconds | int | 秒 |
| ageAdd / maxEarlyData | uint64 | 原生 uint32 元数据 |
| issuedAt / expiresAt | time | xlang 固定快照的微秒时间 ABI |

`exportSecret()` 显式返回 PSK 的独立 `bytes` 副本。该副本与其他普通 `bytes` 一样管理生命周期，不承诺自动安全擦除；应用应避免日志、无保护持久化或无必要的副本。原生凭据自身在最后一个引用释放时仍由 XRT 清零。

`validAt(time)` 在半开区间 `[issuedAt, expiresAt)` 内返回 true；时钟早于签发时刻或已过期返回 false，不产生错误。`ticketAge(time)` 返回 TLS 1.3 线路混淆年龄，单位固定为毫秒、按 uint32 模加；无有效年龄时 checked 报状态错误，不把数值 0 当失败哨兵。

## 编译与拥有权

声明使用现行的驻留 direct-bridge 合同：16 个精确签名、枚举、时间、效果、错误和独立 native-handle policy。Cgen 只生成普通 ABI 调用与清理，没有票据算法或包装函数体。`@pure @nonblocking` 不等于 `@no_raise`；可分配和检查参数的接口保留 checked 错误边界。

凭据是拥有图中的独立叶能力，不持有 Session、模块回调、临时视图或语言子对象。句柄装箱失败也消费原生拥有值，不能遗留裸引用。仅使用该对象时按需选择 `XRT_MODULE_TLS_RESUME`，不引入客户端、服务端、握手或传输模块。

## 本批验证边界

两端均从本批固定源码构建实际编译器与运行时适配对象（Windows 1254、Linux 1253 个编译单元），不以已发布编译器替代。只复用真实 TCC 核心；这不是 shipping DLL/VFS 验收。

Windows/Linux 各 42 步，共 84 步，包括声明与 XLIR、严格 GCC O0/O2、JIT、模块、独立 AOT、精确合同拒绝、真实裁剪计划、全量拥有目录唯一性及回滚。构造/导出/装箱共 1536 个分配失败前缀，完整目录共 232 个，总计 1768 个；内存快照平衡。

验证没有网络流量、外部 DNS、扫描、畸形密码材料专项或安全测试。记录位于 `F:/xlang-validation/five-groups-20261025/p6-tls-resume-language/next.json`。首次空程序对照因验证脚本把同名生成 C 放在语言文件旁而触发正常伴随 C 接入，失败证据保留；输入和产物分目录后完整重跑通过，未改编译器来规避它。

## 服务端类型化查找

`TlsServerContext` 新增：

```xlang
context.withResumeLookup(func(TlsResumeRequest) as TlsResume? lookup,
                         uint64 ageToleranceMillis = 10000) as TlsServerContext
context.withoutResumeLookup() as TlsServerContext
context.hasResumeLookup as bool
context.resumeAgeToleranceMillis as uint64
```

这些方法创建不可变快照，不修改原 Context，也不取消已创建的 Session/Listener。普通参数、身份、ALPN 和选择器克隆均保留查找配置；`withoutResumeLookup()` 只移除新快照的查找，并恢复默认容差。容差单位为毫秒，允许 0（严格匹配）；超出 uint32 范围 checked 拒绝，不截断。

`TlsResumeRequest` 只能由驻留运行时产生，无裸指针或默认构造入口。只读属性为 `serverName: bytes`、`protocolCount: int`、`ticket: bytes`、`age: uint64`，以及 `protocolAt(int): bytes`。`age` 是线路上的 uint32 混淆毫秒年龄，不是 xlang `time`。完整 SNI、ALPN 和票据由请求对象独立复制；属性和方法再次返回拥有副本。没有短生命周期视图泄漏到语言层。

查找返回 null 表示缓存未命中，继续正常完整认证握手；抛错或资源失败是 checked 失败，不能当作未命中。返回临时 `TlsResume.fromParts(...)` 有效：驻留适配在语言临时结果释放前 retain 原生凭据，通过 `ResumeOwnedV1` 转交；框架按已有原生合同消费所有输出路径。不需要线程局部最后结果缓存或隐藏临时租约。

Context、内存 Session、Listener 分别持有真实的回调拥有者，并在图中枚举。Session 达到 Ready/Failed 后释放捕获；Listener 必须等原生聚合 Data 引用退役，才在 Source 的 checked 清理尾部释放，不能在原生 drop 线程上任意析构捕获。选择器和查找可同时存在，两条边均计数和追踪。

回调是同步函数。内存 Session 在调用线程执行；Listener 在实际 Engine 工作线程执行，多个连接可能并发，捕获的可变状态需由应用同步。应使用预先准备的缓存，不在固定工作线程上执行阻塞 I/O 或等待同一执行器。已有回调的真实代码租约允许其在模块 Close 后完成，不开放新的外部注册，也不靠借用 CodeOwner 保活。

本批两端从实际固定源码构建（Windows 1257、Linux 1256 个编译单元）。有效覆盖各 126 步、合计 252 步；分配前缀检查各 756 个、合计 1512 个，包括真实 TCC 语言回调的临时结果、命中/未命中内存握手、抛错、Close 后调用、分配失败不能变成缓存未命中，以及双回调拥有图、延迟 checked 释放和完整目录。

静态 Context 和空程序的真实裁剪计划排除恢复查找模块；Cgen 没有新增实现体。全批同时发现并修复了通用语义重载问题：已知非函数属性值不能匹配函数形参；具体 callable 属性检查完整签名，现有方法组/动态 callable 投影规则保持不变。新增回归和四项既有 callable 回归均通过。

首次宿主验证漏用现行 `xl6_export_` 导出前缀；首轮保存请求也必须先建立持久状态基线。这两项仅修测试宿主，所有内存/失败断言保留。失败记录和旧控制源码已归档；重用两端各 120 项未变检查，单独重跑宿主的 6 项，最后重新核验全部源码、对象、控制工具、日志和产物哈希。记录为 `F:/xlang-validation/five-groups-20261025/p6-tls-resume-lookup/next.json`。

仅有正常固定有效材料的内存握手和自建字面量 loopback Listener 生命周期，没有公网、外部 DNS、扫描、畸形密码材料专项或安全测试。未修改独立 XRT，未升级时间 ABI；本批不是客户端恢复配置、票据收发、shipping DLL/VFS 或跨组发布联合验收。

## 客户端不可变恢复配置

`TlsClientContext` 新增四个入口：

```xlang
context.withResume(TlsResume resume) as TlsClientContext
context.withoutResume() as TlsClientContext
context.hasResume as bool
context.resume as TlsResume?
```

配置独立保留原生凭据引用；调用者释放临时 `TlsResume` 或原 Context，不会改变新快照。`resume` 返回新的拥有引用；未配置时返回 null，保留引用失败仍是 checked 错误，不能伪装成未配置。

`withParameters()` 和 `withProtocols()` 保留凭据，由原生握手检查版本、密码套件、名称与 ALPN 等兼容性。`withoutResume()` 只清除新快照，不修改原对象。显式 `withVerifier()` 是信任重绑定，始终清除新快照的凭据，即使传入同一个 verifier；应用必须在新信任配置上显式重新绑定凭据。此规则避免无意间跨信任配置复用已认证的 PSK。

同一配置用于内存 `TlsSession`、直接 `TlsStream` 和 `TlsStream.dialAsync()`；原生创建过程取得自己的引用，不借用语言 Context 或临时参数。凭据仅是不可变原生叶能力，不引入新的语言对象拥有边、模块回调或最后结果缓存。普通 Context 的创建、清理与 `hasResume` 不强行引入客户端恢复实现；实际 `withResume()` 才选择 `TLS_CLIENT_RESUME`。

`TlsSession.resumed: bool` 通过创建角色的驻留操作表读取，未协商时为 false。此属性反映已选择的恢复状态，不独自证明握手或认证成功；仅在 `ready` 后将其用于连接结果判断。传输对象继续通过实际网络 Worker 上的 `info()/infoAsync()` 获取不可变 `TlsConnectionInfo.resumed` 快照。

原生能力检查也已补齐：未编入 `TLS_CLIENT_RESUME` 时，无论 `ResumeOnly` 是否为 false，只要传入非空 `Resume` 都明确拒绝，不静默忽略。未配置恢复的 verified 客户端行为不变。本项无公开布局或函数签名变化，定点 backport 保留两边时间 ABI；记录见 `lib/xrt/backports/tls_client_resume_capability.json`。

本节不代表新票据的发送/接收 API、传输 KeyUpdate 或跨组发布验收完成；现阶段只完成已有完整凭据的客户端绑定与正常恢复/完整握手路径。

本批 Windows/Linux 各用完整固定源码构建实际编译器（1258/1257 个编译单元）。客户端配置、三条真实连接路径、XLIR 精确合同与拒绝、严格 GCC O0/O2、JIT/模块/独立 AOT、未使用能力裁剪及 Close 后清理共 142 步通过。分配失败检查共 1256 个有界前缀，覆盖配置创建/克隆/清除/信任重绑与全量拥有目录；内存快照平衡。AOT 不依赖 xl/xlc 动态库，Cgen 无新增实现体。

原生未启用能力的单独前置验收共 242 步、20 次执行，覆盖模块化与 core/runtime/full 三种单头产品、GCC O0/O2 与 TCC。源码、工具、对象、日志及产物哈希在两端分别重新核验。记录为 `F:/xlang-validation/five-groups-20261025/p6-tls-client-resume/next.json`。只有正常有效材料的内存握手和自建字面量回环通信；无外部 DNS、公网、安全专项或畸形密码测试。这仍不是 shipping DLL/VFS 或发布联合验收。

后续已补齐 `xrtTlsStreamFlush()` 原生前置：可在所属 Worker 上显式转交排队的控制记录，票据后无需空应用写入；AGAIN 保留密文等待普通传输驱动，不能重复签发。两端模块化/单头 GCC O0/O2/TCC 共 418 步、80 次执行通过，含四轮票据轮换恢复和双端 KeyUpdate 后数据往返。定点 backport 仅涉及两个 Stream 段，保留时间 ABI、对象布局和独立拥有合同。详见 [TLS 传输标准库](TLS传输标准库.md)。默认语言票据收发、传输 KeyUpdate 及发布级联合验收仍未完成。

该原生前置之后，默认 `TlsStream.keyUpdate()/keyUpdateAsync()` 已完成语言接入和两端 58 步实际源码验收。操作在连接所属 Worker 执行，保留独立拥有/checked 清理；异步结果为精确的 `future<TlsResult>`，并支持既有 `task.all`、动态装箱路径。使用通用 IR 枚举 Future 合同，没有 Cgen 专用实现，也不要求恢复票据模块。详见上述传输文档。票据发送/弹出及发布级联合验收仍未完成。

## 会话票据：拥有式结果与客户端缓存配置

本批新增公开面如下（源码联合验收结果见本节末尾）：

```xlang
// 只读、仅由运行时产生；无默认或裸指针构造。
TlsTicketResult.status as TlsResult
TlsTicketResult.resume as TlsResume?

// 仅用于客户端 Session。
client.ticketCount as int
client.droppedTickets as uint64
client.tryTakeTicket() as TlsTicketResult

// 仅用于服务端 Session。
server.issueTicket() as TlsTicketResult
server.issueTicket(bytes ticket, int lifetimeSeconds, int size = -1) as TlsTicketResult

clientContext.withRequiredResume(TlsResume resume) as TlsClientContext
clientContext.withTicketCapacity(int capacity) as TlsClientContext
clientContext.resumeRequired as bool
clientContext.ticketCapacity as int
```

`tryTakeTicket()` 非阻塞地取出当前队列的一张凭据；空队列返回
`wouldBlock` 与 null，成功返回 `ok` 与独立拥有的 `TlsResume`。它不等待
未来票据，也不暗中驱动握手。结果类、原生记录和句柄字段全部在原生弹出前
构造完成；之后仅转交原生引用，避免语言结果分配失败导致票据从队列丢失。
静态调用与动态调用共享同一输出槽合同，没有“最后一次结果”缓存。

服务端省略参数时使用 XRT 的安全随机票据与默认生命周期；显式票据接收
完整 bytes（含 NUL），`size = -1` 使用全部字节，超长截断到实际长度，
小于 -1 报错。`lifetimeSeconds` 转换为 uint32 前先检查范围；其他合法性
继续由 XRT 决定。成功仅表示控制记录已进入会话密文队列；内存 Session
调用者仍需通过 `takeCipher()/feed()/drive()` 交付，不伪装成对端已收到。
返回凭据独立拥有，可由应用保留到服务端缓存中。

缓存默认容量为 4，语言层显式允许 0—64。满载时由 XRT 淘汰最旧票据并
增加丢弃计数；容量 0 禁用保存而非忽略接收和统计。配置不可变，
`withParameters()/withProtocols()` 保留容量与强制恢复状态；
`withoutResume()/withVerifier()` 清除强制恢复状态，保留缓存容量。
普通 `withResume()` 允许原生的正常完整握手回退，`withRequiredResume()`
则设置原生 ResumeOnly，不把未选择 PSK 静默当作强制恢复成功。
Session、Stream 与 Dial 的创建都传递同一组原生配置。
只读取这些配置属性不强行引入恢复实现；设置缓存容量或绑定恢复凭据才
显式选择客户端恢复模块。仅使用结果类型／属性也不引入 Session 票据
生产器或客户端／服务端恢复模块。

本批还修正通用类型化回调的注册检查：闭包的可选参数名／默认值元数据
数量不等于 invoke 参数个数。对象方法经精确类型转换后的回调没有这份
元数据，但其单参数及可空结果 ABI 已由 XLIR 的 sealed callback plan
验证；运行时仍验证真实闭包拥有者并持有真实 CodeLease，不调用 erased
入口。错误参数数目、请求类型和返回类型继续在编译期拒绝。

Session 票据不是 Stream 票据接口，不能将本节视为传输票据或最终发布验收
已经完成；后续传输操作仍须在实际连接所属 Worker 上执行。

本批 Windows/Linux 分别从固定完整源码构建实际编译器（1263/1262 个
编译单元），共复核 124 项检查及 2280 个有界分配失败前缀：typed/lowered
IR、严格 GCC O0/O2、JIT/模块/独立 AOT、11 个精确目标与 42 项合同变异
拒绝、八类错误声明、未使用能力裁剪、全量拥有目录和通用回调 Close。
真实静态／动态导出在弹出失败时保留队列票据；独立结果与凭据跨模块
关闭后仍保持有效，最后引用释放后 checked 退役完成，内存快照平衡。

首轮暴露对象方法类型化回调被可选元数据误拒绝，修正生产实现后重新
冻结并完整构建两端源码。随后宿主用例误用普通 `xrtValueArrayGet()` 读取
typed array；仅将测试改为现行拥有式 `xl6ArrayValueReadCopyV1()` 并对称
释放引用，未改变生产实现或降低断言。保留原控制文件与失败记录，各端
复用 56 项输入未变的通过项，重跑宿主 O0/O2 的六项编译／链接／执行，
最后逐项重验完整源码、工具、对象、控制文件、日志与产物哈希。
记录见 `F:/xlang-validation/five-groups-20261025/p6-tls-session-tickets/next.json`。
没有外部通信、安全专项或畸形密码材料测试；未修改独立 XRT、时间 ABI
或 Cgen。这是完整工作区的源码专项验收，不是 shipping DLL/VFS 或跨组
发布联合验收。

## Stream 票据操作

传输对象将内存 Session 的同一凭据／结果模型带到真实连接所属 Worker：

```xlang
stream.tryTakeTicket() as TlsTicketResult
stream.tryTakeTicketAsync() as future<TlsTicketResult>
stream.issueTicket() as TlsTicketResult
stream.issueTicket(bytes ticket, int lifetimeSeconds, int size = -1) as TlsTicketResult
stream.issueTicketAsync() as future<TlsTicketResult>
stream.issueTicketAsync(bytes ticket, int lifetimeSeconds, int size = -1) as future<TlsTicketResult>
stream.ticketCount(int timeoutMillis = -1) as int
stream.ticketCountAsync() as future<int>
stream.droppedTickets(int timeoutMillis = -1) as uint64
stream.droppedTicketsAsync() as future<uint64>
```

`tryTakeTicketAsync()` 是排队到实际 Worker 的一次非阻塞队列检查，不是等待
未来票据到达；空队列返回 `wouldBlock/null`。同步版本等待该命令完成，不能
在网络 Worker 内调用，Worker 回调与高并发代码使用异步版本。签发和弹出
不提供同步超时重试参数，避免把已经生效的操作误读为“超时＝未发生”。
只读统计查询可以超时。

命令受理前取得真实 Stream／Engine 拥有引用，复制显式字节输入与字段名，
通过真实 Task CodeLease 保护不可变类型 schema。不会把借用、临时输出对象
或模块 factory 回调存进命令。到 Worker 后校验实际角色，完整构造 Future
载荷、结果类及原生句柄字段，再进行破坏式弹出／签发。结果引用独立拥有；
原始字符串／bytes 修改、临时 Context 释放和模块逻辑关闭不会制造悬挂视图。

取消是协作请求：任务执行前与输出准备后检查取消，尚未生效时可取消；越过
原生操作生效点后不再用晚到的取消请求覆盖成功。已受理的异步操作不会因为
调用者丢弃 Future 而自动回滚。普通异常也不是通用事务回滚保证；不得把失败
当作可以无条件重试签发／弹出的证据。

签发成功后调用 XRT `StreamFlush` 交付控制记录。Producer 的 `AGAIN` 才表示
本次未受理；Flush 的 `AGAIN` 仍返回已接受的 `ok`，由正常传输驱动处理背压，
不能再次签发。其他传输失败保留 checked 错误边界，不伪装成可以盲目重试的
`wouldBlock`。没有空应用写入的隐式刷新技巧。

客户端接收、服务端签发、只读统计及同步等待是独立组件。统计／单向异步
消费者不会为另一角色或同步等待拉入实现。Cgen 只使用通用 class-result
和 Future ABI；没有票据包装函数体、语言占位对象或最后结果缓存。

### Stream 专项验收

Windows/Linux 分别从完整固定源码构建 1271/1270 个编译单元；两端合计
146 项检查与 744 个有界分配失败前缀通过。包含 typed/lowered IR、严格
GCC O0/O2、JIT/模块/独立 AOT、10 个精确目标和 48 项合同变异拒绝、五类
错误声明，以及接收／签发／统计／空程序的独立裁剪。单向异步消费者不拉入
另一角色或同步等待实现，统计消费者不拉入结果类构造器；AOT 无 xl/xlc
动态库依赖。

真实两个 Worker 的自建字面量回环连接覆盖同步／异步签发与弹出、动态
Future 装箱、`task.all`、NUL 与长度截断、实际控制记录刷新、容量淘汰、
Direct 与 Dial 的强制恢复和禁用缓存，以及错误角色和关闭后的 checked
拒绝。宿主在真实 Worker 线程注入故障：每次执行 128 个前缀，覆盖静态与
动态导出；失败时队列票据保留，成功结果独立拥有，内存快照平衡。模块
关闭前已受理的命令在关闭后完成结果构造，证明使用了真实派生 CodeLease；
最后结果释放后 checked 退役成功。

初版测试写了当前不支持的 `export async func`，改为普通导出返回异步
函数的 Future 后，未改变语言或生产实现。一次续跑中两端验收进程已结束，
完整归档检查点，复用原先 57/47 项通过记录后接续缺失阶段；材料生成器的
非确定性重生成被哈希检查拒绝，原始材料已从核验归档恢复，之后不再生成。
另一个宿主用例误用结束语言生命周期的 `xlRunModuleMain`，仅改为持久
`xlActivateModule` 后，重跑两端 O0/O2 编译／链接／执行。原记录与拒绝的
材料版本保留。最终在两端分别重验源码、对象、输入、归档、控制器、日志
和产物哈希。记录为
`F:/xlang-validation/five-groups-20261025/p6-tls-transport-tickets/next.json`。

本批未修改独立 XRT、时间 ABI 或 Cgen；没有外部 DNS、公网、扫描、畸形
密码材料或安全专项测试。专项通过不代表旧外观已清理，也不代替五组功能
的实际 shipping DLL/VFS/IDE 等发布联合验收。
