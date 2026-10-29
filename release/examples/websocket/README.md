# WebSocket 标准库示例

在 `release` 执行：

```console
xl run examples/websocket/protocol.xl
xl run examples/websocket/loopback.xl
```

`protocol.xl` 离线使用帧和 Close 值；`loopback.xl` 创建仅绑定本机的
TCP 回显连接，展示 Upgrade、拥有型事件、精确 bytes、异步发送与关闭。
两份示例已显式登记到 `tools/check_stdlib_websocket.py`，随 Windows 标准库
套件和 `test_linux.sh` 自动验证 JIT、模块及独立 AOT，收据保留输入哈希。
真实 HTTP 服务应使用 `Http1TcpTransport`／`Http1TlsTransport` 读取握手，
再调用 `detach().webSocketAsync(...)`，以保留已预读的帧字节。

Worker 回调内可直接调用同步发送接口；回调外使用 Async 入口。
`sendBinaryAllAsync` 表示全部字节已受理，`waitDrainedAsync` 表示本地发送
队列排空；二者均不表示对端应用已经处理。事件可保存并通过 `event.stream`
访问发送方，完整拥有图负责循环清理。Buffer 接收绑定期间需要外部同步。

TLS、压缩、取消和 HTTP 交接的完整可运行例见
`test/cases/stdlib_websocket_*_v2.xl` 与标准库验收文档。
