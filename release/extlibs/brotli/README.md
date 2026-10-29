# Brotli

2026-10-27 使用当前源码重建 Windows 发布版后，默认
`tools/check_extlib_brotli.ps1` 退出 0：严格 C、分配故障、原生入口
访问负例、外部 Brotli 流夹具、生成原型、语言 JIT/AOT、C/XLang
范例同输出，以及事务文件分配故障的 C/JIT/AOT 均通过。
这是 Windows x64 的包级短时验收；本次未重跑 Linux、32 位、TSan
或长时压力，不据此签收这些范围。

Brotli 1.1.0 后端，提供一次性 Buffer/UTF-8 文本、增量流和事务文件接口。
压缩输出直接是标准 Brotli 字节流，不附加私有帧头、结束记录或校验码。
该包依赖宿主 XRT/xruntime，不需要另外安装 Brotli 动态库。

> 文件 OOM 修复已定点同步源码。2026-10-17 使用当前 `release/xl.exe` 与本包
> 源码执行 `test/file_allocation.c`，JIT、生成的 AOT 程序及当前 `xl.dll`/`xlc.dll`
> 公开 `xlRunCSourceFiles` 宿主均通过覆盖/不覆盖目标的逐分配点故障扫描、目标保留
> 和临时文件清理断言。DLL 可单独运行
> `tools/check_extlib_codec_dll_file.ps1 -Package brotli` 复验；不推断旧预编译程序已修复。

## xlang

```xlang
import brotli

Buffer packed = brotli.compress("你好，Brotli", quality = 4)
assert brotli.decompressText(packed) == "你好，Brotli"

auto encoder = brotli.compressor(quality = 4)
Buffer output = Buffer()
encoder.update("part one")
output.append(encoder.drain())
encoder.update("part two")
encoder.finish()
output.append(encoder.drain())
```

| API | 返回与用途 |
| --- | --- |
| `version()` | 后端版本字符串 |
| `compress(Buffer|string, quality=11, window=22, mode=0, maxInput=268435456, maxOutput=536870912)` | 压缩 Buffer；文本按 UTF-8 字节长度处理 |
| `decompress(Buffer, maxInput=268435456, maxOutput=536870912, maxRatio=1000)` | 解压 Buffer |
| `decompressText(Buffer, maxInput=268435456, maxOutput=536870912, maxRatio=1000)` | 校验 UTF-8 并接管文本缓冲；任意二进制用 Buffer |

`decompressText` 保留内嵌 NUL，以 `cstring` 接管 XRT 解压缓冲，
不再经 `Buffer.text()` 无条件复制；追加末尾 NUL 时容量不足仍可能扩容。
非法 UTF-8 抛 `ValueError`。通过公开 C 导出调用时按普通字符串 ABI 转换。
| `compressFile(source, destination, quality=11, window=22, mode=0, overwrite=true, maxInput=268435456, maxOutput=536870912, onProgress=null)` | 压缩到目标文件 |
| `decompressFile(source, destination, overwrite=true, maxInput=268435456, maxOutput=536870912, maxRatio=1000, onProgress=null)` | 解压到目标文件 |
| `compressor(quality=11, window=22, mode=0, maxInput=268435456, maxOutput=536870912)` | `BrotliCompressor` |
| `decompressor(maxInput=268435456, maxOutput=536870912, maxRatio=1000)` | `BrotliDecompressor` |

压缩器支持 `update(Buffer|string)`、`drain() as Buffer`、`finish() as bool`，
只读属性为 `valid`、`inputBytes`、`outputBytes`。解压器支持 `update(Buffer)`、
`drain()`、`finish()`，另有 `finished`、`members`。
`*Native` 成员仅供包内包装代码使用，导入方使用上述公开方法和属性。
包级门禁以只编译负例验证访问权限，不执行伪造指针与长度的调用。

质量范围 0..11，窗口范围 10..24，模式为 `BROTLI_MODE_GENERIC=0`、
`BROTLI_MODE_TEXT=1`、`BROTLI_MODE_FONT=2`。模块同时提供相应的
`BROTLI_MIN_QUALITY`、`BROTLI_MAX_QUALITY`、`BROTLI_DEFAULT_QUALITY`、
`BROTLI_MIN_WINDOW`、`BROTLI_MAX_WINDOW`、`BROTLI_DEFAULT_WINDOW` 常量。
Q11 是默认配置，不是所有实时请求场景的推荐性能档位。

## 流与资源合同

- 输入在 `update` 调用期间借用，不保留调用方 Buffer。`drain` 转移本次积累的输出；
  没有可读数据时返回空 Buffer。它不是强制压缩刷新，后端可以继续缓存输入。
- `finish` 可重复调用，结束后不能再 update。解压 `finished` 表示目前位于完整流
  边界，仍需 `finish` 确认调用方已结束全部输入。
- 解压器可以串联处理多个独立 Brotli 流，`members` 记录数量。这是本接口的便利能力，
  **不能将拼接输出当作所有第三方 Brotli 解码器都接受的单个标准流**。
- 空明文可压缩并恢复；空压缩输入不是有效流。截断和已识别的非法格式抛 `IOError`。
  Brotli 没有内容校验和，某些翻位仍会产生合法数据，不能用于认证或完整性校验。
- xlang 的 maxInput/maxOutput 必须为正，按累计字节计算，drain 不重置限额。
  maxRatio 必须为 1..4294967295，在完整流边界检查累计解压比；硬输出限额始终生效。
  这些不是 CPU 截止时间或后端堆内存峰值限额。
- 裸 reader/writer 流可能在后续截断、取消或限额错误之前已经写出部分数据；需要
  失败原子性的文件操作应使用文件便捷接口，不能假设任意网络 writer 可以回滚。
- 对象需要调用方串行访问。独立对象可分别使用，不能在同一对象 update 时重入它。

## 文件与回调

```xlang
brotli.compressFile("input.txt", "output.br", quality = 4,
    onProgress = func(int inputBytes, int totalBytes, int outputBytes) as bool
        println(inputBytes, "/", totalBytes)
        return true
    end)
```

三个参数都是**字节数**，回调同步运行。false 表示取消并抛 `CancelledError`；
回调抛出的异常保留原类型和消息。内部与 XRT 的 uint64 进度帧做显式范围转换，
不能用 xvalue 的有符号精确读取器直接读取无符号计数。

文件接口在目标所在目录建立独占临时文件，读写和关闭成功后再替换目标。
取消、回调抛错或格式失败会清理临时文件并保留原目标；支持输入和输出路径相同。
这不是断电持久性承诺，也不是异步接口。路径使用 XRT 的 UTF-8 文件 API；
源或目标路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。

## C API

公共核心声明在 `include/xrt_brotli.h`，函数为 `xrtBrotli*`；薄绑定物理 ABI 在
`include/brotli_native.h`，函数为 `xbrotli_*`。C 编写算法通常直接用前者。
模块选择必须在第一次包含 XRT 单头文件之前完成；包清单的 `artifact.xrtModules`
给出完整依赖，独立 C 门禁也从清单读取。公共核心头自身选择 IO_BUFFER。

- `xrtBrotliOptionsInit` 初始化版本、默认质量和 `xrtresourcelimits`。
  Brotli 只使用其中的 maxInput、maxOutput、maxRatio（仅解压）；OptionsInit 将
  无关的单项、条目、节点及深度限额清零。显式设置这些字段或 `iFlags` 会报
  `ARGUMENT`，不会静默忽略。配置及嵌套限额的长度、版本必须匹配，限额
  保留字段必须为零；无效配置在读取 Reader 前拒绝。
  核心 C options 中这三个限额的零表示不设置该限额；薄绑定中零使用默认限额。
- `xrtBrotliCompress/Decompress` 失败返回 false，输出指针置 NULL，详情写入 error。
  成功转移 `xbuffer*`，由调用方 `xrtBufferDestroy`；error 结构无需单独释放。
- Create 分配失败可以返回 NULL；其他构造错误可能返回带 Error 的句柄。检查 Error
  后才能操作，所有非 NULL 句柄均须对应 Destroy。终止错误之后保留首次失败原因。
- `*TakeBuffer` 仅在 Finish 后可调用一次。`*DrainBuffer` 可以多次调用，返回拥有型
  Buffer；更换空缓冲失败时已有输出不丢失，清除 XRT 错误后可以重试 drain。
- Stream 接口借用 reader/writer，不关闭调用方对象。可定位 reader 的 total 是
  当前位置到末尾的剩余字节，无法查询时 total=0 且不设置 TOTAL_KNOWN。
  C 进度回调使用 `xrtprogressproc`，可区分 FINAL 和 TOTAL_KNOWN。
- 薄绑定的版本文本和 last-error 文本为借用返回值，不应 xrtFree；需要跨调用保存
  时由调用方复制。xlang 字符串返回桥会形成语言层所需的字符串。

## 构建与验证

从 demo6 工作目录执行：

```powershell
release/xl.exe run release/examples/extlibs/brotli_basic.xl
pwsh -File tools/run_extlib_c.ps1 -Package brotli -Source release/extlibs/brotli/examples/basic.c
pwsh -File tools/check_extlib_brotli.ps1
```

两份 basic 范例执行相同的 UTF-8、二进制、增量流与截断处理，黄金输出为
`release/examples/extlibs/brotli_basic.expected.txt`。C 范例需要链接包的源文件，
没有通过在一个翻译单元中拼接全部 vendor 源码来模拟独立可执行文件。
C 范例的压缩、解压、流式与截断检查不依赖 `assert` 副作用；
Windows/Linux 包门禁以 `-DNDEBUG` 构建并执行范例，对照同一黄金输出。

Windows 门禁包含独立 GCC、C/xlang JIT/AOT、23 个真实生成原型、回调/事务文件、
教学范例、同输出对照及逐分配点故障注入。Linux 使用
`bash tools/check_extlib_brotli.sh` 同次运行 C ASan/UBSan、中央和包内
XLang JIT、六条独立 EXE AOT、C 文件分配故障 AOT，以及教学范例和
C/XLang 基础范例逐字节黄金输出对照。
完整教学范例的文件段使用 XRT 独占系统临时目录，并在异常路径清理；
不再向固定的 `build/test/brotli-example` 写入文件。
`test/wire_fixture.xl` 已登记到 `coverage.tests`，用 Node.js 内置 Brotli 产生的固定字节流检验二进制互通和输出限额。
`test/error_dispatch.xl` 同样纳入覆盖，固定截断输入在一次性和流式接口均抛 `IOError`、完成后写入抛 `RuntimeError`，并验证失败后新对象仍可用。Windows 门禁在默认模式下同时运行该用例的 JIT 和独立 EXE；Linux 门禁运行 JIT。
测试数量与输入哈希以 `build/test/brotli_guard/results.json` 等实际记录为准。
`tools/check_extlib_brotli.ps1 -SkipAot` 仅供聚焦快测；默认模式执行
全部 AOT，聚焦模式不能作为整包发布签收。

2026-10-26 当前发布物：Windows 门禁报告 `passed`，73 项检查、
23 个生成原型、严格 C、分配故障、C/XLang JIT/AOT、文件事务与
双语范例通过，输入快照无变化。Linux C ASan/UBSan、语言 JIT/
六条独立 EXE AOT、C 文件分配故障 AOT 与范例输出对照通过。
旧 AOT 缺符号不再复现；本项签收当前 Windows/Linux x64 短时
发布候选门禁，不代表 32 位/ARM、TSan、长时压力和性能基准完成。

后端配置补丁仅在 `third_party/enc/memory.h`：强制使用上游 CLEANUP_ON_OOM 路径，
拒绝 EXIT_ON_OOM，防止库在内存不足时直接退出宿主。更新 vendor 时必须保留并重测。
尚未作 Windows ARM、其他操作系统、外部命令行解码器矩阵或生产负载性能认证。

2026-10-17：Linux 联合门禁运行通过；输入快照包含实际执行的 XLang 用例、
范例及发布运行器。完整带输出 AOT 仍受共享符号阻断，未签收发布级别。

2026-10-19：非零 `xrtresourcelimits.iReserved` 原被一次性、增量对象和 Reader
流接口静默接受；C 负例先红后绿，现由共用选项校验拒绝，错误流配置不消费
Reader。Linux C ASan/UBSan、分配故障、文件事务、XLang 与双语输出门禁通过；
Windows 严格 C/JIT 通过，完整 AOT 仍缺共享 `xl6NativeHandleTraceLeaf`。

2026-10-19：C 定向红测确认四个无关资源限额继承通用非零默认值，压缩/解压
对象还会接受显式无效字段或未知标志。现清零无关默认值，并在共用配置校验中
拒绝。Windows 严格原生编译、C 边界、分配故障、`-DNDEBUG` 范例和 XLang
JIT 通过；Linux C ASan/UBSan、文件事务、XLang 和双语黄金输出通过。
当前完整 Windows 构建仍缺主线 `xl6NativeHandleTraceLeaf`，未签收整包发布级。

2026-10-20：中央教学范例的文件段原写入固定目录，且只在成功时逐个
删除文件，存在覆盖与残留风险。现使用 `dir.temp`、`path.join` 和
`try/finally`，从 `demo6` 根与 `release` 直接运行均输出
`brotli example: PASS`；Linux C ASan/UBSan、分配故障、文件事务、
XLang 与双语联合门禁通过。压缩实现未改，完整 AOT 阻断仍保留。

2026-10-21：两份包内 C 测试及共享文件分配测试均含带副作用的
`assert`；发布配置定义 `NDEBUG` 时会删除实际测试操作。包内测试入口
现保持断言启用，Windows/Linux GCC 门禁以 `-DNDEBUG` 编译执行。
Windows `-SkipAot` 门禁已走完此前被 AOT 提前挡住的文件分配与 JIT
检查；Linux C ASan/UBSan、109 个分配故障点、文件事务、XLang 与
双语门禁通过。默认 AOT 定向构建仍缺 `xl6NativeHandleTraceLeaf`，
库实现/API 未改，不签收整包发布级。

2026-10-24：外部固定流夹具改用当前公开的 `Buffer.fromHex`；包级
Windows 门禁将文件分配故障夹具路径直接传给 JIT/AOT，避免空源码参数。
当前发布版完整门禁 70 项通过、输入快照无变化，覆盖中央语言用例
JIT/AOT、23 个生成原型、C/XL 范例、109 个分配故障点和文件事务。
Linux C ASan/UBSan、泄漏检查、固定流与双语门禁也通过。旧 AOT
缺符号是历史发布物状态；Linux XLang AOT、Windows ARM、长期压力
和性能基准仍未在本轮验收。

2026-10-25：`decompressText` 改为校验 UTF-8 后将解压缓冲交给
`cstring`，补空文本、内嵌 NUL、非法 UTF-8、原址所有权和分配失败
回归。Windows 严格 C、分配注入，以及发布目录的 XLang JIT/EXE
通过；完整根目录门禁仍受共享 VFS `include/xruntime_decl.h` 查找问题
阻断。Linux C ASan/UBSan 与文件事务通过；旧 Linux `release/xl`
尚不识别 `cstring`，语言阶段需重建后复验。
