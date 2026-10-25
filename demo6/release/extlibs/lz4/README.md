# lz4

标准 LZ4 Frame 编解码、增量对象、文件事务及同步进度回调。复用官方 LZ4F 和
XRT 的缓冲、文件、错误、资源限额，不要求额外安装压缩 DLL。

## 快速使用

```xlang
import lz4

Buffer packed = lz4.compress("hello 世界", acceleration = 4)
assert lz4.decompressText(packed) == "hello 世界"

auto encoder = lz4.compressor()
encoder.update("left")
Buffer wire = encoder.drain()
encoder.update("right")
encoder.finish()
wire.append(encoder.drain())
assert lz4.decompressText(wire) == "leftright"
```

任意二进制数据使用 Buffer，包括无效 UTF-8 字节。string 重载面向文本，
长度通过 string.byteLen 传递，内嵌 NUL 不截断。
`decompressText` 校验 UTF-8 后取走 XRT 解压缓冲，保留内嵌 NUL，
不再经 `Buffer.text()` 无条件复制；追加末尾 NUL 时容量不足仍可能扩容。
非法 UTF-8 抛 `ValueError`。通过公开 C 导出调用时按普通字符串 ABI 转换。

## xlang API

下列签名均位于 lz4 模块；同名函数只使用这里列出的参数顺序。

```text
version() as string
compress(Buffer|string data, int acceleration=1,
    int maxInput=268435456, int maxOutput=536870912) as Buffer
decompress(Buffer data, int maxInput=268435456,
    int maxOutput=536870912, int maxRatio=1000) as Buffer
decompressText(Buffer data, int maxInput=268435456,
    int maxOutput=536870912, int maxRatio=1000) as cstring
compressor(int acceleration=1, int maxInput=268435456,
    int maxOutput=536870912) as Lz4Compressor
decompressor(int maxInput=268435456, int maxOutput=536870912,
    int maxRatio=1000) as Lz4Decompressor
compressFile(string source, string destination, int acceleration=1,
    bool overwrite=true, int maxInput=268435456, int maxOutput=536870912,
    (func(int, int, int) as bool)? onProgress=null)
decompressFile(string source, string destination, bool overwrite=true,
    int maxInput=268435456, int maxOutput=536870912, int maxRatio=1000,
    (func(int, int, int) as bool)? onProgress=null)
```

压缩加速因子范围 1..65537，常量为 LZ4_MIN_ACCELERATION、
LZ4_MAX_ACCELERATION、LZ4_DEFAULT_ACCELERATION；不是 HC 压缩等级。
便捷函数要求正数限额，maxRatio 必须能放入 uint32。

| 对象 | 方法 | 只读属性 |
| --- | --- | --- |
| Lz4Compressor | update(Buffer)、update(string)、drain() as Buffer、finish() as bool | valid、inputBytes、outputBytes |
| Lz4Decompressor | update(Buffer)、drain() as Buffer、finish() as bool | valid、inputBytes、outputBytes、finished、members |

`*Native` 成员仅供包内包装代码调用，导入方使用上表的公开方法。包级门禁以只编译负例验证原生入口不可访问，不执行伪造指针与长度的调用。

优先使用 compressor/decompressor 工厂。原生类构造器的参数分别为
`(c_int, c_ulonglong, c_ulonglong)` 和 `(c_ulonglong, c_ulonglong, c_uint)`，
遵循 C 限额合同，零表示关闭对应限制，不等同便捷工厂的正数校验。
计数器累计输入/输出字节，drain 不会重置它们；超过 xlang int 范围时报 RangeError。

## 流状态和格式

- 压缩产生独立 64 KiB 块，开启块与内容校验和。解压接受标准大小、关联/独立块、
  拼接数据帧和 16 种 skippable 帧；非零外部字典 ID 不支持。
- 空内容压缩生成有效空帧；零字节压缩输入是截断错误。完整 skip-only 流可解出空结果。
- finished 表示当前处于完整帧边界，不表示调用方再也不能 update 下一帧。
  members 只统计已完成的普通帧，不统计 skippable；最终仍须调用 finish 校验尾部。
- finish 成功后重复调用成功；之后 update 失败。drain 转移当前结果的拥有权，
  无数据返回空 Buffer；finish 不会代替最后一次 drain。
- 参数错误可纠正；数据、限额、IO 和重入错误使对象进入失败终态，须创建新对象。
  drain 的替换空缓冲分配失败时，原输出保留，可在内存恢复后重试。
- 对象串行使用；独立对象可用于不同线程。同一个对象不能从其 writer 回调重入或销毁，
  也不支持多个线程无锁同时操作。

maxInput/maxOutput 为累计字节。解压 maxRatio 按每个数据帧已消费输入计算，
其他帧及 skippable 数据不能稀释比例；这是逐前缀启发式限制，不是仅比较最终比值。
C 的 maxEntries 默认 100000，包含数据帧和 skippable 帧。OptionsInit 将不适用的
单项、节点和深度字段清零；显式设置这些字段或 `iFlags` 会报参数错误，
不会静默忽略。输入默认 256 MiB、输出 512 MiB、倍率 1000。
这些不是 CPU 超时或工作内存上限；上游解码器另需标准块和历史窗口的工作空间。

drain/Writer 已交出的前缀无法撤回，完整 finish 成功前不要把它视为已校验数据。
校验和用于检测损坏，不是加密认证。本接口仅定义标准 Frame，不提供其他私有外层格式。

## 文件和进度

文件接口使用同目录临时文件，完整处理、最终回调、flush/close 成功后才提交。
支持源与目标同路径。overwrite=false 同时检查初始存在和提交竞争；失败保留原目标，
并清理本次临时文件。此处不承诺断电持久性，也无法保证操作系统拒绝删除时仍能清理。
源或目标路径含 NUL 时抛 `ValueError`，不会访问截断后的前缀路径。

onProgress 同步收到 `(inputBytes, totalBytes, outputBytes)`；返回 false 抛
CancelledError，抛异常则保留其类型、消息及错误因果链。最终通知也可以取消。
回调可操作其他独立编解码器，不能重入当前对象。流式 C 接口未知总量时 totalBytes 为 0，
支持 size/tell 的 reader 从当前游标计算剩余总量。

参数错误为 ValueError，限额为 RangeError，状态为 RuntimeError，内存不足为
MemoryError，压缩数据损坏/截断及文件 IO 为 IOError。C 层保留更细的 xrtlz4status；
底层 IO 或回调已有错误时不改写为泛化的压缩错误。

## C API 与所有权

[xrt_lz4.h](include/xrt_lz4.h) 是完整公共合同，C 可独立使用，不依赖 xlang。
options 必须先调用 xrtLz4OptionsInit；传 NULL 使用默认值。版本、结构大小和保留位均校验。
原始 C 数据借用至调用返回；调用成功得到的 xbuffer 用 xrtBufferDestroy 释放。

- 一次性：xrtLz4Compress / xrtLz4Decompress。
- 增量：xrtLz4Compressor* / xrtLz4Decompressor*，包含 Update、Finish、Drain、
  DrainBuffer、TakeBuffer、计数和 Error。TakeBuffer 只能完成后取走一次。
- 流：xrtLz4CompressStream / xrtLz4DecompressStream，借用 xreader/xwriter，
  不关闭或 flush，进度使用普通 xrtprogressproc 和 user 指针。
- 文件：xrtLz4CompressFile / xrtLz4DecompressFile，与 XL 共用同一事务实现。

基础 C 实现只需要 XRT；lz4_native.c 中的语言 callable 适配另需 xruntime。
只链接一份 XRT/xruntime，不创建第二套堆。打包源码见 xlang.package.json，
第三方来源、符号隔离和许可见 [VENDOR.md](VENDOR.md)。

## 范例与验证

发行目录中的 `examples/extlibs/lz4.xl` 为综合范例；lz4_basic.xl、lz4_files.xl
与 [basic.c](examples/basic.c)、[files.c](examples/files.c) 使用相同输入和黄金输出。
在 demo6 源码目录可执行：

```powershell
./release/xl.exe run release/examples/extlibs/lz4_basic.xl
./release/xl.exe run release/extlibs/lz4/examples/basic.c
./release/xl.exe run release/extlibs/lz4/examples/files.c
./tools/run_extlib_c.ps1 -Package lz4 -Source release/extlibs/lz4/examples/basic.c
./tools/check_extlib_lz4.ps1 -Interop
```

`basic.c` 和 `files.c` 的 TCC 单文件入口包含各自所需的包内源码，
可直接 `xl run` 或 `xl build`；正式包仍按 manifest 分离编译，
不重复包含。Windows 门禁核对两份单文件运行与构建后的黄金输出；
Linux 门禁将单文件运行结果与严格 GCC 分离编译结果逐字比较。

`-Interop` 要求 WSL、Python 3 和系统 liblz4，缺少依赖时报错，不作静默跳过。
Linux C sanitizer、XLang 边界/文件进度与双语输出门禁：`bash tools/check_extlib_lz4.sh`，当前独立参照路径为
`/usr/lib/x86_64-linux-gnu/liblz4.so.1`。普通 Windows 门禁不带 -Interop 时仍运行
C/XL 边界、JIT/AOT、原型和双语范例，但不能据此宣称独立互操作已验证。
文件范例排他创建 XRT 临时目录并在结束时清理，不会覆盖工作目录中的同名文件；
中央 XLang 文件范例也已使用 `dir.temp` 和 `finally`，不再写入固定的
`build/test/lz4-files-example`；文件写入失败会立即报错。
文件读写、取消及基础范例的压缩、流式和截断验证使用显式检查，
禁用 C 断言时也会执行，并在失败路径释放已有资源。
内部 C 文件事务测试 `test/test_lz4_files.c` 必须传入本次测试独占目录；
无参数时只报用法错误，不会创建或删除固定工作目录。包级门禁已提供该参数。

2026-09-28 已执行 Windows 63 步专项：23 个实际生成原型、C 严格编译、分配失败、
短读短写、回调取消/错误/重入、文件提交竞争、四线程 1000 轮，以及 C/XL JIT/AOT。
Windows 驱动和 Linux 独立 C 驱动各通过 514 项系统 liblz4 互操作检查；Linux 同时
开启 ASan/UBSan 和泄漏检测。包内 boundaries.xl 还验证与 xxhash 同时链接。
Linux XLang 包内边界、中央文件进度和两组 C/XL 固定输出对照已纳入同一门禁；
完整中央用例仍受已记录的字节位运算 XLIR 问题阻断，不代表 Linux XLang AOT、
32 位平台、TSan、长期压力或性能基准通过。

2026-10-19：C 定向红测确认通用初始化带入三个无效非零限额；压缩/解压
对象接受显式无效字段和未知标志。现清零不适用默认值，并在共用初始化
入口拒绝；`maxEntries` 继续作为解码帧数上限。Windows 严格 C、线程、
分配故障、文件事务及 514 项系统 liblz4 互通通过；Linux C ASan/UBSan、
独立互通和 XLang 双语门禁通过。完整语言用例第 92 行仍遇到既有
typed XLIR 字节运算错误，整包发布验收未签收。

2026-10-20：`lz4_files.xl` 原写入固定目录，仅在正常路径逐个删除文件。
现建立独占系统临时目录并在 `finally` 清理。从 `demo6` 根与 `release`
直接运行均逐行输出 `lz4_files.expected.txt`，C 文件范例输出一致；Linux
`tools/check_extlib_lz4.sh` 的 ASan/UBSan、514 项系统库互通、线程、文件
事务与 C/XLang 双语门禁通过。综合范例 `lz4.xl` 的字节 XOR 损坏检测
仍须等共享 XLIR 修复后完整复验。

2026-10-20：综合范例 `lz4.xl` 的文件段改用 XRT 独占临时目录，
写入显式检查并在 `finally` 清理，不再占用固定 `build/test/lz4-example`。
独立的 `lz4_files.xl` 和 C 文件范例当前同输出；综合范例仍在第 78 行
字节 XOR 处被已登记的 typed XLIR 主线问题阻断，因此新文件段尚未在
综合范例中完成端到端运行验收，不签收整包发布级。

2026-10-21：六份 C 边界测试将压缩、流式 IO、分配故障、
线程和文件事务调用放在 `assert` 内；现测试单元保持断言启用，
Windows/Linux 门禁均以 `-DNDEBUG` 编译执行。Windows 严格 GCC
六组测试全部通过，随后联合门禁在 `extlib_lz4.xl:92` 的已登记
typed XLIR 错误处停止。Linux C ASan/UBSan、514 项系统 liblz4
互通、XLang 边界和双语对照通过。库实现/API 未改，主线阻断仍在，
不签收整包发布级。

2026-10-24：使用当前发布版 `xl.exe` 重跑 Windows 全门禁，修正 C
教学范例内含实现却又被测试脚本重复追加包源码的问题。综合范例现可
JIT 运行并编译执行；两组 C/XLang 黄金输出、23 个生成原型、严格
GCC、线程、故障注入、文件事务及 514 项系统库互通均通过。门禁报告
`outcome=passed`、69 项检查、输入快照无变化。Linux 门禁同时通过
ASan/UBSan、泄漏检测、独立互通及 C/XLang 双语对照。上面的 XLIR
阻断是历史记录，不再代表当前 Windows 发布版；Linux XLang AOT、
32 位、TSan、长期压力与性能基准仍未纳入本次验收。

2026-10-25：`decompressText` 改为校验后移交缓冲给 `cstring`，
补空文本、内嵌 NUL、非法 UTF-8、原址所有权和分配失败回归。
Windows 独立 C 门禁及发布目录的 XLang JIT/EXE 通过；从项目根目录
运行完整门禁仍被共享 VFS `include/xruntime_decl.h` 查找问题阻断。
Linux C ASan/UBSan 与 514 项系统库互通通过；旧 Linux `release/xl`
尚不识别 `cstring`，语言门禁需在重建发布物后复验。
