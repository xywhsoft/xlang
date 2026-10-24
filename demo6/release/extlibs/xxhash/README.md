# xxhash

XXH32/XXH64 非密码哈希，支持 UTF-8 文本、Buffer、增量对象和带进度/取消的文件读取。
使用随包 C 后端，复用 XRT 的资源、文件和错误设施，无额外 xxHash DLL。
不用于口令、签名或防恶意篡改；来源和许可证见 [VENDOR.md](VENDOR.md)。

## 使用

```xlang
import xxhash

println(xxhash.hash64Hex("abc")) // 44bc2cf5ad770999
auto h = xxhash.hasher()
h.update("a")
h.update("bc")
assert h.digest() == xxhash.hash64("abc")
assert h.inputBytes == uint64(3)
```

所有文本按 UTF-8 **字节数**处理，不按字符数截断，不先创建 Buffer。
任意二进制数据用 Buffer，包括零字节。一次性输入为空是合法情况。
整数摘要和种子是无符号数，不经过有符号 int；hex 是固定宽度小写文本。

## 公开函数

| 函数 | 参数与结果 |
| --- | --- |
| `version()` | 返回固定后端版本 `0.8.3` |
| `hash64(Buffer|string data, uint64 seed = 0) as uint64` | 完整 64 位摘要 |
| `hash64Hex(Buffer|string data, uint64 seed = 0) as string` | 16 个小写 hex 字符 |
| `hash32(Buffer|string data, uint32 seed = 0) as uint32` | 完整 32 位摘要 |
| `hash32Hex(Buffer|string data, uint32 seed = 0) as string` | 8 个小写 hex 字符 |
| `hashFile64(string path, uint64 seed = 0, int maxInput = 268435456, (func(int, int, int) as bool)? onProgress = null) as uint64` | 文件摘要 |
| `hashFile64Hex(...) as string` | 与 hashFile64 相同参数，返回 hex |
| `hasher(uint64 seed = 0, int maxInput = 268435456) as Xxhash64Hasher` | 增量 XXH64 对象 |

`Buffer|string` 在表中表示两个重载，不是新的类型语法。`XXHASH_DEFAULT_SEED` 为零。
种子必须在对应无符号类型的范围内，不隐式截断高位或把负数当作无符号种子。
语言层的 `maxInput` 必须为正，默认为 256 MiB；不是 CPU 时限、峰值内存或文件快照保证。
一次性函数不另设输入预算，因为输入已由调用方持有。

## 增量对象

| 成员 | 含义 |
| --- | --- |
| `update(Buffer)` / `update(string)` | 追加字节，空输入有效 |
| `digest() as uint64` | 读取当前摘要，可重复读取并继续追加 |
| `digestHex() as string` | 同样的快照，以 16 个小写 hex 字符表示 |
| `reset() as bool` | 清除输入和失败状态，保留种子和预算 |
| `inputBytes as uint64` | 已成功消费的字节数 |
| `valid as bool` | 原生句柄是否非空，不表示对象没有发生过 update 错误 |

超过输入预算或传入非法 C 数据后，对象进入失败状态，后续 update/digest 继续报错，
直到 reset。失败的 update 不增加计数、不吞掉半个输入。计数溢出始终拒绝。
`inputBytes` 可读取已经成功消费的计数；C 核心与薄绑定对空句柄均返回 0
并设置 `XERR_STATE`，有效句柄读取成功时清除本线程旧错误。
推荐使用 `hasher()` 工厂；`*Native` 成员仅供包内包装代码使用，导入方不能自行传入指针或字符串长度。
C 调用方通过下述公开头使用原生接口，仍须遵守其借用与长度合同。
状态由 XRT 分配一次，上游流式状态直接内嵌；语言对象自动析构，不隐藏复制资源。
不同对象可以独立并发；同一可变对象必须串行访问。

## 文件、回调与错误

```xlang
uint64 sum = xxhash.hashFile64("data.bin",
    onProgress = func(int current, int total, int output) as bool
        return true
    end)
```

三项回调参数依次为已读字节、总字节、输出字节。输出恒为零，未知总长为零。
每块最多读取 64 KiB，EOF 再通知一次，空文件也通知；语言回调没有 FINAL 标记，
所以末尾计数可能重复。XRT 的 uint64 进度经范围检查后才转换为语言回调的 int。

- 返回 false：抛出 CancelledError；包括末尾取消，不返回已完成的摘要。
- 回调主动抛异常：保留原始类别和消息，可捕获后正常调用下一次操作。
- 可嵌套另一独立文件哈希；没有全局 file-status 或循环复用的 hex 槽。
- 非法参数为 ValueError，超过预算为 RangeError，分配失败为 MemoryError。
- 打开失败沿用 XRT FileError；Reader 原始错误不改写为统一的 I/O 错误。
- 文件始终通过 XRT UTF-8 API 打开；内嵌 NUL 的路径报 `ValueError`，不会打开截断后的前缀路径。成功/失败都关闭自己打开的 reader；不修改输入文件。

## C API

核心头：[include/xrt_xxhash.h](include/xrt_xxhash.h)。
提供 OptionsInit、LimitsInit、ErrorInit、32/64、HasherCreate/Update/Digest/Reset/InputBytes/Error/Destroy、
File64，统一前缀为 `xrtXxhash`。所有失败进入 XRT 错误系统，可选详细 error 记录辅助定位。
一次性和 Reader 接口失败时不写调用方摘要输出。错误后不能仅依赖零摘要判断失败。

`xrtXxhashFile64` **借用** reader，从当前游标读到 EOF，不关闭、不回退；
总长度是剩余字节，不是整个文件长度。可寻址且超预算时，在读之前拒绝。
非寻址 Reader 按已读块检查预算，失败可能已经消费该块，不承诺游标回滚。
C 的 xrtprogress 帧保留 TOTAL_KNOWN/FINAL 标志。

NULL 配置使用默认输入预算。流对象用 `xrtXxhashOptionsInit`，文件接口的显式
`xrtresourcelimits` 用 `xrtXxhashLimitsInit` 初始化；不要直接使用通用
`xrtResourceLimitsInit`，因为它会设置与哈希无关的非零限额。
明确初始化后，`iMaxInputBytes=0` 表示不设业务上限，但仍检查整数溢出。
Options/Limits 的 size/version 必须匹配；除 `iMaxInputBytes` 外的资源限额、
标志和保留字段必须为零，显式设置无效字段会在创建状态或读取 Reader 前拒绝，
不消费输入。
薄绑定构造和文件函数的 maximum=0 表示默认预算，语义与核心显式零限额不同。

薄绑定头：[include/xxhash_native.h](include/xxhash_native.h)。point 对应 `void*`，
string 对应 `const char*`，C 整数使用精确的 C 类型；进入核心时才作为 const 借用。
hex 返回值借用当前 XRT 临时区，临时区清理/回退后失效；长期保留请复制。
成功的下一次调用不会覆盖此前文本；不同线程使用各自的临时区。

## 范例与验证

- [xlang 教学范例](../../examples/extlibs/xxhash.xl)：文件、回调、二进制、错误、增量。
  文件段在 XRT 独占系统临时目录运行，并在异常路径清理；缺失文件负例引用清理后的路径，
  不写入固定的 `build/test` 文件。
- [xlang 对照范例](../../examples/extlibs/xxhash_basic.xl) 与 [C 对照范例](examples/basic.c)。
- [共同固定输出](../../examples/extlibs/xxhash_basic.expected.txt)。
C 对照范例的一次性哈希、增量更新及 reset 均显式执行；Windows/Linux 包门禁
以 `-DNDEBUG` 构建运行并核对六行黄金输出，避免 Release 模式打印未初始化结果。
- [核心 C 边界](test/test_xxhash.c)、[原生层/分配故障/线程](test/test_xxhash_native.c)。
- [高位种子与空输入](test/seeded_boundaries.xl)：公开 32 位已知向量、空 Buffer、64 位高位种子及增量对象；登记在包清单的 `coverage.tests`。
- [增量限额与恢复](test/stream_limit.xl)：超限报错、失败状态、保留计数及 reset 后重用；登记在包清单的 `coverage.tests`。
- [原生入口访问负例](test/private_update_negative.xl)、[字符串长度访问负例](test/private_string_length_negative.xl)：只做编译检查，不执行伪造指针或长度。
- [负数种子拒绝](test/invalid_seed_negative.xl)、[32 位种子溢出拒绝](test/invalid_seed32_overflow.xl)：
  预期语义错误的负例，不能当作成功运行的范例。
- `tools/check_extlib_xxhash.ps1`：严格 GCC、124 条独立向量、实际生成声明、JIT/AOT、双语输出；`-SkipAot` 仅供开发时缩短反馈。
- `tools/check_extlib_xxhash.sh`：Linux 独立 C ASan/UBSan/泄漏检查与范例对照。
- `bash tools/check_extlib_linux_xl.sh xxhash`：Linux XLang JIT 的普通、边界、文件进度用例及双语范例固定输出；不替代 C sanitizer 或 AOT 门禁。
- `bash release/extlibs/xxhash/test/check.sh`：组合上述 Linux C/XLang 门禁，验证包内正例与原生入口访问负例。

原有 `tools/check_extlib_xxhash.*` 门禁记录输入清单和哈希；新增包内脚本复用其验证并加一条定向 XLang 用例。门禁包含编译步骤，不把检查项数当作 API 数。
2026-10-24 使用更新后的 Windows 发布版复验，默认门禁完整通过，
包含 C/ABI、124 条独立向量、生成原型、边界/进度与双语范例 AOT；
Linux C ASan/UBSan 与 XLang JIT 联合门禁也通过。旧版共享 AOT 缺符号
阻断已解除；Linux XLang AOT、XXH3/128 语言 API、32 位/ARM 仍不在
本次验收范围内。

以下为历史验证记录，其中共享缺符号描述均指当时的旧发布版。
2026-09-27 的专项覆盖 37 项 Windows 检查、全部 19 个实际生成的原生声明，
以及 Linux 独立 C ASan/UBSan/泄漏检查。种子负数/超宽负例在语义阶段拒绝；
非法输入预算在文件打开前拒绝。C 和 xlang 对照范例的六行输出完全一致。
TCC 适配也检查系统头包含顺序，不依赖目标宏提前定义，不要求额外的 xxHash DLL。
当前语言公开面是 XXH32 一次性、XXH64 一次性/流式/文件；没有宣称 XXH3/128 语言 API、
XXH32 流式对象、Linux xlang AOT、32 位/ARM、共享可变 hasher 并发或性能认证。
原生后端保留上游函数；不能把上游 API 存在当作对应语言包装已实现。
2026-10-15 当前正式 `xl.exe` 的 C/XL JIT 与双语对照通过；完整 AOT
在普通 `Buffer` 最小复现中缺少 `xl6NativeHandleTraceLeaf`，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`，本包不绕开主线链接问题。

2026-10-17：补包内高位种子/空 Buffer 边界并登记目录覆盖；中央文件进度用例改用独立临时目录，避免并行运行争用固定路径。Windows 正式 `xl.exe` 定向运行和 Linux 包级 C ASan/UBSan、XLang、双语门禁通过。完整 AOT 的共享缺符号阻断不变。
2026-10-18：限额和 reset 用例确认现有公开方法已按文档抛错，无需另造错误包装；包外 `updateNative(point,size)` 与 `updateStrNative(string,size)` 两条只编译负例先红后绿，四个仅供包装的 `*Native` 成员私有化。Linux C ASan/UBSan、XLang JIT、双语对照与新用例联合通过；完整 AOT 共享阻断不变。
2026-10-19：C 配置中的 `xrtresourcelimits.iReserved` 非零原被静默接受；流对象创建及 Reader 文件哈希负例先红后绿，现通过共用校验拒绝，并检查文件输入未被消费。Linux C ASan/UBSan、124 条向量、XLang JIT 与双语对照通过；Windows 严格 C 与 JIT 通过。Windows 完整脚本仍在共享 AOT 符号 `xl6NativeHandleTraceLeaf` 处停止，未签收完整 AOT。

2026-10-20：C 教学范例现在可从 `demo6` 目录直接执行
`release/xl.exe run release/extlibs/xxhash/examples/basic.c`；仅 TCC 单文件路径
包含所需后端源码，正式 GCC/manifest 构建保持分离编译。Windows
`tools/check_extlib_xxhash.ps1 -SkipAot` 和 Linux 包门禁将直跑结果与
固定六行摘要对照；严格 C、124 条独立向量、Linux ASan/UBSan 和
XLang JIT 同次通过。`-SkipAot` 不改变共享 AOT 缺符号状态，未签收发布级。

2026-10-21：两份 C 边界测试曾把哈希操作、分配和故障注入写在 `assert`
表达式内，定义 `NDEBUG` 后会连同操作一起消失。测试单元现保持断言启用，
Windows/Linux 包门禁均以 `-DNDEBUG` 编译执行；预处理确认关键调用仍在。
Windows 严格 C、XLang JIT 和双语范例，以及 Linux C ASan/UBSan/泄漏门禁通过。
库实现和 API 未改，完整 AOT 的共享缺符号阻断仍未解除，不签收整包发布级。
