# SHA3 / SHAKE

FIPS 202 的 SHA3-224/256/384/512 固定摘要与 SHAKE128/256 可变长输出。
这不是 Keccak-256、SHA-256、HMAC、密码哈希或签名接口。
包版本和 `version()` 均为 `1.0.0`，无外部动态库依赖。

## xlang API

```xlang
import sha3

auto h = sha3.sha3Hasher()
h.update("你")
h.update("好")
string digest = h.digestHex()
auto branch = h.clone()
branch.update("!")
assert h.inputBytes == uint64(6)
```

| 函数 | 合同 |
|---|---|
| `version()` | 包版本字符串 |
| `hashHex(data, bits=256)` | 小写 hex，bits 只接受 224、256、384、512 |
| `hashBytes(data, bits=256)` | 对应的 28、32、48、64 字节 Buffer |
| `hash256Hex(data)` / `hash256Bytes(data)` | 固定 SHA3-256 的便捷接口 |
| `shake128Hex(data, outLen=32)` / `shake256Hex(data, outLen=32)` | outLen 是输出字节数，hex 长度为两倍 |
| `shake128Bytes(data, outLen=32)` / `shake256Bytes(data, outLen=32)` | 对应的 Buffer；零长度合法 |
| `hashFileHex(path, bits=256, maxInput=268435456, onProgress=null)` | 文件固定摘要 |
| `hashFileBytes(path, bits=256, maxInput=268435456, onProgress=null)` | 文件摘要 Buffer |
| `sha3Hasher(bits=256, maxInput=268435456)` | 创建 Sha3Hasher |
| `shakeHasher(strength=128, maxInput=268435456)` | 创建 ShakeHasher，strength 只接受 128、256 |

所有 data 接口均有 string / Buffer 重载。普通文本按 UTF-8 **字节**处理；任意二进制和 NUL 使用 Buffer。
空输入合法。不会把无效位宽、负输出长度、大整数截断成默认参数。

## 流式对象

- `update(string|Buffer)` 追加输入；`inputBytes` 为 uint64，`valid` 仅表示句柄非空。
- Sha3Hasher 的 `digestHex(outLen=0)` / `digestBytes(outLen=0)`：零选择自然长度；显式长度必须精确等于该算法的固定摘要长度，不允许截断/扩展。
- ShakeHasher 的 `digestHex(outLen=32)` / `digestBytes(outLen=32)`：输出任意非负字节数，受分配预算限制；没有 128 字节上限。
- digest 是**非终结快照**，重复调用输出相同前缀，之后仍可 update。这不是推进读取游标的 squeeze API。
- `clone()` 创建独立算法状态，不共享可变吸收缓冲区。
- `reset()` 返回 bool，清空输入和失败状态，保留算法及资源预算。
- update 失败后不接收部分输入，后续 update、digest、clone 同样失败，直至 reset；digest 参数错误不污染有效流。
- 独立对象可并发，同一个可变对象由调用者串行访问。析构不承诺安全擦除敏感材料。

## 文件与错误

`onProgress` 为可空 `func(int current, int total, int output) as bool`，同步调用。
每块最多 64 KiB；EOF 另通知一次，包括空文件，最后一次 current 可能重复。
total 为可查询的剩余字节数，未知为零；output 恒为零。
XRT uint64 计数转 int 之前检查范围。回调可嵌套独立操作。

返回 false 抛 CancelledError，最终 EOF 回调也可以取消；抛出的异常保留类别和消息。
取消/失败不交付部分摘要。参数错误为 ValueError，预算/表示范围超限为 RangeError，
分配失败为 MemoryError，文件错误保留 XRT 类别。路径采用 XRT UTF-8 文件接口。
预算不是读取超时；不承诺文件被并发修改时的快照一致性。

xlang maxInput 必须大于零，默认 256 MiB。分配型 SHAKE 输出采用 XRT 默认预算 512 MiB；
hex 拥有型工作区约为 `3*outLen+1` 字节，XLang 直接接管该分配，不再额外复制完整 XOF 文本；不表示推荐单次分配到上限。

## C API 与所有权

`include/xrt_sha3.h` 是公开 C 核心：OptionsInit/ErrorInit、DigestSize、Hash/ShakeHash、
HasherCreate/ShakeHasherCreate/Update/CheckOutput/Digest/Reset/Clone/InputBytes/OutBytes/Error/Destroy、File。
先配置 XRT 模块，再包含 XRT 头；该头需要 IO_FILE。已有 XRT 宿主只链接一份实现。
`test/test_sha3_standalone.c` 验证不依赖 xlang/xruntime 的 C 核心。

- 所有输入指针在本次调用内借用；size>0 不得传 NULL，输出空间由调用者保证。
- SHA3 的 DigestSize 返回自然长度；非法位宽返回零并设置 XRT 错误。
- SHAKE 的 OutBytes 返回零表示变长；输出零字节可传 NULL，仍是成功。
- 输出参数须精确匹配 SHA3 或实际 SHAKE 长度，调用前可用 CheckOutput 检查状态和预算。
- options 与 limits 严格核验 iSize/iVersion；C 显式零限额表示无业务上限，仍检查累计整数溢出。
- 一次性 C 核心使用调用方输出内存，不另设分配型接口的 512 MiB 上限。
- File 借用 Reader，从当前游标读取，不回退或关闭；已知剩余长度先预检，未知长度逐块限额。
- C 进度帧有 TOTAL_KNOWN/FINAL 标志；最终回调确认成功之后才写出摘要。
- 每次操作清理旧错误，失败查 XRT 错误。Reader/回调的原始错误优先；局部 error 记录类别、位置、说明。

`include/sha3_native.h` 声明全部 30 个薄绑定入口，其准确 ABI 与生成代码对照检查。
原 C hex 文本借用 XRT 临时区，TempClear/回退/线程退出后失效，正常后续调用不会覆盖先前结果。新增 `_owned` C hex 入口返回独立 XRT 分配，调用方必须用 `xrtFree` 释放；XLang 的一次性和流式摘要直接接管这份分配。
返回 xbuffer 归调用者所有，C 使用 xrtBufferDestroy，xlang 由 Buffer 接管。
零长度 hex 同样为空串，所以 C 不以空串单独判断失败，必须查看 XRT 错误。
adopt 仅转移有效句柄所有权，不验证任意地址，也不可将同一句柄交给两个析构所有者。

## 范例与验证

| xlang | 同功能 C |
|---|---|
| `examples/extlibs/sha3_basic.xl` | `extlibs/sha3/examples/basic.c` |
| `examples/extlibs/sha3.xl` | `extlibs/sha3/examples/files.c` |

在 demo6 下执行 `tools/check_extlib_sha3.ps1`：严格 GCC、独立 C 核心、30 个 native 入口、
23 个生成声明、C/xlang JIT/AOT、双语固定输出、UTF-8、失败恢复、Reader/回调边界、
四线程独立对象、176 个普通/临时区分配故障位置。
当前 AOT 主线缺符号期间，`-SkipAot` 可独立复验 C/JIT；默认门禁
仍保留 AOT 构建与失败结果。Linux 使用 `bash tools/check_extlib_sha3.sh`，
同次验证独立 C ASan/UBSan/泄漏、XLang JIT 用例及两组双语黄金输出。
两门禁记录并复核输入文件集合与哈希，不接受测试期间悄悄变化的来源。

546 组边界向量在 8 种地址偏移运行，含速率块边界、65536 字节边界、0..1024 字节 SHAKE 输出；
另有 12 组 abc、UTF-8+NUL、百万 a 答案。预期值由 Node crypto/OpenSSL 独立生成，
`node release/extlibs/sha3/test/generate_vectors.mjs --check` 可复核，不是运行/编译依赖。
这不等于 FIPS 认证、独立密码学审计、Linux xlang AOT、大端、32 位/ARM、共享对象并发或吞吐性能认证。
算法来源和修改记录见 `third_party/PROVENANCE.md`。

2026-10-21：两组完整文件范例不再写固定 `build/test/sha3-example`，
现分别使用独占临时目录并清理；C 范例移除有副作用的 `assert`，
包级门禁以 `-DNDEBUG` 执行并核对固定输出。Windows、Linux 门禁每次
使用独占工作目录，保留此前固定目录中的文件。Windows `-SkipAot` 的
严格 C、完整 XLang JIT、生成 ABI、C JIT/AOT 与双语对照，以及 Linux
C ASan/UBSan、XLang JIT/双语联合门禁通过。

2026-10-24：一次性 SHA3/SHAKE 和流式 digestHex 的 XLang 返回改用拥有型 XRT 缓冲，SHAKE 可变长 hex 不再从临时区整段复制到语言字符串。新增 64 KiB 输出、零长度、流式快照、超限及转移后保留测试；Windows `-SkipAot` 门禁通过。完整 AOT 门禁仍在既有 `xl6NativeHandleTraceLeaf` 未解析处失败，本次不计为 AOT 签收。

两个导出哈希器的句柄接管构造器及原生指针/长度方法已标记私有。
包外 `updateNative` 只编译负例被拒绝，并进入两平台门禁；
包外 `create(point)` 只编译负例目前仍错误返回 `ok`，绝不运行，
见 `demo6/dev/extlib_blockers/sha3_private_adopt_constructor.md`。
默认 Windows 门禁首个 XLang AOT 仍缺 `xl6NativeHandleTraceLeaf`。
两项主线阻断未解除，不签收整包发布级。

2026-10-24 复测：当前 Windows 发布版的默认专项门禁 41 项全部通过，
包括 C 边界、XLang JIT/AOT、拥有型文本返回、23 个生成原型及 C/XLang
范例。此前 `xl6NativeHandleTraceLeaf` 未解析问题在本次 SHA3 AOT
构建中不再出现。`private_adopt_constructor_negative.xl` 仅执行 `xl check`
仍错误输出 `ok`、退出 0，故不能签收完整发布级访问安全，绝不运行
该负例。Linux 联合门禁此次未能使用旧发布物复验；重建阻塞见
`demo6/dev/extlib_blockers/linux_release_x509_sources.md`。
