# XRT 扩展标准库 API

本文记录已经由 XRT 提供、并由 xlang 标准库公开的扩展能力。这里是当前口径，不承担旧版本兼容说明。

## CRC32 校验和

默认 `crc32.checksum/update` 支持 string 与 bytes，另有
`checksumBuffer/updateBuffer`。六个重载均返回 `uint32`，可选 size 按字节
截断到实际长度，保留内嵌 NUL；具体合同见 [CRC32 标准库](CRC32标准库.md)。

## 编码、压缩与模板

本轮源代码合同见 [编码、压缩与模板标准库](编码压缩与模板标准库.md)：
codec 的 string/bytes 编码及 bytes 解码、完整 Base64/Percent 配置；
compress 的整块／流式 raw/zlib/gzip；template 的预算、扩展回调、
include 解析器、Buffer/IoWriter 输出与有界宿主／VFS 输入。
旧裸指针 codec 与生成式模板桥退出；类实例数据只投影 public 存储字段，
不执行 getter、不采用持久化属性、不暴露私有字段。两端实际新候选专项已通过，
没有覆盖 release 中的现有二进制。

## 动态方法与 Stack 接收

typed func 与 xcall 的 nullable 参数／返回值合同见
[Nullable callable 的存在性与载荷合同](stdlib53-b278-nullable-callable.md)。
null 不折叠为 false、零值、空 string 或空 bytes，参数和结果的
基础载荷继续沿用既有精确身份、范围与所有权合同。

动态多值接收的当前合同见
[动态方法多返回值与 Stack 安全接收](stdlib53-b277-dynamic-method-products.md)。
`var` 接收者的类方法与绑定方法保存全部返回值；动态 Stack try 返回
`(found, item)`，其中 `(false, null)` 为未找到，`(true, null)` 为取到
可空元素。多值接收严格检查数量；普通单值表达式只投影第一项。
内部输出参数不构成语言 `byref`，现有参数值语义不变。

## 进程配置与同步捕获

`process.cmd` 使用 typed `array<string>` 直接 argv；`process.shell` 明确
调用 shell。ProcessCommand 的配置文本自有复制，修改/执行要求创建
线程。ProcessRunOptions 提供捕获限制、overflow、取消、相对超时或
绝对期限；run 返回自有 ProcessResult，stdout/stderr 为精确 bytes，
Text 方法严格解码，status 为独立快照。基础设施错误抛异常，非零
退出/超时/取消为真实结果；exitCode 仅普通退出可读。

41 个声明式 resident 桥接分四组件，Cgen 不再实现 exec/success/
exitCode 算法。完整默认值、尺寸/错误/线程/生命周期合同见
`Process配置与运行设计.md`。B23 的 live Process 已增加 id/state/running、
独立的 nullable wait/status、显式停止、唯一拥有的 stdinWriter/
stdoutReader/stderrReader；14 个新桥接经两个按需组件接入，复用 IO，
不创建输出缓存。wait 超时返回 null，非零退出返回真实退出快照；
流可以比父 Process 活得更久，原生工厂失败不误关管道。pipeline、typed
Future、文件重定向和 PTY 会话仍待验收，不声称七个模块全部完成。

## 二进制加密与摘要

文本接口返回十六进制字符串；带 `Bytes` 后缀的接口接收并返回 `Buffer`，不会经过字符集转换。AEAD 解密校验失败时返回空 `Buffer`。

```xlang
Buffer digest1 = crypto.sha1Bytes(data)
Buffer digest256 = crypto.sha256Bytes(data)
Buffer digest384 = crypto.sha384Bytes(data)
Buffer digest512 = crypto.sha512Bytes(data)

Buffer mac256 = crypto.hmacSha256Bytes(key, data)
Buffer mac384 = crypto.hmacSha384Bytes(key, data)
Buffer mac512 = crypto.hmacSha512Bytes(key, data)

Buffer key256 = crypto.hkdfSha256Bytes(key, salt, info, 32)
Buffer key384 = crypto.hkdfSha384Bytes(key, salt, info, 32)
Buffer random = crypto.randomBytes(32)

Buffer cipher = crypto.chacha20Bytes(key32, nonce12, data, 1)
Buffer sealed = crypto.chacha20Poly1305EncryptBytes(key32, nonce12, data, aad)
Buffer opened = crypto.chacha20Poly1305DecryptBytes(key32, nonce12, sealed, aad)
Buffer aes128 = crypto.aes128GcmEncryptBytes(key16, nonce12, data, aad)
Buffer aes128Plain = crypto.aes128GcmDecryptBytes(key16, nonce12, aes128, aad)
Buffer aes256 = crypto.aes256GcmEncryptBytes(key32, nonce12, data, aad)
Buffer aes256Plain = crypto.aes256GcmDecryptBytes(key32, nonce12, aes256, aad)
```

流式 SHA 对象可多次 `update`，最后读取 `bytes()` 或 `hex()`。例如 `crypto.sha1Stream()` 创建 SHA-1 上下文。

椭圆曲线接口返回包含私钥、公钥的字典；共享密钥的双方结果应完全相同。

```xlang
var p256 = crypto.p256Keypair()
string p256Secret = crypto.p256SharedSecret(string(p256["privateKey"]), peerPublicKey)
var p384 = crypto.p384Keypair()
string p384Secret = crypto.p384SharedSecret(string(p384["privateKey"]), peerPublicKey)
bool signatureOk = crypto.ecdsaVerify(publicKey, message, signature)
```

## 字符编码

编码接口只支持 UTF-8/16LE/16BE/32LE/32BE，输入/输出都是精确长度的
string 或 bytes，保留 NUL。严格/替换策略与 BOM 独立；不猜测源编码。
签名、BOM 模式和错误合同见 [文本标准库接入的 B8/B9](文本标准库接入.md)。
`encoding.detect(string|bytes,size=-1)` 返回 `(encoding,bomSize,confidence)`；
这是独立的启发式猜测入口，BOM 100 分不保证正文有效。旧 detectBytes 与
point/Buffer/code-page 外观不保留。

```xlang
bytes utf8 = encoding.encodeUtf8(text)
bytes utf8Bom = encoding.encodeUtf8(text, true)
string text8 = encoding.decodeUtf8(utf8)
bytes utf16le = encoding.encodeUtf16LE(text)
bytes utf16be = encoding.encodeUtf16BE(text)
bytes utf32le = encoding.encodeUtf32LE(text)
bytes utf32be = encoding.encodeUtf32BE(text)
string text16le = encoding.decodeUtf16LE(utf16le)
string text16be = encoding.decodeUtf16BE(utf16be)
string text32le = encoding.decodeUtf32LE(utf32le)
string text32be = encoding.decodeUtf32BE(utf32be)
bytes swapped16 = encoding.transcode(utf16le, encoding.utf16LE, encoding.utf16BE)
bytes swapped32 = encoding.transcode(utf32le, encoding.utf32LE, encoding.utf32BE)
```

## 异步文件与目录

路径级接口返回 `future<T>`，错误通过 future 拒绝传播。二进制接口保持原始字节，文本接口才执行字符集处理。

```xlang
Buffer data = await file.readBytesAsync(path)
bool written = await file.writeBytesAsync(path, data)
AsyncFile stream = file.openAsync(
    path,
    file.asyncRead + file.asyncWrite + file.asyncCreate + file.asyncTruncate,
    file.shareRead + file.shareWrite + file.shareDelete
)
Buffer part = Buffer(await stream.read(0, 4096))
stream.close()

bool created = await dir.createAsync(path)
bool parentsCreated = await dir.createAllAsync(nestedPath)
bool copied = await dir.copyAsync(source, target)
bool moved = await dir.moveAsync(source, target)
bool deleted = await dir.deleteAsync(path)
```

`AsyncFile` 的打开标志为 `file.asyncRead`、`file.asyncWrite`、`file.asyncCreate`、`file.asyncTruncate`；共享标志为 `file.shareRead`、`file.shareWrite`、`file.shareDelete`。标志可相加组合。`close()` 可显式提前释放，析构仍负责兜底。

## HTTP 客户端

`http.client()` 创建可复用连接的 `HttpClient`。`http.copyChunk(data, size)` 把流式回调的临时字节区复制为拥有自身生命周期的 `Buffer`。

响应标志：`http.responseChunked`、`http.responseKeepAlive`、`http.responseUpgrade`。

状态机阶段：`http.phaseNone`、`http.phasePrepare`、`http.phaseConnect`、`http.phaseWrite`、`http.phaseResponseHeaders`、`http.phaseResponseBody`、`http.phaseComplete`。

错误码：`http.errorNone`、`http.errorInvalidArgument`、`http.errorOutOfMemory`、`http.errorInvalidUrl`、`http.errorConnect`、`http.errorWrite`、`http.errorProtocol`、`http.errorCallbackAbort`、`http.errorTimeoutTotal`、`http.errorTimeoutIdle`、`http.errorCancelled`、`http.errorConnectionClosed`、`http.errorTransport`。

## JSON、XSON 与模板

```xlang
int status = json.visit(text, visitor)
JsonWriter writer = json.writer(true)
XsonOptions options = xson.options(true, false, false)

TemplateEngine engine = template.engine()
Template compiled = engine.compile(source)
Template fileCompiled = template.compileFile(path)
```

JSON 访问状态为 `json.visitComplete`、`json.visitStopped`、`json.visitInvalid`。`visitor` 返回 `false` 时得到 `visitStopped`；输入非法时得到 `visitInvalid`。`JsonWriter` 必须成对结束对象和数组后再调用 `finish()`。

## XID

```xlang
Xid value = id.make()
string xidText = id.encode(value)
Xid decoded = id.decode(xidText)
bool same = id.compare(value, decoded)
```

完整 URL、Query 参数和表单编码属于 xhttp 扩展产品，不在核心标准库注册。核心保留 `xrtHttpTarget*`、`xrtHttpHost*` 的 HTTP 线路底座，以及 `codec.percentEncode/Decode` 的 RFC 3986 百分号编解码。

## 正则表达式

Regex 属于 xregex 扩展产品，不在裁剪后的 XRT 核心标准库中隐式注册。当前新 ABI 适配器、对象模型、错误位置语义和回归资产保存在 `dev/deferred/xregex`；正式接入后必须通过显式 `import regex` 获得 `Regex`、`RegexMatch`、`RegexSet` 与 `RegexBuilder`，并由包元数据闭合 xregex 源码和链接依赖。

## 网络配置对象

```xlang
NetAddress address = net.resolve("localhost", 443)
NetAddressList addresses = net.resolveAll("localhost", 443, net.familyAny)
future<NetAddressList> resolving = net.resolveAsync("localhost", 443, net.familyIPv4)
NetResolverStats resolverStats = net.resolverStats()
NetEngineStats runtimeStats = net.stats()
NetProxy proxy = net.proxy(net.proxySocks5, "127.0.0.1", 1080)
NetEngine engine = net.engine(0)
TcpConnectConfig connect = tcp.connectConfig("example.com", 443)
TcpListenConfig listen = tcp.listenConfig(8080, "0.0.0.0")
UdpConfig datagram = udp.config(9000, "0.0.0.0")
```

地址族常量为 `net.familyAny`、`net.familyIPv4`、`net.familyIPv6`。网络结果使用
`net.resultError`、`net.resultOk`、`net.resultAgain`、`net.resultClosed`、
`net.resultTruncated`、`net.resultTimeout`、`net.resultCancelled`，适合不抛出异常的快速路径。

TLS 传输直接建立在 TCP 与 TLS 会话底座之上：

```xlang
TlsStream stream = tls.connect(host, port, config, timeout)
future<TlsStream> connecting = tls.connectAsync(host, port, config, timeout)
TlsServer server = tls.listen(port, serverConfig)
```

代理类型为 `net.proxyNone`、`net.proxySocks5`、`net.proxyHttpConnect`。`NetEngine` 的工作线程数为零时由 XRT 自动选择。

连接配置可设置超时、发送水位、接收上限和代理；监听配置可设置 backlog、发送水位和接收上限；UDP 配置可设置批量接收数及发送、接收队列上限。正常关闭、写半关闭和异常关闭分别由 `TcpStream.close()`、`shutdownWrite()`、`abort()` 明确表达，不再使用位标志组合。

## 任务组

`task.group()` 创建结构化并发的 `TaskGroup`。任务加入组后可统一等待、回收和关闭；`close()` 之后不再接受新任务。

```xlang
TaskGroup group = task.group()
group.add(work)
group.waitTimeout(5000)
group.close()
```

## 信号处理

`signal` 使用 XRT 原生信号机制，但原生分发线程只记录有界元数据，绝不执行
xlang 回调。用户已选择**显式注册线程事件泵**：没有后台自动脚本派发。
回调、channel 和 TaskGroup 路由都必须由注册线程调用泵交付。
常量为 `signal.HUP`、`signal.INT`、`signal.TERM`、`signal.BREAK`、
`signal.CLOSE`、`signal.LOGOFF`、`signal.SHUTDOWN`；先用 supported 查询平台支持。

```xlang
auto listener = signal.on(signal.INT, func(SignalEvent event)
    println("received " + event.name + " count=" + string(event.count))
end)
auto once = signal.once(signal.INT, func()
    println("one-shot callback")
end)
try
    assert signal.raise(signal.INT)
    assert listener.dispatchFor(1000) == 1
    assert once.dispatchFor(1000) == 1
finally
    listener.close()
    once.close()
    signal.reset()
end
```

- `signal.on(code, handler, capacity = 64)`、`signal.once(...)` 支持
  `func()` 和 `func(SignalEvent)`，返回受管理的 `SignalHandle`。
  回调由普通受管理字段保存和追踪，XRT watch 不持有脚本指针或闭包。
- `handle.dispatch(maxEvents = 64)` 不等待；
  `handle.dispatchFor(timeoutMs, maxEvents = 64)` 只等待第一个事件，
  后续立即排空至上限。返回成功交付的**事件批次数**，不是信号次数。
  timeoutMs 为毫秒，0 不等待、-1 无限等待；上限不得负数。
  嵌套派发拒绝为 RuntimeError，finally 总会释放派发标志。
- `signal.sendTo(channel<SignalEvent>, code, capacity = 64)` 同样返回 handle。
  泵以 trySend 交付，不阻塞：满队列返回本轮已交付数量，保留原事件待重试；
  关闭的 channel 抛 RuntimeError，事件仍保留。
  class 消息按已有 retain/move/drop 合同共享对象，不深复制，不创建变量别名。
  当前只允许无继承、非泛型、且仅含基础值字段的 class（包括 native point
  快照）；拒绝含 class/容器/闭包/资源字段的消息，避免未经 trace 合同证明
  的队列对象图回环。复杂消息图仍需后续 channel 专项，不能仅凭 retain 放行。
  可变 class 的线程安全仍由调用方负责；SignalEvent 本身只读。
- `signal.cancelOn(TaskGroup, code, capacity = 64)` 在泵内请求取消；
  不调用泵就不会自动取消。桥接复用已有任务组；这不代表 TaskGroup 全部
  存量方法已完成迁移或验收。
- `SignalSubscription(code, once = false, capacity = 64)` 可直接读取：
  `next(timeoutMs = 0) as SignalEvent?`，超时/关闭/原生订阅退役后无事件
  返回 null。next 先准备不可变快照，再分配受管理事件，最后消费；
  分配失败不丢失事件，可重试。`prepare/consume` 和
  `SignalEvent(source)` 是同一事务的低层入口；snapshot 要求已 prepare。
- subscription 与 handle 均有只读 `code/active/pending/closed` 和幂等
  `close() as bool`。这些操作及派发必须在创建线程执行，跨线程抛
  RuntimeError。外部宿主创建线程必须在使用期间保持存活；托管创建线程
  身份由原生引用保持。隐式析构不派发脚本，会同步等待记录器退役。
- `SignalEvent` 是独立只读快照，可在订阅关闭后持有；字段为
  `code/systemCode`（int）、`count/total`（uint64）、`time`（time）、
  `name`（owned string）、`coalesced/saturated`（bool）。
  total 是 XRT 的进程级累计，time/systemCode 是本批最新元数据。
- capacity 范围 1..65536。固定环满时合并最新批次，coalesced=true；
  count 溢出饱和至 uint64 最大值，saturated=true。prepare 后的事件不会
  被生产者修改，另占一个槽，所以 pending 至多 capacity+1。
  此标志仅描述适配队列合并；OS/XRT 仍可能合并原生信号，不承诺逐次通知。
- 普通回调在调用前消费，失败传播给泵调用者，**不重放已尝试的回调**。
  once 在调用前释放回调并关闭订阅；回调异常也只尝试一次。
  回调可自行 close，局部受管理引用保证本次调用寿命。close 丢弃未派发事件。
- `signal.ignore/restore/restoreAll/reset` 是进程全局操作，可能使其他
  订阅失活；无限等待每至多 10ms 复查处置和健康状态。
  restore 恢复**注册前处置**，不强行改为默认；reset 停机并恢复处置。
  `signal.count(code = 0) as uint64` 保留无符号总计，0 表示合计；
  `received/clear/healthy/supported/name/raise` 沿用基础合同。
  不要在 once 原生退役、且没有其他 watch 时再次 raise 会终止进程的信号。

旧 `func(var)` / 事件字典 / `channel<var>` 信号路由已移除，不做发布前兼容。
Windows console control 与 POSIX 的平台支持不同；生产退出流程应在泵中
请求正常终止，不在原生 handler 中释放对象、执行脚本或进行复杂 I/O。

## 进程状态与终端

`process.terminalSupported()` 查询当前平台是否能创建伪终端。标准流编号为 `process.stdin`、`process.stdout`、`process.stderr`；重定向模式为 `process.stdioInherit`、`process.stdioPipe`、`process.stdioNull`。

终端启动尺寸用 `ProcessCommand.terminalSize` 指定。Windows 10 系统 ConPTY
可能丢失应用尚未就绪时的 resize 请求，即使平台返回成功；首次动态调整应
先等待应用协议的就绪确认。`Process.resize` 不是应用尺寸观察屏障，库不隐式
重发、延时或替换后端；具体支持合同见[进程终端](进程终端.md)。

`process.open(target, size = -1)` 请求系统关联应用/launcher。按字节选择
前缀后严格拒绝空串、不完整 UTF-8 与内嵌 NUL；长度超过输入时截断。
成功仅表示系统接受请求，不等于应用完成，不产生可等待的 Process；
原生操作失败抛出结构化错误。详细合同与不启动 GUI 的专项验证边界见
`Process配置与运行设计.md`。

进程状态直接对应 XRT：`process.stateRunning` 表示正在运行，`process.stateExited` 表示已经退出。状态只用于观察，不替代 `wait`、`close` 等生命周期操作。
