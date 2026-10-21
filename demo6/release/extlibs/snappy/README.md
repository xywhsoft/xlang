# snappy

标准 Snappy framed stream 压缩库，支持一次性、增量和文件流接口。输出的 `.sz` 可与遵循 [Google Snappy framing format](https://github.com/google/snappy/blob/main/framing_format.txt) 的工具互通；不接受旧的私有多块格式。

```xlang
import snappy

Buffer packed = snappy.compress("hello snappy")
assert snappy.decompressText(packed) == "hello snappy"
```

C 侧直接调用 `xrtSnappyCompress` / `xrtSnappyDecompress`，示例见 `examples/basic.c`；XLang 对照见 `demo6/release/examples/extlibs/snappy_basic.xl`。流式接口 `compressor().update/drain/finish` 与 `decompressor().update/drain/finish` 使用同一格式。`members` 统计 stream identifier 的数量，`finished` 仅在 `finish()` 成功后为真。
流对象的 `*Native` 方法只供包内安全包装调用；导入方使用公开的 `update`、`drain`、`finish` 和属性。包级门禁用只编译负例检查访问权限，不执行伪造指针与长度的调用。
压缩器单次 `update` 接收大输入时只缓存当前 64 KiB 原文块；解压器分段解析输入，可跳过 chunk 的主体边读边丢弃。尚未 `drain` 的压缩或解压结果仍由输出缓冲持有，其大小取决于数据和调用方的排空频率。
所有 C 编解码入口先检查 `xrtsnappyoptions` 与内嵌 `xrtresourcelimits` 的版本、完整长度和保留字段，再读取限额。无效选项返回 `XRT_SNAPPY_ERROR_ARGUMENT`；流对象可通过 `...Error()` 读取错误，文件流不会消费输入或产生输出。
资源限额的 `iMaxEntries` 约束解压时的 stream identifier 总数；`iMaxNodes`、`iMaxDepth` 和未知 `iFlags` 不适用，默认清零且显式非零会报参数错误。输入/输出字节、单块原文大小和解压倍率限额保持有效；`iMaxEntries = 0` 表示不限制串联流数量。
文件接口的源或目标路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。

framing 没有结束标记，因此末尾 chunk 不完整会报错，但在完整 chunk 边界提前结束无法检测。校验采用 masked CRC32C。数据块原文上限 64 KiB；未知可跳过 chunk 会跳过，未知不可跳过 chunk 会拒绝。

测试：`test/test_snappy.c` 覆盖固定格式样本、增量输入、资源上限、校验、截断、跳过 chunk 与串联流。`test/file_ops.xl` 覆盖 UTF-8 文件往返、进度回调、覆盖拒绝、取消清理与限额失败时的既有目标保护；中央 XLang 用例在 `demo6/test/cases/extlib_snappy.xl` 和 `extlib_snappy_progress.xl`。Linux 包级门禁同次运行 C ASan/UBSan、上述语言用例与 C/XLang 基础范例逐字节对照。
Windows 可在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/snappy/test/check.ps1 -SkipAot`，同次检查严格 GCC 独立链接运行的 C 边界、TCC C 边界、私有成员负例、XLang 主用例/进度/文件操作，以及 GCC C、TCC C、XLang 三路固定输出。脚本使用每次独立的临时构建目录并清理本次产物；去掉 `-SkipAot` 还会尝试完整 AOT，当前仍受共享缺符号阻断。

包清单现公开 `include/` 并声明六个直接 XRT/XRuntime 模块。
`tools/run_extlib_c.ps1 -Package snappy -Source release/extlibs/snappy/examples/basic.c`
可直接运行 C 范例；按清单模块独立编译运行时、三份包 C 源和范例后，
Windows 严格 GCC 构建及运行通过。裁剪运行时对象为 292,981 字节，
不是最终 `xl build` 应用体积。包内未使用的 varint 长度函数已移除。

当前 `xl.exe` 的 Buffer 单字节异或仍有共享 XLIR 诊断，最小复现见
`dev/extlib_blockers/argon2_byteat_sub.md`；本包用非法 chunk 测损坏路径，
不把它视为主线修复。完整 AOT 仍缺 `xl6ValueDataSnapshotV1`，未签收。

2026-10-17：中央进度用例原写入不存在的 `demo6/tmp` 而在回调前失败，
现使用独立临时目录并在 `finally` 清理；Windows/Linux JIT 均通过。Linux
联合门禁还通过中央主用例、教学范例、文件用例及双语基础输出。单字节
算术的共享 XLIR 最小复现仍失败，完整 AOT 阻断不变。

可选的独立实现互通测试（在 `demo6` 目录执行）：先将 `python-snappy==0.7.3` 安装到测试环境，或运行 `python -m pip install --target build/test/snappy/python_oracle python-snappy==0.7.3`；再执行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/snappy/test/check_interop.ps1`。它用标准 framed stream 双向传递二进制、重复及跨 64 KiB 分块样本；Python 只用于测试，不是扩展库的运行依赖。

2026-10-18：六种选项 ABI 负例先红后绿，覆盖流对象、一次性和文件流入口；Windows 严格 GCC/C/XLang 与 Linux C ASan/UBSan、XLang/双语对照通过。随后包外 `updateNative(point,size)` 两条只编译负例确认可达，再将 13 个原生流方法私有化；Linux 包级联合门禁通过。完整 AOT 与共享 XLIR 问题仍未签收。

2026-10-19：C 流式压缩/解压的进度总量原使用 Reader 物理总长，
从非零位置读取时把前缀误算进本次输入。两个方向的红绿 C 回归
现按 `Size - Tell` 核对本次总量，官方 framing 固定样本同时验证
实际解码；没有长度/位置能力的 Reader 仍正常处理，但不设置
`TOTAL_KNOWN`。Windows TCC C/严格 GCC 与 Linux ASan/UBSan、
XLang/双语范例门禁通过；共享 XLIR 和完整 AOT 阻断不变。

2026-10-19：C 定向回归确认默认资源配置继承了无用的节点/深度限额，
且解压器对 `iMaxEntries` 未执行检查。现清零无关默认值、拒绝未知标志
及显式无效字段，并在 stream identifier 上执行成员数上限；一次性和逐段
解压均覆盖首个成员可用、第二个成员越界。Windows C/XLang 范例与 Linux
ASan/UBSan 联合门禁通过；共享 XLIR/AOT 阻断仍未解除。

2026-10-19：4 MiB 单次写入的红测确认待压缩缓冲原随调用输入线性增长。
现在按 64 KiB 分块接收并立即压缩，独立 GCC 测试借助 XRT 内存调试
断言该输入的压缩阶段峰值分配低于 2 MiB，再完整解压校验原文。
Windows 包级门禁和 Linux ASan/UBSan、XLang/双语对照均通过；此约束
不包括未排空的压缩输出，也不改变完整 AOT 的共享阻断状态。

2026-10-19：解压器原会在单次 `update` 中复制完整输入；4 MiB 的多个
可跳过 chunk 红测确认输入缓冲随调用大小增长。现在每次接收最多 64 KiB
后立即消费；单个 4 MiB 可跳过 chunk 也边读边丢弃主体。两种输入的
XRT 峰值内存断言均低于 2 MiB；逐字节分段与截断负例通过。Windows
严格 GCC/TCC、XLang 与 Linux ASan/UBSan/双语门禁通过；完整 AOT
仍受主线缺符号阻断。

2026-10-20：C 基础范例原须额外传入包头文件和源码，直接执行
`release/xl.exe run release/extlibs/snappy/examples/basic.c` 报找不到
`xrt_snappy.h`。范例现仅在 TCC 单文件路径包含包内两个编解码源文件；
正式 GCC 构建和包清单继续分离编译。Windows/Linux 包级门禁新增
直接运行的 C 路径，与 GCC、包运行器及 XLang 的固定输出对照均通过，
Linux 仍覆盖 C ASan/UBSan。压缩算法未改，完整 AOT/共享 XLIR 阻断不变。

2026-10-20：新增 C 逐点分配故障测试，要求一次性压缩/解压在 XRT
分配失败时返回内存错误和空输出，释放后内存快照回到基线，并可直接
重试。包内 `test/allocations.xl` 进一步检查公开语言入口必须抛
`MemoryError` 而不是 `CompressionError`，已登记到 `coverage.tests`。
Windows 严格 GCC 和 Linux ASan/UBSan 联合门禁均通过；XRT 注入器
不控制上游 Snappy 的内部 `malloc`，完整 AOT/XLIR 阻断仍未解除。

2026-10-21：C 边界测试原把压缩、解压与资源操作放在 `assert`
表达式中，定义 `NDEBUG` 会把测试操作一起删掉。测试单元现保持断言启用，
Windows/Linux 包门禁用 `-DNDEBUG` 编译运行；预处理确认压缩调用仍在。
Windows 严格 C、XLang 与双语对照，以及 Linux C ASan/UBSan 门禁通过。
库实现与公开 API 未改，完整 AOT/XLIR 共享阻断仍未解除，不签收整包发布级。
