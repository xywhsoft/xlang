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
