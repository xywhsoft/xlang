# 标准库函数与常量登记参考

由 `tools/generate_stdlib_reference.py` 从实际编译后的声明登记表生成；不要手工编辑。
这里只列命名空间函数和常量。类方法、错误语义、拥有权和效果合同请同时阅读各模块设计文档。
`[compiler signature]` 表示登记项由编译器专用合同补齐，不表示可变参数或动态返回。
示例或声明可编译不等于相关运行路径已经执行；本轮发布报告分别记录这些结果。

## global

```text
print(...)
println(...)
write(string text)
writeln(string text = "")
```

## array

```text
array.binarySearch<T>(array<T> values, T needle, func(T, T) as int compare) as int
array.sort<T>(array<T> values, func(T, T) as int compare)
array.sortBy<T, K>(array<T> values, func(T) as K keyOfValue, func(K, K) as int compare)
```

## ascii

```text
ascii.isAlpha(char c) as bool
ascii.isAlphaNumeric(char c) as bool
ascii.isBlank(char c) as bool
ascii.isControl(char c) as bool
ascii.isDigit(char c) as bool
ascii.isGraphical(char c) as bool
ascii.isHexDigit(char c) as bool
ascii.isLower(char c) as bool
ascii.isPrintable(char c) as bool
ascii.isPunctuation(char c) as bool
ascii.isUpper(char c) as bool
ascii.isWhitespace(char c) as bool
ascii.toLower(char c) as char
ascii.toUpper(char c) as char
```

## asn1

```text
asn1.parse(bytes data, int size = -1, int byteLimit = 16777216) as DerValue
```

## codec

```text
codec.base64Decode(string text, int size = -1, int flags = 0, string alphabet = "", int alphabetSize = -1) as bytes
codec.base64Encode(bytes data, int size = -1, int flags = 0, string alphabet = "", int alphabetSize = -1) as string
codec.base64Encode(cbytes data, int size = -1, int flags = 0, string alphabet = "", int alphabetSize = -1) as string
codec.base64Encode(string data, int size = -1, int flags = 0, string alphabet = "", int alphabetSize = -1) as string
codec.hexDecode(string text, int size = -1, int flags = 0) as bytes
codec.hexEncode(bytes data, int size = -1, int flags = 0) as string
codec.hexEncode(cbytes data, int size = -1, int flags = 0) as string
codec.hexEncode(string data, int size = -1, int flags = 0) as string
codec.percentDecode(string text, int size = -1, bool form = false) as bytes
codec.percentEncode(bytes data, int size = -1, string safe = "", bool form = false, bool unreserved = true, int safeSize = -1) as string
codec.percentEncode(cbytes data, int size = -1, string safe = "", bool form = false, bool unreserved = true, int safeSize = -1) as string
codec.percentEncode(string data, int size = -1, string safe = "", bool form = false, bool unreserved = true, int safeSize = -1) as string
```

## compress

```text
compress.decode(bytes data, InflateOptions? options = null, int size = -1) as bytes
compress.decodeBuffer(Buffer data, InflateOptions? options = null, int size = -1) as bytes
compress.deflater(DeflateOptions? options = null) as Deflater
compress.encode(bytes data, DeflateOptions? options = null, int size = -1) as bytes
compress.encode(string data, DeflateOptions? options = null, int size = -1) as bytes
compress.encodeBuffer(Buffer data, DeflateOptions? options = null, int size = -1) as bytes
compress.inflater(InflateOptions? options = null) as Inflater
```

## console

```text
console.colorMode(int stream = 1) as int
console.error(string text)
console.errorLine(string text = "")
console.errorStyled(string text, ConsoleStyle style)
console.flush()
console.isTerminal(int stream = 1) as bool
console.openSession(ConsoleOptions? options = null) as ConsoleSession
console.readChar() as char?
console.readLine(int maxBytes = 1048576) as string?
console.rgb(int red, int green, int blue) as int
console.size(int stream = 1) as (int, int)
console.write(string text)
console.writeLine(string text = "")
console.writeStyled(string text, ConsoleStyle style)
```

## crc32

```text
crc32.checksum(bytes data, int size = -1) as uint32
crc32.checksum(string data, int size = -1) as uint32
crc32.checksumBuffer(Buffer data, int size = -1) as uint32
crc32.update(uint32 previous, bytes data, int size = -1) as uint32
crc32.update(uint32 previous, string data, int size = -1) as uint32
crc32.updateBuffer(uint32 previous, Buffer data, int size = -1) as uint32
```

## crypto

```text
crypto.ecdsaP256FromDer(bytes signature) as bytes
crypto.ecdsaP256ToDer(bytes signature) as bytes
crypto.ecdsaP256VerifyHash(bytes publicKey, bytes digest, bytes signature) as bool
crypto.ecdsaP256VerifyHashDer(bytes publicKey, bytes digest, bytes signature) as bool
crypto.ecdsaP384FromDer(bytes signature) as bytes
crypto.ecdsaP384ToDer(bytes signature) as bytes
crypto.ecdsaP384VerifyHash(bytes publicKey, bytes digest, bytes signature) as bool
crypto.ecdsaP384VerifyHashDer(bytes publicKey, bytes digest, bytes signature) as bool
crypto.ed25519KeyPair() as Ed25519Key
crypto.ed25519PublicFromSpki(bytes encoded) as bytes
crypto.ed25519PublicFromSpkiPem(string encoded) as bytes
crypto.ed25519PublicKey(bytes seed) as bytes
crypto.ed25519PublicToSpki(bytes publicKey) as bytes
crypto.ed25519PublicToSpkiPem(bytes publicKey) as string
crypto.ed25519Sign(bytes seed, bytes message, int size = -1) as bytes
crypto.ed25519Sign(bytes seed, string message, int size = -1) as bytes
crypto.ed25519SignBuffer(bytes seed, Buffer message, int size = -1) as bytes
crypto.ed25519Verify(bytes publicKey, bytes message, bytes signature, int size = -1) as bool
crypto.ed25519Verify(bytes publicKey, string message, bytes signature, int size = -1) as bool
crypto.ed25519VerifyBuffer(bytes publicKey, Buffer message, bytes signature, int size = -1) as bool
crypto.ed25519VerifyContext(bytes publicKey, bytes message, bytes signature, bytes context, int size = -1) as bool
crypto.ed25519VerifyContext(bytes publicKey, string message, bytes signature, bytes context, int size = -1) as bool
crypto.ed25519VerifyContextBuffer(bytes publicKey, Buffer message, bytes signature, bytes context, int size = -1) as bool
crypto.ed25519VerifyPrehash(bytes publicKey, bytes digest, bytes signature, bytes context = b"") as bool
crypto.fillRandomBytes(Buffer target)
crypto.hkdfSha256(bytes ikm, bytes salt, bytes info, int length) as bytes
crypto.hkdfSha256(cbytes ikm, cbytes salt, cbytes info, int length) as bytes
crypto.hkdfSha256Expand(bytes prk, bytes info, int length) as bytes
crypto.hkdfSha256Expand(cbytes prk, cbytes info, int length) as bytes
crypto.hkdfSha256Extract(bytes ikm, bytes salt) as bytes
crypto.hkdfSha256Extract(cbytes ikm, cbytes salt) as bytes
crypto.hkdfSha384(bytes ikm, bytes salt, bytes info, int length) as bytes
crypto.hkdfSha384(cbytes ikm, cbytes salt, cbytes info, int length) as bytes
crypto.hkdfSha384Expand(bytes prk, bytes info, int length) as bytes
crypto.hkdfSha384Expand(cbytes prk, cbytes info, int length) as bytes
crypto.hkdfSha384Extract(bytes ikm, bytes salt) as bytes
crypto.hkdfSha384Extract(cbytes ikm, cbytes salt) as bytes
crypto.hkdfSha512(bytes ikm, bytes salt, bytes info, int length) as bytes
crypto.hkdfSha512(cbytes ikm, cbytes salt, cbytes info, int length) as bytes
crypto.hkdfSha512Expand(bytes prk, bytes info, int length) as bytes
crypto.hkdfSha512Expand(cbytes prk, cbytes info, int length) as bytes
crypto.hkdfSha512Extract(bytes ikm, bytes salt) as bytes
crypto.hkdfSha512Extract(cbytes ikm, cbytes salt) as bytes
crypto.hmacSha256(bytes key, bytes data, int size = -1) as bytes
crypto.hmacSha256(bytes key, string data, int size = -1) as bytes
crypto.hmacSha256(cbytes key, cbytes data, int size = -1) as bytes
crypto.hmacSha256(cbytes key, string data, int size = -1) as bytes
crypto.hmacSha256Buffer(bytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha256Buffer(cbytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha256BufferHex(bytes key, Buffer data, int size = -1) as string
crypto.hmacSha256BufferHex(cbytes key, Buffer data, int size = -1) as string
crypto.hmacSha256Hex(bytes key, bytes data, int size = -1) as string
crypto.hmacSha256Hex(bytes key, string data, int size = -1) as string
crypto.hmacSha256Hex(cbytes key, cbytes data, int size = -1) as string
crypto.hmacSha256Hex(cbytes key, string data, int size = -1) as string
crypto.hmacSha256Stream(bytes key) as HmacSha256
crypto.hmacSha384(bytes key, bytes data, int size = -1) as bytes
crypto.hmacSha384(bytes key, string data, int size = -1) as bytes
crypto.hmacSha384(cbytes key, cbytes data, int size = -1) as bytes
crypto.hmacSha384(cbytes key, string data, int size = -1) as bytes
crypto.hmacSha384Buffer(bytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha384Buffer(cbytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha384BufferHex(bytes key, Buffer data, int size = -1) as string
crypto.hmacSha384BufferHex(cbytes key, Buffer data, int size = -1) as string
crypto.hmacSha384Hex(bytes key, bytes data, int size = -1) as string
crypto.hmacSha384Hex(bytes key, string data, int size = -1) as string
crypto.hmacSha384Hex(cbytes key, cbytes data, int size = -1) as string
crypto.hmacSha384Hex(cbytes key, string data, int size = -1) as string
crypto.hmacSha384Stream(bytes key) as HmacSha384
crypto.hmacSha512(bytes key, bytes data, int size = -1) as bytes
crypto.hmacSha512(bytes key, string data, int size = -1) as bytes
crypto.hmacSha512(cbytes key, cbytes data, int size = -1) as bytes
crypto.hmacSha512(cbytes key, string data, int size = -1) as bytes
crypto.hmacSha512Buffer(bytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha512Buffer(cbytes key, Buffer data, int size = -1) as bytes
crypto.hmacSha512BufferHex(bytes key, Buffer data, int size = -1) as string
crypto.hmacSha512BufferHex(cbytes key, Buffer data, int size = -1) as string
crypto.hmacSha512Hex(bytes key, bytes data, int size = -1) as string
crypto.hmacSha512Hex(bytes key, string data, int size = -1) as string
crypto.hmacSha512Hex(cbytes key, cbytes data, int size = -1) as string
crypto.hmacSha512Hex(cbytes key, string data, int size = -1) as string
crypto.hmacSha512Stream(bytes key) as HmacSha512
crypto.p256KeyPair() as P256Key
crypto.p256PublicFromSpki(bytes encoded) as bytes
crypto.p256PublicFromSpkiPem(string encoded) as bytes
crypto.p256PublicKey(bytes privateKey) as bytes
crypto.p256PublicToSpki(bytes publicKey) as bytes
crypto.p256PublicToSpkiPem(bytes publicKey) as string
crypto.p384KeyPair() as P384Key
crypto.p384PublicFromSpki(bytes encoded) as bytes
crypto.p384PublicFromSpkiPem(string encoded) as bytes
crypto.p384PublicKey(bytes privateKey) as bytes
crypto.p384PublicToSpki(bytes publicKey) as bytes
crypto.p384PublicToSpkiPem(bytes publicKey) as string
crypto.pbkdf2Sha256(bytes password, bytes salt, int iterations, int length) as bytes
crypto.pbkdf2Sha256(cbytes password, cbytes salt, int iterations, int length) as bytes
crypto.pbkdf2Sha384(bytes password, bytes salt, int iterations, int length) as bytes
crypto.pbkdf2Sha384(cbytes password, cbytes salt, int iterations, int length) as bytes
crypto.pbkdf2Sha512(bytes password, bytes salt, int iterations, int length) as bytes
crypto.pbkdf2Sha512(cbytes password, cbytes salt, int iterations, int length) as bytes
crypto.randomBytes(int count) as bytes
crypto.randomHex(int byteCount) as string
crypto.randomString(int length = 22, string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_", int alphabetSize = -1) as string
crypto.sha224(bytes data, int size = -1) as bytes
crypto.sha224(cbytes data, int size = -1) as bytes
crypto.sha224(string data, int size = -1) as bytes
crypto.sha224Buffer(Buffer data, int size = -1) as bytes
crypto.sha224BufferHex(Buffer data, int size = -1) as string
crypto.sha224Hex(bytes data, int size = -1) as string
crypto.sha224Hex(cbytes data, int size = -1) as string
crypto.sha224Hex(string data, int size = -1) as string
crypto.sha224Stream() as Sha224
crypto.sha256(bytes data, int size = -1) as bytes
crypto.sha256(cbytes data, int size = -1) as bytes
crypto.sha256(string data, int size = -1) as bytes
crypto.sha256Buffer(Buffer data, int size = -1) as bytes
crypto.sha256BufferHex(Buffer data, int size = -1) as string
crypto.sha256Hex(bytes data, int size = -1) as string
crypto.sha256Hex(cbytes data, int size = -1) as string
crypto.sha256Hex(string data, int size = -1) as string
crypto.sha256Stream() as Sha256
crypto.sha384(bytes data, int size = -1) as bytes
crypto.sha384(cbytes data, int size = -1) as bytes
crypto.sha384(string data, int size = -1) as bytes
crypto.sha384Buffer(Buffer data, int size = -1) as bytes
crypto.sha384BufferHex(Buffer data, int size = -1) as string
crypto.sha384Hex(bytes data, int size = -1) as string
crypto.sha384Hex(cbytes data, int size = -1) as string
crypto.sha384Hex(string data, int size = -1) as string
crypto.sha384Stream() as Sha384
crypto.sha512(bytes data, int size = -1) as bytes
crypto.sha512(cbytes data, int size = -1) as bytes
crypto.sha512(string data, int size = -1) as bytes
crypto.sha512Buffer(Buffer data, int size = -1) as bytes
crypto.sha512BufferHex(Buffer data, int size = -1) as string
crypto.sha512Hex(bytes data, int size = -1) as string
crypto.sha512Hex(cbytes data, int size = -1) as string
crypto.sha512Hex(string data, int size = -1) as string
crypto.sha512Stream() as Sha512
crypto.sha512_256(bytes data, int size = -1) as bytes
crypto.sha512_256(cbytes data, int size = -1) as bytes
crypto.sha512_256(string data, int size = -1) as bytes
crypto.sha512_256Buffer(Buffer data, int size = -1) as bytes
crypto.sha512_256BufferHex(Buffer data, int size = -1) as string
crypto.sha512_256Hex(bytes data, int size = -1) as string
crypto.sha512_256Hex(cbytes data, int size = -1) as string
crypto.sha512_256Hex(string data, int size = -1) as string
crypto.sha512_256Stream() as Sha512_256
crypto.timingSafeEqual(bytes left, bytes right) as bool
crypto.timingSafeEqual(cbytes left, cbytes right) as bool
crypto.x25519KeyPair() as X25519Key
crypto.x25519PublicFromSpki(bytes encoded) as bytes
crypto.x25519PublicFromSpkiPem(string encoded) as bytes
crypto.x25519PublicToSpki(bytes publicKey) as bytes
crypto.x25519PublicToSpkiPem(bytes publicKey) as string
crypto.x448KeyPair() as X448Key
crypto.x448PublicFromSpki(bytes encoded) as bytes
crypto.x448PublicFromSpkiPem(string encoded) as bytes
crypto.x448PublicToSpki(bytes publicKey) as bytes
crypto.x448PublicToSpkiPem(bytes publicKey) as string
```

## cryptoAdvanced

```text
cryptoAdvanced.aes(bytes key) as AesBlockCipher
cryptoAdvanced.aes(cbytes key) as AesBlockCipher
cryptoAdvanced.chacha20(bytes key, bytes nonce, bytes data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.chacha20(bytes key, bytes nonce, string data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.chacha20(cbytes key, cbytes nonce, cbytes data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.chacha20(cbytes key, cbytes nonce, string data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.chacha20Buffer(bytes key, bytes nonce, Buffer data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.chacha20Buffer(cbytes key, cbytes nonce, Buffer data, int counter = 1, int size = -1) as bytes
cryptoAdvanced.p256Add(bytes left, bytes right) as bytes
cryptoAdvanced.p256Multiply(bytes scalar, bytes point) as bytes
cryptoAdvanced.p256Valid(bytes point) as bool
cryptoAdvanced.p384Add(bytes left, bytes right) as bytes
cryptoAdvanced.p384Multiply(bytes scalar, bytes point) as bytes
cryptoAdvanced.p384Valid(bytes point) as bool
cryptoAdvanced.poly1305(bytes key, bytes data, int size = -1) as bytes
cryptoAdvanced.poly1305(bytes key, string data, int size = -1) as bytes
cryptoAdvanced.poly1305(cbytes key, cbytes data, int size = -1) as bytes
cryptoAdvanced.poly1305(cbytes key, string data, int size = -1) as bytes
cryptoAdvanced.poly1305Buffer(bytes key, Buffer data, int size = -1) as bytes
cryptoAdvanced.poly1305Buffer(cbytes key, Buffer data, int size = -1) as bytes
cryptoAdvanced.poly1305Stream(bytes key) as Poly1305
cryptoAdvanced.poly1305Stream(cbytes key) as Poly1305
cryptoAdvanced.rsaPrivate(RsaPrivateKey key, bytes input) as bytes
cryptoAdvanced.rsaPublic(RsaPublicKey key, bytes input) as bytes
cryptoAdvanced.x25519(bytes scalar, bytes point) as bytes
cryptoAdvanced.x448(bytes scalar, bytes point) as bytes
```

## cryptoLegacy

```text
cryptoLegacy.md5(bytes data, int size = -1) as bytes
cryptoLegacy.md5(cbytes data, int size = -1) as bytes
cryptoLegacy.md5(string data, int size = -1) as bytes
cryptoLegacy.md5Buffer(Buffer data, int size = -1) as bytes
cryptoLegacy.md5BufferHex(Buffer data, int size = -1) as string
cryptoLegacy.md5Hex(bytes data, int size = -1) as string
cryptoLegacy.md5Hex(cbytes data, int size = -1) as string
cryptoLegacy.md5Hex(string data, int size = -1) as string
cryptoLegacy.md5Stream() as LegacyMd5
cryptoLegacy.sha1(bytes data, int size = -1) as bytes
cryptoLegacy.sha1(cbytes data, int size = -1) as bytes
cryptoLegacy.sha1(string data, int size = -1) as bytes
cryptoLegacy.sha1Buffer(Buffer data, int size = -1) as bytes
cryptoLegacy.sha1BufferHex(Buffer data, int size = -1) as string
cryptoLegacy.sha1Hex(bytes data, int size = -1) as string
cryptoLegacy.sha1Hex(cbytes data, int size = -1) as string
cryptoLegacy.sha1Hex(string data, int size = -1) as string
cryptoLegacy.sha1Stream() as LegacySha1
```

## dict

```text
dict.remove(var table, string key) as bool
```

## dir

```text
dir.clean(string path) as bool
dir.cleanAsync(string path) as TreeStats
dir.copy(string source, string target, bool replace = true) as bool
dir.copyAsync(string source,string target,bool replace = false) as TreeStats
dir.count(string path, bool recursive = false) as uint64
dir.create(string path, int mode = 511) as bool
dir.createAll(string path, int mode = 511) as bool
dir.createAllAsync(string path,int mode = -1) as future<bool>
dir.createAsync(string path,int mode = -1) as future<bool>
dir.delete(string path) as bool
dir.deleteAsync(string path) as TreeStats
dir.dirs(string path, bool recursive = false, int maxEntries = 1048576) as array<string>
dir.empty(string path) as bool
dir.emptyAsync(string path) as future<bool>
dir.ensureEmpty(string path) as bool
dir.ensureEmptyAsync(string path) as future<bool>
dir.entries(string path, bool recursive = false, int maxEntries = 1048576) as array<WalkEntry>
dir.exists(string path) as bool
dir.files(string path, bool recursive = false, int maxEntries = 1048576) as array<string>
dir.glob(string path, string pattern = "*", bool recursive = false, int maxEntries = 1048576) as array<string>
dir.list(string path, bool recursive = false, int maxEntries = 1048576) as array<string>
dir.matchEntries(string path, string pattern = "*", bool recursive = false, int maxEntries = 1048576) as array<WalkEntry>
dir.move(string source, string target, bool replace = true) as bool
dir.moveAsync(string source,string target,bool replace = false) as future<bool>
dir.open(string path, int flags = 0) as DirectoryIterator
dir.remove(string path) as bool
dir.removeAsync(string path) as future<bool>
dir.roots() as array<string>
dir.size(string path, bool recursive = true) as uint64
dir.sizeAsync(string path,bool recursive = true) as future<uint64>
dir.stats(string path, bool recursive = true) as WalkStats
dir.statsAsync(string path,bool recursive = true) as WalkStats
dir.temp(string directory = "", string prefix = "xl-", string suffix = "") as string
dir.walk(string path, string pattern = "", int maxEntries = 1048576) as array<WalkEntry>
```

## encoding

```text
encoding.bom(bytes data, int size = -1) as int
encoding.bom(cbytes data, int size = -1) as int
encoding.bomBytes(int encoding) as bytes
encoding.bomSize(bytes data, int size = -1) as int
encoding.bomSize(cbytes data, int size = -1) as int
encoding.decode(bytes data, int encoding = 1, int policy = 0, int bom = 1, int size = -1) as string
encoding.decode(cbytes data, int encoding = 1, int policy = 0, int bom = 1, int size = -1) as string
encoding.decodeUtf16BE(bytes data, int policy = 0, int bom = 1, int size = -1) as string
encoding.decodeUtf16LE(bytes data, int policy = 0, int bom = 1, int size = -1) as string
encoding.decodeUtf32BE(bytes data, int policy = 0, int bom = 1, int size = -1) as string
encoding.decodeUtf32LE(bytes data, int policy = 0, int bom = 1, int size = -1) as string
encoding.decodeUtf8(bytes data, int policy = 0, int bom = 1, int size = -1) as string
encoding.detect(bytes data, int size = -1) as (int, int, int)
encoding.detect(cbytes data, int size = -1) as (int, int, int)
encoding.detect(string text, int size = -1) as (int, int, int)
encoding.encode(string text, int encoding = 1, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.encodeUtf16BE(string text, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.encodeUtf16LE(string text, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.encodeUtf32BE(string text, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.encodeUtf32LE(string text, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.encodeUtf8(string text, bool bom = false, int policy = 0, int size = -1) as bytes
encoding.isUtf8(bytes data, int size = -1) as bool
encoding.isUtf8(string text, int size = -1) as bool
encoding.name(int encoding) as string
encoding.transcode(bytes data, int source, int target, int policy = 0, int bom = 1, int size = -1) as bytes
encoding.transcode(cbytes data, int source, int target, int policy = 0, int bom = 1, int size = -1) as bytes
encoding.unitSize(int encoding) as int
```

## env

```text
env.get(string name, int size = -1) as string?
env.has(string name, int size = -1) as bool
env.remove(string name, int size = -1)
env.set(string name, string value, int nameSize = -1, int valueSize = -1)
```

## file

```text
file.accessTime(string path) as time
file.appendBytes(string path, bytes data, int size = -1) as bool
file.appendBytesAsync(string path, bytes data, int size = -1) as future<bool>
file.appendText(string path, string text, int encoding = 1, int policy = 0, int size = -1) as bool
file.appendTextAsync(string path,string text,int encoding = 1,int policy = 0,int size = -1) as future<bool>
file.attr(string path) as int
file.changeTime(string path) as time
file.copy(string source, string target, bool replace = true) as bool
file.copyAsync(string source,string target,bool replace = false) as future<bool>
file.copyTree(string source, string target, int flags = 0) as TreeStats
file.copyTreeAsync(string source,string target,int flags = 0) as TreeStats
file.createFifo(string path, int mode = 438) as bool
file.createHardLink(string source, string target) as bool
file.createLink(string source, string target, bool directory = false) as bool
file.delete(string path) as bool
file.deleteAsync(string path) as future<bool>
file.deleteLink(string path) as bool
file.exists(string path) as bool
file.info(string path, bool follow = true) as FileStat
file.move(string source, string target, bool replace = true) as bool
file.moveAsync(string source,string target,bool replace = false) as future<bool>
file.open(string filePath, int flags = 1, int mode = 438, int share = 7) as File
file.openAsync(string path, int flags = 1, int mode = 438, int share = 7) as AsyncFile
file.readBytes(string path, int maxBytes = 67108864) as bytes
file.readBytesAsync(string path, int limit = 1073741824) as future<bytes>
file.readLink(string path) as string
file.readText(string path, int encoding = 0, int policy = 0, int maxBytes = 67108864) as string
file.readTextAsync(string path, int encoding = 0, int policy = 0, int limit = 1073741824) as future<string>
file.removeTree(string path, bool keepRoot = false) as TreeStats
file.removeTreeAsync(string path,bool keepRoot = false) as TreeStats
file.rename(string source, string target, bool replace = false) as bool
file.replace(string source, string target) as bool
file.root(string path) as FileRoot
file.setAttr(string path, int attributes) as bool
file.setMode(string path, int mode, bool follow = true) as bool
file.setSize(string path, uint64 size) as bool
file.setTimes(string path, int? accessed = null, int? modified = null, bool follow = true) as bool
file.size(string path) as uint64
file.stat(string path, bool follow = true) as FileStat
file.takeAsync(File file) as AsyncFile
file.temp(string directory = "", string prefix = "xl-", string suffix = ".tmp") as File
file.touch(string path) as bool
file.walk(string path, func(WalkEntry) as int visitor, func(WalkError) as int onError, int flags = 0, int maxDepth = -1) as WalkStats
file.walk(string path, func(WalkEntry) as int visitor, int flags = 0, int maxDepth = -1) as WalkStats
file.walkStats(string path, int flags = 0, int maxDepth = -1) as WalkStats
file.writeBytes(string path, bytes data, int size = -1) as bool
file.writeBytesAsync(string path, bytes data, int size = -1) as future<bool>
file.writeBytesAtomic(string path, bytes data, int size = -1) as bool
file.writeBytesAtomicAsync(string path, bytes data, int size = -1) as future<bool>
file.writeText(string path, string text, int encoding = 1, int policy = 0, bool bom = false, int size = -1) as bool
file.writeTextAsync(string path,string text,int encoding = 1,int policy = 0,bool bom = false,int size = -1) as future<bool>
file.writeTextAtomic(string path, string text, int encoding = 1, int policy = 0, bool bom = false, int size = -1) as bool
file.writeTextAtomicAsync(string path,string text,int encoding = 1,int policy = 0,bool bom = false,int size = -1) as future<bool>
```

## float

```text
float.format(float value, string format = "", int size = -1) as string
float.parse(string text, int flags = 8, int size = -1) as float
float.toString(float value, int flags = 0) as string
float.tryParse(string text, int flags = 8, int size = -1) as (bool, float)
```

## hash

```text
hash.u32(bytes data, int size = -1) as uint32
hash.u32(string data, int size = -1) as uint32
hash.u32Buffer(Buffer data, int size = -1) as uint32
hash.u32BufferWithSeed(Buffer data, uint32 seed, int size = -1) as uint32
hash.u32WithSeed(bytes data, uint32 seed, int size = -1) as uint32
hash.u32WithSeed(string data, uint32 seed, int size = -1) as uint32
hash.u64(bytes data, int size = -1) as uint64
hash.u64(string data, int size = -1) as uint64
hash.u64Buffer(Buffer data, int size = -1) as uint64
hash.u64BufferWithSeed(Buffer data, uint64 seed, int size = -1) as uint64
hash.u64WithSeed(bytes data, uint64 seed, int size = -1) as uint64
hash.u64WithSeed(string data, uint64 seed, int size = -1) as uint64
```

## html

```text
html.escapeAttribute(string text, int size = -1) as string
html.escapeText(string text, int size = -1) as string
```

## http

```text
http.coding(string token) as HttpCoding
http.codingName(HttpCoding coding) as string
http.contentLength(string text, int size = -1) as uint64
http.fieldNameEqual(string left, string right) as bool
http.fieldValueValid(bytes data, int size = -1) as bool
http.fieldValueValid(cbytes input, int size = -1) as bool
http.fieldValueValid(string input, int size = -1) as bool
http.methodCode(string text, int size = -1) as HttpMethod
http.methodEqual(string left, string right) as bool
http.methodIdempotent(string text, int size = -1) as bool
http.methodSafe(string text, int size = -1) as bool
http.quality(string text, int size = -1) as int
http.responseContentAllowed(string method, int status) as bool
http.statusText(int status) as string
http.tokenEqual(string left, string right) as bool
http.tokenValid(string text, int size = -1) as bool
http.trimOws(string text, int size = -1) as string
```

## http1

```text
http1.chunk(Buffer input, int size = -1) as bytes
http1.chunk(bytes input, int size = -1) as bytes
http1.chunk(cbytes input, int size = -1) as bytes
http1.chunkEnd(HttpHeaders trailers) as bytes
http1.chunkLine(uint64 size, string extensions = "") as bytes
http1.deadline(int timeoutMillis = -1) as uint64
http1.requestHead(string method, string target, HttpHeaders headers, HttpVersion version = HttpVersion.v11) as bytes
http1.responseHead(int status, string reason, HttpHeaders headers, HttpVersion version = HttpVersion.v11) as bytes
http1.targetValid(string target, int size = -1) as bool
http1.writeChunk(Buffer input, Buffer output, int size = -1) as int
http1.writeChunk(bytes input, Buffer output, int size = -1) as int
http1.writeChunk(cbytes input, Buffer output, int size = -1) as int
http1.writeChunkEnd(HttpHeaders trailers, Buffer output) as int
http1.writeChunkLine(uint64 size, string extensions, Buffer output) as int
http1.writeRequestHead(string method, string target, HttpHeaders headers, Buffer output, HttpVersion version = HttpVersion.v11) as int
http1.writeResponseHead(int status, string reason, HttpHeaders headers, Buffer output, HttpVersion version = HttpVersion.v11) as int
```

## id

```text
id.fromBytes(bytes data) as Xid
id.make() as Xid
id.makeMany(int count) as array<Xid>
id.parse(string text) as Xid
id.tryParse(string text) as Xid?
id.xid() as string
```

## int

```text
int.format(int value, string format = "", int size = -1) as string
int.format(uint64 value, string format = "", int size = -1) as string
int.parse(string text, int base = 10, int flags = 0, int size = -1) as int
int.toString(int value, int base = 10, int flags = 0) as string
int.toString(uint64 value, int base = 10, int flags = 0) as string
int.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int)
```

## int16

```text
int16.parse(string text, int base = 10, int flags = 0, int size = -1) as int16
int16.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int16)
```

## int32

```text
int32.parse(string text, int base = 10, int flags = 0, int size = -1) as int32
int32.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int32)
```

## int64

```text
int64.parse(string text, int base = 10, int flags = 0, int size = -1) as int64
int64.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int64)
```

## int8

```text
int8.parse(string text, int base = 10, int flags = 0, int size = -1) as int8
int8.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int8)
```

## io

```text
io.bufferWriter() as IoWriter
io.copy(IoReadable source, IoWritable target) as int
io.copy(IoReader source, IoWriter target) as int
io.copyExact(IoReadable source, IoWritable target, int count) as int
io.copyExact(IoReader source, IoWriter target, int count) as int
io.copyUpTo(IoReadable source, IoWritable target, int count) as int
io.copyUpTo(IoReader source, IoWriter target, int count) as int
io.discardWriter() as IoWriter
io.fromBuffer(Buffer data, int size = -1) as IoReader
io.fromBytes(bytes data, int size = -1) as IoReader
io.lines(IoReader source, int maxLineBytes = 1048576) as IoLineReader
io.openReader(string path) as IoReader
io.openWriter(string path, int mode = 1) as IoWriter
io.readFile(string path, int maxBytes = 67108864) as bytes
io.stderr() as IoWriter
io.stdin() as IoReader
io.stdout() as IoWriter
io.writeFile(string path, Buffer data, int mode = 1, int size = -1)
io.writeFile(string path, bytes data, int mode = 1, int size = -1)
io.writeFile(string path, string data, int mode = 1, int size = -1)
```

## json

```text
json.parse(string text) as var
json.parse(string text, var options) as var
json.quote(string text, var options = null) as string
json.quoteTo(string text, func(var) as bool sink, var options = null) as bool
json.read(string path) as var
json.read(string text, var options) as var
json.stringify(var value, bool pretty = false) as string
json.stringify(var value, var options) as string
json.valid(string text) as bool
json.valid(string text, var options) as bool
json.visit(string text, func(var) as bool visitor) as int
json.visit(string text, func(var) as bool visitor, var options) as int
json.write(string path, var value, bool pretty = false) as bool
json.write(string path, var value, var options) as bool
json.writeTo(var value, func(var) as bool sink, var options = null) as bool
json.writer(bool pretty = false) as JsonWriter
```

## jsonl

```text
jsonl.parse(string text) as var
jsonl.parse(string text, var options) as var
jsonl.read(string path) as var
jsonl.read(string text, var options) as var
jsonl.stringify(var records) as string
jsonl.stringify(var value, var options) as string
jsonl.valid(string text) as c_bool
jsonl.valid(string text, var options) as bool
jsonl.write(string path, var records) as bool
jsonl.write(string path, var value, var options) as bool
jsonl.writeTo(var value, func(var) as bool sink, var options = null) as bool
```

## log

```text
log.asyncSink(LogSink target, LogAsyncOptions? options = null) as LogSink
log.consoleSink(LogConsoleOptions? options = null) as LogSink
log.create(string name = "xlang", LogLevel level = LogLevel.info) as Logger
log.customSink(func(LogRecord) as LogResult write, string name = "custom", LogLevel level = LogLevel.trace, func() as bool flush = null) as LogSink
log.debug(string message, int size = -1) as LogResult
log.default() as Logger?
log.error(string message, int size = -1) as LogResult
log.fatal(string message, int size = -1) as LogResult
log.flush()
log.info(string message, int size = -1) as LogResult
log.json(LogRecord record, LogJsonOptions? options = null) as string
log.jsonFileSink(string path, LogFileOptions? options = null, LogJsonOptions? json = null) as LogSink
log.jsonToBuffer(LogRecord record, Buffer target, LogJsonOptions? options = null) as int
log.jsonToWriter(LogRecord record, IoWriter target, LogJsonOptions? options = null) as int
log.jsonWriterSink(IoWriter target, LogJsonOptions? options = null, LogLevel level = LogLevel.trace) as LogSink
log.levelName(LogLevel level) as string
log.printf(LogLevel level, string format, c_vararg ) as LogResult
log.record(LogLevel level, string message, int size = -1) as LogRecord
log.ringSink(LogSink target, LogRingOptions? options = null) as LogSink
log.setDefault(Logger? logger)
log.shutdown()
log.text(LogRecord record, LogTextOptions? options = null) as string
log.textFileSink(string path, LogFileOptions? options = null, LogTextOptions? text = null) as LogSink
log.textToBuffer(LogRecord record, Buffer target, LogTextOptions? options = null) as int
log.textToWriter(LogRecord record, IoWriter target, LogTextOptions? options = null) as int
log.textWriterSink(IoWriter target, LogTextOptions? options = null, LogLevel level = LogLevel.trace) as LogSink
log.trace(string message, int size = -1) as LogResult
log.warn(string message, int size = -1) as LogResult
log.write(LogLevel level, string message, int size = -1) as LogResult
log.writeRecord(LogRecord record) as LogResult
```

## math

```text
math.abs(float x) as float
math.abs(int x) as int
math.abs(int16 x) as int16
math.abs(int32 x) as int32
math.abs(int8 x) as int8
math.acos(float x) as float
math.acosh(float x) as float
math.asin(float x) as float
math.asinh(float x) as float
math.atan(float x) as float
math.atan2(float y, float x) as float
math.atanh(float x) as float
math.cbrt(float x) as float
math.ceil(float x) as float
math.clamp(float value, float minValue, float maxValue) as float
math.classify(float x) as FloatClass
math.copySign(float x, float y) as float
math.cos(float x) as float
math.cosh(float x) as float
math.deg(float x) as float
math.divRem(int numerator, int denominator) as (int, int)
math.erf(float x) as float
math.erfc(float x) as float
math.exp(float x) as float
math.exp2(float x) as float
math.expm1(float x) as float
math.floor(float x) as float
math.fma(float x, float y, float z) as float
math.fract(float x) as float
math.frexp(float x) as (float, int)
math.gamma(float x) as float
math.hypot(float x, float y) as float
math.ilogb(float x) as int
math.intNear(int a, int b, uint64 tolerance) as bool
math.isClose(float a, float b, float absTolerance, float relTolerance) as bool
math.isFinite(float x) as bool
math.isInf(float x) as bool
math.isNaN(float x) as bool
math.isNormal(float x) as bool
math.isSubnormal(float x) as bool
math.isUnordered(float x, float y) as bool
math.ldexp(float x, int exponent) as float
math.log(float x) as float
math.log10(float x) as float
math.log1p(float x) as float
math.log2(float x) as float
math.logGamma(float x) as float
math.logb(float x) as float
math.max(float a, float b) as float
math.maxNumber(float x, float y) as float
math.min(float a, float b) as float
math.minNumber(float x, float y) as float
math.mod(float value, float divisor) as float
math.modf(float x) as (float, float)
math.nextAfter(float x, float y) as float
math.positiveDiff(float x, float y) as float
math.pow(float x, float y) as float
math.rad(float x) as float
math.remQuo(float x, float y) as (float, int)
math.remainder(float x, float y) as float
math.round(float x) as float
math.roundCurrent(float x) as float
math.scaleByPowerOfTwo(float x, int exponent) as float
math.sign(float x) as int
math.signBit(float x) as bool
math.sin(float x) as float
math.sinh(float x) as float
math.sqrt(float x) as float
math.tan(float x) as float
math.tanh(float x) as float
math.trunc(float x) as float
```

## memory

```text
memory.debugEnabled() as bool
memory.debugSnapshot() as MemoryDebugSnapshot
memory.enableStats(bool enabled)
memory.reportJson(int limit = 16777216) as string
memory.reportText(int limit = 16777216) as string
memory.resetStats()
memory.stats() as MemoryStats
memory.statsEnabled() as bool
```

## net

```text
net.backendCapabilities(NetBackend backend) as int
net.backendCompiled(NetBackend backend) as bool
net.backendName(NetBackend backend) as string
net.defaultBackend() as NetBackend
net.engine(int workerCount = 0) as NetEngine
net.hostName() as string
net.interfaceIndex(string name, int family = 0) as int
net.interfaceName(int index, int family = 0) as string
net.interfaces() as NetInterfaceList
net.localAddress(int family = 0) as NetAddress
net.localHardware() as bytes
net.localIP(int family = 0) as string
net.localMAC() as string
net.localName() as string
net.proxy(NetProxyType type, string host, int port, bytes user, bytes password, NetProxyAuth auth = NetProxyAuth.automatic) as NetProxy
net.proxy(NetProxyType type, string host, int port, string user = "", string password = "", NetProxyAuth auth = NetProxyAuth.automatic) as NetProxy
net.resolve(string host, int port = 0) as NetAddress
net.resolveAll(string host, int port = 0, int family = 0) as NetAddressList
net.resolveAsync(string host, int port = 0, int family = 0) as future<NetAddressList>
net.resolverStats() as NetResolverStats
net.stats(NetEngine engine) as NetEngineStats
```

## os

```text
os.architecture() as string
os.args() as array
os.chain(string command) as int
os.cwd() as string
os.env(string name, string defaultValue = "", int size = -1) as string
os.exec(string command, int timeoutMs = -1, int size = -1) as string
os.littleEndian() as bool
os.logicalProcessorCount() as int
os.platform() as string
os.pointerBits() as int
os.run(string command, int timeoutMs = -1, int size = -1) as ProcessResult
os.setCwd(string path) as bool
os.start(string command, int size = -1) as Process
os.startFile(string program, array<string> args = [], int size = -1, int argSize = -1) as Process
```

## path

```text
path.abs(string path) as string
path.appDir() as string
path.build(array parts, int style = 0) as string
path.clean(string path, int style = 0) as string
path.components(string path, int style = 0) as PathIterator
path.cwd() as string
path.dir(string path) as string
path.executable() as string
path.exists(string path) as bool
path.ext(string path) as string
path.home() as string
path.isAbs(string path) as bool
path.isLocal(string path, int style = 0) as bool
path.isRoot(string path) as bool
path.isRooted(string path) as bool
path.isSafeEntry(string path, bool directory = false) as bool
path.join(string first, string second, ...) as string
path.listSep() as string
path.name(string path) as string
path.nameExt(string path) as string
path.normalize(string path, int style = 0) as string
path.parent(string path) as string
path.parse(string path, int style = 0) as PathParts
path.random(string head = "", string foot = "", int len = 12) as string
path.real(string path) as string
path.rel(string base, string target) as string
path.relative(string base, string target, int style = 0) as string
path.sep() as string
path.setCwd(string path) as bool
path.stem(string path) as string
path.temp() as string
path.withExt(string path, string extension) as string
path.withName(string path, string name) as string
```

## pattern

```text
pattern.compile(PatternOptions options, array<PatternSpec> specs) as Pattern
pattern.compile(PatternOptions options, string expression, var tag = null, int size = -1) as Pattern
pattern.compile(array<PatternSpec> specs) as Pattern
pattern.compile(string expression, var tag = null, int size = -1) as Pattern
pattern.errorOffset(error value) as int
pattern.errorPatternIndex(error value) as int
```

## pem

```text
pem.decode(string text, string label = "", int size = -1, int byteLimit = 16777216) as bytes
pem.encode(string label, bytes data, int size = -1) as string
pem.parse(string text, string label = "", int size = -1, int byteLimit = 16777216) as PemBlock
```

## process

```text
process.cmd(string program, array<string> args = [], int size = -1, int argSize = -1) as ProcessCommand
process.exec(string command, int timeoutMs = -1, int size = -1) as string
process.exitCode(string command, int timeoutMs = -1, int size = -1) as int
process.open(string target, int size = -1) as bool
process.pipeline(array<ProcessCommand> commands, ProcessRunOptions options, bytes input = b"", int size = -1) as PipelineResult
process.pipeline(array<ProcessCommand> commands, bytes input = b"", int timeoutMs = -1, int size = -1) as PipelineResult
process.run(ProcessCommand command, ProcessRunOptions options, bytes input = b"", int size = -1) as ProcessResult
process.run(ProcessCommand command, bytes input = b"", int timeoutMs = -1, int size = -1) as ProcessResult
process.run(string command, int timeoutMs = -1, int size = -1) as ProcessResult
process.runFile(string program, array<string> args = [], int timeoutMs = -1, int size = -1, int argSize = -1) as ProcessResult
process.shell(string command, int size = -1) as ProcessCommand
process.start(ProcessCommand command) as Process
process.start(string command, int size = -1) as Process
process.startFile(string program, array<string> args = [], int size = -1, int argSize = -1) as Process
process.success(string command, int timeoutMs = -1, int size = -1) as bool
process.terminalSupported() as bool
```

## random

```text
random.below(uint64 bound) as uint64
random.bytes(int count) as bytes
random.fillBytes(Buffer target)
random.range(int min, int max) as int
random.rangeClosed(int min, int max) as int
random.real() as float
random.seed(uint64 seed, uint64 seq = 1)
random.shuffle(array values)
random.shuffleBuffer(Buffer target)
random.string(int length = 16, string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_", int alphabetSize = -1) as string
random.u32() as uint32
random.u64() as uint64
```

## regex

```text
regex.compile(string pattern, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int size = -1) as Regex
regex.errorOffset(error value) as int
regex.errorPatternIndex(error value) as int
regex.escape(string text, int size = -1) as string
regex.set(array<string> patterns, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int maxPatterns = 4096, int itemSize = -1) as RegexSet
```

## signal

```text
signal.cancelOn(TaskGroup group, int code, int capacity = 64) as SignalHandle
signal.clear(int code = 0)
signal.count(int code = 0) as uint64
signal.healthy() as bool
signal.ignore(int code) as bool
signal.name(int code) as string
signal.on(int code, func() handler, int capacity = 64) as SignalHandle
signal.on(int code, func(SignalEvent) handler, int capacity = 64) as SignalHandle
signal.once(int code, func() handler, int capacity = 64) as SignalHandle
signal.once(int code, func(SignalEvent) handler, int capacity = 64) as SignalHandle
signal.raise(int code) as bool
signal.received(int code) as bool
signal.reset()
signal.restore(int code) as bool
signal.restoreAll() as bool
signal.sendTo(channel<SignalEvent> ch, int code, int capacity = 64) as SignalHandle
signal.supported(int code) as bool
```

## siphash

```text
siphash.hash64(bytes data, bytes key, int size = -1) as uint64
siphash.hash64(string data, bytes key, int size = -1) as uint64
siphash.hash64Buffer(Buffer data, bytes key, int size = -1) as uint64
siphash.hasher(bytes key) as SipHasher64
```

## string

```text
string.approx(string left, string right, float tolerance = 0.95, int leftSize = -1, int rightSize = -1) as bool
string.byteAt(string text, int index, int size = -1) as uint8
string.byteLen(string text, int size = -1) as int
string.bytes(string text, int size = -1) as bytes
string.charAt(string text, int index, int size = -1) as char
string.chars(string text, int size = -1) as array<char>
string.compare(string left, string right, bool ignoreCase = false, int leftSize = -1, int rightSize = -1) as int
string.contains(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as bool
string.count(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int
string.distance(string left, string right, int limit = -1, int leftSize = -1, int rightSize = -1) as int
string.endsWith(string text, string suffix, bool ignoreCase = false, int size = -1, int suffixSize = -1) as bool
string.equals(string left, string right, bool ignoreCase = false, int leftSize = -1, int rightSize = -1) as bool
string.fields(string text, int size = -1) as array<string>
string.fieldsIter(string text, int size = -1) as StringIterator
string.filter(string text, string chars, int size = -1, int charsSize = -1) as string
string.format(float value, string format = "", int size = -1) as string
string.format(int value, string format = "", int size = -1) as string
string.format(uint64 value, string format = "", int size = -1) as string
string.glob(string text, string pattern, int flags = 0, int size = -1, int patternSize = -1) as bool
string.hasAny(string text, string chars, int size = -1, int charsSize = -1) as bool
string.indexOf(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int
string.insert(string text, int start, string part, int size = -1, int partSize = -1) as string
string.isBlank(string text, int size = -1) as bool
string.isEmpty(string text, int size = -1) as bool
string.isUtf8(string text, int size = -1) as bool
string.join(array<string> values, string sep = "", int sepSize = -1, int itemSize = -1) as string
string.lastIndexOf(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int
string.left(string text, int count, int size = -1) as string
string.len(string text, int size = -1) as int
string.like(string text, string pattern, bool ignoreCase = false, int size = -1, int patternSize = -1) as bool
string.linesIter(string text, int size = -1) as StringIterator
string.lower(string text, int size = -1) as string
string.ltrim(string text, string chars = "", int size = -1, int charsSize = -1) as string
string.mid(string text, int start, int count = -1, int size = -1) as string
string.padCenter(string text, int width, string pad = " ", int size = -1, int padSize = -1) as string
string.padLeft(string text, int width, string pad = " ", int size = -1, int padSize = -1) as string
string.padRight(string text, int width, string pad = " ", int size = -1, int padSize = -1) as string
string.remove(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as string
string.removeAt(string text, int start, int count, int size = -1) as string
string.repeat(string text, int count, int size = -1) as string
string.replace(string text, string oldText, string newText, int size = -1, int oldSize = -1, int newSize = -1) as string
string.reverse(string text, int size = -1) as string
string.right(string text, int count, int size = -1) as string
string.rtrim(string text, string chars = "", int size = -1, int charsSize = -1) as string
string.sim(string left, string right, int leftSize = -1, int rightSize = -1) as float
string.slice(string text, int start, int count = -1, int size = -1) as string
string.split(string text, string sep, int size = -1, int sepSize = -1) as array<string>
string.splitIter(string text, string separator, int size = -1, int separatorSize = -1) as StringIterator
string.splitLines(string text, int size = -1) as array<string>
string.startsWith(string text, string prefix, bool ignoreCase = false, int size = -1, int prefixSize = -1) as bool
string.trim(string text, string chars = "", int size = -1, int charsSize = -1) as string
string.upper(string text, int size = -1) as string
```

## task

```text
task.group(int limit = 0, int cancelOn = 0) as TaskGroup
```

## tcc

```text
tcc.Context() as TccContext
tcc.compile(string source, string output, int outputType = 2) as int
tcc.compileFile(string path, string output, int outputType = 2) as int
tcc.run(string source) as int
tcc.runFile(string path) as int
tcc.vfsClear()
tcc.vfsMount(string path, string text) as bool
```

## tcp

```text
tcp.connect(NetEngine engine, NetAddress remote) as TcpStream
tcp.connect(NetEngine engine, NetAddress remote, TcpStreamOptions options, int worker = 0) as TcpStream
tcp.connectAsync(NetEngine engine, NetAddress remote) as future<TcpStream>
tcp.connectAsync(NetEngine engine, NetAddress remote, TcpStreamOptions options, int worker = 0) as future<TcpStream>
tcp.dial(NetEngine engine, NetAddressList addresses) as TcpStream
tcp.dial(NetEngine engine, NetAddressList addresses, TcpDialOptions options) as TcpStream
tcp.dial(NetEngine engine, NetResolver resolver, string host, int port) as TcpStream
tcp.dial(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options) as TcpStream
tcp.dialAsync(NetEngine engine, NetAddressList addresses) as future<TcpStream>
tcp.dialAsync(NetEngine engine, NetAddressList addresses, TcpDialOptions options) as future<TcpStream>
tcp.dialAsync(NetEngine engine, NetResolver resolver, string host, int port) as future<TcpStream>
tcp.dialAsync(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options) as future<TcpStream>
tcp.dialAsyncCancel(NetEngine engine, NetAddressList addresses, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncCancel(NetEngine engine, NetAddressList addresses, TcpDialOptions options, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncCancel(NetEngine engine, NetResolver resolver, string host, int port, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncCancel(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncUntil(NetEngine engine, NetAddressList addresses, TcpDialOptions options, uint64 deadline) as future<TcpStream>
tcp.dialAsyncUntil(NetEngine engine, NetAddressList addresses, uint64 deadline) as future<TcpStream>
tcp.dialAsyncUntil(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, uint64 deadline) as future<TcpStream>
tcp.dialAsyncUntil(NetEngine engine, NetResolver resolver, string host, int port, uint64 deadline) as future<TcpStream>
tcp.dialAsyncUntilCancel(NetEngine engine, NetAddressList addresses, TcpDialOptions options, uint64 deadline, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncUntilCancel(NetEngine engine, NetAddressList addresses, uint64 deadline, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncUntilCancel(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, uint64 deadline, CancellationToken cancel) as future<TcpStream>
tcp.dialAsyncUntilCancel(NetEngine engine, NetResolver resolver, string host, int port, uint64 deadline, CancellationToken cancel) as future<TcpStream>
tcp.dialCancel(NetEngine engine, NetAddressList addresses, CancellationToken cancel) as TcpStream
tcp.dialCancel(NetEngine engine, NetAddressList addresses, TcpDialOptions options, CancellationToken cancel) as TcpStream
tcp.dialCancel(NetEngine engine, NetResolver resolver, string host, int port, CancellationToken cancel) as TcpStream
tcp.dialCancel(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, CancellationToken cancel) as TcpStream
tcp.dialUntil(NetEngine engine, NetAddressList addresses, TcpDialOptions options, uint64 deadline) as TcpStream
tcp.dialUntil(NetEngine engine, NetAddressList addresses, uint64 deadline) as TcpStream
tcp.dialUntil(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, uint64 deadline) as TcpStream
tcp.dialUntil(NetEngine engine, NetResolver resolver, string host, int port, uint64 deadline) as TcpStream
tcp.dialUntilCancel(NetEngine engine, NetAddressList addresses, TcpDialOptions options, uint64 deadline, CancellationToken cancel) as TcpStream
tcp.dialUntilCancel(NetEngine engine, NetAddressList addresses, uint64 deadline, CancellationToken cancel) as TcpStream
tcp.dialUntilCancel(NetEngine engine, NetResolver resolver, string host, int port, TcpDialOptions options, uint64 deadline, CancellationToken cancel) as TcpStream
tcp.dialUntilCancel(NetEngine engine, NetResolver resolver, string host, int port, uint64 deadline, CancellationToken cancel) as TcpStream
tcp.listen(NetEngine engine, TcpListenOptions options) as TcpListener
tcp.server(NetEngine engine, TcpServerOptions options) as TcpServer
```

## template

```text
template.compile(string text, TemplateCompileOptions? options = null, int size = -1) as Template
template.compileFile(string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template
template.compileFileIn(Vfs filesystem, string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template
template.engine() as TemplateEngine
template.errorLocation(error value) as TemplateLocation?
template.render(string text, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int size = -1) as string
template.renderFile(string path, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as string
template.renderFileIn(Vfs filesystem, string path, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as string
```

## time

```text
time.addDays(time value, int amount) as time
time.addHours(time value, int amount) as time
time.addMilliseconds(time value, int amount) as time
time.addMinutes(time value, int amount) as time
time.addMonths(time value, int amount) as time
time.addQuarters(time value, int amount) as time
time.addSeconds(time value, int amount) as time
time.addWeeks(time value, int amount) as time
time.addYears(time value, int amount) as time
time.approx(time left, time right, uint64 tolerance = 0) as bool
time.clock() as uint64
time.datePart(time value) as time
time.dateString(time value, int offset = 0) as string
time.day(time value) as int
time.dayOfYear(time value) as int
time.daysInMonth(int year, int month) as int
time.daysInYear(int year) as int
time.diffDays(time left, time right) as int
time.diffHours(time left, time right) as int
time.diffMilliseconds(time left, time right) as int
time.diffMinutes(time left, time right) as int
time.diffMonths(time left, time right) as int
time.diffQuarters(time left, time right) as int
time.diffSeconds(time left, time right) as int
time.diffWeeks(time left, time right) as int
time.diffYears(time left, time right) as int
time.firstDayOfMonth(time value) as time
time.firstDayOfWeek(time value, int startDay = 1) as time
time.firstDayOfYear(time value) as time
time.format(time value, string pattern = "%F %T", int offset = 0, int patternSize = -1) as string
time.formatPattern(time value, string pattern, int offset = 0, int patternSize = -1) as string
time.fromDate(int year, int month, int day) as time
time.fromDateTime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int millisecond = 0, int offset = 0) as time
time.fromLocal(DateTime value, TimeFold fold = TimeFold.reject) as time
time.fromTime(int hour, int minute, int second, int millisecond = 0) as time
time.fromUnix(int seconds) as time
time.fromUnixMs(int milliseconds) as time
time.hour(time value) as int
time.httpDate(time value) as string
time.inRange(time value, time start, time endValue) as bool
time.isLeapYear(int year) as bool
time.isSameDay(time left, time right) as bool
time.isSameMonth(time left, time right) as bool
time.isSameYear(time left, time right) as bool
time.isoWeek(time value) as (int, int, int)
time.lastDayOfMonth(time value) as time
time.lastDayOfWeek(time value, int startDay = 1) as time
time.lastDayOfYear(time value) as time
time.local(time value) as DateTime
time.localToUTC(DateTime value, TimeFold fold = TimeFold.reject) as time
time.millisecond(time value) as int
time.minute(time value) as int
time.monotonic() as uint64
time.month(time value) as int
time.monthRange(time value) as (time, time)
time.now() as time
time.nowUTC() as time
time.parse(string text, int size = -1) as time
time.parseFields(string text, string pattern, int size = -1, int patternSize = -1) as DateTime
time.parseHTTPDate(string text, int size = -1) as time
time.parsePattern(string text, string pattern, int size = -1, int patternSize = -1) as time
time.parseRFC3339(string text, int size = -1) as time
time.quarter(time value) as int
time.rangeOverlap(time start1, time end1, time start2, time end2) as bool
time.rfc3339(time value, int offset = 0) as string
time.second(time value) as int
time.sleep(int milliseconds)
time.sleepUntil(uint64 deadline)
time.split(time value, int offset = 0) as DateTime
time.timePart(time value) as int
time.timeString(time value, int offset = 0) as string
time.timer() as float
time.timezoneOffset() as int
time.timezoneOffset(time value) as int
time.toUnix(time value) as int
time.toUnixMs(time value) as int
time.today() as time
time.tryParse(string text, int size = -1) as (bool, time)
time.tryParseHTTPDate(string text, int size = -1) as (bool, time)
time.tryParsePattern(string text, string pattern, int size = -1, int patternSize = -1) as (bool, time)
time.tryParseRFC3339(string text, int size = -1) as (bool, time)
time.utcToLocal(time value) as DateTime
time.weekOfMonth(time value, int startDay = 1) as int
time.weekOfYear(time value) as int
time.weekRange(time value, int firstWeekday = 1) as (time, time)
time.weekday(time value) as int
time.year(time value) as int
time.yearRange(time value) as (time, time)
```

## tls

```text
tls.connect(NetEngine engine, NetAddress remote, TlsClientContext context, string serverName, int size = -1) as TlsStream
tls.connect(NetEngine engine, NetAddress remote, TlsClientContext context, string serverName, int size, string verifyName, int verifySize, TlsStreamOptions stream, TcpStreamOptions tcp, int worker = 0) as TlsStream
tls.connectAsync(NetEngine engine, NetResolver resolver, string host, int port, TlsClientContext context, TlsDialOptions options, int size = -1) as future<TlsStream>
tls.connectAsync(NetEngine engine, NetResolver resolver, string host, int port, TlsClientContext context, int size = -1) as future<TlsStream>
tls.listen(NetEngine engine, TcpListenOptions listen, TlsServerContext context) as TlsServer
tls.listen(NetEngine engine, TcpListenOptions listen, TlsServerContext context, TlsStreamOptions stream, int accepts = 128, int handshakes = 64) as TlsServer
```

## udp

```text
udp.bind(NetEngine engine, NetAddress local) as UdpSocket
udp.bind(NetEngine engine, NetAddress local, UdpConfig config, uint64 affinity = 0) as UdpSocket
udp.config() as UdpConfig
udp.connect(NetEngine engine, NetAddress peer) as UdpSocket
udp.connect(NetEngine engine, NetAddress peer, UdpConfig config, uint64 affinity = 0) as UdpSocket
udp.open(NetEngine engine, NetAddress local, NetAddress peer, UdpConfig config, uint64 affinity = 0) as UdpSocket
```

## uint16

```text
uint16.parse(string text, int base = 10, int flags = 0, int size = -1) as uint16
uint16.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, uint16)
```

## uint32

```text
uint32.parse(string text, int base = 10, int flags = 0, int size = -1) as uint32
uint32.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, uint32)
```

## uint64

```text
uint64.parse(string text, int base = 10, int flags = 0, int size = -1) as uint64
uint64.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, uint64)
```

## uint8

```text
uint8.parse(string text, int base = 10, int flags = 0, int size = -1) as uint8
uint8.tryParse(string text, int base = 10, int flags = 0, int size = -1) as (bool, uint8)
```

## unicode

```text
unicode.byteOffset(string text, int index, int size = -1) as int
unicode.decode(bytes data, int policy = 0, int size = -1) as string
unicode.isScalar(int code) as bool
unicode.repair(string text, int size = -1) as string
unicode.scalarIndex(string text, int offset, int size = -1) as int
unicode.toString(char value) as string
unicode.validate(bytes data, int size = -1) as (bool, int)
unicode.validate(string text, int size = -1) as (bool, int)
unicode.validator() as Utf8Validator
```

## vfs

```text
vfs.create() as Vfs
vfs.disk(string root, int access = 1) as DiskFS
vfs.memory() as MemoryFS
vfs.pack(string source, PackOptions options, uint64 offset = 0, int length = -1) as PackFS
vfs.pack(string source, int offset = 0, int length = -1, int format = 0, int cacheBytes = 67108864, int maxEntryBytes = 1073741824, int maxDecodedBytes = 8589934592) as PackFS
vfs.packOptions(int format = 0, uint64 maxEntries = 100000, uint64 maxIndexBytes = 67108864, uint64 maxEntryBytes = 1073741824, uint64 maxDecodedBytes = 8589934592, uint64 cacheBytes = 67108864) as PackOptions
vfs.takePack(File source, PackOptions options, uint64 offset = 0, int length = -1) as PackFS
```

## x509

```text
x509.matchesDns(string pattern, string host, int patternSize = -1, int hostSize = -1) as bool
x509.parseCrlNumber(bytes encoded, int maxBytes = 16777216) as bytes
x509.parseCrlReason(bytes encoded, int maxBytes = 16777216) as X509CrlReason
x509.parseInvalidityDate(bytes encoded, int maxBytes = 16777216) as time
x509.parseSubjectKeyId(bytes encoded, int maxBytes = 16777216) as bytes
```

## xlang

```text
xlang.Context() as XLangContext
xlang.compiledPackage(string name) as point
xlang.run(string source) as bool
xlang.runFile(string path) as bool
```

## xlon

```text
xlon.options(bool pretty = false, bool ignoreUnsupportedEncode = false, bool ignoreUnsupportedDecode = false) as XlonOptions
xlon.parse(string text) as var
xlon.parse(string text, XlonOptions options) as var
xlon.parse(string text, var options) as var
xlon.parseWith(string text, func(var) as var decoder, var options = null) as var
xlon.read(string path) as var
xlon.read(string path, XlonOptions options) as var
xlon.read(string text, var options) as var
xlon.stringify(var value) as string
xlon.stringify(var value, var options) as string
xlon.stringify<T>(T value, XlonOptions options) as string
xlon.valid(string text) as bool
xlon.valid(string text, XlonOptions options) as bool
xlon.valid(string text, var options) as bool
xlon.visit(string text, func(var) as bool visitor) as int
xlon.visit(string text, func(var) as bool visitor, XlonOptions options) as int
xlon.visit(string text, func(var) as bool visitor, var options) as int
xlon.write(string path, var value) as bool
xlon.write(string path, var value, var options) as bool
xlon.write<T>(string path, T value, XlonOptions options) as bool
xlon.writeTo(var value, func(var) as bool sink, var options = null) as bool
xlon.writer(XlonOptions options) as XlonWriter
xlon.writer(bool pretty = false) as XlonWriter
```

## xlonl

```text
xlonl.parse(string text) as var
xlonl.parse(string text, var options) as var
xlonl.read(string path) as var
xlonl.read(string text, var options) as var
xlonl.stringify(var records) as string
xlonl.stringify(var value, var options) as string
xlonl.valid(string text) as c_bool
xlonl.valid(string text, var options) as bool
xlonl.write(string path, var records) as bool
xlonl.write(string path, var value, var options) as bool
xlonl.writeTo(var value, func(var) as bool sink, var options = null) as bool
```

## 常量

| 名称 | 登记值 |
| --- | --- |
| `channelStatus.cancelled` | `5` |
| `channelStatus.closed` | `3` |
| `channelStatus.empty` | `1` |
| `channelStatus.full` | `2` |
| `channelStatus.ok` | `0` |
| `channelStatus.timeout` | `4` |
| `codec.base64IgnoreSpace` | `4` |
| `codec.base64NoPadding` | `2` |
| `codec.base64OptionalPadding` | `8` |
| `codec.base64Url` | `1` |
| `codec.hexIgnoreSpace` | `2` |
| `codec.hexUpper` | `1` |
| `compress.flushFinish` | `3` |
| `compress.flushFull` | `2` |
| `compress.flushNone` | `0` |
| `compress.flushSync` | `1` |
| `compress.gzip` | `2` |
| `compress.httpDeflate` | `3` |
| `compress.raw` | `0` |
| `compress.strategyDefault` | `0` |
| `compress.strategyFiltered` | `1` |
| `compress.strategyFixed` | `4` |
| `compress.strategyHuffman` | `2` |
| `compress.strategyRle` | `3` |
| `compress.zlib` | `1` |
| `concurrent.acqRel` | `3` |
| `concurrent.acquire` | `1` |
| `concurrent.relaxed` | `0` |
| `concurrent.release` | `2` |
| `concurrent.seqCst` | `4` |
| `console.clearLine` | `1` |
| `console.clearScreen` | `0` |
| `console.clearToEnd` | `2` |
| `console.color256` | `2` |
| `console.colorBasic` | `1` |
| `console.colorNone` | `0` |
| `console.colorTrue` | `3` |
| `console.eventClosed` | `6` |
| `console.eventKey` | `2` |
| `console.eventMouse` | `5` |
| `console.eventPaste` | `4` |
| `console.eventResize` | `3` |
| `console.eventText` | `1` |
| `console.keyBackspace` | `1114115` |
| `console.keyDelete` | `1114123` |
| `console.keyDown` | `1114117` |
| `console.keyEnd` | `1114121` |
| `console.keyEnter` | `1114113` |
| `console.keyEscape` | `1114112` |
| `console.keyF1` | `1114126` |
| `console.keyHome` | `1114120` |
| `console.keyInsert` | `1114122` |
| `console.keyLeft` | `1114118` |
| `console.keyPageDown` | `1114125` |
| `console.keyPageUp` | `1114124` |
| `console.keyRight` | `1114119` |
| `console.keyTab` | `1114114` |
| `console.keyUp` | `1114116` |
| `console.modAlt` | `4` |
| `console.modCtrl` | `2` |
| `console.modMeta` | `8` |
| `console.modShift` | `1` |
| `console.stderrStream` | `2` |
| `console.stdoutStream` | `1` |
| `console.styleBold` | `1` |
| `console.styleDim` | `2` |
| `console.styleItalic` | `4` |
| `console.styleReverse` | `16` |
| `console.styleUnderline` | `8` |
| `encoding.keepBom` | `0` |
| `encoding.rejectBom` | `3` |
| `encoding.replaceInvalid` | `1` |
| `encoding.requireBom` | `2` |
| `encoding.strict` | `0` |
| `encoding.stripBom` | `1` |
| `encoding.unknown` | `0` |
| `encoding.utf16BE` | `3` |
| `encoding.utf16LE` | `2` |
| `encoding.utf32BE` | `5` |
| `encoding.utf32LE` | `4` |
| `encoding.utf8` | `1` |
| `encoding.writeBom` | `4` |
| `file.asyncCreate` | `4` |
| `file.asyncRead` | `1` |
| `file.asyncTruncate` | `8` |
| `file.asyncWrite` | `2` |
| `file.seekCur` | `1` |
| `file.seekEnd` | `2` |
| `file.seekSet` | `0` |
| `file.shareDelete` | `4` |
| `file.shareRead` | `1` |
| `file.shareWrite` | `2` |
| `file.walkAbort` | `0` |
| `file.walkContinue` | `0` |
| `file.walkError` | `3` |
| `file.walkSkip` | `1` |
| `file.walkSkipError` | `1` |
| `file.walkStop` | `2` |
| `file.walkStopError` | `2` |
| `io.append` | `2` |
| `io.createNew` | `0` |
| `io.lineCrLf` | `2` |
| `io.lineLf` | `1` |
| `io.lineNone` | `0` |
| `io.seekCurrent` | `1` |
| `io.seekEnd` | `2` |
| `io.seekStart` | `0` |
| `io.truncate` | `1` |
| `json.visitComplete` | `0` |
| `json.visitInvalid` | `-1` |
| `json.visitStopped` | `1` |
| `log.jsonFields` | `64` |
| `log.jsonLevel` | `2` |
| `log.jsonLogger` | `4` |
| `log.jsonMessage` | `8` |
| `log.jsonNewline` | `128` |
| `log.jsonSource` | `16` |
| `log.jsonThread` | `32` |
| `log.jsonTime` | `1` |
| `log.textFields` | `32` |
| `log.textLevel` | `2` |
| `log.textLogger` | `4` |
| `log.textNewline` | `64` |
| `log.textRawMessage` | `128` |
| `log.textSource` | `8` |
| `log.textThread` | `16` |
| `log.textTime` | `1` |
| `math.e` | `2.71828182845904523536028747135266250` |
| `math.inf` | `(1.0/0.0)` |
| `math.nan` | `(0.0/0.0)` |
| `math.pi` | `3.14159265358979323846264338327950288` |
| `math.tau` | `6.28318530717958647692528676655900576` |
| `net.familyAny` | `0` |
| `net.familyIPv4` | `4` |
| `net.familyIPv6` | `6` |
| `net.interfaceBroadcast` | `8` |
| `net.interfaceLoopback` | `4` |
| `net.interfaceMulticast` | `32` |
| `net.interfacePointToPoint` | `16` |
| `net.interfaceRunning` | `2` |
| `net.interfaceUp` | `1` |
| `net.resultAgain` | `1` |
| `net.resultCancelled` | `5` |
| `net.resultClosed` | `2` |
| `net.resultError` | `-1` |
| `net.resultOk` | `0` |
| `net.resultTimeout` | `4` |
| `net.resultTruncated` | `3` |
| `net.capability.batch` | `16` |
| `net.capability.cancel` | `128` |
| `net.capability.completion` | `2` |
| `net.capability.datagramError` | `512` |
| `net.capability.edge` | `8` |
| `net.capability.fileIO` | `2048` |
| `net.capability.none` | `0` |
| `net.capability.oneShot` | `4` |
| `net.capability.post` | `64` |
| `net.capability.readProbe` | `256` |
| `net.capability.readiness` | `1` |
| `net.capability.sendFile` | `1024` |
| `net.capability.wake` | `32` |
| `process.stateExited` | `1` |
| `process.stateRunning` | `0` |
| `process.stderr` | `2` |
| `process.stdin` | `0` |
| `process.stdioInherit` | `0` |
| `process.stdioNull` | `2` |
| `process.stdioPipe` | `1` |
| `process.stdout` | `1` |
| `queueStatus.closed` | `3` |
| `queueStatus.empty` | `1` |
| `queueStatus.full` | `2` |
| `queueStatus.ok` | `0` |
| `queueStatus.timeout` | `4` |
| `signal.BREAK` | `1001` |
| `signal.CLOSE` | `1002` |
| `signal.HUP` | `1` |
| `signal.INT` | `2` |
| `signal.LOGOFF` | `1003` |
| `signal.SHUTDOWN` | `1004` |
| `signal.TERM` | `15` |
| `taskGroupCancelOn.cancelled` | `2` |
| `taskGroupCancelOn.closed` | `4` |
| `taskGroupCancelOn.failed` | `1` |
| `taskGroupCancelOn.none` | `0` |
| `taskGroupCancelOn.stopped` | `7` |
| `taskGroupState.cancelled` | `3` |
| `taskGroupState.closed` | `4` |
| `taskGroupState.failed` | `2` |
| `taskGroupState.pending` | `0` |
| `taskGroupState.resolved` | `1` |
| `tcc.DLL` | `4` |
| `tcc.EXE` | `2` |
| `tcc.MEMORY` | `1` |
| `tcc.OBJ` | `3` |
| `tcc.PREPROCESS` | `5` |
| `unicode.complete` | `0` |
| `unicode.invalid` | `2` |
| `unicode.more` | `1` |
| `xlon.visitComplete` | `0` |
| `xlon.visitInvalid` | `-1` |
| `xlon.visitStopped` | `1` |
