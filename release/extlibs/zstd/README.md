# zstd

2026-10-29 新增 `test/top_level.xl`：全局持有一个真实压缩器，main
内校验有效性，退出时自然析构。固定发布版 Windows/Linux JIT 均
退出0；独立 EXE 均打印 PASS 后在退出期报状态错误并退出1，确认
**M2 主线阻断**。默认门禁已接入专项；旧函数内使用、帧互通和双语
通过记录不覆盖顶层析构，本包暂不作完整发布签收。报告与恢复方式
见 `dev/extlib_blockers/global_native_class_aot_fini.md`，不移动全局对象、
不提前销毁或吞异常，也不在包内改主线。

2026-10-26 当前发布物：Windows 默认 `test/check.ps1` 与独立
`test/check_interop.ps1` 均通过；严格 C、分配故障、标准帧、串联帧、
文件、进度、错误分类及教学范例覆盖 JIT 和独立 EXE。Python
Zstandard 0.25.0 与 C 核心双向标准帧互通通过。Linux `test/check.sh`
退出码为 0：C ASan/UBSan、中央和包内 JIT 用例、八条独立 EXE AOT
及 C/JIT/AOT 范例输出对照通过。Windows 从 `demo6` 根目录直接
运行基础范例亦通过。此前的 Linux `cstring`、Windows 根目录 VFS
和原生错误先派发阻断在当前发布物均不再复现。未以这些短时门禁
代替 32 位、TSan 或长时间压力测试。

输出限额由 C 层报告 `XERR_RANGE`，XLang 抛 `RangeError`；损坏帧
和非法状态抛 `CompressionError`，分配失败抛 `MemoryError`。

## 既往阻断（现已解除）

2026-10-25：一次性和流对象的显式结果检查入口使用 `@no_raise`，
由包层把格式/状态错误转成 `CompressionError`，分配失败仍是
`MemoryError`。完成后再次 `update`、截断帧一次性解压及流式
`finish` 已在包内负例中验证；完整教学范例的 JIT 和独立 EXE
从 `release` 工作目录均运行到 `zstd example: PASS`。文件入口保留
原生自动派发，以免吞掉进度回调自身抛出的异常；`CancelledError`
和回调原异常复验通过。当时 Windows 包门禁的严格 C、分配故障、
C/TCC 范例通过，但从 `demo6` 根目录运行 XLang 阶段仍受共享
VFS `include/xruntime_decl.h` 查找问题阻断，当时整包尚未签收。
详见 `dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`。

2026-10-24 当前 Windows 发布版默认包门禁通过：严格 GCC C、TCC
单文件、固定标准帧、串联帧、文件/进度回调和双语基础范例；
基础、固定帧、串联、文件、进度共五条 AOT 路径均构建运行成功。
该次完整教学范例虽可构建独立 EXE，但 JIT/EXE 均在截断帧负例收到
`IOError`，无法由承诺的 `CompressionError` 捕获；见
`dev/extlib_blockers/zstd_finish_error_dispatch.md`。不能把包级基础门禁
通过当成当时完整 API 的发布签收。
Linux 本轮 C ASan/UBSan、文件和分配测试通过；旧 `release/xl` 在
包导入处拒绝当前 `cstring` 声明，尚不能运行 Linux 语言门禁。

## 功能与用法

内嵌 Zstandard 后端，公开 Buffer/string 一次性压缩、增量流式处理和文件接口。二进制结果始终是 `Buffer`；只有明确需要 UTF-8 文本时才使用 `decompressText`。

```xlang
import zstd

Buffer packed = zstd.compress("hello zstd")
assert zstd.decompressText(packed) == "hello zstd"
```

`compress` 的默认级别为 3；`compressor(level)` 和 `decompressor()` 提供 `update`、`drain`、`finish`。压缩流须调用 `finish` 写出帧尾；解压流的 `finish` 会检查截断。`decompress` 可处理串联帧。文件接口使用 XRT reader/writer，并在目标目录创建临时文件，成功关闭后重命名；失败或取消会删除临时文件。源或目标路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。文件进度回调返回 `false` 时取消操作。

`decompressText` 校验 UTF-8 后取走 XRT 解压缓冲，保留内嵌 NUL，
不再经 `Buffer.text()` 无条件复制。追加终止字节时若容量不足，XRT
仍可能扩容。非法 UTF-8 抛 `ValueError`；普通解压错误仍按原有类别报告。
XLang 内部返回 `cstring` 共享所有权，通过公开 C 导出时按普通字符串 ABI 转换。

流对象的 `*Native` 成员仅供包内包装代码使用，导入方调用公开的 `update`、`drain`、`finish` 和属性。包级门禁以只编译负例验证原生入口不可访问，不执行伪造指针与长度的调用。

语言接口默认限制输入 256 MiB、输出 512 MiB、解压比 1000；调用方可以显式调小。C 核心接口 `include/xrt_zstd.h` 使用 `xrtzstdoptions`，限额为零表示关闭该项限制；薄绑定的零参数使用 XRT 默认值。调用前应根据不可信数据的大小和部署环境设置合适限额。压缩不是加密或内容认证。

## C 接口与所有权

`include/xrt_zstd.h` 可由独立 C 编译单元直接包含，声明所需 XRT Buffer/IO 模块。`xrtZstdCompress`、`xrtZstdDecompress` 成功时返回拥有型 `xbuffer*`，调用方负责 `xrtBufferDestroy`；失败时输出为空，并填写 `xrtzstderror`。流对象分别用 `xrtZstdCompressorDestroy`、`xrtZstdDecompressorDestroy` 释放。`include/zstd_native.h` 是 XLang 的薄 ABI 桥，C 调用方通常直接使用 XRT 风格接口。

所有 C 编解码入口在读取选项前校验 `xrtzstdoptions` 与内嵌 `xrtresourcelimits` 的版本、完整结构长度和保留字段；不接受未知版本或截断结构。一次性和文件流接口报告 `XRT_ZSTD_ERROR_ARGUMENT`；流对象构造器保留错误状态供 `...Error()` 查询。无效选项不会消费文件流输入或写出结果。
资源配置的 `iMaxEntries` 限制串联解压帧数，`iMaxItemBytes` 限制每帧原文大小；两者为零时不设该项上限。节点数、深度和未知标志不适用于 Zstd，默认清零，显式非零报参数错误。后续帧在解码前检查数量限额。

## 验证与当前限制

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zstd -Source release/extlibs/zstd/test/test_zstd.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zstd -Source release/extlibs/zstd/test/test_file.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zstd -Source release/extlibs/zstd/examples/basic_c.c
release/xl.exe run release/extlibs/zstd/examples/basic_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/zstd/test/check.ps1
.\release\xl.exe run release/extlibs/zstd/examples/basic.xl
cd release
.\xl.exe run ..\test\cases\extlib_zstd_progress.xl
cd ..
wsl bash release/extlibs/zstd/test/check.sh
```

C 测试覆盖二进制往返、标准帧标识、截断、无效帧、输出限额、分段流，以及原生进度回调与取消。包内 `test/wire_fixture.xl` 已登记到 `coverage.tests`，并与 C 边界测试共用 Node.js 24 内置 Zstd（级别 3）独立编码的固定帧，验证含 UTF-8、NUL、0xff 的二进制解码和输出限额；运行门禁不依赖 Node.js。文件 ABI 的二进制往返和禁止覆盖由 `test/test_file.c` 验证；XLang 的文件压缩/解压、进度回调、取消后保留旧目标由 `extlib_zstd_progress.xl` 在独立临时目录验证。另有登记在 `coverage.tests` 的 `test/file_ops.xl`，直接检查文件往返和取消后的目标清理。最小 C/XLang 范例同输入、同输出；C 范例命名为 `basic_c.c`，避免被同名 XL 范例自动混编产生双 `main`。当前 Linux 包级脚本覆盖这些语言边界及八条 AOT 路径。旧发布物的 Buffer 索引异或和 AOT 缺符号诊断见 `demo6/dev/extlib_blockers/argon2_byteat_sub.md`；当前完整教学范例的 JIT/EXE 均通过。

可选的独立实现互通测试：在 `demo6` 目录运行 `python -m pip install --target build/test/zstd/python_oracle zstandard==0.25.0`，再运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/zstd/test/check_interop.ps1`。该测试让 XRT 与 Python Zstandard 双向解压对方的标准帧；测试桥显式放宽解压比限额以覆盖高压缩比样本，不改变产品默认值。Python 不是扩展库运行依赖。

以下为历史验证记录；其中“当前阻断”只描述记录时使用的旧发布物。

2026-10-18：四类选项 ABI 负例先复现后修复，另覆盖两类保留字段以及一次性、流对象和文件流入口。Windows 严格 GCC/C/XLang 与 Linux ASan/UBSan 联合门禁通过。随后包外原可调用 `updateNative(point,size)` 的两条负例先确认可达；12 个原生流成员私有化，负例纳入 Linux 门禁，Windows 最小流/固定帧用例及 Linux 联合门禁通过。完整教学范例和 AOT 主线阻断未变。

2026-10-19：C 流式压缩/解压进度总量原取 Reader 物理总长，
从非零位置读取时多算已跳过的前缀。两个方向的 C 回归先红后绿，
现以 `Size - Tell` 报告本次总量；未知长度 Reader 仍能处理数据，
但不置 `TOTAL_KNOWN`。Windows TCC C/严格 GCC、Linux C ASan/UBSan、
文件 API、XLang 进度及双语对照门禁通过。完整教学范例与 AOT 的
共享主线阻断不变。

2026-10-19：C 定向红测确认通用资源初始化带入无用节点/深度限额；
串联帧的 `iMaxEntries` 原未生效。现清零无用默认值、拒绝显式无效
字段和未知标志，并在后续帧解码前执行数量限额。同批与分次输入、
单帧可用、第二帧越界、逐帧 `iMaxItemBytes`、零表示无限制均已覆盖。
Windows C/XLang 最小范例和 Linux C ASan/UBSan 联合门禁通过；
完整教学范例与 AOT 仍受既有主线阻断，不签收整包发布级。

2026-10-19：C 解压器原在 `finish()` 成功后仍接受 `update()`，导致终止
状态与后续帧不一致。C 红绿回归后，完成态的写入现返回
`XRT_ZSTD_ERROR_STATE`，输入字节计数保持不变；Linux C ASan/UBSan
及包级语言、双语门禁通过。XLang 包装层仍受 `extern` 原生错误先于
`CompressionError` 抛出的主线异常派发问题阻断，最小复现见
`dev/extlib_blockers/zstd_finish_error_dispatch.md`；未把失败例伪装成
绿色包测试，也未签收完整 AOT。

2026-10-20：新增 `test/test_allocations.c`，逐点注入 XRT 分配失败，
检查一次性压缩/解压不交付半成品、内存错误分类、释放平衡和故障解除后的
恢复。Windows 严格 GCC 与 Linux ASan/UBSan 门禁通过；该故障注入
不覆盖上游 Zstandard 内部分配器。同日重新运行 Python Zstandard 0.25.0
的四组固定数据双向互通，检查通过；完整 XLang AOT 的主线阻断仍未解除。

2026-10-24：新增 `test/concat_boundary.xl`，分别验证两个完整帧同批
拼接、一次性帧加分批压缩帧、先单帧解压后再复用输入、两次 `update`
组成的串联解压，按字节核对包含 NUL/0xff 的结果。当前源码驱动的
内存运行与独立 EXE 均通过，测试已登记在包清单及双平台门禁。
完整教学范例的截断负例仍被已登记的 `CompressionError` 派发问题
阻断；不把捕获更宽的 `IOError` 当作修复，也不据此签收整包发布级。
