# XRT 扩展标准库 API

本文记录已经由 XRT 提供、并由 xlang 标准库公开的扩展能力。这里是当前口径，不承担旧版本兼容说明。

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
Template loaded = template.load(path)
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

`signal` 标准库由 XRT signal 运行时提供。Windows 接入 console control handler；POSIX 使用系统 signal handler 与后台分发线程。信号回调应保持轻量，服务端和任务系统建议优先使用 `signal.sendTo(...)` 或 `signal.cancelOn(...)` 做 graceful shutdown。

```xlang
println(signal.name(signal.HUP))
println(signal.name(signal.INT))
println(signal.name(signal.TERM))
println(signal.name(signal.BREAK))
println(signal.name(signal.CLOSE))
println(signal.name(signal.LOGOFF))
println(signal.name(signal.SHUTDOWN))

bool ok = signal.supported(signal.INT)

func(var) onSignal = func(var event)
    println(event["name"], event["count"])
end

func() onOnce = func()
    println("only once")
end

SignalHandle direct = signal.on(signal.INT, onSignal)
SignalHandle once = signal.once(signal.INT, onOnce)

channel<var> events = channel::<var>(2)
SignalHandle route = signal.sendTo(events, signal.INT)

signal.raise(signal.INT)

var event = null
events.recvInto(event)

bool received = signal.received(signal.INT)
int count = signal.count(signal.INT)
signal.clear(signal.INT)

TaskGroup group = task.group()
SignalHandle cancel = signal.cancelOn(group, signal.TERM)

direct.close()
once.close()
route.close()
cancel.close()
group.close()
events.close()

signal.restore(signal.INT)
signal.restore(signal.TERM)
signal.reset()
```

`signal.on(code, handler)` 和 `signal.once(code, handler)` 支持 `func()` 与 `func(var)` 两种 handler。`func(var)` 收到的事件字典包含 `code`、`platformCode`、`count`、`time`、`name`。`signal.sendTo(channel<var>, code)` 会把事件字典送入 `channel<var>`，发送前会提升为 XRT shared 模式，适合和主循环、任务组、服务端 drain/stop 逻辑协同。`SignalHandle.close()` 取消注册，`active` 和 `code` 可用于检查句柄状态。`signal.restore(code)` 恢复指定信号默认行为；`signal.reset()` 恢复全部 signal 默认行为。

## 进程状态与终端

`process.terminalSupported()` 查询当前平台是否能创建伪终端。标准流编号为 `process.stdin`、`process.stdout`、`process.stderr`；重定向模式为 `process.stdioInherit`、`process.stdioPipe`、`process.stdioNull`。

进程状态直接对应 XRT：`process.stateRunning` 表示正在运行，`process.stateExited` 表示已经退出。状态只用于观察，不替代 `wait`、`close` 等生命周期操作。
