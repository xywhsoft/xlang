# xmail

`xmail` 是新版 XRT xmail 产品的正式 xlang 源码包。C 实现、公共头和内部依赖均同步自
`extlibs/xmail`，语言层只负责对象映射、生命周期和错误转换，不重新实现 MIME、SMTP、
POP3 或 IMAP 协议。

语言表面提供以下三层能力：

- `address`、`attachment`、`compose`、`parseLimits` 和 `Message` 负责 RFC/MIME 数据。
- `SmtpClient` 保留 command、DATA、BDAT、AUTH 和流式 `submit`，不会强制创建整封临时报文。
- `Pop3Client` 保留原始线路、command、`Begin/Next`、STAT/LIST/UIDL 和 RETR/TOP。
- `ImapClient` 保留显式 tag、自动 tag、事件、literal 分块读取、认证和邮箱选择摘要。

三个协议客户端都显式接收已经启动的 `NetEngine`。原生桥只在同步 Open 期间创建 resolver，
会话建立后立即释放；客户端在终态前 pin Engine，并在 `quit`、`logout`、`close` 或 `abort`
成功时释放。TLS/STARTTLS 必须显式传入 `TlsClientContext`。编译器封闭的字段投影取得实际拥有句柄，驻留入口在调用期间保留配置并进入 Engine 执行作用域；线路独立持有原生 Context/Verifier，包含语言回调的 verifier 继续参与拥有图。

响应视图在越过 C 边界前都会复制。POP3 多行项目和 IMAP 事件只保存当前项，邮件正文和 IMAP
literal 均可逐块读取，因此内存随调用方选择的块大小增长，而不是随邮箱或报文大小增长。

## SMTP

```xlang
NetEngine engine = NetEngine(NetEngineOptions().withWorkers(2))
engine.start()
xmail.SmtpConfig config = xmail.SmtpConfig(
    "127.0.0.1", 2525, xmail.SECURITY_PLAIN, "client.example")
xmail.SmtpClient client = xmail.SmtpClient.open(engine, config)
assert client.mailFrom("sender@example.com")
assert client.recipient("receiver@example.com")
assert client.data("Subject: hello\r\n\r\nbody")
assert client.quit()
engine.close()
assert engine.wait(5000)
```

## POP3

`retrieve` 和 `top` 只开始多行响应。随后反复调用 `next`，直到 `nextState` 为
`POP3_NEXT_END`；合法空行仍返回 `POP3_NEXT_ITEM`。

## IMAP

`begin` 启动自动 tag 顺序命令，`next` 返回响应事件。事件声明 literal 后必须先用
`readLiteral` 消费全部字节，再继续调用 `next`。需要流水化或扩展命令时，可用 `send` 自行
提供 tag，并通过 `receive` 关联响应。

全部协议测试只使用 `127.0.0.1` 回环服务器，不访问外部邮件服务。

## 原生组合资源合同

客户端使用通用 `xl_foreign_graph.h`，不作为无隐藏引用的叶资源登记。Trace 逐个枚举真实 Engine 拥有句柄、TCP/TLS 线路与解压输入的 owning slot；各子域仍须单独准入。所有方法通过 Enter/Leave 阻止并发调用和回调重入，运行时不会在语言回调或等待期间持有全局锁。

STARTTLS 的工作线程交接使用拥有任务。调用方和任务各持有一个请求引用，排队超时/取消先通过原子 Gate 放弃接管；已开始的接管返回所有权后才能销毁借用输入。队列丢弃任务会释放请求并关闭 Promise，任务返回后由驻留 Drop 释放代码依赖，避免扩展在自身 C 栈尾被卸载。

额外成本是客户端记录、指针参数载体和 releaser 的构造/引用，方法边界的原子状态检查与引用增减，Open 期间实际句柄 pin/执行作用域，以及 STARTTLS 的一次请求/拥有任务分配。MIME 动态容器快照存在遍历和分配。这里不宣称零桥接开销；协议算法与报文分块路径仍复用 XRT。

`test/context_projection.xl` 仅测试包含闭包的配置创建、投影、作用域退出和资源保留，不执行证书验证或握手。`test/native_handoff_lifetime.c` 用拒绝握手的本地桩覆盖交接生命周期。普通邮件会话验收仅使用测试自身创建的 loopback 服务。
