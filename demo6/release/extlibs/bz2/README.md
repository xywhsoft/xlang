# bz2

2026-10-25 文本解压改为 `cstring` 接管 XRT 解压缓冲，不再经
`Buffer.text()` 无条件复制一次。C 桥接层验证 UTF-8、追加终止字节，
然后以实际字节长度移交所有权；保留内嵌 NUL，空文本和无效 UTF-8
均有 C/XLang 专项测试，C 测试还覆盖指针身份及分配失败回收。
Windows 严格 C、内存故障、Python 互通、文件事务已通过；本包
`test/text_transfer.xl` 与中央 `extlib_bz2.xl` 的 JIT/AOT 均通过。
项目根目录完整门禁仍被共享 VFS 的 `include/xruntime_decl.h`
查找故障中断，不能据此签收本次完整门禁。

2026-10-25 当前发布物复验：Windows 默认 `tools/check_extlib_bz2.ps1`
完整通过，包含中央语言用例及进度回调的 JIT/AOT、23 个生成 C 原型、
269 项独立 Python 互通、分配故障、文件事务和双语范例。WSL
`tools/check_extlib_bz2.sh` 通过 C ASan/UBSan、同一互通集、文件路径
及 XLang 文件用例。Windows 原有字节异或 XLIR 故障已不再复现；
现有 Linux `release/xl` 运行 `test/cases/extlib_bz2.xl` 仍在第 68 行
报 typed XLIR 运算符路径错误，故 Linux 完整语言及独立 EXE AOT
尚未签收。需要同步重建 Linux 发布物后再验，不在本包绕过。

> 2026-10-24 当前 Windows 发布版复验：默认
> `tools/check_extlib_bz2.ps1` 完整通过。严格 C、分配故障、
> Python bzip2 互通、文件事务、23 个实际生成原型、中央 XLang
> 与进度回调 JIT/AOT，以及 C/XLang 基础和文件范例均通过。
> 下文 XLIR 字节运算与 AOT 阻断是旧发布物的记录；本次没有
> 重新构建 Linux 发布版，不能据此签收 Linux 完整门禁。

bzip2 字节流、增量编解码与事务文件接口。算法使用仓库内 `_bzip2` 的 bzip2 1.0.8，
内存、Buffer、Reader/Writer、UTF-8 文件路径和结构化错误使用 XRT，不加载压缩 DLL。

> 以下 2026-10-14 记录描述当时的发布物：文件 OOM 事务故障注入在独立 C、`xl run`、
> `xl build` 产物以及 `xl.dll`/`xlc.dll` 的公开 `xlRunCSourceFiles` 宿主上均通过。
> 完整 XLang 用例仍受已记录的 XLIR 字节运算问题阻断，不能把文件专项通过
> 当作整个包已完成 RC 验收。

## xlang API

```xlang
import bz2

Buffer packed = bz2.compress("你好，bz2", level = 6)
assert bz2.decompressText(packed, smallMemory = true) == "你好，bz2"

auto encoder = bz2.compressor()
encoder.update("left")
Buffer wire = encoder.drain()
encoder.update("right")
encoder.finish()
wire.append(encoder.drain())
assert bz2.decompressText(wire) == "leftright"
```

| API | 参数与结果 |
| --- | --- |
| `compress(Buffer|string data, int level=9, int maxInput=268435456, int maxOutput=536870912)` | 拥有型 `Buffer`；字符串按 UTF-8 字节数传入 |
| `decompress(Buffer data, bool smallMemory=false, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000)` | 拥有型 `Buffer` |
| `decompressText(Buffer data, bool smallMemory=false, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000) as cstring` | 验证 UTF-8 后移交解压缓冲；二进制内容使用 `decompress` |
| `compressFile(string source, string destination, int level=9, bool overwrite=true, int maxInput=268435456, int maxOutput=536870912, onProgress=null)` | 同步压缩文件，无返回值 |
| `decompressFile(string source, string destination, bool smallMemory=false, bool overwrite=true, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000, onProgress=null)` | 同步解压文件，无返回值 |
| `compressor(int level=9, int workFactor=30, int maxInput=268435456, int maxOutput=536870912)` | `bz2.Bz2Compressor` |
| `decompressor(bool smallMemory=false, int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000)` | `bz2.Bz2Decompressor` |

常量为 `BZ2_MIN_LEVEL=1`、`BZ2_MAX_LEVEL=9`、`BZ2_DEFAULT_LEVEL=9`、
`BZ2_MIN_WORK_FACTOR=0`、`BZ2_MAX_WORK_FACTOR=250`、`BZ2_DEFAULT_WORK_FACTOR=30`。
level 控制块大小和相应压缩资源；workFactor 控制排序算法切换阈值，0 使用 bzip2
默认值，不是另一个压缩等级。smallMemory 只影响解码内存/速度，不改变解码结果。

两个类均支持 `valid`、`inputBytes`、`outputBytes`、`finished` 只读属性，及
`drain() as Buffer`、`finish() as bool`。压缩器 `update` 接收 string 或 Buffer，
解压器接收 Buffer，并额外提供 `members` 属性。工厂提供常用默认参数；直接调用
native-backed 类的构造入口时必须完整提供底层参数，不要把它误作另一组默认构造。

2026-10-18：两种流对象的 `updateNative(point,size)` 原可由导入方
直接调用，绕过 Buffer/string 的真实长度与结果所有权处理。编译负例
先确认可达，压缩器和解压器合计 14 个 `*Native` 方法现仅供类内部使用；
`test/private_compressor_negative.xl` 和 `private_decompressor_negative.xl`
只做 `xl check`，不运行。Windows XLang 基础流范例、文件事务用例及
通过包源码注入的 C 对照范例均与黄金输出一致。本轮未改 C 编解码器，
未重复完整分配故障/互通矩阵；共享 XLIR 阻断仍未解除。

- `drain()` 取走当前积累的输出，不强制刷新 bzip2，也不重置累计计数。
- `finish()` 只封存或校验流，不返回 Buffer；最后还需一次 `drain()`。未取走 C
  最终 Buffer 时，重复 finish 安全。XL 多次 drain 无新数据时返回空 Buffer。
- 解压器的 `finished` 表示当前恰好处于完整成员边界；显式 finish 前仍可提交下一
  成员。输入结束必须调用 finish，不能凭中间一次 finished=true 忽略后续截断。
- 允许完整空内容和多个串联成员；零字节压缩输入、CRC 错误、截断、尾部垃圾均拒绝。
- 参数误用可纠正；编码、损坏数据、限额、writer 或同对象重入失败是终态。
  drain 为新 Buffer 分配失败时保留内部输出，内存恢复后可以重试 drain。
- 同一对象串行使用，不支持 writer 回调重入；不同对象可独立使用。未提供共享
  可变对象并发承诺，也未以本次测试作为性能基准认证。

## 文件、进度与错误

```xlang
bz2.compressFile("input.bin", "output.bz2",
    onProgress = func(int inputBytes, int totalBytes, int outputBytes) as bool
        println(inputBytes, "/", totalBytes)
        return true
    end)
```

回调在调用线程同步执行，不被保存。参数单位均为字节；XRT 的 uint64 计数经过
范围检查后转换为 XL int，避免带签名函数被错误类型调用。返回 false 抛出
CancelledError；回调抛出的原异常类型和消息保留。最终通知也可取消。XL 回调
没有 C 进度帧的 FINAL 标志，不能把 inputBytes==totalBytes 当作唯一结束事件。
取消在流循环通知点生效，不是线程中断，也不是硬 CPU 截止时间。

文件接口在目标目录独占创建临时文件，编码、最终通知、flush、close 均成功后
才重命名提交；支持 source=destination，overwrite=false 不替换已存在目标。
即使目标在预检后由其他写者创建，最终提交也拒绝替换该文件。
源或目标路径含 NUL 时抛 `ValueError`，不访问截断后的前缀路径。
常规失败、取消及本包已覆盖的晚期 OOM 均保留已有目标并清理本次临时文件。
这不承诺断电后的目录持久性或多个写者之间的事务隔离。

| 条件 | XL 异常 |
| --- | --- |
| 损坏/截断压缩流、文件 IO | `IOError` |
| 无效 level/workFactor、非正限额、maxRatio 超出 1..4294967295、禁止覆盖 | `ValueError` |
| 输入/输出/倍率限额超出、进度计数无法表示为 int | `RangeError` |
| 失败对象继续使用、完成后 update | `RuntimeError` |
| 内存分配失败 | `MemoryError` |
| 回调返回 false | `CancelledError` |

XRT 错误在 native 调用边界自动传播；`CompressionError` 只是没有原生结构化错误
时的防御性失败，不应统一只捕获它。所有限额按累计字节计算。倍率使用累计输出与
**已提交**压缩输入比较，分块大小会影响检查时点，应同时设置 maxOutput。
它不限制 bzip2 内部工作内存、CPU 时间或成员数量，不是不可信输入的完整沙箱。
一次性解码失败不返回残缺 Buffer；增量 drain 或裸 writer 已交付的内容无法撤回。

## C API

包含 `include/xrt_bz2.h`，首次包含完整 XRT 头前完成模块选择；文件/流绑定的
模块集合见 `xlang.package.json`。只链接一份 XRT/xruntime 和 `_bzip2` 的七个
源文件。无需加载 xlang，独立 C 可直接使用以下接口：

- `xrtBz2OptionsInit` / `xrtBz2ErrorInit` 初始化版本化配置及错误快照。
- `xrtBz2Compress` / `xrtBz2Decompress`：成功移交 `xbuffer*`，调用者 destroy；
  失败置输出槽 NULL。不要把尚未释放的旧指针作为输出槽传入。
- `xrtBz2Compressor*` / `xrtBz2Decompressor*`：Create、Update、Finish、Drain、
  DrainBuffer、TakeBuffer、InputBytes、OutputBytes、Finished、Error、Destroy；
  解压器还有 Members。Create 失败返回 NULL，Destroy(NULL) 安全。
- `Drain(writer)` 借用而不关闭 writer。`DrainBuffer` 多次移交输出；`TakeBuffer`
  仅在 Finish 后移交一次，之后对象仅可查询或销毁。Error 返回对象内借用快照。
- `xrtBz2CompressStream` / `xrtBz2DecompressStream` 借用 reader/writer，处理短读
  短写并保留原 IO 错误；known total 按 reader 剩余长度计算，未知为零。
- `xrtBz2CompressFile` / `xrtBz2DecompressFile` 使用上述同一文件事务，不要求 C
  调用者自行实现回滚；接收 options、overwrite、`xrtprogressproc` 和 user 指针。
- C 进度结构提供 TOTAL_KNOWN/FINAL 标志。文件入口通过 `xrtGetError()` 报错，
  编解码/流还可输出 `xrtbz2error`（类型、后端错误码、输入/输出位置、消息）。
- C 限额中的 maxInput/maxOutput/maxRatio=0 关闭对应约束；XL 便捷 API 要求正数。
  options/limits 的 size/version/reserved 必须正确，先调用 Init；OptionsInit
  将不适用的单项、条目、节点和深度字段清零。显式设置这些字段或 `iFlags`
  会报 `ARGUMENT`，不会伪装成已限制节点、嵌套或成员数。

## 范例与门禁

两组 C/XL 范例具有相同输入和逐行相同黄金输出：

| C | xlang | 重点 |
| --- | --- | --- |
| `examples/basic.c` | `../../examples/extlibs/bz2_basic.xl` | UTF-8、二进制、增量流、截断 |
| `examples/files.c` | `../../examples/extlibs/bz2_files.xl` | 文件、进度、取消、目标保留 |
| - | `../../examples/extlibs/bz2.xl` | 工作因子、低内存、多成员、累计属性、限额和错误 |

在 demo6 目录执行：

```powershell
xl run release/examples/extlibs/bz2_files.xl
xl run release/extlibs/bz2/examples/basic.c
xl run release/extlibs/bz2/examples/files.c
powershell -NoProfile -File tools/run_extlib_c.ps1 -Package bz2 -Source release/extlibs/bz2/examples/files.c
powershell -NoProfile -File tools/check_extlib_bz2.ps1
```

Linux C sanitizer 与 XLang 文件门禁：`bash tools/check_extlib_bz2.sh`，需要 GCC、Python3 和 `release/xl`。
文件范例排他创建 XRT 临时目录并在结束时清理，不会覆盖工作目录中的同名文件；
中央 XLang 文件范例也使用独占目录与 `finally` 清理，写入时将 Buffer 明确转为
字节值，读取结果按字节比较，不再依赖固定 `build/test` 路径或对象相等。
文件读写、取消及基础范例的压缩、流式和截断验证使用显式检查，
禁用 C 断言时也会执行，并在失败路径释放已有资源。
包内 `test/file_ops.xl` 已登记到 `coverage.tests`，覆盖 UTF-8 往返、低内存解码、
进度回调、禁止覆盖、回调期间目标出现的提交竞态、取消清理及限额失败时保护原目标文件。
内部 C 文件事务测试 `test/test_bz2_files.c` 必须传入本次测试独占目录；
无参数时只报用法错误，不会创建或删除固定工作目录。包级门禁已提供该参数。
当前发布 DLL 的文件事务专项可单独运行
`powershell -File tools/check_extlib_codec_dll_file.ps1 -Package bz2`；
它使用 `test/support/extlib_codec_dll_host.c` 经公开 API 执行同一份 C 故障夹具。
门禁覆盖 23 个实际 cgen 原型与公共 C 头的一致性、独立 Python bz2 互通、限额、
损坏输入、所有权、分配失败、短 IO、回调取消/错误/重入、文件提交竞争、双语
JIT/AOT 与黄金输出。Linux 使用 ASan/UBSan/泄漏检查，包括 vendor 算法。
完整 XLang 用例仍被共享 XLIR 字节运算问题阻断，包内文件用例通过不等于完整语言验收。
默认门禁必须暴露冻结 XRT 的已知缺陷；`XrtInclude` / `XRT_INCLUDE` 仅用于隔离
C 运行库验证，不改变 XL JIT/AOT 使用的发布程序，也不构成发布验收豁免。

2026-10-19：C 定向红测确认通用初始化带入四个无效的非零限额；压缩和解压
对象也会接受显式无效字段或未知标志。现将默认值清零并在公共初始化入口拒绝。
Windows 严格 C、分配故障、269 项 Python bz2 互通、文件边界与文件 AOT 通过，
完整语言用例仍在第 68 行触发既有 typed XLIR 字节运算错误。Linux C
ASan/UBSan、独立互通、文件事务和 XLang 文件用例通过；整包发布级未签收。

2026-10-20：两份 C 范例原直接 `xl run` 缺 bzip2 实现符号，仅包运行器
显式追加包源时可用。范例现只在 TCC 单文件路径包含上游与包内核心，
正式 manifest/GCC 分离编译不变。Windows 已验证 `basic.c`、`files.c` 的
直接运行、直接构建后执行与包运行器；Linux 联合门禁新增两份直接运行
与 GCC C 黄金输出对照，ASan/UBSan、269 项标准 bz2 互通和文件事务通过。
完整 XLang AOT/XLIR 的既有主线阻断不变。

2026-10-20：`bz2_files.xl` 原在固定目录写入三份文件，并将 Buffer 直接
传给 `file.writeBytes`、以两个 Buffer 对象相等判断解压结果。`xl check`
先报参数类型不匹配；修正后运行阶段的数据断言仍失败。现用 `toBytes()`
表达二进制值、独占临时目录和 `finally` 清理；`demo6` 根与 `release`
直接运行以及 C 范例均逐行输出 `bz2_files.expected.txt` 的三行结果。
Linux `tools/check_extlib_bz2.sh` 的 ASan/UBSan、269 项 Python 互通、
文件事务与新增的中央 XLang/C 黄金对照通过。Windows
`tools/check_extlib_bz2.ps1 -SkipAot` 的 C、互通和文件边界检查通过后，
仍在 `extlib_bz2.xl:68` 的共享 typed XLIR 字节异或处失败；该模式只
显式跳过 AOT，不跳过语言 JIT 阻断。整包未签收发布级。

2026-10-21：四份 C 边界测试的流式、分配故障与文件事务操作原位于
`assert` 内，定义 `NDEBUG` 会删除调用。测试单元现保持断言启用，
Windows/Linux GCC 门禁以 `-DNDEBUG` 编译运行；预处理确认
压缩器构造调用仍在。Windows 严格 C、文件边界及 Python 互通已通过，
联合门禁随后仍在 `extlib_bz2.xl:68` 的已登记 XLIR 错误处停止。
Linux C ASan/UBSan、269 项互通、文件事务与 XLang 文件门禁通过。
库实现/API 未改，不签收整包发布级。

2026-10-21：`test/file_ops.xl` 新增压缩和解压进度回调抛错的
原始 `ValueError` 消息断言；失败的新目标不得落盘，已有解压目标
必须保持原内容。Windows 联合门禁现于 AOT 和中央语言用例之前运行
该文件测试：严格 C、文件边界、269 项 Python bz2 互通及新增用例
均通过，随后仍在 `extlib_bz2.xl:68` 的共享 typed XLIR 错误处停止。
Linux C ASan/UBSan、互通和 XLang 文件门禁通过。没有修改包实现，
不据此签收整包发布级。

2026-10-24：使用当前发布版 `xl.exe` 不带 `-SkipAot` 重跑 Windows
完整门禁，63 项检查全部通过且输入快照无变化，覆盖中央语言用例
JIT/AOT、综合与双语范例、23 个生成 C 原型、严格 GCC、文件事务和
269 项独立 Python bz2 互通。Linux C ASan/UBSan、独立互通及
XLang 文件对照门禁通过。前述 typed XLIR 阻断属于旧发布版历史
记录；Linux 完整 XLang AOT、长期压力和性能基准不在本次验收内。
