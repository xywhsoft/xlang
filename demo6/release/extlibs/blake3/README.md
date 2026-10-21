# blake3

BLAKE3 普通哈希、原生 keyed hash 和密钥派生。使用随包 portable C 后端，
复用宿主 XRT/xruntime；不需要额外 BLAKE3 DLL。上游版本、许可证和本地补丁见
[VENDOR.md](VENDOR.md)。

## 快速使用

```xlang
import blake3

auto hash = blake3.hasher()
hash.update("ab")
auto branch = hash.clone()
hash.update("c")
branch.update("d")
assert hash.digestHex() == blake3.hashHex("abc")
assert branch.digestHex() == blake3.hashHex("abd")
println(hash.digestHex(16))
```

文本按 UTF-8 字节输入，不按字符数量截断。任意二进制使用 `Buffer`。
摘要长度单位是字节；hex 输出为小写，字符数量是摘要长度的两倍。

## 公开函数

| 函数 | 参数与返回 |
| --- | --- |
| `version() as string` | 当前后端版本 `1.5.0` |
| `hashHex(Buffer data, int outLen = 32) as string` | 普通哈希 |
| `hashHex(string text, int outLen = 32) as string` | UTF-8 文本哈希 |
| `hashBytes(Buffer data, int outLen = 32) as Buffer` | 独立拥有型原始摘要 |
| `hashBytes(string text, int outLen = 32) as Buffer` | 文本输入、原始摘要输出 |
| `hashMacHex(Buffer data, Buffer key, int outLen = 32) as string` | 两个 Buffer，密钥必须恰好 32 字节 |
| `hashMacHex(string text, string key, int outLen = 32) as string` | 两个 string，密钥按 UTF-8 字节校验 |
| `hashKdfHex(Buffer keyMaterial, string context, int outLen = 32) as string` | 按用途上下文派生密钥 |
| `hashKdfHex(string keyMaterial, string context, int outLen = 32) as string` | 文本材料的派生模式 |
| `hashFileHex(string path, int maxInput = 268435456, int outLen = 32, (func(int, int, int) as bool)? onProgress = null) as string` | 文件普通哈希、输入预算和进度回调 |
| `hasher(int maxInput = 268435456) as Blake3Hasher` | 普通增量对象 |
| `keyedHasher(Buffer key, int maxInput = 268435456) as Blake3KeyedHasher` | 原生 keyed 增量对象 |
| `deriveHasher(string context, int maxInput = 268435456) as Blake3Hasher` | 派生模式增量对象 |

常量 `BLAKE3_OUT_LEN`、`BLAKE3_KEY_LEN` 都是 32。空密钥、31/33 字节密钥都报错，
不填充、不截断。密钥在创建时进入独立状态，之后修改或释放调用方 Buffer 不改变对象。
`context` 是区分用途的非秘密字符串，应由应用选择稳定、具体的名称；不是口令。
KDF 与 `deriveHasher` 按完整 UTF-8 字节长度读取上下文，内嵌 NUL 不会截断；
带 NUL 的上下文与其 NUL 前缀派生出不同结果。
该模式不代替 Argon2 等口令慢哈希，参见
[固定版本的上游说明](https://github.com/BLAKE3-team/BLAKE3/blob/5aa53f07f7188a569cadfc5daf1522972d9a9630/README.md)。

`outLen` 必须为正。BLAKE3 支持可扩展长度输出，同一输入的短摘要是长摘要的前缀；
不同于 BLAKE2 将长度作为初始化参数。当前分配型薄绑定统一在分配前检查 XRT 默认
512 MiB 输出预算；hex 的摘要和文本临时空间最多约为 `3 * outLen + 1`，
输出预算不是峰值内存承诺。零和负数不表示“默认值”或“无限制”。

没有独立的 keyed/KDF 一次性 Bytes 函数，需要原始字节时使用对应对象的
`digestBytes(outLen)`，无需把 hex 再解析一次。文件接口当前只有普通模式；
keyed/KDF 文件处理可由调用方分块读取后 `update`。

## 增量对象

`Blake3Hasher` 和 `Blake3KeyedHasher` 具有一致的常用成员：

| 成员 | 合同 |
| --- | --- |
| `update(Buffer data)` / `update(string text)` | 追加数据，空输入有效 |
| `digestHex(int outLen = 32) as string` | 当前摘要快照，不终结状态 |
| `digestBytes(int outLen = 32) as Buffer` | 同一快照语义，返回独立 Buffer |
| `inputBytes as int` | 成功累计接收的字节数 |
| `valid as bool` | 句柄非空，不表示未进入失败状态 |
| `clone()` | 返回同类的独立状态，保留模式、密钥、计数、限额和失败状态 |
| `reset() as bool` | 清空输入和失败状态，保留模式、派生后的密钥及限额，成功返回 true |

读取摘要后可以继续更新，也可以再次用不同 `outLen` 读取。失败的 update 不增加输入
计数，并使该对象进入失败状态；需要 reset 才能继续更新或取摘要。克隆失败对象仍是
失败对象，原对象与克隆对象须各自 reset。非法摘要参数不毒化原有增量数据。

推荐通过工厂创建。公开的 keyed 构造器接收 `bytes` 值，由原生入口读取
真实长度并要求恰好 32 字节；`keyedHasher(Buffer)` 先复制 32 字节为 `bytes`，
不会向导入方暴露独立的裸指针和长度。C 调用方仍可使用公开头中的
指针/长度接口，并负责保证两者匹配。`create(point)` 仅用于包内接管句柄，
虽已声明私有，当前编译器仍允许包外调用，见
`demo6/dev/extlib_blockers/blake3_private_adopt_constructor.md`；主线修复前
不能签收发布级访问安全。两个哈希器类的 `*Native` 成员均为私有；
接管构造转移唯一所有权，不能多次接管同一句柄。

不同对象可以独立运行，同一可变 hasher 须串行访问。析构清零自己的状态再释放，
不承诺擦除调用方副本、寄存器、系统交换页或已经导出的摘要。

## 文件、进度与错误

```xlang
import blake3

string digest = blake3.hashFileHex("data.bin",
    onProgress = func(int inputBytes, int totalBytes, int outputBytes) as bool
        return true
    end)
```

文件路径使用 XRT 的 UTF-8 文件 API；内嵌 NUL 会报 `ValueError`，不会打开截断后的路径。
默认输入预算 256 MiB；xlang 工厂和文件函数的
`maxInput` 必须为正，它是累计输入限额，不是 CPU 时间或峰值内存预算。

回调同步执行，每块输入通知一次，EOF 时再通知一次；空文件也收到结束通知。
三个参数依次为已消费字节、可知的输入总字节、输出字节；未知总长为 0，输出字节恒为 0。
语言回调不提供 FINAL 标志，最后一次数值可能与前次相同，不可仅凭数值区分两次通知。
C 的进度帧保留 TOTAL_KNOWN/FINAL 标志。所有计数先从 uint64 检查范围再进入语言 int。

回调返回 false 表示取消；回调抛错保留原类别和消息。可以嵌套另一独立文件哈希，
不能重入同一可变 hasher。取消或失败不返回部分摘要，也不改变源文件；读取过程中
不提供防止其他进程修改文件的一致性快照，不回退 reader 已消费的位置。

| 情况 | 语言错误 |
| --- | --- |
| 非法密钥、摘要长度、非正输入预算 | `ValueError` |
| 输入/输出超预算、计数溢出 | `RangeError` |
| 分配失败 | `MemoryError` |
| 文件打开失败 | 沿用 XRT `FileError` |
| Reader 读取失败 | 保留原始 XRT 错误；无诊断时为 `IOError` |
| 无效句柄 | `RuntimeError` |
| 回调返回 false 且没有原始错误 | `CancelledError` |
| 用户回调抛出异常 | 保留原类别和消息 |

## C API

核心头 [include/xrt_blake3.h](include/xrt_blake3.h) 不依赖 xlang 编译器：

- `xrtBlake3OptionsInit` / `xrtBlake3LimitsInit` / `xrtBlake3ErrorInit` 初始化流对象配置、文件限额与错误记录。
- `xrtBlake3Hash` / `xrtBlake3HashKeyed` / `xrtBlake3HashKdf` 写入调用方的输出缓冲。
- `xrtBlake3HasherCreate` / `CreateKeyed` / `CreateDeriveKey` 创建三种模式。
- `xrtBlake3HasherUpdate` / `Digest` / `Reset` / `Clone` 与语言对象语义一致。
- `xrtBlake3HasherInputBytes` / `Error` 查询状态，`Destroy(NULL)` 安全。
- `xrtBlake3File` 借用 xreader，从当前游标到 EOF，不负责销毁 reader。

核心一次性函数没有分配型包装的默认输出预算，由调用方提供足够大小的缓冲。
输入指针仅在长度为零时允许 NULL；输出必须非空且长度为正，Keyed 的密钥指针必须
指向可读取的 32 字节。KDF 上下文由指针及显式字节长度构成；允许内嵌 NUL，
空上下文可用，NULL 指针不可用。C 字符串调用者应显式传入 `strlen(context)`。
输出在参数失败、Reader 失败或取消时保持不变。

Hasher/File 的 NULL 配置使用默认资源限制。流对象用 `xrtBlake3OptionsInit`，
文件接口的显式 `xrtresourcelimits` 用 `xrtBlake3LimitsInit`；通用
`xrtResourceLimitsInit` 会设置与 BLAKE3 无关的非零限额，不适用于文件接口。
显式配置的输入/输出限额为零时
关闭对应业务限额，但仍检查整数溢出。原生薄绑定 `maxInput=0` 则表示采用默认值，
不是关闭限制；这不同于核心 C 配置，xlang 工厂不接受零。
显式配置的结构长度、版本必须匹配；除输入/输出预算外的资源限额、
标志和保留字段必须为零；
错误配置在创建对象或读取 Reader 前拒绝，输出保持不变。

薄绑定头 [include/blake3_native.h](include/blake3_native.h) 与生成原型一致：
hex 返回值借用当前 XRT 临时区，清理或回退后失效，长期保留需复制；原始摘要返回
拥有型 `xbuffer*`，C 调用方须 `xrtBufferDestroy`。错误来自 `xrtGetError()`，
不要保存跨后续错误操作的借用错误消息。创建得到的原生对象由对应 destroy 释放。
核心 `xrtBlake3File` 借用 Reader；C 调用方须检查自有 Reader 的关闭结果。
XLang 的 `hashFileHex` 自行关闭文件，关闭失败不会返回摘要，并保留先发生的
读取或回调错误。

独立 C 工程编译 `blake3_portable.c`、`xrt_blake3.c`，链接自己的 XRT 实现。
只有使用语言薄绑定才需要 `blake3_native.c` 和 xruntime 可调用值模块；全部 XRT
模块选择须在首次包含头文件前完成，不能在一个程序中再编译第二份 XRT。

## 范例与验收

从 demo6 根目录运行：

```powershell
release\xl.exe run release\examples\extlibs\blake3.xl
release\xl.exe run release\examples\extlibs\blake3_basic.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package blake3 -Source release/extlibs/blake3/examples/basic.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_blake3.ps1
```

Linux 联合门禁：`bash tools/check_extlib_blake3.sh`，同次运行 C ASan/UBSan、三组中央 XLang JIT 用例、一组包内向量用例与 C/XLang 范例逐字节对照。`bash tools/check_extlib_linux_xl.sh blake3` 可单独复跑中央语言用例；两者都不替代 AOT 门禁。
包内 `test/vectors.xl` 已登记到 `coverage.tests`，直接用官方向量检验 XLang 的普通、keyed、KDF 和跨块 XOF 调用。
双语对照源为 [examples/basic.c](examples/basic.c) 和
[blake3_basic.xl](../../examples/extlibs/blake3_basic.xl)，共同预期输出位于
[blake3_basic.expected.txt](../../examples/extlibs/blake3_basic.expected.txt)。
C 对照范例的 hash/clone/reset、keyed/KDF、XOF 与 HEX 编码不依赖 `assert`
副作用；Windows/Linux 包门禁用 `-DNDEBUG` 构建执行，并对照官方向量输出。

Windows 专项覆盖严格 GCC、23 个实际生成原型、C/xlang JIT/AOT、35 条上游向量的
三模式六长度、Reader/回调/Unicode、分配失败和独立四线程。Linux 联合门禁使用
C ASan/UBSan/泄漏检查并运行 XLang JIT。门禁记录实际运行输入的哈希与文件集合，资料变更后应重新运行。
这里不承诺 Linux AOT、32 位/ARM、同一对象并发、SIMD 吞吐或密码学审计认证。

2026-10-17：Linux 包级脚本加入三组中央 XLang 用例、教学范例和 C/XLang
基础范例黄金输出对照；与原 C sanitizer 门禁同次通过。带输出 AOT 的共享
缺符号阻断不变，未签收发布级别。

2026-10-18：文件哈希薄绑定原忽略 Reader 关闭失败，误返有效摘要。
自定义 Reader 的关闭失败负例先红后绿，并验证读取先失败时保留原错误、
正常路径仍与一次性哈希一致；新用例进入两平台包级门禁。Windows 严格
GCC/C 与 XLang JIT、Linux C ASan/UBSan/XLang 双语联合门禁通过。
当前 Windows 完整门禁在首个 AOT 构建缺 `xl6NativeHandleTraceLeaf`，
见 `demo6/dev/extlib_blockers/native_handle_trace_aot.md`；不签收 AOT。

2026-10-18：包外 `updateNative(point, size)` 可伪造输入长度，两类哈希器的
只编译负例先确认可达，再将 14 个原生方法私有化；负例已进入 Windows/Linux
包级门禁。接管构造器的主线访问控制缺陷与完整 AOT 阻断均未因此解除。

2026-10-19：C 配置中非零 `xrtresourcelimits.iReserved` 原被普通、keyed/KDF 共用
创建入口和 Reader 文件哈希接受；对象与文件负例先红后绿，现在统一拒绝，
无效配置不消费 Reader、不改输出。Linux 官方 35 组向量、ASan/UBSan、
XLang 双语门禁和 Windows 严格 C/JIT 通过；完整 AOT 仍缺共享
`xl6NativeHandleTraceLeaf`，未签收。

2026-10-20：C 对照范例现在可从 `demo6` 目录直接执行
`release/xl.exe run release/extlibs/blake3/examples/basic.c`，也可直接
`xl build` 生成可执行文件。仅 TCC 单文件路径包含便携后端与核心源，
正式 GCC/manifest 构建保持分离。Windows
`tools/check_extlib_blake3.ps1 -SkipAot` 与 Linux
`tools/check_extlib_blake3.sh` 均将 C 直跑、严格 C 构建和 XLang 范例
分别核对官方向量派生的七行黄金输出；Linux C 测试启用 ASan/UBSan。
完整 XLang AOT 和私有接管构造器访问边界仍待主线修复，不签收发布级。

同日移除导出 keyed 类的 `point + keyLen` 构造入口。只编译负例先证明
短 Buffer 可以伪造 32 字节长度，修复后要求 `create` 重载拒绝该调用；
公开工厂保持 `Buffer` 参数，C 原始 ABI 保持不变。新 `bytes` 构造入口由
XRT 验证实际字节长度，C 正/负例和 XLang 官方向量、Windows 严格 GCC/JIT、
Linux ASan/UBSan/双语门禁均通过。私有句柄接管与完整 AOT 阻断不变。

2026-10-21：完整教学范例的文件哈希改用独占系统临时目录，显式检查
写入，并在 `finally` 清理；`xl check` 与
`xl run release/examples/extlibs/blake3.xl` 均通过。Windows 包门禁改为
每次独占 `run_<id>` 目录、只清理本次目录并单独保存唯一 JSON 结果；
Linux 门禁使用 `mktemp` 独占目录。Windows `-SkipAot` 的严格 C/JIT、
官方向量与双语对照，以及 Linux C ASan/UBSan、XLang 门禁均通过。
默认 Windows 门禁首个 AOT 仍缺 `xl6NativeHandleTraceLeaf`；包外
`private extern create(point)` 负例 `xl check` 仍错误返回 `ok`。
两项主线阻断仍在，不签收发布级。

2026-10-21：四份 C 测试原将官方向量、分配故障、原生对象、
关闭错误和线程操作放在 `assert` 内，定义 `NDEBUG` 会删掉这些操作。
测试单元现保持断言启用，Windows/Linux 门禁以 `-DNDEBUG` 编译运行；
预处理确认 hasher 更新调用仍在。Windows 严格 C/JIT、生成原型和双语
门禁，以及 Linux 35 条官方向量、ASan/UBSan、线程与 XLang 门禁通过。
库实现/API 未改，完整 AOT 和私有接管构造器的主线阻断仍在，
不签收整包发布级。
