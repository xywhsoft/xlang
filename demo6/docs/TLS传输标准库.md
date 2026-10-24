# TLS 传输标准库

本页描述驻留运行时上的默认 `TlsStream`／`TlsServer` 接口，不代表 TLS
全部任务已经封板。高级会话元数据、动态回调、恢复票据和最终发布验收仍
单独推进。旧 `TlsConfig`、旧 WebSocket TLS 接管不是本页接口的一部分。

## 对象与配置

- `NetEngine` 显式管理网络工作线程、后端和拥有图；不会为每次连接创建隐藏 Engine。
- `TlsClientContext` 包含不可变参数、验证器、信任快照和 ALPN 配置。
- `TlsServerContext` 包含不可变参数、身份和 ALPN 配置。它不能冒充客户端上下文。
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

## 时间、错误和清理

- 所有 `timeoutMillis` 相对参数使用毫秒，`-1` 表示无限等待。
- `Until` 使用同一网络单调时钟的绝对期限。当前固定快照保持微秒合同；
  不可传 UTC 时间戳，也不能混用独立 XRT 新时间 ABI 的毫秒值。
- 同步等待不允许阻塞连接所属 Worker；这种调用 checked 拒绝。
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
