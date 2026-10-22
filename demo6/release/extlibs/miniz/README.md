# miniz

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
`decompress` 返回二进制安全的 `Buffer`；`decompressText` 仅用于 UTF-8
明文，解压成功但内容含非法 UTF-8 时抛 `ValueError`。含 NUL 或任意
二进制字节的载荷应保留为 `Buffer`，不要走文本便利入口。

测试 `test/test_miniz.c` 覆盖 Python zlib 固定样本、分段输入、空流、损坏、截断、串联拒绝、输出限额，以及 XRT Reader/Writer 的二进制流往返和进度回调。`test/file_ops.xl` 覆盖文件往返、覆盖拒绝、取消清理和解压限额下的目标保护。Linux 使用 `bash release/extlibs/miniz/test/check.sh`，会运行 ASan/UBSan、Python zlib 反向解码和包内 XLang 文件测试；中央 XLang 用例见 `demo6/test/cases/extlib_miniz.xl`。
Windows 在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/miniz/test/check.ps1 -SkipAot`：同次验证严格 GCC 独立构建、TCC C、私有成员负例、中央/文件 XLang 用例及 GCC C/TCC C/XLang 范例逐字输出。脚本使用每次独立的临时构建目录；去掉 `-SkipAot` 会额外尝试完整 AOT，当前仍受共享缺符号阻断。

包清单公开 `include/` 并声明所需的 XRT/XRuntime 模块。Windows 上可直接用
`tools/run_extlib_c.ps1 -Package miniz -Source release/extlibs/miniz/examples/basic.c`
运行 C 范例；按清单模块独立编译运行时、包源码和 C 范例也已通过，运行时
对象为 342,087 字节。该数字不是最终 `xl build` 产物大小。
`examples/basic.c` 的 TCC 单文件路径还可直接用
`release/xl.exe run release/extlibs/miniz/examples/basic.c` 运行，或单独
`xl build`；正式包仍按清单分离编译原生源码。

当前 `xl.exe` 的 `Buffer` 单字节结果参与异或仍触发 XLIR 算术转换错误，
最小复现见 `dev/extlib_blockers/argon2_byteat_sub.md`。包级用例用损坏帧头
测试解码失败，不将绕开该表达式视为主线问题已解决。完整 AOT 仍缺
`xl6ValueDataSnapshotV1`，发布级签收待主线修复后复验。

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
