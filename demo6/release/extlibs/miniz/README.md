# miniz

2026-10-28 原生构造入口收口：工厂原已拒绝负级别，但导出类的直接构造
和 C 桥接入口仍把负数当成默认级别。现所有压缩入口统一遵守 0..10，
内部解压需要默认值时显式传 `XDEFLATE_LEVEL_DEFAULT`，不保留负数别名。
C 的五个非法级别 × 三种内存/构造入口及文件入口共 20 组负例、0..10
有效级别往返，以及 XLang 显式类型变量和直接构造器的正反例均已验证。
非法文件调用不发布目标，内存负例释放后回到活动分配基线。

桥接头 `include/miniz_native.h` 使用标准 bool/size_t 与公开不透明 xvalue
前向声明，不强制启用 VALUE，也不依赖它与核心头的包含顺序。严格 C
分别检查两种顺序；测试所用目录能力只加到验证运行时，不扩充产品依赖。
新增 `test/native_options.xl` 登记到 coverage；本轮默认 Windows/Linux
门禁通过严格 GCC、Linux ASan/UBSan、Python zlib、新旧双语 JIT/AOT。
中间一次宿主虚拟内存不足及 WSL 服务中断另行留档，保持原参数串行复验
后通过；详见[资源事件说明](../../../docs/普通扩展库验证资源事件_2026-10-28.md)，
不据此关闭其他包的未知风险。

结构化底层错误会直接分发：损坏/截断通常为 `IOError`，输出限额为
`RangeError`，分配失败为 `MemoryError`；不能按旧注释一概捕获
`CompressionError`。C 调用方对应检查错误结构及当前 XRT 错误槽。

2026-10-28 流式 IO 首因收口：C 的压缩/解压 Reader 读取失败和两个
drain 的 Writer 失败原统一报 IO，并丢失后端代码和消息。现复用既有
XRT 错误映射，保留 ARGUMENT、MEMORY、LIMIT、STATE、CANCELLED、IO
等可识别首因，以及 `iBackendCode` 和原始消息；当前 XRT 错误槽不被改写。
无详细错误的失败仍按 IO 处理。只修改失败分支，没有增加正常路径的
包装层或数据复制，也没有替换 XRT 编解码底座。

`test/test_io_errors.c` 的四个入口 × 七类错误共 28 组先红后绿，还覆盖
两种真实 Buffer Writer 扩容 OOM、错误位置计数、失败后状态保持、借用
Reader/Writer 的关闭归属和活动分配恢复。C 层的错误结构现在与原始
XRT 首因一致；这不是新增 XLang 自定义 Reader/Writer API。

本轮 Windows 默认门禁、Linux 同输入原生文件系统默认门禁均退出 0，
包含严格 GCC、Linux ASan/UBSan、Python zlib 互通、28 组对照/真实 OOM、
语言文件/进度/分配/文本转移、新旧双语 JIT/AOT。Windows 默认模式补齐
此前只在 Linux 完整执行的中央、进度、文件、分配及完整范例 AOT；
测试使用实际发布物和输入指纹，失败保留命令/状态/输出，成功清理独占产物。
中央文件用例改用独占临时目录和 finally，不再复用固定 miniz-baseline。
未修改主线或 XRT 快照；这不关闭其他包的旧风险，也不证明所有文件系统
或长期压力已验收。当前基线见[发布矩阵](../../../docs/普通扩展库发布验收矩阵_2026-10-28.md)。

## 增量流双语范例

[`examples/streaming.xl`](examples/streaming.xl) 与
[`examples/streaming_c.c`](examples/streaming_c.c) 共同展示 update、及时
drain、finish 后尾部收集、分块解压以及输入/输出计数。C 的 Writer 由
调用方创建和关闭；XLang 范例用两个临时 Buffer 构造传输碎片，真实网络
接收块可以直接 update。两份均验证结果，无文件、服务或数据依赖。

在 `demo6` 目录运行（Linux 把 xl.exe 改为 xl）：

```text
./release/xl.exe run release/extlibs/miniz/examples/streaming.xl
./release/xl.exe run release/extlibs/miniz/examples/streaming_c.c
```

两份固定输出：

```text
hello stream
12 / 12
```

2026-10-26 当前发布物：Windows 默认 `test/check.ps1` 退出 0，
严格 C、Python 标准库 zlib 双向互通、分配故障、JIT/AOT 与 C/XLang
范例输出一致。Linux `test/check.sh` 退出 0：C ASan/UBSan、
Python zlib 互通、JIT、七条独立 EXE AOT 以及文件/基础/完整范例
输出对照通过。旧 `cstring`、根目录 VFS 和 AOT 快照缺符号阻断
在当前发布物不再复现。未据此宣称 32 位、TSan 或长时压力已验收。

2026-10-25 历史记录：文本解压改为返回拥有型 `cstring`，经 UTF-8 验证后
直接接管解压缓冲，不再由 `Buffer.text()` 无条件复制。明确字节长度
保留文本中的内嵌 NUL；C 回归验证指针身份、空结果、坏 UTF-8、
逐点分配失败和释放恢复，XLang 专项及中央用例 JIT/AOT 均通过。
当时 Windows 包门禁的严格 C、zlib 互通与分配阶段通过，随后受共享
VFS `include/xruntime_decl.h` 查找故障中断；当时未签收完整门禁。
Linux C ASan/UBSan 与新分配回归通过；旧 `release/xl` 不识别
`cstring`，当时尚未签收语言阶段。

2026-10-24 当时 Windows 发布版默认 `test/check.ps1` 完整通过：
严格 GCC、C/XLang 边界与分配故障、双语范例和独立 EXE AOT。
门禁现要求 Python 标准库 `zlib` 解开本包 GCC 现场生成的流；
反方向的 Python 固定流仍由 C 边界测试读取。下文“当前缺符号”
是旧发布物的历史记录；当时未重建 Linux 发布版，未据 Windows
结果签收 Linux AOT。

本包提供标准 [RFC 1950 zlib 流](https://www.rfc-editor.org/rfc/rfc1950.html) 的一次性、增量和文件接口。编解码复用 XRT 的 deflate/inflate；不再输出或读取旧的私有多块容器。

```xlang
import miniz

Buffer packed = miniz.compress("hello miniz")
assert miniz.decompressText(packed) == "hello miniz"
```

公开 C 入口 `xrtMinizCompress` / `xrtMinizDecompress` 见 `examples/basic.c`；相同输出的 XLang 范例见 `demo6/release/examples/extlibs/miniz_basic.xl`。流式接口按 `update`、可选 `drain`、`finish`、最后一次 `drain` 使用。`finished` 在完整尾部解出后立即为真；仍应调用 `finish()` 明确结束输入。单条 zlib 流结束后不接受额外字节或第二条流。文件接口可将输出保存为 `.zz`。
流对象的 `*Native` 成员只供包内包装代码使用，导入方使用公开的 `update`、`drain`、`finish` 和属性。门禁以只编译负例验证访问权限，不执行伪造指针与长度的调用。
所有 C 编解码入口在读取限额前校验 `xrtminizoptions` 和内嵌 `xrtresourcelimits` 的完整长度、版本、保留字段及 0..10 级别。无效选项报告 `XRT_MINIZ_ERROR_ARGUMENT`；文件流不消费输入或写出结果，流对象的错误可通过 `...Error()` 查询。
此包仅处理单条 zlib 流：输入/输出字节、解压后总量及解压倍率限额有效；`iMaxEntries`、`iMaxNodes`、`iMaxDepth` 和未知 `iFlags` 不适用，默认清零，显式非零报参数错误。
文件接口的源或目标路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。

配置中的输入、输出和解压倍率上限仍有效。编码级别为 0..10，默认 6。损坏的 Adler-32、截断、尾部数据和私有旧格式均被拒绝。压缩流不提供加密认证，不应用于验证发送方身份。
`decompress` 返回二进制安全的 `Buffer`；`decompressText` 返回 `cstring`，
仅用于 UTF-8 文本，解压成功但内容含非法 UTF-8 时抛 `ValueError`。
内嵌 NUL 按真实字节长度保留；任意非 UTF-8 二进制载荷仍应使用 `Buffer`。

测试 `test/test_miniz.c` 覆盖 Python zlib 固定样本、分段输入、空流、损坏、截断、串联拒绝、输出限额，以及 XRT Reader/Writer 的二进制流往返和进度回调。`test/file_ops.xl` 覆盖文件往返、覆盖拒绝、取消清理和解压限额下的目标保护。Linux 使用 `bash release/extlibs/miniz/test/check.sh`，运行 ASan/UBSan、Python zlib 反向解码、XLang JIT 和七条独立 EXE AOT；中央 XLang 用例见 `demo6/test/cases/extlib_miniz.xl`。
Windows 在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/miniz/test/check.ps1`：同次验证严格 GCC 独立构建、Python zlib 互通、TCC C、私有成员负例、中央/文件 XLang 用例、GCC C/TCC C/XLang 范例逐字输出和完整 AOT。脚本使用每次独立的临时构建目录；`-SkipAot` 仅供聚焦快测，不作为发布签收。

包清单公开 `include/` 并声明所需的 XRT/XRuntime 模块。Windows 上可直接用
`tools/run_extlib_c.ps1 -Package miniz -Source release/extlibs/miniz/examples/basic.c`
运行 C 范例；按清单模块独立编译运行时、包源码和 C 范例也已通过，运行时
对象为 342,087 字节。该数字不是最终 `xl build` 产物大小。
`examples/basic.c` 的 TCC 单文件路径还可直接用
`release/xl.exe run release/extlibs/miniz/examples/basic.c` 运行，或单独
`xl build`；正式包仍按清单分离编译原生源码。

旧发布物的 `Buffer` 单字节结果参与异或曾触发 XLIR 算术转换错误，
最小复现见 `dev/extlib_blockers/argon2_byteat_sub.md`；完整 AOT 曾缺
`xl6ValueDataSnapshotV1`。这些记录描述旧版本，不是当前门禁结果。

2026-10-22 复验：中央 `extlib_miniz.xl` 已改用直接写入损坏帧头，
`xl run` 成功；Windows `check.ps1 -SkipAot` 的 C 边界、语言用例与
双语范例对照全部通过。共享的字节/整数运算问题仍有其他最小复现，
不能因本包测试绕开表达式就视作修复。默认门禁在完整 XLang AOT
构建时报缺少 `xl6ValueDataSnapshotV1`，整包暂不签收发布级。

2026-10-18：六类畸形选项负例先红后绿，覆盖流对象、一次性及文件流入口。Windows 严格 GCC/C/XLang、Linux ASan/UBSan/Python zlib 互通通过。随后包外 `updateNative(point,size)` 两条只编译负例确认可达，再将 12 个原生流方法私有化；Linux 包门禁补中央 XLang 主/进度用例与 C/XLang 最小范例逐字节对照，联合通过。完整 AOT 与共享 XLIR 阻断未变。

2026-10-19：C 流式压缩/解压的进度总量原取 Reader 物理总长，
从非零位置开始时把前缀误算为本次输入。两个方向的回归先红后绿，
现用 `Size - Tell` 报告本次剩余字节；不能同时取得两者时仍正常处理，
但不置 `TOTAL_KNOWN`，总量为 0。Windows TCC C/严格 GCC 与 Linux
ASan/UBSan、Python zlib、XLang/双语范例门禁通过；完整 AOT 与共享
XLIR 阻断仍未解决。

同日补 XLang 文本边界：`00ff41` 字节经标准 zlib 流压缩后，二进制
`decompress` 必须原样往返，而 `decompressText` 必须拒绝非法 UTF-8。
Windows C 定向用例与 Linux C ASan/UBSan、Python zlib、XLang/双语
范例联合门禁通过。包实现无需改动；共享 XLIR 和完整 AOT 阻断不变。

2026-10-19：C 定向红测确认通用资源初始化带入条目、节点和深度
三个无效非零限额。公共选项入口现清零这些默认值，并拒绝显式设置
的无效字段及未知标志；压缩、解压、一次性及文件流入口统一生效。
Windows C/XLang 范例、Linux ASan/UBSan、Python zlib 互通通过；
完整 AOT 与共享 XLIR 阻断未变。

2026-10-19：补 Windows 包级联合门禁，并以 C 负例证实非法级别原在
不同入口被静默接受或映射为非参数错误。选项复制入口现统一检查
`iLevel` 属于 0..10；流对象、一次性和文件流的压缩/解压均拒绝 -1/11，
全部 11 个有效级别可往返。Windows 严格 GCC/TCC C、XLang 和双语
范例，以及 Linux ASan/UBSan、Python zlib 互通门禁通过；完整 AOT
与共享 XLIR 阻断仍未解除。

2026-10-20：新增 `test/test_allocations.c`，逐点注入一次性压缩和解压的
XRT 分配失败，检查内存错误分类、空输出、释放平衡与清除故障后的恢复。
Windows 严格 GCC 联合门禁及 Linux ASan/UBSan、Python zlib、XLang
联合门禁通过。此扫描只控制 XRT 分配点，不替代持续模糊测试或完整 AOT。
包内 `test/allocations.xl` 同时扫描两条公开语言入口：凡触发故障注入均须
抛 `MemoryError`，不能误报为 `CompressionError`；故障解除后再次往返成功。
该用例已加入包清单 `coverage.tests` 和双平台联合门禁。

2026-10-21：C 边界测试原将压缩、解压和资源操作置于 `assert`
表达式内，定义 `NDEBUG` 后操作会消失。测试单元现保持断言启用，
Windows/Linux 包门禁均以 `-DNDEBUG` 编译运行；预处理确认独立
Python zlib 样本的解压调用仍在。Windows 严格 C/XLang/双语门禁和
Linux C ASan/UBSan/Python zlib 互通门禁通过。库实现与公开 API 未改，
完整 AOT/XLIR 共享阻断仍在，不签收整包发布级。

2026-10-24：使用当前发布版 `xl.exe` 不带 `-SkipAot` 运行 Windows
包级门禁，严格 GCC、TCC C、XLang 主/进度/文件/分配用例、单文件
C JIT/AOT、基础范例 C/XLang 对照及 XLang 基础范例 AOT 全部通过。
Linux 门禁通过 C ASan/UBSan、Python zlib 互通和 XLang 双语对照。
前述 AOT 缺符号是旧发布版的历史状态；本次未覆盖 Linux AOT、
持续模糊测试或长期压力，不能把包级门禁扩大为这些范围的认证。
