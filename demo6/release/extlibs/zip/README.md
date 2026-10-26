# zip

2026-10-27 Linux 门禁环境收口：无并行构建时，同一 `text_transfer.xl`
在 WSL 的 Windows 挂载盘约 131 秒，在 WSL 原生 `/tmp` 副本约 9 秒。
`test/check.sh` 仍从包清单编译 C 并执行原有边界/互通测试，仅将 XLang
JIT/AOT 放到一次性原生文件系统副本；完整 Linux 门禁现退出 0，
包含 Python 互通、C ASan/UBSan、双语 AOT 和符号链接边界。正式包与
依赖清单没有改动，独立运行的包外私有构造器及并发提取安全阻断仍在，
整包尚不签收发布级。下方同日的 180 秒超时是本次调整前的记录。

2026-10-27：`ZipArchive.readText(name)` 现返回带长度的 `cstring`，
直接接管原生读取缓冲的 XRT 分配；内嵌 NUL 不截断，关闭归档后结果仍独立有效。
读取文本要求有效 UTF-8，无效字节抛 `ArchiveError`；任意二进制继续使用
`read(name)` 的 `Buffer`。为 `cstring` 写入尾部终止字节时，如果读取缓冲
没有富余容量，XRT 可能扩容，因此不承诺每次物理地址都不移动。
新增 C/XLang 文本所有权测试，XLang JIT 与独立 EXE 已通过；包外私有
构造器及并发提取路径的共享阻断仍未解除，不据此签收整包发布级。
Windows 全包门禁通过。Linux C ASan/UBSan、格式互通及新增 XLang
JIT/独立 EXE 用例通过；随后旧 `boundaries.xl` 在 180 秒阶段限时内
超时，故本次 Linux 全包门禁未完成，不能按完整跨平台验收报告。
2026-10-27 进一步定位：Linux `check`、IR、cgen 很快完成，但 `run`
和 `build` 在后续阶段超时；较小的文本用例也受影响。当前只确认
阶段边界，未证明根因，详见 `../../../dev/extlib_blockers/zip_linux_tcc_stage_timeout.md`。

2026-10-26：当前 Windows/Linux 发布物的功能门禁均退出 0。
两平台覆盖 C 边界、语言 JIT/AOT、私有成员负例及公开 C/XLang 范例；
Python `zipfile` 与 C 接口双向验证 UTF-8、DEFLATE/STORED、
二进制内容和 CRC。Linux 另通过 C ASan/UBSan 与符号链接边界。
C 范例单独 `xl build zip_c.c` 不会自动导入包；混编 AOT 用
`test/c_aot_import.xl` 显式 `import zip` 后与 C 源同编。
Linux 完整门禁的重型编译/运行设有 180/240 秒单阶段上限。
但当前 `xl check test/private_memory_constructor_negative.xl` 仍返回
`ok`，且发布单头尚无 `xrtRootFileRename`、包内未接入根锚定提交；
因此只确认功能门禁，不签收包外构造器与不可信并发目录的安全边界。
下方旧 TCC VFS 头路径失败记录只描述当时发布物。

2026-10-25 独立格式互通：新增 `test/interop.c` / `test/interop.py`，
由 Python 标准库 `zipfile` 读取 C 创建的归档，再由 C 读取 Python 创建的
归档。目录、UTF-8 文件名、DEFLATE/STORED、二进制正文及 CRC 均通过；
篡改 STORED 正文后 C 端拒绝读取。`python -O` 仍执行全部检查。
Windows 默认门禁把这项 C 侧验证放在 XLang 阶段之前。当前新构建的
`xl.exe` 导入扩展包仍被共享 TCC VFS 头路径阻断，本次不据独立互通
结果签收 XLang JIT/AOT；见
`demo6/dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`。

2026-10-25：当前 Windows 发布版 `xl.exe` 的默认 `test/check.ps1`
通过 C/XLang 边界、私有成员拒绝、双语范例及核心、边界、范例 AOT。
原生布尔失败由包内包装层转换为 `ArchiveError`，内存故障仍为
`MemoryError`；`dev/extlib_blockers/zip_error_dispatch.xl` 复验退出码为 0。
这不是最终发布签收：包外私有内存构造器仍可调用，Linux 解压目标目录
仍有并发替换风险。旧版 `cstring` 和 AOT 缺符号诊断仅作历史记录。

基于 XRT ZIP/libzip 的内存及文件归档接口。`import zip` 后可用
`create`、`open(Buffer)`、`open(path, mode)`；`ZipArchive` 提供增删条目、
读取、元数据、解压、关闭和 `finish()`。C API 见 `include/xrt_zip.h`。

`open(Buffer)` 会把输入复制进归档对象，调用方可独立释放原 Buffer。
`finish()` 返回调用方拥有的 Buffer；`read()` 也是独立结果。
原生 `xrtZipArchiveReadTo` / `xrtZipArchiveReadAtTo` 接受调用方 Writer；
写出回调报告内存故障时，归档错误为 `XRT_ZIP_ERROR_MEMORY`，其他
写出故障为 `XRT_ZIP_ERROR_IO`，不会将内存故障伪装为设备 I/O。
文件提取的 Writer 在写入、刷新或关闭时报内存故障时也保留
`XRT_ZIP_ERROR_MEMORY`；若多个阶段失败，以最先失败的阶段为准。
`ZipArchive` 和 `ZipInfo` 已作为包类型导出，可以显式声明
`zip.ZipArchive archive = zip.create()`、`zip.ZipInfo info = archive.info(0)`。
`ZipArchive` 的原生 `*Native` 方法只供类内使用；导入方无法自造
`point` 与长度调用 `addNative`，也无法直接拿走尚未接管的原生结果。
`test/private_native_negative.xl` 只编译以固定访问控制，不执行伪造指针。
内存构造器 `ZipArchive(point,size,...)` 已声明私有，但当前编译器仍
接受包外直接调用；见 `demo6/dev/extlib_blockers/zip_private_memory_constructor.md`。
导入方应使用 `zip.open(Buffer)`，在主线修复前不能签收发布级访问安全。
路径安全检查、条目数、大小、压缩比和解压输出限制由 XRT ZIP 后端执行。
解压时还会拒绝目标目录已有父级、目标根目录本身及其下级中已存在的
符号链接、Windows 重解析点和类型不符的路径分量；根目录的末尾 `/`、
`/.` 写法也会检查。
Linux 的已有链接越界写入有独立回归用例。
当前临时文件提交仍使用路径式 rename；若其他进程在检查后并发替换
目标目录，仍存在 TOCTOU 风险。XRT 源码已新增根目录锚定的
`xrtRootFileRename`，并通过 Windows/Linux 定向测试，但尚未同步到
xlang 主线使用的 XRT 单头文件，ZIP 包也尚未接入该 API。因此对
不可信或可并发修改的目标目录，不应视为安全解压沙箱。
XL 工厂限额必须为正，负索引与不安全路径会报错。
C 配置使用 `xrtZipOptionsInit` 初始化。ZIP 实际使用输入、输出、单项、
条目数和压缩倍率限额；节点数、深度、资源标志及保留字段必须为零。
外层与内嵌限额的尺寸、版本必须精确匹配。显式设置无效配置会在打开
归档前报告参数错误，不会被静默忽略。
条目名、文件路径、注释及解压目标在进入 C 字符串接口前拒绝内嵌 NUL，
不会将 NUL 前缀误作另一条目或路径；条目内容仍可为任意二进制 Buffer。
公开的 `ZipArchive(path, ...)` 原生构造器也通过 XRT `xvalue` 读取字符串
实际长度，拒绝内嵌 NUL 与非字符串值；`zip.open(path)` 的 `string`
参数及原有 C `xzip_archive_open_file` ABI 不变。构造器内部参数使用
`var` 是为了保留字符串长度，实际接受类型仍仅为 `string`。

验证：

- `python release/extlibs/zip/test/interop.py` 独立验证 C/Python 双向 ZIP
  线格式。只需 Python 标准库和现有 `xl.exe` 的 C 运行能力；测试不写文件。
- `powershell -ExecutionPolicy Bypass -File release/extlibs/zip/test/check.ps1 -SkipAot`
  验证包侧 C/XLang 和双语范例固定输出；去掉 `-SkipAot` 后依次
  构建/运行核心、边界及带输出的 ZIP 范例 AOT，并核对输出。
  门禁 AOT 产物使用本次独占的 `build/test` 子目录，退出时清理，
  不复用或覆盖旧的 `zip_aot_core.exe`。
- `xl run examples/extlibs/zip.xl`
- `powershell -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zip -Source release/examples/extlibs/zip_c.c`
- 从 `demo6` 目录运行 `xl run release/extlibs/zip/test/boundaries.xl`；
  该用例使用独立临时目录，已登记到统一扩展库 smoke。
- Linux：`sh extlibs/zip/test/check_symlink.sh`
- `powershell -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zip -Source release/extlibs/zip/test/test_zip.c`
- `zip_native.c` 在 Windows/Linux GCC 的 `-Wall -Wextra -Werror` 语法检查通过。
- Linux 从 `demo6` 目录运行 `bash release/extlibs/zip/test/check.sh`；需要
  GCC、Python 3 和 Linux 版 `release/xl`。脚本按 JSON 清单编译三层 C 源，
  对包自有源码执行严格 GCC 检查，以 ASan/UBSan 跑完整 C 边界和 C 范例，
  再运行 XLang 边界、双语同输出及符号链接专项。产物置于独占系统临时目录。

2026-10-21：C 边界测试原从调用方当前目录递归删除固定的
`build/tmp/zip_*` 夹具目录；现在测试自身创建独占系统临时目录，进入后
运行原有相对路径夹具，恢复工作目录再只清理本次目录。Windows
`test/check.ps1 -SkipAot` 的 C/XLang 边界与双语范例通过。随后新增 Linux
整包聚焦门禁，C ASan/UBSan 边界、严格 GCC、C/XLang 范例固定输出和
符号链接专项均通过；AOT 仍未运行。

C 范例依赖 ZIP 封装、`_libzip` 和其传递依赖，必须由包运行器按清单
一起编译；单独执行 `xl run release/examples/extlibs/zip_c.c` 不会自动
导入这些 C 实现。上面的包级 C 命令是当前正式入口。

包级门禁还按 `_libzip` manifest 传入 `XRT_MODULE_FILE_TEMP` 做严格 GCC
编译检查，并构建运行纯内存 `test/aot_core.xl`。在同一输入下，
补齐 `zip`、`_libzip` 的实际 XRT 模块及 `_zlib` 的空模块清单后，
此前 Windows AOT 从 4,039,680 缩至 1,175,040 字节；这不是完整 ZIP
文件解压路径的 AOT 验收。2026-10-20 的构建曾被
`xl6NativeHandleTraceLeaf` / `xl6ValueDataSnapshotV1` 缺符号阻断，
2026-10-25 的发布版已通过默认包门禁的三个 AOT 目标。
2026-10-16 的危险路径曾被原生 `IOError` 抢先抛出；当前包使用
`@no_raise` 让包装层统一转换，并对内存故障单独保留 `MemoryError`。
包内部 XRT
模块宏改为条件定义，避免导入生成 C 时与 manifest 预定义重复。

2026-10-18：Linux 最小用例先复现解压目标根目录本身是符号链接时，
文件被写到链接指向的外部目录；随后确认末尾 `/` 会绕过简单的不跟随
链接查询。现对根目录检查用的路径副本去掉末尾分隔符与 `/.`，拒绝
`link`、`link/`、`link/.`、`link/./` 四种形式；外部目录保持空白。
Windows 严格 GCC 语法、C/XLang 包级 `-SkipAot` 与 Linux 严格 GCC
语法、链接负例通过。**目录分量被并发替换的 TOCTOU 风险仍在**，
此次修复不构成不可信解压目标的沙箱签收。

2026-10-21：C 自定义 Writer 的 `XERR_MEMORY` 故障原被流式读取固定
报为 ZIP I/O；注入内存与普通 I/O 的双分支 C 测试先红后绿。现从本次
底层错误区分 `MEMORY` / `IO`，并统一用于读取到 Buffer 的关闭失败
路径。Windows `test/check.ps1 -SkipAot` 的严格 C/XLang 边界及双语范例，
Linux `test/check.sh` 的 C ASan/UBSan、XLang 对照及符号链接专项通过。
文件提取的 Writer 阶段现有写入、刷新、关闭及复合失败的 C 注入矩阵；
不会让关闭故障覆盖之前的写入/刷新错误。该矩阵使用包内 Writer helper，
尚未对真实文件句柄注入操作系统刷新/关闭失败，因此不能据此宣称磁盘
事务清理已穷尽验证。该段记录中的完整 AOT 和异常分派阻断已由
2026-10-25 Windows 门禁解除；私有构造器及解压目录并发安全仍未解决。

同日：Linux 负例先复现目标目录的父级为符号链接时仍向链接外写入；
解压入口现于创建目录前逐级检查已有父路径，拒绝链接及非目录。
Windows `-SkipAot` C/XLang 与双语门禁、Linux C ASan/UBSan、XLang 和
符号链接专项均通过。此检查不锚定文件描述符，不能阻止检查后被并发
替换的目录；私有构造器仍未签收。异常分派与 AOT 的当时阻断现已解除。
