# gzip

标准 gzip 字节流与事务文件压缩。编解码、CRC32 和 ISIZE 校验复用宿主 XRT，
包内不再重复实现 gzip 封装。无额外压缩 DLL 依赖。

> 2026-10-14 当前发布物的文件 OOM 事务故障注入在独立 C、`xl run`、
> `xl build` 产物以及 `xl.dll`/`xlc.dll` 的公开 `xlRunCSourceFiles` 宿主上均通过。
> 完整 XLang 用例仍受已记录的 XLIR 字节异或问题阻断，不能把文件专项通过
> 当作整个包已完成 RC 验收。不覆盖模式区分目标不存在与查询失败。

## xlang

```xlang
import gzip

Buffer packed = gzip.compress("你好，gzip", level = 6)
assert gzip.decompressText(packed) == "你好，gzip"

gzip.GzipCompressor encoder = gzip.compressor()
encoder.update("left")
Buffer stream = encoder.drain()
encoder.update("right")
stream.append(encoder.finish())
assert gzip.decompressText(stream) == "leftright"
```

| API | 参数与结果 |
| --- | --- |
| `compress(Buffer|string data, int level=-1, int mtime=0, int maxInput=268435456, int maxOutput=536870912)` | `Buffer` |
| `decompress(Buffer data, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000)` | `Buffer` |
| `decompressText(Buffer data, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000)` | 解压后按字符串读取；二进制内容使用 `decompress` |
| `compressFile(string source, string destination, int level=-1, int mtime=0, bool overwrite=true, int maxInput=268435456, int maxOutput=536870912, onProgress=null)` | 同步文件压缩，无返回值 |
| `decompressFile(string source, string destination, bool overwrite=true, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000, onProgress=null)` | 同步文件解压，无返回值 |
| `compressor(int level=-1, int mtime=0, int maxInput=268435456, int maxOutput=536870912)` | `gzip.GzipCompressor` |
| `decompressor(int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000)` | `gzip.GzipDecompressor` |

两个导出类均有只读属性 `valid`、`inputBytes`、`outputBytes`、`finished`，
以及 `drain() as Buffer`、`finish() as Buffer`。压缩器 `update` 接收字符串或
Buffer，解压器只接收 Buffer。也可通过 `from gzip import GzipCompressor`
选择性导入类型；日常构造建议用模块工厂，避免手动指定 `c_*` 参数。
`*Native` 成员只供包内包装代码调用，导入方使用上述公开方法和属性。
包级门禁通过只编译负例验证访问权限，不执行伪造指针与长度的调用。

- 字符串按 UTF-8 **字节数**传入；Buffer 保留 NUL 和任意字节。
- level 为 -1 或 0..9，-1 使用 XRT 默认级别。mtime 为 Unix 秒数 0..4294967295；
  默认零，输出不会自动写入当前时间、源文件名或注释。
- xlang maxInput/maxOutput 必须为正；maxRatio 为 1..4294967295。所有计数均为字节。
- `drain` 移交目前积累的输出，不强制刷新编码器。`finish` 完成后只返回未交付部分；
  重复 finish 返回空 Buffer。调用者必须收集每次 drain 和 finish 的结果。
- 完成后禁止 update；输入参数错误可纠正重试，编解码、限额或 writer 失败为终态。
  同一对象串行使用，不支持重入；已交付的 Buffer 与对象生命周期独立。

## 文件与进度

```xlang
gzip.compressFile("input.txt", "output.gz",
    onProgress = func(int inputBytes, int totalBytes, int outputBytes) as bool
        println(inputBytes, "/", totalBytes)
        return true
    end)
```

回调在调用线程同步执行，不被持久保存。返回 false 抛 CancelledError；回调抛出的
异常保留原类型与消息。无法查询总量时 totalBytes=0；最后一次通知也可以取消，
空文件压缩只产生最终通知。xlang 三个整型参数不含 C 进度结构的 flags，不应把
`inputBytes == totalBytes` 当成唯一的最终事件，输入读完后还可能有结束输出。

文件接口在目标目录使用独占临时文件，所有处理、最终回调、flush 和 close 成功后
才替换目标。失败或取消保留原目标，清理本次临时文件。支持 source=destination；
overwrite=false 不覆盖已存在文件。这是正常进程运行中的失败原子性，不保证断电
后目录项持久性，也不提供多个写者的事务隔离。
两个文件接口在进入零终止 C 路径前检查 source 和 destination；内嵌 NUL 抛
`ValueError`，不会静默操作 NUL 前的文件名。

## 格式和错误

支持合法空内容、多个串联成员、FEXTRA/FNAME/FCOMMENT/FHCRC；不暴露头元数据
读取接口。CRC、ISIZE、保留标志错误、截断和尾部垃圾均拒绝，不以部分结果冒充成功。
CRC 用于损坏检查，不是加密认证。

| 条件 | xlang 异常 |
| --- | --- |
| 无效压缩流、截断、文件 IO | `IOError` |
| 错误 level、非正限额、禁止覆盖、文件路径含 NUL | `ValueError` |
| 输出/输入/解压比超限，mtime/maxRatio 超 uint32 | `RangeError` |
| 已完成后 update、失败流继续使用 | `RuntimeError` |
| 分配失败 | `MemoryError` |
| 回调返回 false | `CancelledError` |

核心 XRT 错误会在 native 调用边界传播；不要统一只捕获 CompressionError。
解压比按“累计输出 / 已提交压缩输入”约束，增量分块会影响检查时点；需要严格控制
资源时始终设置 maxOutput。限额不是 CPU 超时、压缩器内存上限或不可信输入沙箱。

## C API

包含 `include/xrt_gzip.h`。首次包含 XRT 完整头前完成模块选择，完整依赖见
`xlang.package.json` 的 `artifact.xrtModules`；只链接一份 XRT/xruntime 实现。

- `xrtGzipOptionsInit` / `xrtGzipErrorInit` 初始化配置/错误快照。
- `xrtGzipCompress` / `xrtGzipDecompress` 的成功结果为拥有型 `xbuffer*`，调用方
  `xrtBufferDestroy`；失败时输出槽置 NULL，同时保留 XRT 结构化错误。
- `xrtGzipCompressor*` / `xrtGzipDecompressor*` 提供 Create、Update、Finish、
  Drain、DrainBuffer、TakeBuffer、Error、InputBytes、OutputBytes、Finished、Destroy。
- `Drain(writer)` 借用 writer，不关闭它。`DrainBuffer` 可多次移交；`TakeBuffer`
  只允许 Finish 后取一次，之后对象仅可查询和销毁。Finish 本身不移交 C Buffer。
- `xrtGzipCompressStream` / `xrtGzipDecompressStream` 借用 reader/writer，接收
  `xrtprogressproc` 和 user 指针；C 进度结构提供 TOTAL_KNOWN、FINAL 标志。
  短读短写可用，原始 IO/回调错误保留。裸流失败前可能已经写出部分数据。
- C 配置 level -1..9、strategy 为 XRT 的 DEFAULT..FIXED。tLimits 只使用
  iMaxInputBytes、iMaxOutputBytes、iMaxCompressionRatio；这三项零值关闭对应限制。
  OptionsInit 将其他归档/文档型限额清零；显式设置这些字段或 iFlags 会返回
  XERR_ARGUMENT，不表示已经得到额外保护。

薄绑定头 `include/gzip_native.h` 与 xlang 实际生成的 22 个 extern 原型接受 GCC
逐项一致性检查；它不是另一套压缩算法。C 与 xlang 基础范例使用相同输入和黄金输出：

- `examples/basic.c`
- `../../examples/extlibs/gzip_basic.xl`
- `../../examples/extlibs/gzip.xl`：时间戳、串联成员、增量流、文件回调和清理。

C 基础范例的压缩、解压、流式与截断检查使用显式条件和统一清理；
`-DNDEBUG` 构建仍会执行完整流程，并仅在全部检查成功后输出黄金结果。
从 `demo6/release` 可直接执行 `./xl run extlibs/gzip/examples/basic.c`；
范例仅在 TCC 单文件路径包含它实际使用的 `xrt_gzip.c`，无需额外源文件参数。
正式包仍通过清单独立编译原生单元，GCC 范例也保持分离编译。

## 验证

在 demo6 根目录运行 `powershell -File tools/check_extlib_gzip.ps1`，需要 GCC 与
Python 3 在 PATH 中。Linux C 与 XLang 文件验证为 `bash tools/check_extlib_gzip.sh`。
包内 `test/file_ops.xl` 已登记到 `coverage.tests`，覆盖 UTF-8 文件往返、进度回调、
禁止覆盖、取消清理及解压超限时保护原目标文件。
当前发布 DLL 的文件事务专项可单独运行
`powershell -File tools/check_extlib_codec_dll_file.ps1 -Package gzip`；
它使用 `test/support/extlib_codec_dll_host.c` 经公开 API 执行同一份 C 故障夹具。

2026-09-27 固定快照：Windows 34 项检查通过；含 C/xlang JIT/AOT、22 个生成原型、
139 项 Python/zlib 独立互通检查、9 个分配故障点、UTF-8 路径、最终取消、回调异常、
同路径文件替换、原目标保留和临时文件清理。WSL Linux GCC 的 C 路径、同组互通、
分配测试和双语黄金输出通过 ASan/UBSan/泄漏检查。

完整 Linux xlang JIT/AOT、超大文件、并发压力、断电恢复和性能基准不在本批验证范围；
源码与发布二进制升级后需要重跑，不把本记录称为所有平台的生产认证。

2026-10-16：当前 Linux 发布物从 `demo6` 根目录运行 `gzip_basic.xl` 成功，覆盖 UTF-8、二进制、增量流和截断拒绝。完整 `test/cases/extlib_gzip.xl` 仍在第 49 行的 Buffer 字节索引异或触发已记录的 XLIR 错误；基础范例通过不代表完整 Linux JIT/AOT 验收。
2026-10-17：当前 Windows/Linux `xl run` 的包内文件事务用例通过；Linux 包级门禁同时执行 C sanitizer、Python 互通和该语言用例。共享 XLIR 阻断未变。

2026-10-19：C 基础范例单文件 `xl run` 原因未带入 `xrt_gzip.c` 而缺符号；
现按上文方式包含所用实现。Windows TCC 直跑与独立 `xl build`、包运行器均输出
共同黄金结果；Linux GCC ASan/UBSan、Python/zlib 139 项互通、文件事务和
TCC 单文件输出对照门禁通过。完整 XLang 用例的既有 XLIR 阻断仍在，
不据此签收整包发布级。

2026-10-19：修正 C 配置初始化继承无关非零资源限额的问题，显式设置不支持的
资源字段/标志时拒绝，而非悄悄忽略。Windows C 定向用例及完整门禁的 C、互通、
文件事务阶段通过；门禁仍在已有的字节异或 XLIR 错误处停止。Linux C ASan/UBSan、
139 项互通及 XLang 文件门禁通过，未据此签收完整 XLang/AOT。

同日核对完整 XLang 教学范例：文件写入与读回比较原把 `Buffer` 直接当作
`bytes`，现显式使用 `source.toBytes()` 并检查写入结果。当前 `xl check`
已越过文件调用，在已登记的 Buffer 索引异或 XLIR 缺陷处停止；独立
`gzip_basic.xl` 实跑通过。不得将基础范例通过视为完整范例验收。

2026-10-20：综合范例文件段改用 XRT 独占临时目录，结束或异常时由
`finally` 删除目录及其文件，不再写固定 `build/examples/gzip`。包内
`test/file_ops.xl` 和基础范例实跑通过；综合范例仍在第 53 行的 typed
XLIR 字节 XOR 处编译失败，新的文件段尚未在该综合程序中完成运行验收。
主线阻断不变，不据此签收整包发布级。

2026-10-21：三份包内 C 测试和共享文件分配测试原在 `assert`
内执行压缩、流式 IO 与故障注入，定义 `NDEBUG` 会删掉实际操作。
测试单元现保持断言启用，Windows/Linux GCC 门禁以 `-DNDEBUG`
编译运行；预处理确认文件故障注入仍在。Windows 严格 C、Python
zlib 互通、文件事务 JIT/AOT 和中央 C AOT 通过，联合门禁随后在
`extlib_gzip.xl:49` 的既有 typed XLIR 错误处停止。Linux C
ASan/UBSan、139 项互通、文件事务及 XLang 文件门禁通过。
库实现/API 未改，不签收整包发布级。

2026-10-21：文件事务回归增加压缩与解压进度回调抛异常的路径：
原始 `ValueError` 消息透传；新目标不落盘，已有解压目标保留原内容。
Windows `xl run test/file_ops.xl` 与 Linux C ASan/UBSan、Python/zlib
互通及 XLang 文件门禁通过。Windows 联合门禁现于 AOT 前执行该用例；
严格 C、互通和文件事务 JIT 均通过，随后在文件分配夹具的 AOT
构建遇到共享符号 `xrtNetWorkerPortCapabilitiesV1` 未解析。独立
`xl build release/extlibs/gzip/examples/basic.c` 可复现，详见
`dev/extlib_blockers/gzip_worker_capabilities_link.md`。没有修改
gzip 实现或 XRT，不据此签收整包发布级。
