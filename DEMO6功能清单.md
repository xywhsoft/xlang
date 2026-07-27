# DEMO6 功能清单

> 更新日期：2026-07-16
>
> 本清单用于快速核对当前 `demo6/release/xl.exe` 已实现能力、标准写法、测试入口和验收边界。DEMO6 不背负 demo3/demo4/demo5 的历史兼容口径；旧版本文档只能作为历史参考。

## 1. 当前状态

- 编译管线固定为四阶段：词法分析、语法分析、语义检查/IR、C 代码生成/TCC 后端。
- 源文件固定 UTF-8；标识符大小写敏感；支持中文 UTF-8 标识符；保留字不能作为普通标识符。
- `xl run` 支持内存态 TCC 运行；默认不应生成 `tmp` 或 `release/tmp` 临时产物，除非显式要求保存中间文件。
- XRT 是 xlang 标准库和运行时基础；能由 XRT 提供的能力优先使用 XRT，不在 xlang 里重复制造平行实现。
- 本轮暂缓扩展 `http` 与 `websocket` 两个库；现有能力可继续使用，但不作为本轮新增和压实目标。

## 2. 命令行

```bat
xl lex <file.xl> [--format text|json] [--emit output.tokens]
xl parse <file.xl> [--format text|json] [--emit output.ast]
xl check <file.xl>
xl ir <file.xl> [--format text|json] [--emit output.xlir]
xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]
xl abi <file.xl> [--json] [--emit output.abi.txt]
xl run <file.xl|file.c|file.xlp> [helper.c ...] [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]
xl build <file.xl|file.c|file.xlp> [helper.c ...] [-o <output.exe|dll>] [-shared] [--res file.json|file.xson] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]
xl impdef <file.dll> -o <output.def>
```

- `xl build main.xl` 的 `-o` 可省略，默认输出为第一个输入文件名加平台可执行后缀；Windows 为 `.exe`。
- `.xl` 与 `.c` 可以混合传入，默认注入 XRT/xlang/TCC 所需环境。
- C 文件运行和编译走同一套内嵌 TCC/VFS 路径。

## 3. 语言基础

- 注释：`//` 单行注释；`/* ... */` 多行注释，支持嵌套。
- 语句分隔：换行和 `;` 均可分隔语句；`()`, `[]`, `{}` 内部换行不结束语句。
- 字面量：十进制整数、`0x` 十六进制、`0o` 八进制、`0b` 二进制、浮点、科学计数、字符串、raw string、多行字符串、f-string/rf-string、time literal、数组/列表/字典/集合字面量。
- 已移除旧口径：不支持 `0X`、`0O`、`0B`、`&H`、`&O`、`&B` 数字前缀；不支持 `->` 返回值语法。
- 函数返回值统一使用 `as`：`func foo(int a) as int`；多返回值使用 `as (int, string)`。
- 函数类型使用 `func`：`func(int) as int proc = foo`。
- 可选参数使用默认值：`func foo(int a = 1)`。
- xlang 可变参数：`...` 为数组可变参数，`***` 为字典可变参数；函数体通过 `arg` 访问。
- C vararg 使用 `cvararg`：`extern "printf" func printf(string fmt, cvararg) as c_int`。
- 支持命名参数；不存在的命名参数可进入 `***` 字典可变参数。
- 顶层语句可以和 `main` 共存，顶层语句先于 `main` 执行。

## 4. 类型与转换

- 基础类型：`null`、`bool`、`int`、`int8/uint8`、`int16/uint16`、`int32/uint32`、`int64/uint64`、`float`、`float32/float64`、`string`、`time`、`point`、`type`、`function`。
- ABI C 类型：`c_char`、`c_schar`、`c_uchar`、`c_short`、`c_ushort`、`c_int`、`c_uint`、`c_long`、`c_ulong`、`c_longlong`、`c_ulonglong`、`c_size`、`c_ssize`、`c_ptrdiff`、`c_intptr`、`c_uintptr`、`c_bool`、`c_float`、`c_double`、`c_wchar`。
- 容器类型：`array<T>`、`list<T>`、`set<T>`、`dict<T>`；动态 `var` 可保存基础值、容器、函数和 class/native-backed class 对象。
- `auto` 用于局部推断；`type` 是真实类型值，可用于 `type(a) == int`。
- 类型标识符可用于声明、构造/转换、type 字面量比较。
- 支持安全拓宽转换；危险窄化或 ABI 截断应显式转换或报错。
- 整数字面量必须落在目标类型范围内；无显式无符号目标时按有符号 `int64` 解释，`auto` / `var` 不会把超出 `INT64_MAX` 的值静默环绕。
- `uint64` 转字符串、f-string 和 `print` / `println` 均按十进制无符号全精度输出。
- 支持运行时 `is` / `as` 类型判断与转换；函数返回类型的 `as` 和表达式中的 `as` 由语法位置区分。

## 5. 表达式、控制流与容器

- 控制流：`if then else elseif end`、`while ... end`、`do ... while ... end`、`for in end`、`break`、`continue`、`return`。
- 逻辑关键字：`and`、`or`、`not`。
- range 和 `in` 按 Python 风格收口；`~=` 按 XRT 模式匹配机制收口。
- 数组、列表、集合、字典支持字面量、索引、成员方法和运算符重载。
- 容器运算符：数组/列表拼接，集合并交差补，字典合并等走 XRT 容器能力。
- 字符串默认 UTF-8；`string.len` 按 codepoint，`string.byteLen` 按字节；默认字符串迭代按字符。
- `Buffer` 用作二进制载体和高性能字符串拼接缓冲，支持索引、迭代、拼接和文件二进制读写。

## 6. 函数、闭包、泛型

- 函数是一等公民，可赋值、传参、调用。
- 函数签名可表达参数类型、返回类型和多返回值。
- 闭包支持捕获外部变量。
- 泛型函数、泛型 class、静态泛型容器可用。
- 泛型实例化调用采用 Rust 风格 turbofish：`max::<int>(a, b)`。
- 泛型约束语法采用简化写法，例如 `func cloneAll<T is cloneable>(array<T> items) as array<T>`。

## 7. OOP 与协议

- `class` 是静态类基座，支持字段、方法、继承、构造、析构、访问控制、静态成员、override/final、virtual 动态派发。
- `object` 已并入 class 体系：`extends object` 获得动态字段能力；object 不再是独立黑盒类型。
- 旧 `custom` 用户语法已移除；原 custom 能力并入 native-backed class。
- native-backed class 支持 native handle、extern create/destroy、extern 成员函数、生命周期映射、重载、XAPI 和 C ABI。
- 协议方法使用双下划线命名，避免占用普通用户命名：`__item__`、`__setItem__`、`__field__`、`__setField__`、`__contains__`、`__call__`、`__iter__`、`__next__`、`__add__`、`__eq__` 等。
- 属性系统使用 `get` / `let`，支持维度参数和重载；`get`、`let`、`set` 不允许作为普通标识符。
- 支持 `derive string/hash/equals`，用于生成字符串化、哈希和等价比较。
- `copy` 与 `clone` 标准库函数接入协议能力；`clone` 表示深拷贝语义。

## 8. 错误、文档、模块与扩展库

- 异常处理：`try` / `catch` / `finally` / `throw` / `assert`。
- 错误模型参考 Python 分类，避免只依赖字符串判断异常。
- 运行时整数除零/模零抛出 `ZeroDivisionError`；`INT64_MIN / -1` 及对应取模抛出 `OverflowError`，二者均可由 `ArithmeticError` 捕获。
- `throw` 只抛出字符串描述，由运行时包装为分类错误。
- 文档注释：`#doc ... #end`，支持 `desc`、`param`、`return`、`note`、`example`。
- 模块系统：`import`、`export`、package、XAPI/interface metadata。
- 扩展库推荐通过 `export func name(...)` 导出能力；内部 wrapper 名称不作为用户手写接口。
- 支持同一扩展库多版本共存，降低传递依赖版本冲突。

## 9. 并发、异步与运行时

- 支持 `async`、`await`、`yield`、future/task、thread、mutex、cond、semaphore、channel、generator。
- 支持 task 高层 API：spawn/all/race/cancel/whenAny 等当前已实现接口。
- `channel<T>` 支持 `send`、`trySend`、`recvInto`、`tryRecvInto`、`recvTimeoutInto(outValue, timeoutMs)`、`count`、`close`、`isClosed`；`recvTimeoutInto` 使用 XRT `xrtChannelRecvTimeout`，`timeoutMs < 0` 等价阻塞接收，`0` 表示立即超时检查。
- 支持同步与异步基础设施向 XRT 下沉；网络、协程、任务能力以后继续以 XRT 为基础暴露。
- `async generator` 复杂融合已实现/压实范围以当前测试为准；新增高级组合需单独补设计和测试。

## 10. C 混编、TCC 与 DLL

- xlang 编译结果是 C 代码，可与 C 文件混合编译。
- C 函数通过声明进入 xlang；xlang 导出给 C 通过 `export`、ABI 元数据、自动头文件实现。
- C 可访问 xlang/XRT 数据结构；复杂 class/function/object 通过稳定 ABI 和 XRT 协议接口访问。
- `xl.dll` 面向内嵌运行；`xlc.dll` 面向编译能力；二者边界保持独立。
- TCC API 通过 `tcc.Context` 和 `tcc.*` 标准库能力暴露；运行态默认注入 XRT 与 xlang 导出符号。
- 内置 VFS 查询顺序：动态 VFS -> 外部文件系统 -> 静态 VFS。

## 11. 标准库总览

当前标准库公开函数为 515 个，标准库常量为 115 个。以下列表由 demo6/src/stdlib/xl6_stdlib_data.c 自动提取。

### codec

- `codec.base64Decode()`、`codec.base64DecodeBytes()`、`codec.base64Encode()`、`codec.base64EncodeBytes()`、`codec.formUrlDecode()`、`codec.formUrlEncode()`、`codec.hexDecode()`、`codec.hexDecodeBytes()`、`codec.hexEncode()`、`codec.hexEncodeBytes()`、`codec.percentDecode()`、`codec.percentEncode()`

### crypto

- `crypto.aes128GcmDecrypt()`、`crypto.aes128GcmDecryptBytes()`、`crypto.aes128GcmEncrypt()`、`crypto.aes128GcmEncryptBytes()`、`crypto.aes256GcmDecrypt()`、`crypto.aes256GcmDecryptBytes()`、`crypto.aes256GcmEncrypt()`、`crypto.aes256GcmEncryptBytes()`、`crypto.chacha20Bytes()`、`crypto.chacha20Decrypt()`、`crypto.chacha20Encrypt()`、`crypto.chacha20Poly1305Decrypt()`、`crypto.chacha20Poly1305DecryptBytes()`、`crypto.chacha20Poly1305Encrypt()`、`crypto.chacha20Poly1305EncryptBytes()`、`crypto.ecdsaVerify()`、`crypto.ed25519Keypair()`、`crypto.ed25519PublicKey()`、`crypto.ed25519Sign()`、`crypto.ed25519Verify()`、`crypto.hkdfSha256()`、`crypto.hkdfSha256Bytes()`、`crypto.hkdfSha384()`、`crypto.hkdfSha384Bytes()`、`crypto.hmacSha256()`、`crypto.hmacSha256Bytes()`、`crypto.hmacSha384()`、`crypto.hmacSha384Bytes()`、`crypto.hmacSha512()`、`crypto.hmacSha512Bytes()`、`crypto.p256Keypair()`、`crypto.p256SharedSecret()`、`crypto.p384Keypair()`、`crypto.p384SharedSecret()`、`crypto.randomBytes()`、`crypto.randomHex()`、`crypto.rsaPkcs1Verify()`、`crypto.rsaPssVerify()`、`crypto.sha1()`、`crypto.sha1Bytes()`、`crypto.sha1Stream()`、`crypto.sha256()`、`crypto.sha256Bytes()`、`crypto.sha256Stream()`、`crypto.sha384()`、`crypto.sha384Bytes()`、`crypto.sha384Stream()`、`crypto.sha512()`、`crypto.sha512Bytes()`、`crypto.sha512Stream()`、`crypto.x25519Keypair()`、`crypto.x25519SharedSecret()`、`crypto.x448Keypair()`、`crypto.x448SharedSecret()`

### dict

- `dict.remove()`

### dir

- `dir.clean()`、`dir.copy()`、`dir.copyAsync()`、`dir.count()`、`dir.create()`、`dir.createAll()`、`dir.createAllAsync()`、`dir.createAsync()`、`dir.delete()`、`dir.deleteAsync()`、`dir.dirs()`、`dir.empty()`、`dir.ensureEmpty()`、`dir.entries()`、`dir.exists()`、`dir.files()`、`dir.glob()`、`dir.list()`、`dir.match()`、`dir.move()`、`dir.moveAsync()`、`dir.size()`、`dir.walk()`

### encoding

- `encoding.charSize()`、`encoding.convertBytes()`、`encoding.decodeUtf16BE()`、`encoding.decodeUtf16LE()`、`encoding.decodeUtf32BE()`、`encoding.decodeUtf32LE()`、`encoding.decodeUtf8()`、`encoding.detect()`、`encoding.detectBytes()`、`encoding.encodeUtf16BE()`、`encoding.encodeUtf16LE()`、`encoding.encodeUtf32BE()`、`encoding.encodeUtf32LE()`、`encoding.encodeUtf8()`、`encoding.fromBytes()`、`encoding.isUtf8()`、`encoding.isUtf8Bytes()`、`encoding.name()`、`encoding.swapUtf16Endian()`、`encoding.swapUtf32Endian()`、`encoding.toBytes()`

### file

- `file.accessTime()`、`file.appendBytes()`、`file.appendText()`、`file.appendTextAsync()`、`file.attr()`、`file.changeTime()`、`file.copy()`、`file.copyAsync()`、`file.delete()`、`file.deleteAsync()`、`file.exists()`、`file.info()`、`file.move()`、`file.moveAsync()`、`file.open()`、`file.openAsync()`、`file.readBytes()`、`file.readBytesAsync()`、`file.readText()`、`file.readTextAsync()`、`file.rename()`、`file.replace()`、`file.setAttr()`、`file.setSize()`、`file.size()`、`file.touch()`、`file.writeBytes()`、`file.writeBytesAsync()`、`file.writeText()`、`file.writeTextAsync()`、`file.writeTextAtomic()`

### float

- `float.format()`

### hash

- `hash.u32()`、`hash.u32WithSeed()`、`hash.u64()`、`hash.u64Micro()`、`hash.u64MicroWithSeed()`、`hash.u64Nano()`、`hash.u64NanoWithSeed()`、`hash.u64WithSeed()`

### http

- `http.client()`、`http.copyChunk()`、`http.get()`、`http.post()`、`http.request()`

### http.server

- `http.server.any()`、`http.server.anyRequest()`、`http.server.anyStreamRequest()`、`http.server.app()`、`http.server.appAny()`、`http.server.appAnyRequest()`、`http.server.appAnyStreamRequest()`、`http.server.appError()`、`http.server.appGet()`、`http.server.appGetRequest()`、`http.server.appPost()`、`http.server.appPostRequest()`、`http.server.appPostStreamRequest()`、`http.server.appRoute()`、`http.server.appRouteRequest()`、`http.server.appRouteStreamRequest()`、`http.server.appStatic()`、`http.server.appStaticEx()`、`http.server.chunkBytes()`、`http.server.chunkText()`、`http.server.connectionCount()`、`http.server.create()`、`http.server.createTls()`、`http.server.destroy()`、`http.server.destroyApp()`、`http.server.drain()`、`http.server.error()`、`http.server.get()`、`http.server.getRequest()`、`http.server.host()`、`http.server.mimeByPath()`、`http.server.port()`、`http.server.post()`、`http.server.postRequest()`、`http.server.postStreamRequest()`、`http.server.reloadApp()`、`http.server.removeHost()`、`http.server.removeTlsHost()`、`http.server.requestCount()`、`http.server.route()`、`http.server.routeRequest()`、`http.server.routeStreamRequest()`、`http.server.setApp()`、`http.server.start()`、`http.server.static()`、`http.server.staticEx()`、`http.server.stop()`、`http.server.tlsHost()`

### id

- `id.compare()`、`id.decode()`、`id.encode()`、`id.make()`、`id.xid()`

### int

- `int.format()`

### json

- `json.parse()`、`json.read()`、`json.stringify()`、`json.valid()`、`json.visit()`、`json.write()`、`json.writer()`

### log

- `log.addConsole()`、`log.addFile()`、`log.addRollingFile()`、`log.create()`、`log.debug()`、`log.default()`、`log.error()`、`log.fatal()`、`log.flush()`、`log.getLevel()`、`log.info()`、`log.levelName()`、`log.setLevel()`、`log.trace()`、`log.warn()`、`log.write()`
- `Logger(name)` / `log.create(name)`：创建独立 logger；`log.default()` 包装默认 logger。
- `Logger`：`setLevel()`、`getLevel()`、`makeDefault()`、`addConsole()`、`addFile()`、`addRollingFile()`、`consoleAppender()`、`fileAppender()`、`rollingFileAppender()`、`customAppender()`、`removeAppender()`、`trace()`、`debug()`、`info()`、`warn()`、`error()`、`fatal()`、`write()`、`flush()`。
- `LogAppender`：`name`、`setLevel()`、`getLevel()`、`setFormat()`、`getFormat()`、`setColor()`、`getColor()`，用于控制单个 appender。

### math

- `math.abs()`、`math.acos()`、`math.asin()`、`math.atan()`、`math.atan2()`、`math.cbrt()`、`math.ceil()`、`math.clamp()`、`math.cos()`、`math.deg()`、`math.exp()`、`math.exp2()`、`math.expm1()`、`math.floor()`、`math.fract()`、`math.hypot()`、`math.isFinite()`、`math.isInf()`、`math.isNaN()`、`math.log()`、`math.log10()`、`math.log1p()`、`math.log2()`、`math.max()`、`math.min()`、`math.mod()`、`math.pow()`、`math.rad()`、`math.round()`、`math.sign()`、`math.sin()`、`math.sqrt()`、`math.tan()`、`math.trunc()`

### net

- `net.engine()`、`net.localIP()`、`net.localMAC()`、`net.localName()`、`net.proxy()`、`net.resolve()`

### TLS 原始会话对象

- `TlsConfig`：配置主机名、ALPN 协议列表、证书、CA/CRL、是否校验 peer、证书验证回调、TLS 版本上限、TLS 1.2 Ed25519 兼容开关，并创建 `TlsSession`。
- `TlsConfig.alpn("h2,http/1.1") as bool`：设置逗号分隔的 ALPN 协议列表；服务端按自身列表顺序选择客户端也支持的第一个协议。
- `TlsConfig.verifyCallback(func(var) as bool handler) as bool`：设置最终证书验证回调；事件表包含 `defaultOk`、`certSize`、`certHex`，返回 `true` 接受证书，返回 `false` 拒绝握手。
- `TlsSession`：暴露 XRT TLS 状态机，支持 `handshake()`、`feed()`、`peekCipher()`、`consumeCipher()`、`write()`、`read()`、`close()`、`sni()`、`alpn()`、`exportResume()`、`certificate()`、`certificateData()`、`allowTls12Ed25519()`。
- `TlsSession.alpn() as string`：握手完成后返回协商出的 ALPN 协议名；未协商时返回空字符串。
- `TlsSession.peerCert() as Buffer`：握手完成后返回对端叶子证书 DER 的独立 `Buffer` 副本；握手前或对端未提供证书时返回空 `Buffer`。
- `TlsResume`：持有 TLS session resume 信息，用于后续会话恢复。

### os

- `os.args()`、`os.chain()`、`os.cwd()`、`os.env()`、`os.exec()`、`os.platform()`、`os.run()`、`os.setCwd()`、`os.start()`、`os.startFile()`

### path

- `path.abs()`、`path.appDir()`、`path.cwd()`、`path.dir()`、`path.exists()`、`path.ext()`、`path.home()`、`path.isAbs()`、`path.join()`、`path.name()`、`path.nameExt()`、`path.normalize()`、`path.random()`、`path.rel()`、`path.sep()`、`path.setCwd()`、`path.temp()`、`path.withExt()`、`path.withName()`

### (global)

- `print()`、`println()`、`write()`、`writeln()`

### process

- `process.cmd()`、`process.exec()`、`process.exitCode()`、`process.pipeline()`、`process.run()`、`process.runFile()`、`process.shell()`、`process.start()`、`process.startFile()`、`process.success()`、`process.terminalSupported()`

### query

- `query.append()`、`query.count()`、`query.decode()`、`query.encode()`、`query.find()`、`query.fromDict()`、`query.items()`、`query.pair()`、`query.toDict()`、`query.valid()`

### random

- `random.range()`、`random.seed()`、`random.string()`、`random.u32()`、`random.u64()`

### regex

- `regex.builder()`、`regex.compile()`、`regex.error()`、`regex.errorBytePos()`、`regex.errorPos()`、`regex.find()`、`regex.fullMatch()`、`regex.isMatch()`、`regex.match()`、`regex.set()`、`regex.valid()`

### signal

- `signal.cancelOn()`、`signal.clear()`、`signal.count()`、`signal.name()`、`signal.on()`、`signal.once()`、`signal.raise()`、`signal.received()`、`signal.reset()`、`signal.restore()`、`signal.sendTo()`、`signal.supported()`

### string

- `string.approx()`、`string.byteAt()`、`string.byteLen()`、`string.bytes()`、`string.charAt()`、`string.chars()`、`string.compare()`、`string.contains()`、`string.count()`、`string.endsWith()`、`string.equals()`、`string.filter()`、`string.format()`、`string.hasAny()`、`string.indexOf()`、`string.insert()`、`string.isBlank()`、`string.isEmpty()`、`string.isUtf8()`、`string.join()`、`string.lastIndexOf()`、`string.left()`、`string.len()`、`string.like()`、`string.lower()`、`string.ltrim()`、`string.mid()`、`string.padCenter()`、`string.padLeft()`、`string.padRight()`、`string.remove()`、`string.removeAt()`、`string.repeat()`、`string.replace()`、`string.reverse()`、`string.right()`、`string.rtrim()`、`string.sim()`、`string.slice()`、`string.split()`、`string.splitLines()`、`string.startsWith()`、`string.trim()`、`string.upper()`

### task

- `task.spawn(funcValue, ...args)`：将完整签名的 `func` 值提交为后台任务，返回 `future<T>`；支持普通函数、闭包、成员函数适配值和可变参数函数。
- `task.all(array<future<T>> items)`：等待全部 future 完成，成功时返回 `array<T>`，失败或取消时按聚合 future 的错误语义传播。
- `task.race(array<future<T>> items)`：等待第一个完成的 future，成功、失败或取消均按第一个完成结果传播，并向 loser 发出协作式取消请求。
- `task.whenAny(array<future<T>> items)`：等待任意 future 完成，返回 `future<int>`，`await` 后得到完成 source 的索引。
- `task.cancel(futureOrTask)`：向 future/task 发出协作式取消请求，返回是否成功发出取消。
- `task.group()`：创建结构化并发 `TaskGroup`，支持 `add`、`wait`、`waitTimeout`、`joinAsync`、`close` 等生命周期管理。

### special collections

- `Stack<T>`：`Stack::<T>()` 创建 XRT typed stack；支持 `push(value)`、`popInto(outValue)`、`peekInto(outValue)`、`clear()`、`count()`。
- `AvlTree<K, V>`：`AvlTree::<K, V>()` 创建 XRT typed AVL tree；支持 `set(key, value)`、`getInto(key, outValue)`、`contains(key)`、`remove(key)`、`clear()`、`count()`、`keys()`、`values()`。
- `SpscQueue<T>`、`MpscQueue<T>`、`MpmcQueue<T>`、`WaitQueue<T>`：`Type::<T>(capacity)` 创建 typed queue；支持 `tryPush(value)`、`tryPopInto(outValue)`、`popInto(outValue)`、`popTimeoutInto(outValue, timeoutMs)`、`count()`、`close()`、`isClosed()`、`isDrained()`。

### tcc

- `tcc.compile()`、`tcc.compileFile()`、`tcc.Context()`、`tcc.run()`、`tcc.runFile()`、`tcc.vfsClear()`、`tcc.vfsMount()`

### tcp

- `tcp.connect()`、`tcp.connectAsync()`、`tcp.connectConfig()`、`tcp.listen()`、`tcp.listenConfig()`

### template

- `template.compile()`、`template.engine()`、`template.load()`、`template.render()`、`template.renderFile()`

### time

- `time.addDays()`、`time.addHours()`、`time.addMinutes()`、`time.addMonths()`、`time.addQuarters()`、`time.addSeconds()`、`time.addWeeks()`、`time.addYears()`、`time.approx()`、`time.clock()`、`time.datePart()`、`time.dateString()`、`time.day()`、`time.dayOfYear()`、`time.daysInMonth()`、`time.daysInYear()`、`time.diffDays()`、`time.diffHours()`、`time.diffMinutes()`、`time.diffMonths()`、`time.diffQuarters()`、`time.diffSeconds()`、`time.diffWeeks()`、`time.diffYears()`、`time.firstDayOfMonth()`、`time.firstDayOfWeek()`、`time.firstDayOfYear()`、`time.format()`、`time.formatPattern()`、`time.fromDate()`、`time.fromDateTime()`、`time.fromTime()`、`time.fromUnix()`、`time.hour()`、`time.inRange()`、`time.isLeapYear()`、`time.isSameDay()`、`time.isSameMonth()`、`time.isSameYear()`、`time.lastDayOfMonth()`、`time.lastDayOfWeek()`、`time.lastDayOfYear()`、`time.localToUTC()`、`time.minute()`、`time.month()`、`time.now()`、`time.nowUTC()`、`time.parse()`、`time.parsePattern()`、`time.quarter()`、`time.rangeOverlap()`、`time.relative()`、`time.second()`、`time.sleep()`、`time.timePart()`、`time.timer()`、`time.timeString()`、`time.timezoneOffset()`、`time.today()`、`time.toUnix()`、`time.tryParse()`、`time.tryParsePattern()`、`time.utcToLocal()`、`time.weekday()`、`time.weekOfMonth()`、`time.weekOfYear()`、`time.year()`

### udp

- `udp.bind()`、`udp.config()`

### url

- `url.authority()`、`url.build()`、`url.defaultPort()`、`url.fragment()`、`url.host()`、`url.hostHeader()`、`url.isAbsolute()`、`url.isDefaultPort()`、`url.isSecure()`、`url.normalizePath()`、`url.path()`、`url.port()`、`url.query()`、`url.resolve()`、`url.scheme()`、`url.target()`、`url.userInfo()`、`url.valid()`

### xlang

- `xlang.Context()`、`xlang.run()`、`xlang.runFile()`

### xson

- `xson.options()`、`xson.parse()`、`xson.read()`、`xson.stringify()`、`xson.valid()`、`xson.visit()`、`xson.write()`、`xson.writer()`

## 12. 标准库常量

### encoding

- `encoding.autoDetect`、`encoding.binary`、`encoding.bom`、`encoding.oem`、`encoding.utf16`、`encoding.utf16BE`、`encoding.utf32`、`encoding.utf32BE`、`encoding.utf8`

### file

- `file.asyncCreate`、`file.asyncRead`、`file.asyncTruncate`、`file.asyncWrite`、`file.seekCur`、`file.seekEnd`、`file.seekSet`、`file.shareDelete`、`file.shareRead`、`file.shareWrite`

### http

- `http.errorCallbackAbort`、`http.errorCancelled`、`http.errorConnect`、`http.errorConnectionClosed`、`http.errorInvalidArgument`、`http.errorInvalidUrl`、`http.errorNone`、`http.errorOutOfMemory`、`http.errorProtocol`、`http.errorTimeoutIdle`、`http.errorTimeoutTotal`、`http.errorTransport`、`http.errorWrite`、`http.phaseComplete`、`http.phaseConnect`、`http.phaseNone`、`http.phasePrepare`、`http.phaseResponseBody`、`http.phaseResponseHeaders`、`http.phaseWrite`、`http.responseChunked`、`http.responseKeepAlive`、`http.responseUpgrade`

### http.server

- `http.server.allowBackslash`、`http.server.allowDotfiles`、`http.server.cookieHttpOnly`、`http.server.cookiePartitioned`、`http.server.cookieSameParty`、`http.server.cookieSecure`、`http.server.sameSiteLax`、`http.server.sameSiteNone`、`http.server.sameSiteStrict`、`http.server.sameSiteUnspecified`

### json

- `json.visitComplete`、`json.visitInvalid`、`json.visitStopped`

### log

- `log.debugLevel`、`log.errorLevel`、`log.fatalLevel`、`log.infoLevel`、`log.jsonFormat`、`log.offLevel`、`log.simpleFormat`、`log.textFormat`、`log.traceLevel`、`log.warnLevel`

### math

- `math.e`、`math.inf`、`math.nan`、`math.pi`、`math.tau`

### net

- `net.closeAbort`、`net.closeGraceful`、`net.closeWaitPeer`、`net.engineAutoWorkers`、`net.proxyHttpConnect`、`net.proxyNone`、`net.proxySocks5`

### process

- `process.stateClosed`、`process.stateExited`、`process.stateFailed`、`process.stateInit`、`process.stateRunning`、`process.stderr`、`process.stdin`、`process.stdioInherit`、`process.stdioNull`、`process.stdioPipe`、`process.stdout`

### regex

- `regex.dotAll`、`regex.ignoreCase`、`regex.multiline`、`regex.ungreedy`

### signal

- `signal.BREAK`、`signal.CLOSE`、`signal.HUP`、`signal.INT`、`signal.LOGOFF`、`signal.SHUTDOWN`、`signal.TERM`

### tcc

- `tcc.DLL`、`tcc.EXE`、`tcc.MEMORY`、`tcc.OBJ`、`tcc.PREPROCESS`

### tcp

- `tcp.connectKeepAlive`、`tcp.connectNoDelay`、`tcp.listenKeepAlive`、`tcp.listenNoDelay`、`tcp.listenReuseAddr`、`tcp.listenReusePort`

### udp

- `udp.reuseAddr`、`udp.reusePort`

### xson

- `xson.visitComplete`、`xson.visitInvalid`、`xson.visitStopped`

## 13. JSON / XSON visitor/writer 口径

- `json.visit(text, visitor)` 遍历 JSON 值树并回调 visitor。visitor 参数是事件字典，字段包括：`type`、`depth`、`key`、`index`、`value`。
- JSON visitor 返回 `true` 继续遍历，返回 `false` 停止遍历；返回码：`json.visitComplete`、`json.visitStopped`、`json.visitInvalid`。
- `json.writer(pretty = false)` 创建流式 JSON 写入器 `JsonWriter`，支持 object/array、key 字段、标量、value 与 `finish()`；writer 只允许一个根值，未闭合容器或写入非法 key 时 `finish()` 返回空字符串。

- `xson.visit(text, visitor)` 与 `xson.visit(text, visitor, options)` 遍历 XSON 值树并回调 visitor。`xson.writer(pretty = false)` 与 `xson.writer(options)` 创建流式 XSON 写入器。
- visitor 参数是事件字典，字段包括：`type`、`depth`、`key`、`index`、`value`。
- 返回 `true` 继续遍历，返回 `false` 停止遍历。
- 返回码：`xson.visitComplete`、`xson.visitStopped`、`xson.visitInvalid`。
- `XsonWriter` 支持 array/list/dict/set、key 字段、list 稀疏 index、标量、time、value 与 `finish()`；time 值可通过 `writeTime(value)`、`writeTime(key, value)`、`writeTime(index, value)` 或 `writeValue(varTime)` 写入；`xson.writer(options)` 与 stringify/write/visit 共用 `XsonOptions` 口径。

## 14. 自带数据格式与归档扩展库

这些模块随 `xl.exe` 发布，但采用源码 package 按需编译；未 `import` 时不会把对应格式引擎链接进程序。二进制统一使用 `Buffer`，文本统一使用 UTF-8 `string`，文件路径统一经过 XRT UTF-8/VFS 能力。

- `gzip`：`compress/decompress`、增量 compressor/decompressor、流式 `compressFile/decompressFile`；使用官方 zlib 1.3.2。
- `bz2`：与 gzip 对齐的一次性、增量和文件流接口；使用官方 libbzip2 1.0.8。
- `zip`：基于 libzip 1.11.4，支持内存/文件归档、ZIP64、stored/Deflate、目录、读取、添加、覆盖、删除、重命名、注释、提取与安全路径检查。
- `tar`：支持 ustar、PAX、GNU longname/longlink、内存/文件归档、顺序读取和安全提取；硬链接可前向引用。符号链接、硬链接、FIFO 必须显式允许，设备节点不执行提取。
- `csv`：Reader/Writer/DictReader/DictWriter，支持引号内换行、自定义分隔符、BOM、严格模式和资源限制。
- `ini`：保留 section/key 顺序与注释，支持类型 getter、大小写策略、重复项策略和 `${section.key}` 显式插值。
- `toml`：`parse/load/stringify/save`，支持 TOML 原生 local/offset date-time 无损往返。
- `plist`：XML 与 binary plist；Data 映射 `Buffer`，Date 映射 `time`，UID 使用专用 `UID` 值。
- `netrc`：machine/default/macdef 解析、查询、修改和保存；公开字符串不会泄露密码。
- `xml.sax`：流式事件与回调。
- `xml.etree`：轻量元素树、属性、查询、修改和序列化。
- `xml.dom`：完整文档树，保留元素、文本、注释、PI、父子关系和节点顺序。

```xlang
import gzip
import zip

Buffer packed = gzip.compress("hello")
assert gzip.decompressText(packed) == "hello"

auto archive = zip.create()
archive.add("hello.txt", "hello")
Buffer zipBytes = archive.finish()
assert zip.open(zipBytes).readText("hello.txt") == "hello"
```

错误使用分类异常：压缩模块抛 `CompressionError`，归档模块抛 `ArchiveError`，CSV/配置/TOML/plist/XML 分别抛对应类型错误。损坏输入、截断、路径穿越、资源上限和 I/O 失败不会用空值伪装成功。

## 15. 验证入口

- 构建：`demo6/build.bat`。
- 全量/分组测试：`demo6/test.bat` 与 `demo6/test/suites/*.bat`。
- 标准库覆盖守门：`powershell -ExecutionPolicy Bypass -File demo6/tools/check_stdlib_coverage.ps1`。
- 数据格式扩展库：`demo6/test/suites/10_format_extlibs.bat`。
- RC 工程守门：`demo6/rc0_guard.bat`、`demo6/tools/rc0_guard.ps1`。
- 示例目录：`demo6/release/examples`。
