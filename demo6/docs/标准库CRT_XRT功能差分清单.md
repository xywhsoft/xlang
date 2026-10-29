# xlang 标准库 CRT/XRT 功能现状与差分清单

更新日期：2026-09-22

本文回答三个问题：

1. 当前哪些 XRT/xruntime 函数已经进入 xlang 标准库；
2. 已决定保留的 CRT 能力在去除 XRT 重复项后，应以什么功能进入 xlang；
3. 现状与目标之间需要补充、替换、迁移或删除什么。

本文是函数级快照与候选方案，不自动改变《标准库决策清单》中任何项目的状态。
后续每组 API 仍需经过语义、错误模型、所有权、平台差异和命名评审。

## 一、口径和统计

权威源是 `src/stdlib/xl6_stdlib_data.c` 当前注册表和注入声明，而不是历史文档或
XRT 头文件中“可供使用但尚未接入”的全部函数。

| 口径 | 数量 | 说明 |
|---|---:|---|
| 标准库函数注册项 | 737 | 含公开入口、重载和私有桥接 |
| 公开函数注册项 | 442 | 含 XRT、CRT 数学、TCC 和 xlang 宿主入口 |
| 私有桥接注册项 | 295 | 实现细节，不是公开标准库 API |
| XRT/xruntime 公开函数注册项 | 415 | 重载按多个注册项计数 |
| XRT/xruntime 公开限定名 | 401 | 去重后的模块/全局函数名 |
| XRT-backed 编译器 task intrinsic | 5 | 不在通用函数注册表中 |
| XRT/xruntime-backed 注入 class | 66 | 对象方法与模块函数分开统计 |

因此，当前有 **401 个去重的 XRT/xruntime 模块或全局函数名**；计入
5 个 task intrinsic 后，共有 **406 个语言级可调用名**。重载展开后为 420 个入口。

下列内容不纳入 401 个 XRT 函数名：

- 直接 CRT/编译器实现的 16 个数学名：`sin`、`cos`、`tan`、`asin`、`acos`、
  `atan`、`atan2`、`sqrt`、`pow`、`exp`、`log`、`log10`、`floor`、`ceil`、`round`、`abs`；
- `tcc.*` 7 个公开入口和 `xlang.*` 3 个公开入口；
- 仅供包装层使用的私有桥接，包括 `__crt_string.strlen`；
- class 的构造、方法和属性；它们在后文单独列出类型面。

## 二、当前已进入 xlang 的 XRT 函数

以下是当前注册表中的公开、去重限定名全集。括号是去重后函数数；
`int.format`、`process.*`、`signal.*`、`string.*` 和 `xlon.*` 中的部分函数另有重载注册项。

### 2.1 全局输出（4）

`print`、`println`、`write`、`writeln`

### 2.2 codec（10）

`base64Decode`、`base64DecodeBytes`、`base64Encode`、`base64EncodeBytes`、
`hexDecode`、`hexDecodeBytes`、`hexEncode`、`hexEncodeBytes`、
`percentDecode`、`percentEncode`

### 2.3 crypto（54）

`aes128GcmDecrypt`、`aes128GcmDecryptBytes`、`aes128GcmEncrypt`、`aes128GcmEncryptBytes`、
`aes256GcmDecrypt`、`aes256GcmDecryptBytes`、`aes256GcmEncrypt`、`aes256GcmEncryptBytes`、
`chacha20Bytes`、`chacha20Decrypt`、`chacha20Encrypt`、
`chacha20Poly1305Decrypt`、`chacha20Poly1305DecryptBytes`、
`chacha20Poly1305Encrypt`、`chacha20Poly1305EncryptBytes`、
`ecdsaVerify`、`ed25519Keypair`、`ed25519PublicKey`、`ed25519Sign`、`ed25519Verify`、
`hkdfSha256`、`hkdfSha256Bytes`、`hkdfSha384`、`hkdfSha384Bytes`、
`hmacSha256`、`hmacSha256Bytes`、`hmacSha384`、`hmacSha384Bytes`、
`hmacSha512`、`hmacSha512Bytes`、
`p256Keypair`、`p256SharedSecret`、`p384Keypair`、`p384SharedSecret`、
`randomBytes`、`randomHex`、`rsaPkcs1Verify`、`rsaPssVerify`、
`sha1`、`sha1Bytes`、`sha1Stream`、`sha256`、`sha256Bytes`、`sha256Stream`、
`sha384`、`sha384Bytes`、`sha384Stream`、`sha512`、`sha512Bytes`、`sha512Stream`、
`x25519Keypair`、`x25519SharedSecret`、`x448Keypair`、`x448SharedSecret`

### 2.4 dict（1）

`remove`

### 2.5 dir（23）

`clean`、`copy`、`copyAsync`、`count`、`create`、`createAll`、`createAllAsync`、`createAsync`、
`delete`、`deleteAsync`、`dirs`、`empty`、`ensureEmpty`、`entries`、`exists`、`files`、`glob`、
`list`、`matchEntries`、`move`、`moveAsync`、`size`、`walk`

### 2.6 encoding（21）

`charSize`、`convertBytes`、`decodeUtf16BE`、`decodeUtf16LE`、`decodeUtf32BE`、`decodeUtf32LE`、
`decodeUtf8`、`detect`、`detectBytes`、`encodeUtf16BE`、`encodeUtf16LE`、`encodeUtf32BE`、
`encodeUtf32LE`、`encodeUtf8`、`fromBytes`、`isUtf8`、`isUtf8Bytes`、`name`、
`swapUtf16Endian`、`swapUtf32Endian`、`toBytes`

### 2.7 file（31）

`accessTime`、`appendBytes`、`appendText`、`appendTextAsync`、`attr`、`changeTime`、`copy`、
`copyAsync`、`delete`、`deleteAsync`、`exists`、`info`、`move`、`moveAsync`、`open`、`openAsync`、
`readBytes`、`readBytesAsync`、`readText`、`readTextAsync`、`rename`、`replace`、`setAttr`、
`setSize`、`size`、`touch`、`writeBytes`、`writeBytesAsync`、`writeText`、`writeTextAsync`、
`writeTextAtomic`

### 2.8 float（1）、int（1）

- `float.format`
- `int.format`（有重载）

### 2.9 hash（4）

`u32`、`u32WithSeed`、`u64`、`u64WithSeed`

### 2.10 id（5）

`compare`、`decode`、`encode`、`make`、`xid`

### 2.11 json（7）

`parse`、`read`、`stringify`、`valid`、`visit`、`write`、`writer`

### 2.12 log（16）

`addConsole`、`addFile`、`addRollingFile`、`create`、`debug`、`default`、`error`、`fatal`、
`flush`、`getLevel`、`info`、`levelName`、`setLevel`、`trace`、`warn`、`write`

### 2.13 math：XRT 部分（18）

`cbrt`、`clamp`、`deg`、`exp2`、`expm1`、`fract`、`hypot`、`isFinite`、`isInf`、`isNaN`、
`log1p`、`log2`、`max`、`min`、`mod`、`rad`、`sign`、`trunc`

### 2.14 net（13）

`engine`、`interfaceIndex`、`interfaceName`、`interfaces`、`localIP`、`localMAC`、`localName`、
`proxy`、`resolve`、`resolveAll`、`resolveAsync`、`resolverStats`、`stats`

### 2.15 os（10）

`args`、`chain`、`cwd`、`env`、`exec`、`platform`、`run`、`setCwd`、`start`、`startFile`

### 2.16 path（19）

`abs`、`appDir`、`cwd`、`dir`、`exists`、`ext`、`home`、`isAbs`、`join`、`name`、`nameExt`、
`normalize`、`random`、`rel`、`sep`、`setCwd`、`temp`、`withExt`、`withName`

### 2.17 process（11）

`cmd`、`exec`、`exitCode`、`pipeline`、`run`、`runFile`、`shell`、`start`、`startFile`、
`success`、`terminalSupported`

`process` 共有 13 个重载展开注册项。

### 2.18 random（5）

`range`、`seed`、`string`、`u32`、`u64`

### 2.19 signal（12）

`cancelOn`、`clear`、`count`、`name`、`on`、`once`、`raise`、`received`、`reset`、
`restore`、`sendTo`、`supported`

`signal` 共有 14 个重载展开注册项。

### 2.20 string（44）

`approx`、`byteAt`、`byteLen`、`bytes`、`charAt`、`chars`、`compare`、`contains`、`count`、
`endsWith`、`equals`、`filter`、`format`、`hasAny`、`indexOf`、`insert`、`isBlank`、
`isEmpty`、`isUtf8`、`join`、`lastIndexOf`、`left`、`len`、`like`、`lower`、`ltrim`、
`mid`、`padCenter`、`padLeft`、`padRight`、`remove`、`removeAt`、`repeat`、`replace`、
`reverse`、`right`、`rtrim`、`sim`、`slice`、`split`、`splitLines`、`startsWith`、
`trim`、`upper`

`string` 共有 46 个重载展开注册项。

### 2.21 task（1）

`group`

### 2.22 tcp（5）

`connect`、`connectAsync`、`connectConfig`、`listen`、`listenConfig`

### 2.23 template（5）

`compile`、`compileFile`、`engine`、`render`、`renderFile`

### 2.24 time（67）

`addDays`、`addHours`、`addMinutes`、`addMonths`、`addQuarters`、`addSeconds`、`addWeeks`、
`addYears`、`approx`、`clock`、`datePart`、`dateString`、`day`、`dayOfYear`、`daysInMonth`、
`daysInYear`、`diffDays`、`diffHours`、`diffMinutes`、`diffMonths`、`diffQuarters`、
`diffSeconds`、`diffWeeks`、`diffYears`、`firstDayOfMonth`、`firstDayOfWeek`、
`firstDayOfYear`、`format`、`formatPattern`、`fromDate`、`fromDateTime`、`fromTime`、
`fromUnix`、`hour`、`inRange`、`isLeapYear`、`isSameDay`、`isSameMonth`、`isSameYear`、
`lastDayOfMonth`、`lastDayOfWeek`、`lastDayOfYear`、`localToUTC`、`minute`、`month`、
`now`、`nowUTC`、`parse`、`parsePattern`、`quarter`、`rangeOverlap`、`relative`、`second`、
`sleep`、`timePart`、`timer`、`timeString`、`timezoneOffset`、`today`、`toUnix`、
`tryParse`、`tryParsePattern`、`utcToLocal`、`weekday`、`weekOfMonth`、`weekOfYear`、`year`

### 2.25 tls（3）

`connect`、`connectAsync`、`listen`

### 2.26 udp（2）

`bind`、`config`

### 2.27 xlon（8）

`options`、`parse`、`read`、`stringify`、`valid`、`visit`、`write`、`writer`

`xlon` 共有 15 个重载展开注册项。

### 2.28 task 编译器 intrinsic（5）

`task.spawn`、`task.cancel`、`task.whenAny`、`task.all`、`task.race`

这些入口由编译器识别，并使用 XRT/xruntime 的 task/future 能力，不在通用标准库
函数注册表内。

### 2.29 XRT/xruntime-backed class 面（66）

这些是已进入 xlang 的对象型 API，不应误计为模块函数：

`AsyncFile`、`Buffer`、`BufferIter`、`DynamicFields`、`File`、`FileInfo`、`Http1`、
`Http1Body`、`Http1Head`、`HttpDecoder`、`JsonWriter`、`LengthFramer`、`LineFramer`、
`Logger`、`LogSink`、`NetAddress`、`NetAddressList`、`NetBytes`、`NetEngine`、
`NetEngineStats`、`NetFrame`、`NetInterface`、`NetInterfaceAddress`、`NetInterfaceList`、
`NetProxy`、`NetResolverStats`、`object`、`PipelineResult`、`Process`、`ProcessCommand`、
`ProcessResult`、`Random`、`Sha1`、`Sha256`、`Sha384`、`Sha512`、`SignalHandle`、
`TaskGroup`、`TcpConnectConfig`、`TcpListenConfig`、`TcpServer`、`TcpServerStats`、
`TcpStream`、`TcpStreamStats`、`Template`、`TemplateEngine`、`TlsConfig`、`TlsResume`、
`TlsServer`、`TlsServerStats`、`TlsSession`、`TlsStream`、`UdpBatch`、`UdpConfig`、
`UdpPacket`、`UdpSocket`、`UdpStats`、`WebSocket`、`WebSocketConfig`、`WsClose`、
`WsFrame`、`WsHandshake`、`WsUpgrade`、`Xid`、`XlonOptions`、`XlonWriter`

另外 4 个注入 class 属于宿主/工具链，不属于 XRT 标准库：`TccContext`、
`TccHandler`、`XLangContext`、`XLangHandler`。

## 三、已保留 CRT 分类去重后的目标功能

### 3.1 总原则

“保留 CRT 分类”不等于“把 CRT 符号直接暴露给 xlang”。去除 XRT 重复后，仅存三种
合理路径：

1. XRT 已有成熟实现：只增加 xlang 语言外观；
2. XRT 能力有缺口：先补 XRT，再从 xlang 公开；
3. 真正应由平台数学库承担的实数数学：通过可测试的窄桥接接入，不在
   xlang 层重写数值算法。

### 3.2 C04 原始内存与 C 字符串

#### 应进入默认标准库

- 安全字节容器：`Buffer.resize`、`Buffer.remove`、`Buffer.trim`、`Buffer.fill`、
  `Buffer.copyFrom`、`Buffer.copyWithin`、`Buffer.compare`、`Buffer.indexOf`；
- 字符串补充：`string.cut`、`string.rcut`、`string.findAny`、`string.fields`。

其中 `Buffer.resize/remove/trim` 和 `string.cut/rcut/findAny/fields` 已由 XRT 提供，只缺
xlang 公开外观；`Buffer.fill/copyFrom/copyWithin/compare/indexOf` 是 XRT 缺口，
应先进入 XRT。

#### 不应进入默认标准库

- `memcpy`、`memmove`、`memset`、`memcmp`、`memchr` 的裸指针形态；
- `strcpy`、`strncpy`、`strcat`、`strncat`、`strtok`、`strcoll`、`strxfrm` 等 C 字符串/
  locale 形态；
- `strlen` 的公开 CRT 形态；xlang 已有明确的 string/view/UTF-8 长度边界。

如果 C FFI 确实需要裸内存操作，它们只能进入显式 `unsafe.memory`/FFI 层，
不属于默认标准库。

### 3.3 C05 字符分类

C `ctype` 依赖 locale，并对负值 `char` 存在调用前置条件，不能装成 Unicode API。
目标应是先在 XRT 建立稳定的 Unicode code point 属性层，再公开：

- `unicode.category`；
- `unicode.isLetter`、`isNumber`、`isDigit`、`isAlphaNumeric`、`isWhitespace`；
- `unicode.isUpper`、`isLower`、`isTitle`、`isControl`、`isPunctuation`、`isSymbol`、`isMark`；
- `unicode.toUpper`、`toLower`、`toTitle`（单 code point 映射；完整字符串 case mapping 保留在
  string/Unicode 文本层）；
- 如果低层协议解析需要，另提供确定性 `ascii.isAlpha/isDigit/isAlphaNumeric/isHexDigit/
  isWhitespace/toUpper/toLower`，不受 locale 影响。

### 3.4 C06 数字转换与通用算法

XRT 已有严格数字解析、格式化、数组排序/二分查找和随机打乱。应进入 xlang 的是
语言化外观，而不是 CRT 直连：

- `int.parse`、`int.tryParse`、`uint64.parse`、`uint64.tryParse`，以及固定宽度整数对应入口（xlang 没有独立 `uint` 类型）；
- `float.parse`、`float.tryParse`；
- `array.sort`、`array.sortBy`、`array.binarySearch`、`array.shuffle`；
- `math.divRem`（多返回值，消除 C `div_t`/`ldiv_t` 类型泄漏）。

不直接公开 `atoi/atol/atoll/atof`、`strtol/strtoul/strtod`、`qsort`、`bsearch`、
`rand/srand`。它们分别与 XRT number、array 和 random 重复，且 CRT 错误或全局状态语义
不适合直接进入 xlang。

### 3.5 C07 实数数学

#### 已有且应保留

- CRT/窄桥接：`sin`、`cos`、`tan`、`asin`、`acos`、`atan`、`atan2`、`sqrt`、`pow`、
  `exp`、`log`、`log10`、`floor`、`ceil`、`round`、`abs`；
- XRT：`cbrt`、`clamp`、`deg`、`exp2`、`expm1`、`fract`、`hypot`、`isFinite`、`isInf`、
  `isNaN`、`log1p`、`log2`、`max`、`min`、`mod`、`rad`、`sign`、`trunc`。

#### 应补充且不与 XRT 重复

- 双曲与反双曲：`sinh`、`cosh`、`tanh`、`asinh`、`acosh`、`atanh`；
- 特殊函数：`erf`、`erfc`、`gamma`（CRT `tgamma`）、`logGamma`（CRT `lgamma`）；
- IEEE/精度：`fma`、`copySign`、`nextAfter`；
- 分解与缩放：`frexp`、`ldexp`、`modf`、`scalbn`、`logb`、`ilogb`；
- 余数与差：`remainder`、`remQuo`、`positiveDiff`（CRT `fdim`）；
- NaN-aware 顺序：`minNumber`、`maxNumber`（CRT `fmin/fmax` 语义）。

`frexp`、`modf`、`remQuo` 应使用 xlang 多返回值，不暴露 C 输出指针。
`minNumber/maxNumber` 不能取代 XRT `min/max`：两组 API 的 NaN 传播语义不同。

不新增 `fmod`（已有 `math.mod`）、`nan(tag)`、`nexttoward`/long double 变体、
`sinf/cosf/...` 类型后缀变体，也不新增返回 C integer 的 `lrint/llrint/lround/llround`
家族。类型选择由 xlang 重载和显式转换承担。

### 3.6 C08 浮点分类、舍入和环境

应进入默认数学层的纯查询能力：

- `math.classify`、`math.isNormal`、`math.isSubnormal`、`math.signBit`、`math.isUnordered`；
- 确定性舍入 `math.roundEven`；
- 只有在浮点环境明确启用时才提供的 `math.roundCurrent`/`nearbyInt` 语义。

会改变执行环境的能力不能裸绑定 `fenv.h`，应先有 XRT 跨平台抽象：

- `floatEnv.roundingMode`、`floatEnv.setRoundingMode`；
- `floatEnv.clearExceptions`、`floatEnv.testExceptions`、`floatEnv.raiseExceptions`；
- `floatEnv.save`、`floatEnv.restore`，并明确线程局部/进程状态、嵌套和恢复保证。

### 3.7 C09 控制台

XRT 已有输出、flush 和终端判定；xlang 目前只公开了全局输出便利函数。
目标语言外观应包含：

- 复用当前 XRT：`console.write`、`console.writeLine`、`console.error`、
  `console.errorLine`、`console.flush`、`console.isTerminal`；
- 先补 XRT：`console.readLine`、`console.isInputTerminal`；
- `console.readKey` 仅作为后续交互式终端能力，不阻塞首版。

`FILE*`、`fopen/fclose/fread/fwrite/fprintf/fscanf` 不进入 xlang；文件与通用 I/O 继续走
XRT `file`/`File`/`AsyncFile`/IO 抽象。全局 `print/println/write/writeln` 可作为便利层保留。

### 3.8 C13 环境变量

XRT 已有 lookup/get/set/remove，当前 xlang 只有 `os.env` 查询入口。应增加独立命名空间：

- 复用当前 XRT：`env.get`、`env.tryGet`、`env.has`、`env.set`、`env.remove`；
- 先补 XRT：`env.entries`或 `env.snapshot`，并定义 Unicode、大小写、空值、线程安全和
  权限边界。

`getenv/putenv/setenv/unsetenv` 不直接公开。`os.env` 先保留兼容，待 `env` 命名空间
稳定后再决定是否废弃。

### 3.9 C13 进程生命周期

该项仍在《标准库决策清单》中未决策。但技术去重结论已明确：`system/exit/abort/atexit`
等 CRT 生命周期 API 不应另建一套公开面；如果保留语言外观，应建立在现有
XRT `process.*` 和宿主退出契约之上。

### 3.10 后置的复数、泛型数学和编译器运行时

- 复数类型根基建立后，再提供 `complex.abs/arg/conj/proj/exp/log/pow/sqrt/sin/cos/tan/
  sinh/cosh/tanh` 等数学面；当前不用 C `_Complex` ABI 抢跑语言类型设计；
- 泛型数学应是 xlang 类型系统与数值 protocol 的结果，不直连 C `_Generic`；
- 编译器运行时与解释执行一并设计，默认不作为用户可见标准库 API。

## 四、需要补充的功能

### 4.1 只需增加 xlang 外观，XRT 已有实现

| 优先级 | 功能组 | 建议入口 |
|---:|---|---|
| P0 | 数字解析 | `int/uint64/float.parse`、`tryParse` 及固定宽度整数变体 |
| P0 | 环境变量 | `env.get/tryGet/has/set/remove` |
| P0 | 控制台输出 | `console.write/writeLine/error/errorLine/flush/isTerminal` |
| P0 | 字节容器 | `Buffer.resize/remove/trim` |
| P1 | 字符串 | `string.cut/rcut/findAny/fields` |
| P1 | 通用算法 | `array.sort/sortBy/binarySearch/shuffle` |

### 4.2 需要先补 XRT，再进入 xlang

| 优先级 | XRT 缺口 | xlang 目标外观 |
|---:|---|---|
| P0 | 安全字节填充、复制、比较和查找 | `Buffer.fill/copyFrom/copyWithin/compare/indexOf` |
| P0 | Unicode code point 属性表与 case mapping | `unicode.category/is*/to*` |
| P0 | 标准输入和输入终端检测 | `console.readLine/isInputTerminal` |
| P1 | 环境变量稳定快照/枚举 | `env.entries` 或 `env.snapshot` |
| P1 | 可移植浮点环境 | `floatEnv.*` |

### 4.3 需要以 CRT/平台数学窄桥接补充

- `sinh/cosh/tanh/asinh/acosh/atanh`；
- `erf/erfc/gamma/logGamma`；
- `fma/copySign/nextAfter`；
- `frexp/ldexp/modf/scalbn/logb/ilogb`；
- `remainder/remQuo/positiveDiff/minNumber/maxNumber`；
- `classify/isNormal/isSubnormal/signBit/isUnordered/roundEven`。

这一组需同时验收 Windows/MSVC CRT、TCC 可见符号、Linux libc/libm、NaN/Inf/有符号零、
domain/range error 和 fast-math 编译选项，不能只验证“能链接”。

## 五、需要删除、替换或迁移的功能

### 5.1 明确应替换

| 当前项 | 处置 | 理由 |
|---|---|---|
| 私有 `__crt_string.strlen` 注册 | 用 XRT string view/长度契约替换后删除 | 当前 41 个包装源码行共有 85 次调用（另有 1 个注册项），使标准库仍存在隐性 CRT 字符串边界 |
| `math.round` 的编译器 `floor/ceil` 回退 | 换成语义明确且经边界测试的 XRT/CRT 实现 | 避免自建公式与 IEEE/溢出语义漂移 |

### 5.2 需要用户决策的废弃/迁移候选

| 候选项 | 建议去向 | 兼容策略 |
|---|---|---|
| `os.env` | `env.get` | 先保留别名并警告，下一个主版本再移除 |
| `os.cwd/setCwd` | `path.cwd/setCwd` | 保留单一权威命名空间；先做使用量与兼容调查 |
| `os.exec/run/start/startFile` | `process.*` | 收敛进程 API；先确认签名和返回语义完全可替代 |
| `tcc.*` 与 TCC class | 显式工具链/宿主模块 | 不默认注入通用标准库，但保留嵌入能力 |
| `xlang.*` 与 XLang host class | 显式宿主模块 | 与语言标准库分发边界分离 |

以上项目都是候选，在 XRT 分类逐项评审前不执行删除。

### 5.3 明确不应因去重而删除

- 不删除全局 `print/println/write/writeln`：它们可作为 `console` 上层便利函数；
- 不用 CRT `fmin/fmax` 覆盖 XRT `math.min/max`：两者 NaN 语义不同；
- 不因为 CRT 也有文件、时间、信号、线程或随机 API 而替换 XRT 实现；
- 不将 class 私有桥接数量误当成公开 API 膨胀；它们应由生成/验证工具管理，而不是
  依靠手工删减。

## 六、建议的实施顺序

1. 先删除私有 `__crt_string.strlen` 依赖，使字符串边界彻底回到 XRT；
2. 只增加无需修改 XRT 的语言外观：number parse、env、console output、Buffer 已有方法、
   string 补充和 array 算法；
3. 一次性补齐 XRT 缺口：Buffer 安全原语、Unicode 属性、console input、env snapshot；
4. 完成实数数学窄桥接和 IEEE 边界测试；
5. 最后单独设计 float environment，因为它涉及可观测的线程/进程状态；
6. 等待语言类型根基后再做复数/泛型数学，等待解释器立项后再做编译器运行时。

## 七、本轮不改变的决策状态

- CRT 的 8 个具体 API 范围仍是未决策项；本文只给出去重后候选集。
- XRT/Xruntime 的 X01–X29 仍全部未决策；“当前已进入”不等于“重设计后已确认保留”。
- 本文不执行任何 API 删除、改名或兼容性变更。
