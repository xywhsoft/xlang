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

显式编码接口不做自动探测。`bom` 只控制 UTF-8 BOM；UTF-16/32 的 LE/BE 已由函数名确定。

```xlang
Buffer utf8 = encoding.encodeUtf8(text)
Buffer utf8Bom = encoding.encodeUtf8(text, true)
string text8 = encoding.decodeUtf8(utf8)

Buffer utf16le = encoding.encodeUtf16LE(text)
Buffer utf16be = encoding.encodeUtf16BE(text)
Buffer utf32le = encoding.encodeUtf32LE(text)
Buffer utf32be = encoding.encodeUtf32BE(text)
string text16le = encoding.decodeUtf16LE(utf16le)
string text16be = encoding.decodeUtf16BE(utf16be)
string text32le = encoding.decodeUtf32LE(utf32le)
string text32be = encoding.decodeUtf32BE(utf32be)
Buffer swapped16 = encoding.swapUtf16Endian(utf16le)
Buffer swapped32 = encoding.swapUtf32Endian(utf32le)
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
```

JSON 访问状态为 `json.visitComplete`、`json.visitStopped`、`json.visitInvalid`。`visitor` 返回 `false` 时得到 `visitStopped`；输入非法时得到 `visitInvalid`。`JsonWriter` 必须成对结束对象和数组后再调用 `finish()`。

## URL、Query 与 XID

```xlang
string built = url.build("https", "example.com", 443, "/api", "a=1")
int port = url.defaultPort("https")
bool defaultPort = url.isDefaultPort(built)
string hostHeader = url.hostHeader(built)

int pairCount = query.count("a=1&a=2")
var first = query.find("a=1&a=2", "a")
array pairs = query.items("a=1&a=2")
dict values = query.toDict("a=1&b=2")
string encoded = query.fromDict(values)

Xid value = id.make()
string xidText = id.encode(value)
Xid decoded = id.decode(xidText)
bool same = id.compare(value, decoded)
```

`query.items` 保留重复键和原始顺序；`query.toDict` 采用字典语义，重复键只保留最终值。需要无损往返时使用 `items`。

## 正则表达式

```xlang
RegexBuilder builder = regex.builder(pattern)
bool patternOk = regex.valid(pattern)
string message = regex.error(pattern)
int charPosition = regex.errorPos(pattern)
int bytePosition = regex.errorBytePos(pattern)
```

`errorPos` 使用 UTF-8 字符索引，`errorBytePos` 使用字节偏移；只有需要向底层字节协议报告位置时才应使用后者。

## 网络配置对象

```xlang
NetAddress address = net.resolve("localhost", 443)
NetProxy proxy = net.proxy(net.proxySocks5, "127.0.0.1", 1080)
NetEngine engine = net.engine(0, net.engineAutoWorkers)
TcpConnectConfig connect = tcp.connectConfig("example.com", 443)
TcpListenConfig listen = tcp.listenConfig(8080, "0.0.0.0")
UdpConfig datagram = udp.config(9000, "0.0.0.0")
```

代理类型为 `net.proxyNone`、`net.proxySocks5`、`net.proxyHttpConnect`。关闭策略为 `net.closeAbort`、`net.closeGraceful`、`net.closeWaitPeer`，可按底层 API 要求组合。

TCP 连接标志为 `tcp.connectNoDelay`、`tcp.connectKeepAlive`；监听标志为 `tcp.listenReuseAddr`、`tcp.listenReusePort`、`tcp.listenNoDelay`、`tcp.listenKeepAlive`。UDP 标志为 `udp.reuseAddr`、`udp.reusePort`。

## 任务组

`task.group()` 创建结构化并发的 `TaskGroup`。任务加入组后可统一等待、回收和关闭；`close()` 之后不再接受新任务。

```xlang
TaskGroup group = task.group()
group.add(work)
group.waitTimeout(5000)
group.close()
```

## 进程状态与终端

`process.terminalSupported()` 查询当前平台是否能创建伪终端。标准流编号为 `process.stdin`、`process.stdout`、`process.stderr`；重定向模式为 `process.stdioInherit`、`process.stdioPipe`、`process.stdioNull`。

进程状态为 `process.stateFailed`、`process.stateInit`、`process.stateRunning`、`process.stateExited`、`process.stateClosed`。状态只用于观察，不替代 `wait`、`close` 等生命周期操作。
