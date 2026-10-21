# lzma

本包使用 7-Zip 26.03 的公共领域 C 实现，输出和读取标准 XZ（`.xz`）流，
提供 XLang/C 一次性、分段输入和文件接口。只支持 XZ，不读取旧 `.lzma` 流。
编码器使用 LZMA2 和 CRC64；解码器校验容器结构与校验和，并支持串联流。
独立互通测试还验证解码器读取 CRC32、SHA-256、无校验、Delta+LZMA2
和 x86 BCJ+LZMA2 的标准 `.xz` 流；无校验流不提供损坏检测。
流间及末尾允许 4 字节倍数的零填充；不完整填充和尾随垃圾会报错。
CRC64 仅用于检测意外损坏，不是密码学认证。

```xlang
import lzma

Buffer packed = lzma.compress("hello lzma")
assert lzma.decompressText(packed) == "hello lzma"
```

C 调用方包含 `include/xrt_lzma.h`，使用 `xrtLzmaCompress` / `xrtLzmaDecompress`；
成功返回的 `xbuffer*` 由调用方以 `xrtBufferDestroy` 释放。流式接口借用 XRT
reader/writer，文件接口由 XLang 的 native 绑定提供。默认输入、输出和解压比限额
分别是 256 MiB、512 MiB 和 1000；压缩不是加密或内容认证。
解码器还将 XRT 默认的 `iMaxItemBytes=256 MiB` 用作 7-Zip SDK 的单次
分配上限：即使压缩输入很小，XZ 头声明的 1 GiB LZMA2 字典也会在分配前
被拒绝为资源限额错误。这不是进程总内存配额；C 调用方可通过
`xrtlzmaoptions.tLimits.iMaxItemBytes` 调整。
OptionsInit 将 XZ 不使用的条目、节点、深度字段清零；C 调用方显式设置
这些字段或 `iFlags` 会得到 `XRT_LZMA_ERROR_ARGUMENT`，不会静默忽略。
C 入口统一要求 `xrtlzmaoptions` 与内嵌 `xrtresourcelimits` 的当前完整长度、
版本和零保留字段；无效选项报告 `XRT_LZMA_ERROR_ARGUMENT`，不会开始读写 XZ 流。
源或目标文件路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。
`LzmaCompressor.update()` 暂存输入，`finish()` 才编码整条流，因此其
`drain()` 在 `finish()` 前返回空 Buffer；大文件用 `compressFile()` / C
`xrtLzmaCompressStream()`，它们通过 XRT reader/writer 直接流动，不缓存整个输入。
流对象的 `*Native` 成员仅供包内包装代码调用，导入方使用公开的
`update`、`drain`、`finish` 和属性。包级门禁以只编译负例验证访问权限，
不执行伪造指针与长度的调用。
解压器允许提前 `drain()`，但调用方只有在 `finish()` 成功后才能信任输出。
损坏或截断经 XRT 错误映射为 `IOError`，资源限额映射为 `RangeError`。
流式 C 入口在创建编解码器的首次分配失败时返回 `false`，并在
`xrtlzmaerror` 中报告 `XRT_LZMA_ERROR_MEMORY`，不会留下 `OK` 状态。
原生对象的成功操作会清除本线程之前遗留的 XRT 错误；失败仍保留本次错误。
原生对象方法和状态查询拒绝空句柄，C 调用方收到 `XERR_ARGUMENT`，
不会把调用参数错误误报为压缩流 I/O 故障。

在 `demo6` 目录执行聚焦验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/lzma/test/check.ps1 -SkipAot
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package lzma -Source release/extlibs/lzma/test/test_lzma.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package lzma -Source release/extlibs/lzma/examples/basic_c.c
cd release
.\xl.exe run extlibs/lzma/examples/basic.xl
.\xl.exe run extlibs/lzma/test/boundaries.xl
.\xl.exe run extlibs/lzma/test/file_ops.xl
.\xl.exe build extlibs/lzma/test/aot_core.xl -o ../build/test/lzma_aot_core.exe
..\build\test\lzma_aot_core.exe
.\xl.exe run ..\test\cases\extlib_lzma_progress.xl
cd ..
wsl bash release/extlibs/lzma/test/check.sh
```

`test/test_lzma.c` 覆盖二进制、资源限额、截断、尾部 CRC 篡改、非法属性及
XZ 填充/尾随数据；篡改后的单次解压不返回部分结果，恢复原字节后仍可解码。
`test/boundaries.xl` 同样验证 CRC 损坏映射为 `IOError`，且后续有效输入可用。
`test/file_ops.xl` 在独立系统临时目录验证文件往返、进度回调、拒绝覆盖、
取消后不提交输出，以及解压超限时保留原目标文件；成功和失败都会清理目录。
C 边界用例另验证 XRT Reader/Writer 的压缩与解压成功往返和进度计数。
`test/interop.py` 用 Python 标准库 `lzma.FORMAT_XZ` 双向验证格式。
CRC32、CRC64 和 SHA-256 的块校验字段损坏由独立解码器和本包共同拒绝。
互通门禁还读取 CRC32 首流与 SHA-256 次流的串联输入；次流校验损坏时，
本包拒绝整段输入且不返回首流明文。Python 标准库分别验证原始次流和
损坏次流：其便捷 `lzma.decompress` 对损坏的尾随流可能仅返回首流，
不能据此判定完整串联输入有效。流间零填充由包内 C 边界单独验证。
其中非默认 x86 BCJ 滤镜曾在 7-Zip 的未对齐读路径触发 UBSan；
本包在 `third_party/xz/CpuArch.h` 使用定义良好的逐字节路径，
完整 sanitizer 门禁已覆盖该样本。
Linux 包级门禁会将 `test/interop.c` 编译为命令行工具并自动运行该互通测试。
C/XLang 最小范例同输出。Linux 包级门禁以严格 GCC、ASan/UBSan 验证 C 实现，
并运行 XLang 边界、文件用例及双语范例逐字节输出对照。
中央进度用例在独立系统临时目录内验证压缩、解压、取消与 NUL 路径，
由 Linux 包级门禁一并运行；成功或失败都会清理临时目录。
完整 `test/cases/extlib_lzma.xl` 目前被已记录的 Buffer 字节索引
XLIR 问题阻断；本任务不修改编译器，改用包内 `test/boundaries.xl` 覆盖主要边界。
独立 `test/aot_core.xl` 的裁剪 AOT 已构建并运行（1,000,960 字节）；
`xl build extlibs/lzma/test/boundaries.xl` 当前在主线缺少
`xl6ValueDataSnapshotV1` 处链接失败，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。
独立 C 范例命名为 `basic_c.c`，不会被 `basic.xl` 的 AOT 编译自动当作伴随源。
完整 AOT 的共享符号阻断仍需在主线修复；`test/aot_core.xl` 曾在旧构建中独立编译运行，当前发布物状态见下方复核记录。

Windows 包级脚本 `test/check.ps1` 将严格 GCC、C 边界、XLang 边界/文件/进度、原生方法访问负例、C/XLang 同输出及核心 AOT 合为一次验收；`-SkipAot` 只跳过当前受主线阻断的 AOT，不能作为发布签收。脚本使用独立临时构建目录，结束后核对绝对路径并清理。

2026-10-16 复核最终格式仅为标准 `.xz`，未发现私有容器回退。Linux 包级脚本现于同次运行中执行 XLang 边界并逐字节对照 C/XLang 范例输出；严格 GCC、ASan/UBSan、Python 标准库双向互通均通过。完整 AOT 的共享主线阻断不变。

2026-10-18：六类选项 ABI 负例覆盖流对象、一次性及文件流入口；未知的内嵌限额版本、截断长度和非零保留字段先红后绿。Windows 严格 GCC/C/XLang 与 Linux ASan/UBSan、双语对照、Python `.xz` 互通通过；完整 AOT 和旧 XLIR 阻断未变。
同日追加：包外原可调用 `updateNative(point,size)`；两条只编译负例先确认可达，13 个原生流成员私有化。Linux C ASan/UBSan、XLang 边界/文件、双语对照和标准 `.xz` 双向互通联合通过；完整 AOT 与旧 XLIR 阻断不变。

2026-10-18 当前发布物复核：新增 Windows 包级脚本的 `-SkipAot` 路径全部通过，Linux ASan/UBSan 与 Python 标准 `.xz` 互通门禁复跑通过。默认 Windows 门禁在核心 AOT 构建时报 `xl6NativeHandleTraceLeaf` 缺符号；不依赖扩展包的 `Buffer` 最小复现同错。上文核心 AOT 成功是旧构建记录，不能证明当前发布物可构建；待主线重建后需复跑默认门禁及完整边界 AOT。

2026-10-19 复核用户定稿：本包现有编码/解码只走标准 XZ 容器，没有私有分块格式兼容分支。Windows PowerShell 5.1 原将预期的包外私有成员负例诊断当作脚本终止错误；门禁仅在捕获该诊断期间临时放宽错误动作，随后恢复。PowerShell 5.1 和 7 的 `-SkipAot` 门禁均通过；Linux ASan/UBSan 与 Python `FORMAT_XZ` 双向互通通过。完整 AOT 的主线阻断仍在，不签收发布级。

同日补大字典资源边界：C 用例在正常小流的 XZ 块头中声明 1 GiB 字典，
重算块头 CRC 后验证默认限额返回 `LIMIT`、不交出部分结果；恢复原头后
再次解码成功。Windows 包级严格 GCC/C/XLang、Linux C ASan/UBSan
及 Python 标准 `.xz` 双向互通门禁通过。编解码实现无需修改。

2026-10-19：C 选项定向红测确认通用初始化带入三个无效非零限额；代码审计
发现对象、一次性和流式入口共用的校验未检查显式无效字段及未知标志。
现清零不适用默认值，并
在共用选项入口拒绝；有效的解码单次分配限额保持不变。Windows 严格 C、
XLang 边界和双语范例，以及 Linux C ASan/UBSan、Python 标准 `.xz` 互通
均通过。当前完整 Windows AOT 仍缺 `xl6NativeHandleTraceLeaf`，未签收发布级。

2026-10-20：逐点 XRT 分配故障扫描发现流式 Writer 失败时将内存错误固定
误报为 I/O 错误。压缩输入/输出、解压输入/输出及增量 drain 现保留 XRT 的
内存和取消类别，其他流错误仍为 I/O。新增 C 用例检查失败后无部分结果、
内存快照平衡与恢复；新增公开 XLang 用例检查 `MemoryError` 分类并登记
包级 coverage。Windows 独立 GCC/XRT 内存调试宿主、Linux ASan/UBSan、
标准 `.xz` 双向互通和 C/XLang 对照均通过。故障扫描覆盖 XRT 分配点，
不代表进程内所有第三方分配点；完整 XLang AOT 的主线阻断不变。

同日补齐 C 单文件范例：`xl run release/extlibs/lzma/examples/basic_c.c`
以及对该文件直接 `xl build` 后运行均可得到与 XLang 范例一致的输出。
范例仅在 TCC 单文件路径包含 XZ 后端源码；正式包仍按 manifest 分离编译。
Windows 包级门禁固定两条直接路径，Linux 门禁固定直接运行对照。
