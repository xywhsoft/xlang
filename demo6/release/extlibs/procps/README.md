# procps

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

定向验证（在 `demo6` 目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/procps/test/check.ps1
release/xl.exe run release/extlibs/procps/examples/procps_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package procps -Source release/extlibs/procps/test/test_procps.c
release/xl.exe run release/extlibs/procps/test/boundaries.xl
release/xl.exe run test/cases/extlib_procps.xl
release/xl.exe run release/examples/extlibs/procps.xl
wsl bash release/extlibs/procps/test/check.sh
```

Windows 默认包门禁在 C/JIT 之后构建并运行中央无输出用例与公开 XLang
范例的 AOT 产物，后者逐行对照固定事实输出；`-SkipAot` 仅运行聚焦
C/JIT 检查。AOT 文件放在独占系统临时目录，退出时只清理本次产物。
共享主线符号缺失时默认门禁必须非零退出，聚焦模式通过不代表发布签收。

Windows 独立 GCC、Linux GCC + ASan/UBSan、XL JIT 和 C/XL 范例已通过。
Windows 包级脚本还逐一核对包运行器 C、单文件 C 与 XLang 范例的固定事实输出；单文件 C 范例只在 TCC 路径包含原生实现，正式包仍按清单分离编译。
Linux 包级脚本运行包内及中央 XLang 用例，并逐字比较 C/XL 范例输出；
中央用例的旧 `listPids()` 调用已于 2026-10-18 修正，详见
`demo6/dev/extlib_blockers/procps_catalog_contract.md`。公开接口为
`pids()` 的 `array<int>`，不提供字符串兼容别名。
2026-10-16 将包内 XLang `test/boundaries.xl` 登记到 `coverage.tests`；
从 `demo6` 工作目录单独运行通过，本轮未重跑 C/Linux/AOT 门禁。
Linux 门禁还按 manifest 传入 XRT 模块宏做严格 GCC 编译，并运行一个以非 UTF-8 文件名命名的真实进程，验证错误分类。
另以链接器夹具注入跨缓冲的 `/proc/pid/exe` 目标，验证完整名称和重读路径。
双语范例对照可复查 `examples/procps_c.c`；输出使用布尔事实，不把
动态 PID/进程数写死。当前 XL AOT 构建仍缺少共享主线符号
`xl6ValueDataSnapshotV1`，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`；
暂不标记发布验收完成。

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
