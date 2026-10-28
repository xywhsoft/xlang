# tar 扩展库

2026-10-28 数值边界收口：GNU 二进制 mtime 下溢原先会变成正数或零，
超宽 mode 原先截断为低32位；C 红例确认后修复。mode/设备号的原生
桥接改用 unsigned int / c_uint，避免有效高位被读成负数。按用户确认，
`TarInfo.uid/gid`、其构造参数和两个 `writer.add` 重载的 `uid/gid`
统一为 `uint64`，默认仍为0；删除旧有符号检查入口，不提供兼容重载。

Windows/Linux 数值专题完整门禁通过：C18组内存归档边界，Python
独立编码/解码的40组归档分别逐行核对 C、XLang JIT 和独立 EXE，
以及完整 uint64 文本/Buffer 写入、持有元数据、双语范例和负 ID
只编译负例。Linux C 使用 ASan/UBSan。两平台原有 C/TAR 套件通过，
整包门禁仍因 M1 名称数组投影失败，根内原子提交缺口也未关闭；
不以数值专题通过代替选择提取、完整回调或整包发布签收。

2026-10-24 当前 Windows 发布版复验：严格 C/TAR 主用例、默认读取限额、
C/TCC 直接运行与构建、内存/文件追加双语范例均通过；默认门禁在中央
XLang 范例的 `extractSelected(..., ["docs/hello.txt"])` 处报数组类型错误。
JIT 与独立 EXE 的最小复现见
`../../../dev/extlib_blockers/tar_selected_names_projection.xl`；修复边界
见[共享容器投影记录](../../../dev/extlib_blockers/foreign_var_container_projection.md)。
本轮未签收完整语言/AOT 门禁。下文覆盖旧文件失败非原子性是另一个
独立风险，不能因数组投影修复而视为关闭。

提供内存/文件 TAR 读写、条目元数据、归档追加、安全提取、选择性提取及进度回调。C 使用 `include/xrt_tar.h`，XLang 使用 `import tar`；最小双语范例在 `examples/basic_c.c` 与 `examples/basic.xl`，两者都输出 `hello`。
原生 `xrtTarWriterAdd` 只允许普通文件携带正文；目录、链接和特殊条目的 `iSize` 与 `iSizeBytes` 必须同时为零。否则在写入任何归档字节前返回 `XRT_TAR_ERROR_ARGUMENT`，不会把调用方提供的数据静默丢弃。
`examples/append_c.c` 与 `examples/append.xl` 成对展示文件归档的创建、追加和读回：旧条目、新条目都逐一核对，使用独占临时目录且结束时删除，不覆盖调用方文件。两者都输出 `first`、`second`。
在 `demo6/release` 目录可直接运行 `./xl run extlibs/tar/examples/basic_c.c`；
该单文件 C 范例包含读写所需的三个 TAR 核心源码，正式扩展库仍按清单将核心源码
分别编译。不要在同一范例构建命令中重复加入这些源文件。

## 跨平台权限

`preservePermissions = true` 表示恢复归档中的 POSIX mode。Windows 的 XRT 根文件 API 明确不支持设置 POSIX mode，因此提取会在创建目标目录或文件**之前**返回 `XRT_TAR_ERROR_UNSUPPORTED`；默认 `false` 仍可正常提取。Linux 使用原生文件系统执行权限回归；WSL 的 `/mnt/*` Windows 挂载盘不适合作为 POSIX mode 验收位置。

提取使用 XRT 锚定根文件接口，拒绝不安全归档路径；是否允许符号链接、硬链接和特殊文件由显式选项控制。`test/test_tar_c.c` 包含路径、上限、追加/恢复、选择、回调和权限边界。
**覆盖提取尚无单文件失败原子性。** 若已有目标文件，归档正文损坏或写入失败可能在返回错误前截断/改写旧文件；不要直接覆盖需要保留的生产数据。Windows/Linux 最小复现及根内原子提交所需接口见 `dev/extlib_blockers/tar_overwrite_failed_entry.md`。锚定根目录防止路径逃逸，但不等于失败时自动回滚。
调用方指定的目标根路径可以经过符号链接，锚点为其解析后的目录；归档条目不能借目标目录内部已有的链接越过该锚点。归档内创建链接仍受显式选项控制。
读写器创建时同时校验 `xrttaroptions` 和内嵌 `xrtresourcelimits` 的版本与结构长度；未知版本或截断的限制结构报告 `XRT_TAR_ERROR_ARGUMENT`，不进入归档处理。
当前结构中的两个保留字段也必须为零；不读取调用方声明长度之外的顶层字段。
TAR 不使用 `iMaxDepth`、`iMaxCompressionRatio` 或 `iFlags`；OptionsInit 将前两项
清零，C 调用方显式设置这三项会得到 `XRT_TAR_ERROR_ARGUMENT`，不会假装限额已生效。
不安全条目路径在 C 侧报告 `XRT_TAR_ERROR_UNSAFE_PATH` / `XERR_PERMISSION`；
XLang 原生调用按当前运行时错误映射抛 `IOError`，并保留具体路径诊断。
硬链接只能指向**本次提取中已写出的普通文件或硬链接**，不能借用目标目录中预先存在的文件；前向与链式链接在目标落盘后解析。后续同名归档条目会撤销尚未落盘的旧硬链接，保持归档顺序。提取不是事务操作，失败时可能留下已写出的文件。
进度回调在普通条目边界返回 `false` 可取消后续提取；带 `FINAL` 的空条目回调仅通知已经完成的结果，返回值不再改变成功状态。
开启 `preservePermissions` 时，后出现的同名条目决定最终权限；目录的 `dir` 与 `dir/` 视为同一目标。旧文件被符号链接替换后，其待恢复的 mode 不会跟随链接改动目标文件；实际设置权限时也不跟随末级链接。
`TarWriter.save(path, "xz")` 写出标准 XZ 流，要求路径以 `.xz` 结尾；
`tar.open(path)` 根据 XZ 文件头识别并读取，不再提供旧 `.lzma` 路径。
读取 gzip/bz2/XZ 归档时，`maxInput` 同时限制压缩文件和解压出的 TAR
字节数，解压器在扩容前执行该上限；显式传 0 时与 TAR Reader 一样使用
XRT 默认的 256 MiB，而非无上限。`maxOutput` 仍限制提取的文件内容总量。
原生文件读取桥 `xtar_read_file(path, 0)` 遵循同一默认限额，直接调用
XRT 限额读取；它不会在 0 限额时回退到无上限整文件读取。

XLang `tar.open(Buffer)` 的原生桥会复制输入并持有至 Reader 销毁，允许内联
`tar.open(writer.finish())`，不要求调用方额外保留原 Buffer。XRT 原始
`xrtTarReaderOpenMemory` 仍只借用输入，不改变其 C API 约定。文本条目按
UTF-8 **字节数**写入，内嵌 NUL 的正文仍可保存；进入 C 零终止字符串接口的
路径、条目名、链接目标和用户/组名则拒绝内嵌 NUL。负资源限额在转成
无符号 C 类型前报 `ValueError`；限额 0 使用 XRT 默认值。XLang 条目
写入的 `mode` 限定为 0..4095 的权限位；`uid/gid` 使用 `uint64`，
负数实参在类型检查时拒绝，不再依赖运行时捕获有符号参数的检查。
XRT 的独立 C 结构和读写 API 不变。
`TarReader.extractSelected(destination, names)` 按完整名称精确选择条目；
名称数组经当前 `xvalue` ABI 读取，含 NUL 的名称在提取前抛 `ValueError`，
不会意外匹配 NUL 前缀。空白名单不写出条目。
`TarWriter.save(path, compress)` 先校验压缩格式，再取走并关闭内存归档；
非法格式抛 `ValueError` 时 writer 仍可继续追加或 `finish()`。
原生 C Reader/Writer 可接入自定义流。归档正文提前 EOF 属于
`XRT_TAR_ERROR_FORMAT`；流回调报告内存故障时返回
`XRT_TAR_ERROR_MEMORY`，其他读写或刷新故障返回
`XRT_TAR_ERROR_IO`，不把底层 I/O 故障伪装成归档格式错误。
`TarReader.info()` 逐项接管 C 分配的名称和用户/组元数据，后续字段
读取失败时不会遗留已经取得的原生字符串。
`TarInfo` 是可导出的纯数据类，可在导入方声明 `tar.TarInfo entry =
reader.info()` 或 `array<tar.TarInfo> entries = reader.entries()`，也可用于
函数参数类型。它不持有 Reader 的原生句柄；Reader/Writer 仍须通过
`tar.open` / `tar.create` 工厂取得，不导出其底层构造器。
`test/private_reader_constructor.xl` 是只编译的包外负例，固定了
`TarReader(point,size,...)` 不可从导入方命名和调用的边界；
Windows/Linux 包门禁都会检查“类型未导出”的诊断，不执行伪造指针。
读写器的 `*Native` 成员仅供类内部使用，包外应调用公开方法，由包装层检查参数、计算正文长度并接管原生结果。`test/private_native.xl` 是预期编译失败的可见性负例，不执行其中长度不匹配的写入。

## 验证

### 数值元数据与双语范例

`TarInfo.mtime` 保留 int64 整秒；GNU 二进制时间戳超出这个范围时报
格式错误，不截取低64位。读取的 mode/deviceMajor/deviceMinor 保留
uint32 范围并安全拓宽到 XLang int；超宽字段报格式错误，不回绕。
uid/gid 的 PAX 记录支持完整 uint64，GNU 短字段继续遵循其编码范围。
元数据的表示范围不意味着操作系统允许应用任意时间戳或权限。
读取这些越界归档时，C 为 `XRT_TAR_ERROR_FORMAT`，当前 XLang 映射
为 `IOError`，domain 为 `xlang.extlib.tar`、code 为3。

在 `demo6/release` 运行两份内存范例，均应输出
`mtime=-1;uid=18446744073709551615;gid=9223372036854775808`：

```powershell
./xl run extlibs/tar/examples/metadata.xl
./xl run extlibs/tar/examples/metadata_c.c
```

独立数值门禁从 `demo6` 运行，需要 Python 3；Windows 另需 PATH 中
的 GCC。两套默认都检查 C/JIT/AOT，Windows 的 `-SkipAot` 仅供聚焦
开发，不代表完整专题通过：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/tar/test/check_numeric.ps1
```

```sh
sh release/extlibs/tar/test/check_numeric.sh
```

40组固定归档通过 Python tarfile 的独立编码器与头字段/PAX解码器
产生预期：分别批量运行 C、JIT 和 AOT，避免每个数值反复编译程序。
测试只要求超出本包表示范围的数据被拒绝，不把 Python 的任意精度
整数能力等同于 int64/uint64。GNU 短 ID 的高位用 PAX 样本覆盖；
负 size 只进入头字段解码器，不依赖不同 Python 版本的归档偏移策略。
输入/依赖与发布物指纹、命令和失败现场会保留；成功只清理本次目录。

### 包级门禁

在 `demo6` 目录运行 Windows 门禁：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/tar/test/check.ps1
```

当前只复验包内 C/XLang 路径时可加 `-SkipAot`；默认仍尝试 AOT，
依次构建、运行无输出核心和公开 XLang 范例，后者还要与 C 范例同输出；
共享链接符号缺失时会失败，不将跳过 AOT 视为发布签收。

Linux/WSL 的 C ASan/UBSan 与 XLang 联合门禁：

```sh
sh release/extlibs/tar/test/check.sh
```

该脚本要求可执行的 `release/xl`，运行 XLang 边界用例，逐字比较
C/XLang 最小范例输出，并校验生成原型与 TAR 原生桥接在 LP64 上的 C ABI。
编译和运行产物放在本次独立的系统临时目录并于结束时清理。
`c_ulonglong` / `c_longlong`
必须对应 C 的 `unsigned long long` / `long long`，不能按平台改用 XRT
`uint64` / `int64` 作为桥接导出参数。

独立格式互通测试需要 Python 3，在 `demo6` 目录运行：

```powershell
python release/extlibs/tar/test/interop.py release
```

它让 Python `lzma.FORMAT_XZ` + `tarfile` 读取 XLang 写出的归档，
再让 XLang 读取 Python 写出的归档；测试文件只存在于系统临时目录。
脚本在 Windows 使用 `xl.exe`，在 Linux 使用 `xl`，且 Python 优化模式下
仍会检查条目和正文。两平台包级门禁在安装 Python 3 时自动运行该测试；
没有 Python 3 时明确提示跳过，不将跳过视为格式互通验收。

两个脚本只使用测试目录与临时运行目录；Windows 脚本也运行 C/XLang 最小范例。原生 ABI 中 `point` 对应 `void*`，C `int` getter 在 XLang 侧声明为 `c_int`，避免同一编译单元内的声明冲突与返回值宽度不一致。
Windows 门禁每次使用独占构建目录，成功或失败后只清理本次产物；`-SkipAot` 只用于聚焦 C/JIT 验证，不代表完整 AOT 签收。
2026-10-22：默认门禁原只构建无输出核心；现在还构建、运行公开的
`examples/basic.xl` 并与 C 范例核对，避免核心通过时误签收带输出的语言路径。
`-SkipAot` 聚焦门禁已通过；默认门禁仍先停在核心 AOT 缺
`xl6NativeHandleTraceLeaf`，直接构建公开范例另确认缺
`xl6ValueDataSnapshotV1`。新增门禁不是整包验收通过。
原生桥门禁还使用 XRT 独占临时文件，将其扩展为 256 MiB + 1 字节稀疏文件，验证默认限额在分配前拒绝超限输入；Windows TCC 与 Linux `release/xl` 均运行此用例，成功或失败均清理本次文件。
2026-10-15：该负例先复现了返回完整缓冲而不报错，修复后两平台通过；Windows 包级脚本的其余 C/XLang 用例通过，AOT 构建则停在主线缺符号 `xl6NativeHandleTraceLeaf`。完整回调和 AOT 仍未签收。
Windows 包级门禁默认还会构建并运行 `test/aot_core.xl`，验证无输出、无回调的 TAR 内存往返及标准 XZ 压缩/解压。历史裁剪基线曾通过（产物 1,839,104 字节，补清单前为 4,518,912 字节）；当前构建再次实测在主线共享符号 `xl6NativeHandleTraceLeaf` 处失败，不能沿用旧结论。`tar` 和 `lzma` 的清单均声明各自原生源所需的 XRT 模块。
Windows 门禁还运行 `test/boundaries.xl`，覆盖 UTF-8 文本、临时 Buffer
生命周期、负限额、内嵌 NUL、条目元数据范围拒绝、无效压缩格式后
writer 保持可用、选择白名单的正常提取和 NUL 前缀拒绝，以及无压缩文件保存往返；原生语言桥另经
Windows GCC `-Wall -Wextra -Werror` 语法检查同时覆盖独立源码与 manifest 传入 XRT 模块宏两种构建方式，避免桥接源码重复定义宏。
`test/named_type.xl` 单独验证导入方的 `tar.TarInfo` 参数、变量和类型化数组。

**尚未发布签收。** `test/test_tar.xl` 的 `extractWhere`/`extractWithProgress` 回调在当前主线被函数值进入 `var` 的语义限制拦截；最小复现见 `demo6/dev/extlib_blockers/function_into_var.xl`。本包不改主线，不把回调用例失败解释成普通归档读写失败；主线修复后须重跑完整 XLang 测试和 AOT。

2026-10-17：Linux 包级脚本改为必须同时完成 C ASan/UBSan、LP64 桥接
探针、XLang 边界和 C/XLang 最小范例逐字输出对照；两次运行通过。
编译产物从固定共享目录改为本次独立临时目录。完整回调及 AOT 阻断未变。

2026-10-17：内部归档源码原在清单预定义 XRT 模块宏时触发严格 GCC 重定义错误，现条件定义八项宏。Windows 门禁将清单宏检查扩至全部原生源码；严格编译、C/XL 边界和双语范例通过，AOT 核心仍因主线缺少 `xl6NativeHandleTraceLeaf` 失败。Linux ASan/UBSan 与 XLang 对照门禁通过，回调与完整 AOT 均未签收。

2026-10-18：新增读写器嵌套资源限制版本/长度负例，先复现四处错误接受，再在公共选项入口统一拒绝。Windows `-SkipAot`、Linux C ASan/UBSan 与 XLang 双语对照门禁通过；完整回调和 AOT 的既有阻断未变。

2026-10-17：PAX `size/uid/gid/mtime` 数字原先经 C 字符串解析，
`size=2` 后接内嵌 NUL 和隐藏后缀会被误收。现使用 XRT 长度感知的
严格整数解析；`mtime` 接受有数字的小数部分，但当前 `int64` 元数据
只保存向零截取的整秒，不承诺亚秒精度往返。
C 负例先复现，再以合法数字、NUL 后缀、无符号字段的正号及空小数
做正反矩阵；Windows `-SkipAot` 包级门禁和 Linux ASan/UBSan、XLang
双语门禁通过。完整 AOT 仍缺 `xl6NativeHandleTraceLeaf`，未发布签收。
同轮又复现 PAX `path=safe\0suffix` 与未知扩展键内的 NUL 被接受；
现整条 PAX 记录按声明长度拒绝 NUL，文本 helper 也拒绝有效内容中的
NUL（仍允许 GNU 长名称末尾的填充字节）。上述两项负例与既有长名称
正例在 Windows/Linux 包级门禁通过。

2026-10-19：C `TarReader` 从已定位的 Reader 创建时，输入限额原误用
底层 Reader 总长度，提取进度总量也把归档前缀算入。带前缀 Seek 的 C
回归分别先复现误拒绝和进度多报；现预检按剩余长度判断，进度总量
与归档起点一致。只有 `Size` 而没有 `Tell` 的 Reader 继续由流式读取
执行限额，并将进度总量标为未知。Windows `-SkipAot` 与 Linux C
ASan/UBSan、LP64、XLang/双语范例门禁通过；完整回调和 AOT 的共享
主线阻断不变，尚未发布签收。

2026-10-19：C 最小范例原在单独 `xl run` 时缺 `xrtTarWriterDestroy`。
现自包含三个 TAR 读写单元，且只声明其实际需要的 XRT 模块；
Windows 严格 GCC、TCC 直接运行、独立 `xl build`、包运行器与 XLang
同输出，Linux C ASan/UBSan 与 TCC/XLang 同输出门禁通过。
正式包与 C 边界测试仍分离编译，回调和完整 AOT 的主线阻断不变。

2026-10-19：C 选项的顶层和嵌套限额保留字段原被读写器接受；
读、写各两条负例先红后绿，现于公共选项复制后校验，不改变已有结构
长度策略。Windows 严格 C/XLang 与 Linux ASan/UBSan、LP64、双语对照
门禁通过；回调与完整 AOT 的共享阻断不变。

2026-10-19：C 选项原继承无用的深度和压缩倍率默认值；reader/writer 也会
接受显式设置的无效深度、倍率及资源标志。六条读写负例先红后绿，现统一
在选项复制入口拒绝，并将无用默认值置零。Windows 严格 GCC、C/XLang 边界、
双语范例及 Linux ASan/UBSan、LP64 桥接门禁通过；完整 Windows AOT 核心
仍缺共享符号 `xl6NativeHandleTraceLeaf`，回调和发布验收未签收。

2026-10-20：中央 `release/examples/extlibs/tar.xl` 原在尚不存在的固定
`build/tmp/tar_example_selected` 路径上先调用 `dir.delete`，从正常工作
目录运行即抛文件元数据错误。现使用 `dir.temp` 创建独占系统临时目录，
在 `finally` 清理；从 `demo6` 根目录和 `release` 均输出
`tar example: PASS`。Windows `test/check.ps1 -SkipAot` 与 Linux
`test/check.sh` 已纳入该范例的固定输出检查，后者含 C ASan/UBSan。
未修改 TAR 原生实现，回调与完整 AOT 的主线阻断仍未签收。

同日：中央 C 范例 `release/examples/extlibs/tar_c.c` 原对固定
`build/tmp/tar_c_selected` 目录执行递归删除，存在误删用户文件、并发冲突
和工作目录依赖。现使用 XRT 创建独占系统临时目录，并在所有退出路径清理；
归档缓冲区和 Reader 创建失败也不再解引用空指针。从 `demo6` 根目录与
`release` 直接 `xl run` 均输出 `tar C example: PASS`，Windows 聚焦门禁
及 Linux ASan/UBSan 门禁通过，本次运行无临时目录残留。

2026-10-21：包内综合测试 `test/test_tar.xl` 原先启动时递归删除固定的
`build/tmp/tar_xlang_test`；现改用独占临时目录并在 `finally` 清理。
修改前确认 `demo6` 根目录与 `release` 下的固定目标均不存在。
该测试直接运行仍在第 62 行 `extractWhere` 的函数值入 `var` 主线语义处
停止，尚未执行到文件路径；Windows `-SkipAot` 聚焦门禁与 Linux C
ASan/UBSan、XLang 双语门禁通过，但不覆盖被阻断的综合回调测试。
主线修复后需重跑该测试，整包发布级验收仍未完成。

同日：`test/test_tar_c.c` 的多个边界夹具原在调用方当前目录的固定
`build/tmp/tar_*` 路径上递归清理；包级门禁虽有外层隔离，直接通过
C 包运行器执行时仍可能误删已有目录。测试入口现自己创建独占系统
临时目录，进入后运行原有夹具，恢复工作目录后只清理本次目录；
测试宿主仅为此增加 XRT 临时目录模块。Windows `-SkipAot`、Linux
C ASan/UBSan 与 XLang 联合门禁及从仓库根目录直接运行 C 测试均通过，
未留下固定路径或系统临时目录。TAR 实现未变，回调与 AOT 阻断未解除。

同日：自定义 C Reader/Writer 的故障原有分类偏差：读取回调失败被统一
报成归档格式错误，写入及刷新时的内存故障被报成普通 I/O。新增内存与
I/O 回调夹具先复现，再按本次 XRT 底层错误分类；真正提前 EOF 继续报
格式错误。Windows `test/check.ps1 -SkipAot` 通过严格 C、XLang 边界、
双语范例及 Python/XZ 互通；Linux `test/check.sh` 通过 C ASan/UBSan、
XLang 对照和互通。文件提取的 Writer 阶段现有写入、刷新、关闭与
复合失败的 C 注入矩阵，确认后续关闭故障不覆盖首次失败，并始终关闭
Writer。该矩阵不模拟真实文件句柄的操作系统刷新/关闭失败；提取本身
仍非事务操作，完整回调与 AOT 也尚未签收。

2026-10-26：PAX 文本字段原与 GNU 长名称共用尾部换行裁剪，导致合法的
`path=foo\n` 被静默改为 `foo`，提取时绕过控制字符路径拒绝并写到
另一名称。现按 PAX 记录长度保留 `path/linkpath/uname/gname` 的完整值，
只对 GNU 长名称保留原有终止符裁剪。新增同一归档夹具覆盖四个 PAX
字段、实际提取拒绝及 GNU 名称。Windows 严格 C 与 Linux C ASan/UBSan
回归通过；两平台整包语言门禁仍在既有 `array<string>` 投影错误处停止，
不能据此签收完整提取或 AOT。
