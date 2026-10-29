# blake2

BLAKE2b/BLAKE2s 一次性、增量及文件哈希。C 核心和 xlang 包使用同一份实现，
不依赖外部 BLAKE2 DLL，不重新编译第二份宿主 XRT。

## xlang API

```xlang
import blake2

auto digest = blake2.blake2bHashHex("hello", 16)
auto hasher = blake2.blake2sHasher(16)
hasher.update("hel")
hasher.update("lo")
assert hasher.digestHex() == blake2.blake2sHashHex("hello", 16)
```

下表的 `b/s` 表示分别存在 `blake2b...` 与 `blake2s...` 两组函数。

| API | 参数与结果 |
| --- | --- |
| `version()` | 返回内置实现版本字符串 |
| `blake2b/sHashHex(data, digestSize)` | data 为 Buffer 或 string；返回小写十六进制 string |
| `blake2b/sHashBytes(data, digestSize)` | 相同输入，返回拥有型 Buffer |
| `blake2b/sKeyedHex(data, key, digestSize)` | 两个 Buffer 或两个 string；返回原生 keyed hash 的小写十六进制 |
| `blake2b/sKeyedBytes(data, key, digestSize)` | 相同输入，返回拥有型 Buffer |
| `blake2b/sFileHex(path, maxInput, digestSize, onProgress)` | 从文件生成小写十六进制摘要；使用 XRT UTF-8 文件接口 |
| `blake2b/sHasher(digestSize, maxInput)` | 创建无密钥增量对象 |
| `blake2b/sKeyedHasher(key, digestSize, maxInput)` | key 为 Buffer；构造时复制密钥 |

- BLAKE2b 默认摘要为 64 字节，允许 1..64 字节，密钥最多 64 字节。
- BLAKE2s 默认摘要为 32 字节，允许 1..32 字节，密钥最多 32 字节。
- 空密钥等价于不使用密钥。非空密钥超过上限报错，不截断。
- `BLAKE2B_OUT_LEN`、`BLAKE2S_OUT_LEN`、`BLAKE2B_KEY_MAX`、`BLAKE2S_KEY_MAX`
  为公开常量。string 按 UTF-8 字节处理；任意二进制内容应使用 Buffer。
- `maxInput` 默认 268435456 字节，xlang 工厂和文件接口要求正数。
- 短摘要在算法初始化时确定，不等于截取完整摘要的前几个字节。
- keyed hash 是 BLAKE2 原生 MAC，不是 HMAC；不是密码存储的口令派生函数。

### 增量对象

四个公开 native-backed class 为 `Blake2bHasher`、`Blake2sHasher`、
`Blake2bKeyedHasher`、`Blake2sKeyedHasher`。通常使用上表工厂函数创建。
公开 keyed 构造器只接收自带真实长度的 `bytes` 值；工厂接收 `Buffer`，
仅复制密钥字节后调用该构造器。C 原始指针/长度 ABI 仍可直接使用，
调用方须自行保证指针有效且长度匹配。
四类接管原生句柄的 `create(point)` 已声明私有，但当前编译器仍接受
包外直接构造；见 `dev/extlib_blockers/blake2_private_adopt_constructor.md`。
在主线修复前，本包不能签收完整发布级访问安全。
四类对象的 `*Native` 方法已限制在类内部，导入方需通过公开的 `update`、
摘要、重置和克隆成员操作，不直接传入裸指针和自定义字节长度。

| 成员 | 合同 |
| --- | --- |
| `update(Buffer|string)` | 接收任意分块；空输入有效 |
| `digestHex()` | 读取当前状态的摘要，不结束对象，不接受另一种摘要长度 |
| `digestBytes()` | 返回独立拥有型 Buffer，读取不修改状态 |
| `inputBytes` | 已成功接收的输入字节数，不包括密钥填充块 |
| `valid` | 是否拥有非空原生句柄 |
| `clone()` | 创建独立增量状态，后续输入互不影响 |
| `reset()` | 返回 true，恢复初始状态并保留摘要长度、密钥和限额 |

对象销毁时释放并清零本对象的状态和保存的密钥；不承诺清除用户的输入副本、
编译器寄存器副本或操作系统交换页。同一个对象须串行使用；不同对象可以独立运行。
输入非法或超过限额后，对象保持失败状态，禁止继续更新或读取，直到显式 reset。
本次失败的输入不增加计数。普通 digest 输出参数错误不会破坏已有哈希状态。

### 进度与异常

`onProgress` 默认为 null，类型为 `(func(int, int, int) as bool)?`：

```xlang
string digest = blake2.blake2bFileHex("data.bin", onProgress =
    func(int inputBytes, int totalBytes, int outputBytes) as bool
        return true
    end)
```

回调同步执行，参数均为字节数，`outputBytes` 为 0。未知总长为 0。每个输入块通知
一次，完成时再通知一次，空文件也有最终通知。最终通知的输入值可与上一通知相同。
返回 false 抛 CancelledError；回调抛出的异常保留原类型和消息。
回调可以发起另一次独立哈希。文件函数只读取源文件，失败或取消不返回部分摘要。
文件路径通过 XRT UTF-8 API 打开；内嵌 NUL 会报 `ValueError`，不会读取截断后的前缀路径。
读取过程中不保证文件内容不被其他进程修改，不提供一致性文件快照。

- 非法密钥或摘要长度：ValueError。
- 超过输入限额：RangeError。
- 对失败对象 clone：RuntimeError；继续 update/digest 保留原失败原因。
- 打开文件失败：XRT 的 FileError；读取失败：IOError。
- 分配失败：MemoryError；回调取消：CancelledError。

## C API 与所有权

核心头为 `include/xrt_blake2.h`，薄绑定原型为 `include/blake2_native.h`。
模块选择须在第一次包含 XRT 之前完成，包清单的 `artifact.xrtModules` 给出完整依赖。

- `xrtBlake2OptionsInit` 初始化带尺寸和版本的配置，不能传入未初始化结构。
  文件接口的显式 `xrtresourcelimits` 使用 `xrtBlake2LimitsInit` 初始化；
  通用 `xrtResourceLimitsInit` 会设置与哈希无关的非零限额，不适用于此处。
  内嵌限额的长度、版本必须匹配；除 `iMaxInputBytes` 外的资源限额、
  标志和保留字段必须为零。无效配置在分配对象或读取 Reader 前拒绝，不消费输入。
- `xrtBlake2b/sHasherCreate/Update/Digest/Reset/Clone/Destroy` 对应同一套增量语义。
  `HasherDigestSize` 查询构造长度；C Digest 的长度参数必须与构造配置精确相同。
- `xrtBlake2b/sHash` 使用调用方输出内存，容量必须足够容纳请求的摘要。
- `xrtBlake2b/sFile` 借用 reader，从其当前游标读到 EOF，不负责销毁 reader。
  原始 C 进度帧还包含 `XRT_PROGRESS_FINAL` 与 `XRT_PROGRESS_TOTAL_KNOWN` 标志。
  C 调用方关闭自有 reader 时须检查 `xrtReaderDestroy` 返回值。XLang 的
  `blake2b/sFileHex` 会自行关闭文件，关闭失败不返回摘要而报告 IO 错误。
  Reader 读取时的内存失败保留为 `XRT_BLAKE2_ERROR_MEMORY` / `MemoryError`，
  不归类为普通 IO；结构化错误消息保留 Reader 的原始诊断。
- C 核心限额中的零表示关闭业务限额；uint64 累计计数仍做溢出校验。
  xlang 薄绑定的零值采用默认限额，公开 xlang 工厂不接受零。
- 对象错误通过 `HasherError` 借用获取；一次性和 reader 接口通过可选 error 输出。
  构造或 clone 分配失败通过 XRT 当前错误返回。C 调用必须检查返回值。
- 薄绑定返回的 `char*` 借用 XRT 当前临时区，直到该区清理或回退；长期保存应复制。
  后续哈希调用不会覆盖已有临时字符串。薄绑定返回的 Buffer 归调用方，需
  `xrtBufferDestroy`。`hasher_adopt` 只转移已有句柄，禁止重复接管或自行提前释放。

## 范例与验证

- 教学范例：`release/examples/extlibs/blake2.xl`。
- 对照范例：`release/examples/extlibs/blake2_basic.xl` 与 `examples/basic.c`，
  逐字匹配 `release/examples/extlibs/blake2_basic.expected.txt`。
- C 对照范例的哈希、clone/reset 与 HEX 编码不依赖 `assert` 副作用；Windows/Linux
  包门禁以 `-DNDEBUG` 构建并实际执行，再与同一黄金输出对照。
- Windows 门禁：`tools/check_extlib_blake2.ps1`；`-SkipAot` 仅供聚焦快测，仍检查私有接管构造器，不能用于发布签收。
- Linux 联合门禁：`tools/check_extlib_blake2.sh`，同次运行 C ASan/UBSan、三组中央 XLang 用例、包内向量及双语范例的 JIT/AOT，并检查私有接管构造器。`bash tools/check_extlib_linux_xl.sh blake2` 可单独复跑中央语言用例，但不替代包级门禁。
- 包内 `test/vectors.xl` 已登记到 `coverage.tests`，独立检查 BLAKE2b/BLAKE2s 的默认及 keyed 可变长度已知答案。

2026-10-26 当前发布版的正例复验：Windows 严格 C、216 条已知答案、
分配故障、生成 ABI、三组中央用例及双语范例 JIT/AOT 均通过；Linux
C ASan/UBSan、六条独立 EXE AOT 和双语输出对照也通过。旧 AOT
缺符号已不复现。但包外 `private extern create(point)` 在两平台
`xl check` 仍返回 `ok`；默认门禁现纳入这条只编译负例，并须在
访问控制修复后才允许整包发布签收。绝不执行该危险负例。

2026-09-27 Windows 固定输入门禁通过 36 项检查，包括 38 个实际生成的原生原型、
C/xlang JIT/AOT/GCC、教学范例、共同黄金输出、216 条 Python hashlib 已知答案、
状态/reader 边界、16 个注入分配失败点、四线程独立临时结果及错误状态。
Linux 联合门禁使用 ASan/UBSan 与泄漏检查，并运行 XLang JIT；不等于 Linux AOT 已验收。
普通堆故障和临时区 backing 分配故障分开注入，已有对象摘要和分配计数保持不变。

本批不包含 salt/personalization、树模式、SIMD 加速、32 位/ARM 平台、完整性能基准、
密码学审计认证或共享同一个可变对象的并发操作。这些边界不因单项测试通过而扩大。

2026-10-17：Linux 包级脚本加入现有三组 XLang 中央用例、完整教学范例和
C/XLang 基础范例逐字节黄金输出对照；与原有 C sanitizer 测试同次通过。
完整带输出 AOT 仍受共享运行时缺符号阻断，发布验收状态不变。

2026-10-18：文件哈希薄绑定原忽略 Reader 关闭失败，BLAKE2b/s 会误返有效摘要。
自定义 Reader 的关闭失败负例先红后绿，另验证读取先失败时保留首个错误、
正常路径仍与一次性哈希一致；新用例进入两平台包级门禁。Windows 严格
GCC/C 与 XLang JIT、Linux C ASan/UBSan/XLang 双语联合门禁通过。
当前 Windows 完整门禁在首个 AOT 构建缺 `xl6NativeHandleTraceLeaf`，
见 `dev/extlib_blockers/native_handle_trace_aot.md`；不签收 AOT。

2026-10-18：包外 `updateNative(point, size)` 原可伪造输入长度；b/s、普通/keyed
四个导出类的只编译负例先确认可达，随后将 28 个原生方法私有化。负例已纳入
Windows/Linux 包级门禁；接管构造器的主线访问控制缺陷及完整 AOT 阻断仍在。

2026-10-18：BLAKE2b/s 的 Reader 内存读取失败原固定报告结构化 `IO`。
C 自定义 Reader 负例先红后绿，现保留 `MEMORY` 分类和原始消息，XLang
薄绑定对应抛 `MemoryError`。Windows 严格 C 和 Linux ASan/UBSan、
XLang 双语联合门禁复验通过；AOT 共享阻断未变。

2026-10-20：C 对照范例现在可从 `demo6` 目录直接运行
`release/xl.exe run release/extlibs/blake2/examples/basic.c`，也可直接
`xl build` 生成可执行文件。仅 TCC 单文件路径包含核心与上游源；正式
GCC/manifest 构建仍分别编译。Windows
`tools/check_extlib_blake2.ps1 -SkipAot` 与 Linux
`tools/check_extlib_blake2.sh` 将直跑 C 输出、
严格 GCC C 输出和 XLang 范例分别核对固定六行摘要；Linux 还通过
216 条已知答案、C ASan/UBSan 和语言 JIT。完整 XLang AOT 及私有
接管构造器的主线阻断仍在，未签收发布级。

同日移除两个导出 keyed 类的 `point + keyLen` 构造入口，改用原生
`bytes` 值读取真实密钥长度；b/s 的短 Buffer 伪造长度只编译负例均由
通过转为 `create` 重载错误。C 桥接验证空指针、超长和有效密钥，
XLang 官方向量补直接构造正例。Windows 严格 C/JIT 与 Linux C ASan/UBSan、
XLang、双语门禁通过。私有句柄接管和完整 AOT 阻断不变。

2026-10-19：C 选项内嵌限额的非零保留字段原被 BLAKE2b/s 对象和 Reader
接口静默接受；两套 C 负例先红后绿，现由共用限额校验拒绝，且错误输入不消费
Reader。Linux 216 条答案、ASan/UBSan、XLang 双语门禁与 Windows 严格
C/JIT 通过；完整 AOT 仍缺共享 `xl6NativeHandleTraceLeaf`，未签收。

2026-10-21：完整教学范例的文件哈希不再写固定 `examples_out`，改用
独占系统临时目录，写入后检查成功，并在 `finally` 删除。`xl check` 和
`xl run release/examples/extlibs/blake2.xl` 均通过；原固定路径下的旧文件
未改动。Windows 包门禁每次使用独占 `run_<id>` 目录，清理时只删除本次
目录并另存唯一 JSON 结果；Linux 门禁改用 `mktemp` 独占目录。Windows
`-SkipAot` 的严格 C/JIT/向量/双语对照与 Linux C ASan/UBSan、XLang
门禁均通过。默认 Windows 门禁的首个 AOT 仍缺
`xl6NativeHandleTraceLeaf`；包外 `private extern create(point)` 负例
`xl check` 仍错误返回 `ok`。两项主线阻断未解除，不签收发布级。

2026-10-21：四份 C 测试原在 `assert` 表达式中执行已知向量、
分配故障、原生对象、关闭错误和线程调用；定义 `NDEBUG` 会删掉这些操作。
测试单元现保持断言启用，Windows/Linux 包门禁均以 `-DNDEBUG`
编译执行；预处理确认 keyed hasher 调用仍在。Windows 严格 C、
XLang JIT 和双语门禁，以及 Linux 216 条已知答案、ASan/UBSan、
线程和双语门禁通过。库实现/API 未改，完整 AOT 和私有构造器
的主线阻断仍未解除，不签收整包发布级。

2026-10-24：当前 Windows 发布构建的默认专项门禁 48 项通过，包括
216 条已知答案、严格 C 边界、38 个生成原型、C/XLang JIT 与 AOT
以及双语范例输出对照。此前的 `xl6NativeHandleTraceLeaf` 缺符号
在本次 BLAKE2 复测中不再出现。两份可执行范例改为显式 `main`，
使原生 hasher 按函数生命周期释放；顶层写法的 AOT 退出状态错误
仍存在，同类最小复现与修复建议见
`dev/extlib_blockers/blake3_global_native_aot.md`。
包外 `private extern create(point)` 的只编译负例仍被错误接受，
不能运行该负例或签收完整发布级访问安全。Linux 包门禁待当前 Linux
发布版重建，构建脚本阻塞见
`dev/extlib_blockers/linux_release_x509_sources.md`。
