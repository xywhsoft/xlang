# procps

2026-10-29 名称字节与诊断复核：修复 Linux `comm` 回退截断合法 CR/LF、
并因前缀截断漏掉后续非法 UTF-8/NUL 的问题；现在按真实读取长度验证，
只移除内核最后附加的一个 LF，名称内部与名称末尾的 CR/LF 均保留。
Windows 删除包内 WinAPI 测长/分配/转换代码，直接复用 XRT 的严格
`xrtUtf16To8`，非法 UTF-16 明确给出 `XERR_VALUE`，不再与正常不可用混淆。
没有新增 API 或改变所有权；正常不可用仍为 C NULL、语言 ValueError。

受控 Linux 23 检查由十失败归零，并用当前进程真实内核 comm 验证
CR/LF 完整保留；Windows 11 检查由三诊断失败归零，含代理对/单个
代理项、最长快照名、一次分配预算、关闭与内存平衡。最终 Windows
默认35阶段和Linux原生快照门禁通过，包含七项实际生成原型与C头、
旧长路径/statm/OOM、完整返回无副本、双语六行黄金输出、Windows六条/
Linux五条独立程序。范围限当前冻结 x64 发布物，不外推其他架构。

2026-10-28 完整返回链复查：`name` 的可空局部返回仍复制已接管正文。
不能直接改成非空声明，因为 C 的 NULL 也表示正常不可读取/进程消失。
现保留 `@owned string?`，使用调用临时值 `??` 仅在缺失时调用原有
ValueError 抛错函数；成功结果直接转移，不增加 C 适配器或改变 C ABI。
完整 cgen 夹具从一处返回复制变为零，原生实现未改。

C/XL 在当前短可执行路径各用 0/1 两组预算，语言侧先红后绿：0 保持
内存错误，1 即成功。三种无效 PID 的 C NULL/无错误与 XL ValueError
及随后的恢复均通过。该预算不外推 Linux 超长路径回退等内部多次
分配情形；长度扫描、系统查询及错误作用域仍存在，不宣称整个桥零开销。

Windows/Linux 默认完整门禁通过严格 GCC/TCC C、Linux ASan/UBSan、
名称回退/长路径/畸形 statm/非法 UTF-8、中央/边界/新预算的语言 JIT/AOT
与双语固定输出。Windows 新增边界 AOT，不再仅验证核心和公开范例；
两平台校验输入指纹，失败保留现场，文本黄金只统一行尾。测试运行库
沿用产品模块并加内存调试，主线/XRT 未改。

2026-10-27：`name(pid)` 直接声明公开的 `xprocps_name` 为
`@owned string?`，删除只转发指针的 `xprocps_name_abi` 和语言侧
`point/takeCstr` 交接。进程名不可用仍按原合同报错；生成 C 直接
保存原生结果指针，不复制正文，但仍扫描一次以取得字节长度。
Windows 默认门禁通过严格 C、语言边界和双语 AOT；Linux 门禁通过
C ASan/UBSan、异常 `/proc` 数据、非法 UTF-8 名称和双语 AOT。
WSL 挂载盘下中央 JIT 曾触及旧 90 秒限额；单独运行实测约 57 秒，
仅将 Linux 脚本的语言编译限额提高到有界的 180 秒后完整复验通过。

2026-10-26 重建 Windows/Linux x64 发布物后，两平台默认包门禁
均退出 0：严格 C/ABI、Linux ASan/UBSan、异常 `/proc` 数据与
非法 UTF-8 名称、C/XLang 固定输出、中央/边界独立 EXE，以及
C/XLang 单文件范例独立 AOT。旧共享快照缺符号不再复现。
这只签收包级短时可见进程查询合同；不保证进程在跨调用期间
保持存在、具备读取权限，或其他平台/架构的行为。

2026-10-25 在 WSL 中运行现有 `test/check.sh` 全通过：严格 GCC、
C 边界 ASan/UBSan、长可执行路径、畸形 `/proc` 数据、非法 UTF-8
进程名，以及 XLang 边界与 C/XLang 固定输出对照。本次语言用例
使用现有 Linux `release/xl`；没有重建 Linux 发布版或运行其
独立 EXE。

2026-10-24 当前 Windows 发布版默认 `test/check.ps1` 全通过：
公开 C 头与清单宏的严格 GCC 检查、C/XLang 边界、C 单文件
与包运行器范例、XLang 范例固定输出，以及核心和公开范例的
独立 EXE AOT 构建、执行及输出核对。下文共享 AOT 缺符号
记录属于旧发布物；Linux 仍按其独立门禁验收。

`procps` 查询操作系统现有进程，而 XRT 的 `process` 模块负责创建和管理
本程序启动的子进程；两者用途不同。支持 Windows 和 Linux。

```xlang
import procps

int selfId = procps.pid()
array<int> ids = procps.pids()
println(procps.name(selfId))
println(procps.memKb(selfId))
```

| API | 结果 |
| --- | --- |
| `pid()`、`ppid()` | 当前进程及其父进程 ID |
| `exists(pid)` | 目标 PID 当前是否存在；`pid <= 0` 为 `false` |
| `name(pid)` | UTF-8 可执行文件名；不可访问或已退出时抛 `ValueError` |
| `pids()` | `array<int>`，完整进程快照；不保证顺序 |
| `count()` | 调用时刻的进程数量 |
| `memKb(pid)` | 常驻内存（Windows Working Set、Linux RSS），单位 KiB；进程退出或不可读取时为 `-1` |

进程可能在枚举与后续查询之间退出，因此 `count()` 不保证等于先前
`pids().length`，`exists(pid)` 为真也不保证下一次调用仍可读取数据。
Windows 使用系统进程快照，不扫描固定 PID 范围；Linux 从 `/proc`
枚举完整数字目录。Windows 名称从 Unicode 进程快照转换为 UTF-8；
Linux 优先读取 `/proc/pid/exe`，无可执行路径时回退 `comm`（内核可能
截短其名称）。`exe` 路径超过栈缓冲时用 XRT 按需扩容并重读，不将长路径
误判为读取失败；两条路径都校验 UTF-8。若系统文件名包含非法 UTF-8 字节，
`name()` 抛 `ValueError`，C 接口返回 `NULL` 并设置 `XERR_VALUE`，不返回
非法字符串。错误和权限不会由静态缓冲或固定文本长度静默截断。
`comm` 不是按行拆分的文本列表：保留真实名称中的换行/回车，仅去掉
内核附加的最终 LF；其原始输出方式见 Linux
[comm_show](https://github.com/torvalds/linux/blob/v6.12/fs/proc/base.c#L1641)
和 [proc_task_name](https://github.com/torvalds/linux/blob/v6.12/fs/proc/array.c#L96)。
完整原始名称含 NUL 或非法 UTF-8 时拒绝，不只校验有效前缀。Windows
同样严格拒绝不完整 UTF-16，沿 XRT 保留 Unicode 错误位置与类别；
Windows 转换失败规则可参阅 [WideCharToMultiByte](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)。
进程已退出或名称不可读时，C 接口返回 `NULL` 且不留文件错误，XLang
包装抛 `ValueError`；真正的内存失败仍保留 `XERR_MEMORY` 并抛 `MemoryError`。

Linux 的 `/proc/<pid>/statm` 前两列须为完整的非负十进制整数；负数、
数字尾部杂项或文件中的内嵌 NUL 均视为不可读取，`memKb` 返回 `-1`，
不把原始字节当作有效 RSS，也不把探测失败变成 XLang 异常。
读取期间进程退出或文件不可访问同样返回 `-1`，且 C 错误槽不残留文件错误；
内存分配失败仍保留 `XERR_MEMORY`，由 XLang 抛出 `MemoryError`。

C 头文件为 `include/procps_native.h`。`xprocps_pids` 返回调用方持有的
XRT `xvalue*` 数组，使用 `xrtValueRelease`；`xprocps_name` 返回独立
UTF-8 字符串，使用 `xrtFree`。结果没有共享全局缓冲。公开 C 入口
在开始时清理上次调用残留的 XRT 错误；失败路径若产生诊断，调用方可读取本次错误。
公开头可独立包含；应用要遍历 PID 数组时自行启用 XRT value container 模块。
名字可能含 CR/LF 等控制字符，不应未经转义直接拼入日志的一行。
父进程 ID 可以为 0（如 Linux PID 1）；`count` 与 `pids` 是分别查询，
不能要求它们在进程变化期间严格相等。Windows 快照可能列出 PID 0
空闲项，但 `exists/name/memKb` 对非正 PID 仍按不可访问规则处理。

定向验证（在仓库根目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/procps/test/check.ps1
release/xl.exe run release/extlibs/procps/examples/procps_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package procps -Source release/extlibs/procps/test/test_procps.c
release/xl.exe run release/extlibs/procps/test/boundaries.xl
release/xl.exe run test/cases/extlib_procps.xl
release/xl.exe run release/examples/extlibs/procps.xl
wsl bash release/extlibs/procps/test/check.sh
```

Windows/Linux 默认包门禁在 C/JIT 之后构建并运行中央、边界、新结果
预算、公开 XLang 范例和 C 单文件范例的 AOT 产物，双语范例逐行对照
固定事实输出。`test/owned_results.xl` 登记到 coverage，
`test/owned_probe.xl` 独立核对完整可空返回链。
`-SkipAot` 仅运行 Windows 聚焦 C/JIT 检查，不代表发布签收。
Windows 产物在 build/test 的独占目录，Linux 在独占系统临时目录；
成功时只清理本次产物，失败时保留输入指纹、命令与现场。

当前双语范例补齐 `ppid/count`，七个公开入口均有对应调用；用六行
事实输出比较，不固定动态 PID、进程数或 RSS 数值。Windows 另以
独立 GCC 编译 C 范例，对照 TCC 直跑/包运行器与 XLang，不只比较两条
同后端路径。成功名称的单次分配与直接移交保持，仍有编码测长/转换、
语言侧 strlen、系统查询与错误作用域，不宣称零扫描或全链零开销。

新增 `test/name_windows_c.c` 只在本测试翻译单元替换快照入口，实际
Unicode 转换不替换：验证 UTF-16 正/负例、旧错清除、预算、每次快照
只关闭一次及活动分配。Linux `test/name_fallback.c` 扩至23检查，
包括多行、末尾换行、中文、空名、NUL/非法字节、不可用/内存区分及
直接移交；`test/comm_real_c.c` 则只强制 readlink 回退，comm 内容
来自真实内核，仅修改并恢复本测试进程自身名称，不修改其他进程。

门禁当前逐进程有界，成功/失败都核对完整包/头/运行库/工具输入并
保留报告，成功清理独占产物、失败保留现场。最终 Windows 35阶段、
43指纹变化0；Linux 38源件快照内外一致，含GCC共39项指纹不变。
缺C编译器实测退出127，不伪报通过。Linux首轮新超时包装遇到uutils
拒绝非法UTF-8 argv的测试工具问题；现仅经环境传递原路径字节，由
有界bash exec启动同一个非法文件名程序，真实负例保留，完整复验通过。
没有为换绿灯而将文件改成有效UTF-8、删掉负例或解除超时。

Windows 独立 GCC、Linux GCC + ASan/UBSan、XL JIT 和 C/XL 范例已通过。
Windows 包级脚本还逐一核对包运行器 C、单文件 C 与 XLang 范例的固定事实输出；单文件 C 范例只在 TCC 路径包含原生实现，正式包仍按清单分离编译。
Linux 包级脚本运行包内及中央 XLang 用例，并逐字比较 C/XL 范例输出；
中央用例的旧 `listPids()` 调用已于 2026-10-18 修正，详见
`dev/extlib_blockers/procps_catalog_contract.md`。公开接口为
`pids()` 的 `array<int>`，不提供字符串兼容别名。
2026-10-16 将包内 XLang `test/boundaries.xl` 登记到 `coverage.tests`；
从仓库根目录单独运行通过，本轮未重跑 C/Linux/AOT 门禁。
Linux 门禁还按 manifest 传入 XRT 模块宏做严格 GCC 编译，并运行一个以非 UTF-8 文件名命名的真实进程，验证错误分类。
另以链接器夹具注入跨缓冲的 `/proc/pid/exe` 目标，验证完整名称和重读路径。
双语范例对照可复查 `examples/procps_c.c`；输出使用布尔事实，不把
动态 PID/进程数写死。旧发布物的 XL AOT 曾缺少共享主线符号
`xl6ValueDataSnapshotV1`，见 `dev/extlib_blockers/text_snapshot_link.md`；
2026-10-26 两平台默认包门禁已验证不再复现。

2026-10-17：中央用例的旧 `listPids()` 调用在导入阶段复现失败，按主线
测试边界记录并交其维护者修正，不恢复旧 API。Linux 门禁追加 C/XL
范例逐字输出对照；包内 XLang、严格 GCC、ASan/UBSan 及非法 UTF-8
进程名负例通过。完整 AOT 阻断未变。
2026-10-17 修复公开头在未预先启用 XRT value container 模块时无法独立
包含的问题；Linux 包级门禁固定无模块和显式模块两种严格 C 编译方式。
Windows C 边界、Linux ASan/UBSan、XLang 边界及双语范例通过，AOT 阻断不变。

2026-10-17：链接器夹具先复现 `statm` 第一列为负数却被 `strtoull`
接受、返回正常 RSS。现用 XRT 严格整数解析验证前两列，并按 XRT
读取的实际长度拒绝内嵌 NUL；负数、第二列尾部垃圾和 NUL 负例均返回
`-1` 且不留解析错误。Linux 严格 GCC、C ASan/UBSan、XLang JIT 与
C/XLang 范例对照，Windows C/XLang 边界通过。完整 AOT 重试仍首先缺
`xl6ValueDataSnapshotV1`，未签收发布级。

2026-10-18：中央用例改为验证 `pids()` 的整数快照包含当前 PID，并加入
Linux 包级门禁；Windows 直接运行通过。未恢复旧字符串接口。

2026-10-19：C 教学范例补 TCC 单文件入口，直接 `xl run` 与包运行器路径均通过；新增 Windows `test/check.ps1`，联合严格 GCC 头文件与清单宏检查、C/XLang 边界、三条范例路径的固定进程事实输出。Linux `test/check.sh` 同步固定结果和单文件 C 对照，严格 GCC、ASan/UBSan、非法 UTF-8 进程名与双语门禁通过。完整 AOT 共享缺符号阻断仍在，不签收发布级。

2026-10-23：修复 Linux 查询已退出进程时 `statm` 文件错误被 XLang 自动抛出的情况；现在按公开合同返回 `-1`，仅保留真实内存故障。C 链接器夹具分别验证文件不存在与内存失败，C/XLang 用例验证不存在的 PID。Windows `check.ps1 -SkipAot`、Linux `check.sh`（含 ASan/UBSan）通过；完整 AOT 仍未签收。

同日修复 Linux `name(pid)` 在进程消失后将 `/proc/<pid>/comm` 文件错误直接抛为 `FileError` 的问题。缺失/不可读回到公开的 `ValueError` 语义；注入夹具固定保留 `XERR_MEMORY`，真实不存在 PID 的 C/XLang 负例固定无残留文件错误与异常类型。Windows 聚焦门禁、Linux ASan/UBSan 与双语门禁通过，完整 AOT 未签收。
