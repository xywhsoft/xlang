# TLS 传输标准库

本页描述驻留运行时上的默认 `TlsStream`／`TlsServer` 接口，不代表 TLS
全部任务已经封板。已提供不可变会话元数据和类型化服务端身份选择；
恢复票据、传输 KeyUpdate 和最终发布验收仍单独推进。
旧 `TlsConfig`、旧 WebSocket TLS 接管不是本页接口的一部分。

## 对象与配置

- `NetEngine` 显式管理网络工作线程、后端和拥有图；不会为每次连接创建隐藏 Engine。
- `TlsClientContext` 包含不可变参数、验证器、信任快照和 ALPN 配置。
- `TlsServerContext` 包含不可变参数、静态身份或类型化身份选择器及 ALPN 配置。它不能冒充客户端上下文。
- `TlsStreamOptions` 设置握手、关闭与异步资源预算。
- `TlsDialOptions` 设置 Resolver/TCP 参数、TLS 参数、SNI 和验证名称。
- `TlsStream` 是真实的受管理 TLS 连接，保持实际 Engine 的生命周期。
- `TlsServer` 包装 TLS Listener：负责接受连接，不是另一个应用任务调度器。

所有 native 字段私有。没有 `point` 导入构造，也没有 `nativeHandle()`；
不能把一个裸地址转成连接，或者把短期原生借用保存在语言对象里。

## 建立连接

已知地址时：

```xlang
auto stream = TlsStream(engine, remoteAddress, clientContext, "localhost")
// 等价的命名空间入口：
auto other = tls.connect(engine, remoteAddress, clientContext, "localhost")
```

按地址构造会启动连接，但不承诺握手已经完成；需要就绪时使用 `waitOpen`
或等待 `openAsync()`。静态 `dial` 及 `dialAsync` 的成功结果则是完成拨号和
握手后的连接，不能混淆“已创建”和“已就绪”。

完整构造的参数依次为 `engine, remote, context, serverName, size,
verifyName, verifySize, streamOptions, tcpOptions, worker = 0`。两个名称都按
选中的 UTF-8 字节长度处理，禁止选中范围内包含 NUL；不会使用 `strlen`。

名称解析与拨号使用静态工厂：

```xlang
auto options = TlsDialOptions().withServerName("localhost").withVerifyName("localhost")
auto stream = await TlsStream.dialAsync(engine, resolver, "127.0.0.1", port, clientContext, options)
// 异步拨号的命名空间入口：
auto other = await tls.connectAsync(engine, resolver, "127.0.0.1", port, clientContext, options)
```

默认与显式 `TlsDialOptions` 都提供以下工厂；代理族在 `resolver` 后增加
`NetProxy proxy` 参数，其余合同相同：

| 同步 | 异步 |
|---|---|
| `dial` / `dialProxy` | `dialAsync` / `dialProxyAsync` |
| `dialCancel` / `dialProxyCancel` | `dialAsyncCancel` / `dialProxyAsyncCancel` |
| `dialUntil` / `dialProxyUntil` | `dialAsyncUntil` / `dialProxyAsyncUntil` |
| `dialUntilCancel` / `dialProxyUntilCancel` | `dialAsyncUntilCancel` / `dialProxyAsyncUntilCancel` |
| — | `dialAsyncTimeout` / `dialProxyAsyncTimeout` |
| — | `dialAsyncTimeoutCancel` / `dialProxyAsyncTimeoutCancel` |

普通拨号在 `context`（或 `options`）后接受 `size = -1`；同步入口再接受
`timeoutMillis = -1`。取消／期限变体保留明确的位置参数，不隐式重排原生 ABI。

## 服务端

```xlang
auto listen = TcpListenOptions(NetAddress.loopback(4, 0))
auto server = TlsServer(engine, listen, serverContext)
// 或 tls.listen(engine, listen, serverContext)
auto pending = server.acceptAsyncTimeout(5000)
auto address = server.local()
auto port = server.port
auto peer = await pending
```

高级构造在 `context` 后增加 `TlsStreamOptions stream, int accepts = 128,
int handshakes = 64`；两个队列预算必须是正数。`accept` 返回 `TlsStream`，
`acceptAsync` 返回 `future<TlsStream>`；还提供 `Cancel`、`Until`、
`UntilCancel`，异步额外提供 `Timeout`、`TimeoutCancel`。

## 数据与等待

| 方法族 | 输入／输出 |
|---|---|
| `send` | `string`、`bytes`、`Buffer` → 实际选中的发送字节数 |
| `sendVector` | `array<string>`、`array<bytes>`、`array<Buffer>` → 发送字节数 |
| `recv` | 最大字节数 → `bytes` |
| `recvOwned` | 最大字节数 → `NetBytes`，可继续交给拥有式分帧／缓冲接口 |
| `wait` | 等待条件 → `bool` |

各族提供同步基本、`Cancel`、`Until`、`UntilCancel`；异步提供 `Async`、
`AsyncCancel`、`AsyncUntil`、`AsyncUntilCancel`、`AsyncTimeout`、
`AsyncTimeoutCancel`。发送方法的输入后是 `size = -1`，接收默认
`maximum = 65536`；同步基本入口最后为 `timeoutMillis = -1`。

发送的 `size` 是字节前缀；向量发送的 `size` 是元素个数前缀。`-1` 使用
完整实际长度，超出长度截断，其他负数报错。`string` 的内嵌 NUL 不截断，
字节长度不等于 Unicode 字符数。异步提交时由 XRT 完成必要的拥有／复制，
不会在返回后继续借用调用者的字符串、字节值或可变 Buffer。

`wait` 的条件为 `0=open, 1=readable, 2=writable, 3=drain, 4=end,
5=close`，默认等待 open。便捷方法为 `waitOpen`、`waitReadable`、
`waitWritable`、`drain`、`waitEnd`、`waitClose`；相应无参 Future 方法为
`openAsync`、`readableAsync`、`writableAsync`、`drainAsync`、`endAsync`、
`closeAsync`。

`peek` 返回拥有的字节快照，`consume` 返回消费字节数，`sendBound` 返回
发送上界，`protocol` 返回拥有的 ALPN 字节串。它们均有 `Async` 版本；
涉及会话／缓冲的操作在真实所属 Worker 上执行，不泄漏借用视图。

## 连接信息和服务端统计

```xlang
auto info = await stream.infoAsync()
// 同步版本：stream.info(timeoutMillis = -1)
assert info.role == TlsRole.client
if info.hasSelection then println(info.version) end
auto negotiatedProtocol = info.protocol
auto stats = server.stats()
```

`TlsConnectionInfo` 和 `TlsServerStats` 是受语言生命周期管理的不可变快照，
没有裸指针构造或公开 native handle。快照可在原连接关闭后继续使用；
读取属性不会再次访问原 Session。`infoAsync` 在连接所属 Worker 上采样，
复用真实 Engine 的任务队列，不创建额外线程或异步结果映射回调。
`info` 不允许在同一 Engine 的 Worker 上同步等待。两者均可能因资源、期限或分配失败报错。

连接快照包含：

- `role`、`state: TlsStreamState`、`sessionState: TlsState`、`ready`、
  `hasSelection`、`version: TlsVersion`、`cipher: TlsCipherSuite`、`resumed`。
  尚无协商结果时读取 `version`／`cipher` checked 报错，不伪造枚举值。
  `ready` 仅表示采样时 `sessionState == TlsState.ready`；判断传输状态还应
  检查 `state`，它不承诺后续网络操作一定成功。
- `available`、`pending`、`asyncBytes`、`asyncCount`；这些是采样时刻的数量。
- `protocol: bytes`、`serverName: bytes`；未收到 SNI 或无相应角色能力时为空。
  字节值保留精确长度，不将协议数据隐式当作文本。
- `hasServerCookie`／`serverCookie`、`hasPeerAlert`／`peerAlertLevel`／`peerAlertCode`。
  可选值应先检查相应的 `has*`。
- `hasError`、`errorKind`、`errorCode`、`errorDomain`、`errorOperation`、`errorMessage`。
  错误文本也是副本，不保留内部 `xerror*`。
- `peerCertificateCount`、`peerCertificateAt(index = 0): Certificate`；证书 DER
  随快照复制，返回的 Certificate 独立拥有数据。握手未完成时不公开临时证书链，
  越界 checked 报错。当前静态服务端不提供客户端证书认证，因而服务端链为空。

统计快照包含 `state: TlsListenerState`、`handshakes`、`accepted`、`rejected`、
`handshakeErrors`、`activeHandshakes`、`peakHandshakes`、`queuedAccepts`、
`peakQueuedAccepts`、`acceptWaiters`。计数沿用 XRT 的并发原子观察语义，
不是把多个计数冻结在同一时刻的事务性读数；快照创建后自身不再变化。

快照访问器按内容值返回 bytes／string，不向语言暴露借用。内部私有的原始
Stream 收养路径若未携带真实角色读取合同，采样 checked 返回 unsupported，
不会猜测底层 Session 类型；默认标准库的创建、接受和拨号路径均安装此合同。

## 时间、错误和清理

- 所有 `timeoutMillis` 相对参数使用毫秒，`-1` 表示无限等待。
- `Until` 使用同一网络单调时钟的绝对期限。当前固定快照保持微秒合同；
  不可传 UTC 时间戳，也不能混用独立 XRT 新时间 ABI 的毫秒值。
- 同步等待不允许阻塞同一 Engine 的任何 Worker；这种调用 checked 拒绝。
- 取消或超时不保证撤回已经发送的字节或已经交付的结果。
- `close()` 请求有序关闭，`abort()` 请求中止；返回不表示所有子操作已退役。
- 已经接受的操作持有真实 Engine／连接引用；逻辑 Engine.close 不伪造完成。
- 析构交由拥有图和 checked 退役处理，不在工作线程 abort，也不假报清理成功。

`state` 暂为原生状态整数：Stream 的 `0..5` 分别为 connecting、handshake、
open、closing、closed、failed；Server 的 `0..2` 为 open、closing、closed。
`available`／`pending` 为字节数，`local()`／`remote()` 返回拥有的 `NetAddress`。

## 实现边界与未完成部分

标准库源位于 `src/stdlib/xl6_stdlib_tls_transport.inc`。它仅声明现有驻留
ABI，Cgen 只生成声明、类型化调用和语言清理，不生成 TLS 算法或桥接函数体。
声明完整能力不会自动链接全部实现，最终保留的 XLIR 调用决定源裁剪。

旧 WebSocket TLS 接管仍依赖原来的裸指针表示，尚未迁移到受管理边界。
不能把新的 `TlsStream` 私有字段交给旧入口；本轮不把 WSS 验收算作通过。
HTTP／WebSocket 的公开面仍属于各自未决策项，高级 TLS 剩余工作及发布联合
验收也不会因为本页默认传输接口可用而自动标记完成。

`resumed` 观察器属于基础 Client／Server 角色，不会因查询状态而启用票据、
PSK binder 或恢复握手。未编译恢复能力时，对合法角色会话返回 `false`；
启用后读取真实状态。票据与恢复操作本身仍是可选模块。
TLS 整体的发布与最小体积联合验收仍须独立完成。

### 动态身份选择：请求快照底座

`xl6_tls_server_request` 已实现选择请求的私有驻留 ABI：同步复制 XRT
回调借入的 SNI 和完整 ALPN 列表，一次分配保存全部视图和字节；不保留
Session、源缓冲区、闭包或模块地址。ALPN 解析复用 XRT，名称按不透明
bytes 保存，保留 NUL 和非 UTF-8 字节，返回的 bytes 独立于请求存活。
空 SNI／未提供 ALPN 合法，索引越界、无效 ABI 参数与分配失败正常报错。

这一层尚不是默认标准库的可调用选择器：没有向语言公开裸请求构造，
`TlsServerContext` 当前仍使用静态身份。类型化回调、捕获对象与真实代码
租约的拥有图、异常传递，以及握手至退役的联合验收仍待接入，不能把
下面的单元验收视为它们已经完成。

`tools/check_tls_server_request_owned.py` 在 Windows／Linux 各通过 40 个
步骤：独立编译两份实际快照（GCC O0／O2、真实 TCC），验证单次分配、
输入失效与结果脱离后的寿命、2048 个有序协议以及每种构建 96 个 OOM
前缀。实际产物计划只选择一个驻留源文件和 Hello／Value 原生模块；
空计划裁剪此实现，不引入 Client／Server／Resume／Stream／TCP。
本批未修改时间 ABI 或 Cgen，不包含网络流量、密码学专项或发布验收。

动态选择器的另一项前置是原生选择结果的身份寿命。XRT 新增
`xrtTlsServerChoiceSetIdentityV1`：回调为临时结果设置身份时取得独立引用，
框架在会话 retain 后统一释放；替换、拒绝和 checked 错误也走该释放路径。
旧的直接指针赋值仍是借用，不自动接管引用。回调已有错误保持为首个错误。

这项定点 backport 给 `xtlsserverchoice` 增加了框架私有的身份引用字段，
涉及其布局的 C 单元必须整体重编译；两边的时间 ABI 不变。本批普通固定
证书的内存握手、生命周期和 OOM 单元不能替代类型化语言回调的拥有图、
模块退役、JIT／AOT 或发布验收，默认语言服务端仍为静态身份。

### 后续接入：类型化 Session 选择器

上述两批是前置验收的历史状态。后续已实现默认标准库的
`TlsServerRequest`、`TlsServerChoice` 和类型化 `TlsServerContext` 选择器，
先接入内存 `TlsSession`，再接入组合 `TlsServer`／`tls.listen`。
精确 API、拥有图、效果与验收边界见
[TLS 类型化服务端选择器](TLS类型化服务端选择器.md)。
不能将 Session 的局部接入视为组合传输、票据恢复或发布封板完成。

### 票据恢复：拥有式原生回调前置

XRT 新增 `xtlsserverconfig.ResumeOwnedV1`：回调显式转交独立 `xtlsresume` 引用，
框架在成功、未命中或失败路径上按合同消费结果。它与借用式 `Resume` 互斥，
未编入恢复模块则明确拒绝，不能静默退回完整握手。此接口避免临时语言结果
在回调返回后、原生 retain 前已销毁的问题，不使用隐藏的最后结果缓存。

本批仅定点同步六个原生段和必要声明，保留嵌入版微秒/Until 与独立 XRT
毫秒/For 合同，也保留票据线路年龄的毫秒转换。公开配置布局扩展，嵌入它的
Listener 配置同样受影响，相关 C 单元必须整体重编译；现有函数符号数不变。

Windows/Linux 各通过 162 步，共 324 步、38 次原生执行与 1920 个分配前缀检查；
两端完整原生依赖目录仍为 3898 个符号、426 个候选根。测试仅包含正常固定
材料的内存握手、拥有权、错误与未启用能力配置；没有网络流量或安全专项。
详见 `lib/xrt/backports/tls_resume_owned_callback.json` 和
`F:/xlang-validation/five-groups-20261025/p6-tls-owned-resume/next.json`。

这不是 xlang 票据 API 已完成：语言层 Resume/票据类型、角色配置及回调绑定
仍待接入，传输 KeyUpdate 与发布级联合验收也未完成。

### 后续接入：不可变恢复凭据

上述原生前置之后，语言层已完成现代 `TlsResume`：精确枚举/时间合同、
完整二进制凭据、独立拥有副本、显式 `exportSecret()`、有效期与线路年龄。
旧裸句柄构造声明已退出默认标准库；Cgen 不增加票据包装实现。
详见 [TLS 恢复票据](TLS恢复票据.md)。两端源码验收共 84 步、1768 个分配
失败前缀通过；角色配置、类型化查找回调与 Session/传输票据操作仍未完成，
不能据此宣布恢复握手、传输 KeyUpdate 或整项长期任务完成。
