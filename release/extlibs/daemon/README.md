# daemon

## 本轮验证（2026-11-02）

三项包内修复：清理首因被覆盖、Windows退出码259被误当存活，以及Linux
打开旧文件后的删除/重建竞争。最后一项同时修复已持有路径被替换后，重复
`writePidFile` 仍误报成功的分支；失败后须显式释放旧句柄，再获取当前路径。
另补POSIX特性声明，使普通 `-std=c11 -Wall -Wextra -Werror` 编译不再
缺少 `kill` 原型，不靠放宽警告或隐藏编译错误。

同一最终夹具的新旧对照：两平台156组清理/写入组合，旧118条断言失败，
新0；Windows真实子进程34检查旧1失败、新0，覆盖8种退出码和调用前后
自身句柄计数；Linux12检查保持0。Linux真实文件身份/内核锁交错与同路径
重入16检查旧3失败、新0；Windows只验常规5检查，不宣称重放了相同竞争。
Windows逐预算6次、5次真实OOM；Linux4次、3次真实OOM，均检查立即恢复
及7项内存指标。首因预置后的禁分配组合不算真实OOM点。

Windows默认55阶段前54符合预期，Linux53阶段前52符合；两者都只在最后
M2顶层对象AOT退役失败。11条实际生成原型与错改返回类型控制、四访问
负例、旧边界及全部新测试的GCC/TCC/独立程序通过；各9份普通AOT与新旧
C/XLang黄金输出匹配。Windows54输入/工具指纹、Linux原生48来源/四工具
及boot保持；Linux C启用ASan/UBSan和泄漏检查。不改主线或XRT。

清理及身份核对共用两个小辅助函数，整份native净增37行；
GCC-O2对象text两平台各增加560字节，data/bss不变。新取锁和同路径重入
增加两个元数据查询，Windows实际OOM扫描也多一个失败点；这是明确成本，
不宣称更快、零开销或最终程序仅增加同样大小。完整命令/输出/输入见仓库
`docs/reports/extlib-daemon-lifecycle-20261102.json`。本包保持M，不签收整包。

## PID 生命周期合同（2026-11-02）

PID 文件创建、释放及析构共用一次退休路径：即使删除/解锁失败，仍继续
关闭文件并清除对象的旧句柄；不对已消耗的句柄重试。公开操作清掉上次
调用的错误；析构保留进入时的业务首因。若没有业务首因，保留第一个清理
错误，不被后续失败覆盖。保存/恢复已有 XRT 错误对象本身不再分配诊断；
POSIX 原生错误首次转换为 XRT 错误仍有其正常构造成本。

首次取锁后，以及重复写入同一已持有路径时，使用 XRT FileStat/PathStat
核对 Device/Identity，拒绝锁定旧 inode 却误认为拥有新路径的情况。
真实查询错误原样保留；身份不可用报 Unsupported，身份不一致报 State。
不自动重开、循环等待或重试，调用方显式处理失败后重新获取。每次核对
增加两个元数据查询；这是正确性代价，不宣称零成本或并发吞吐已经验收。
此校验不防御任意时刻的恶意同用户路径替换，也不把 PID 写入变成原子事务。

Windows `isAlive` 查询进程对象是否仍未结束，不再把数值259直接当成存活；
259可以是已结束进程的真实退出码。权限不足仍保留原有保守结果；查询时点
之后进程可能立即退出。POSIX 行为保持 `kill(pid, 0)`，它不负责回收子进程。

新 `test/cleanup_contract_c.c` 覆盖真实文件与内核锁的清理故障、业务首因、
分配失败与同对象恢复；`test/pid_liveness_c.c` 用实际子进程核对已退出状态；
`test/pid_identity_c.c` 在 Linux 确定性重放打开旧文件后的删除/重建竞争，
也覆盖原路径重入、替换文件不误删和恢复。故障钩子只存在于独立 C 测试中。
两平台门禁保留失败现场和命令/退出码，不再无条件删除诊断材料。

`examples/restart.xl` 与 `examples/restart_c.c` 演示竞争、失败保持原持有状态、
显式交接及业务停止；只操作自己创建的临时目录。命令和固定输出见
[双语范例](examples/README.md)。包不负责后台化或操作系统服务注册，
旧顶层对象 AOT 退役阻断仍须在主线修复，不用局部范例代替整包发布验收。

2026-10-26 当前 Windows/Linux 默认门禁暂不签收发布级：严格 C、
Linux ASan/UBSan、中央/FFI 边界 JIT 与 AOT、C/XLang 公开范例
独立 EXE 均通过，且规范化后的 PID 输出匹配固定黄金文件。
`test/top_level.xl` 的 JIT 正常，独立 EXE 在打印 PASS 后于退出期
报运行时状态错误；见 `dev/extlib_blockers/global_native_class_aot_fini.md`。
该红项保留在两平台默认门禁，不通过搬移对象规避。

2026-10-25 在 WSL 中运行现有 `test/check.sh` 全通过：严格 GCC、
PID 文件与锁生命周期的 C ASan/UBSan、XLang FFI 边界、四条包外
访问负例，以及归一化 PID 后的 C 分离编译、C 单文件直跑与
XLang 范例输出对照。本次使用现有 Linux `release/xl`，未重建
Linux 发布版或运行独立 EXE。

2026-10-24 当前 Windows 发布版默认 `test/check.ps1` 全通过：
严格 C 与 PID 文件边界、XLang FFI、四条包外访问负例、C/XLang
双语范例，以及公开范例的独立 EXE AOT 构建、运行与归一化
PID 输出对照。下文共享 AOT 缺符号记录属于旧发布物；Linux
验收仍以其独立门禁为准。

跨平台的**前台服务**辅助包：进程 ID/存活查询、协作式停止请求，以及排他 PID 文件。此包不负责 Unix daemonize、Windows Service 注册或关闭其他进程。需要真正的系统服务生命周期时，由宿主/服务管理器负责启动、重启和退出通知。

## XLang

```xlang
import daemon

auto service = daemon.create("web")
string pidPath = path.join(path.temp(), "web-" + string(service.pid) + ".pid")
assert service.writePidFile(pidPath)
assert daemon.readPidFile(pidPath) == service.pid
service.requestStop()
assert service.stopRequested
assert service.releasePidFile()
```

`requestStop()` 只设置本对象的原子状态，不向操作系统发送信号。业务循环可在另一线程检查 `stopRequested` 并停止接收新工作；对象的其他操作仍须由调用方串行化。`writePidFile` 同路径可重复调用；换路径前必须先 `releasePidFile`。对象销毁时尝试释放锁并删除仍属于自己的 PID 文件。缺失文件的 `readPidFile` 返回 `-1`；输入/IO 错误会保留 XRT 错误状态。XLang 的服务名和 PID 文件路径若含内嵌 NUL 则抛 `ValueError`，不会操作截断后的前缀；原生写入方法为私有成员，不能从调用方绕过检查。
PID 文件内容只接受正整数及可选的单个尾随换行；无效内容（包括嵌入 NUL）返回 `-1`，不会把数字前缀当作完整 PID。
读取成功还要求底层文件关闭成功；读取失败时保留首个 XRT IO 错误。C 边界测试覆盖空文件、零/负 PID、64 位上下界、超长内容及失败后重试，XLang 边界测试覆盖无效内容改正后的再次读取。

## C

公开接口在 `include/daemon_native.h`。`xdaemon_create` 返回的句柄由 `xdaemon_destroy` 释放；`xdaemon_name` 借用对象内部文本。C 与 XLang 使用同一套实现，没有额外包装状态。
中央 C 范例 `release/examples/extlibs/daemon_c.c` 可直接用
`xl run release/examples/extlibs/daemon_c.c` 运行；TCC 单文件路径包含包实现，
普通 GCC 构建仍分别编译并链接原生源码。
XLang 的 `service.name` 是独立的 `string` 值；服务对象离开作用域后，
已取得的名称仍可使用。包内 `test/ffi_boundaries.xl` 固定这一所有权边界。
创建、写入、释放、读取 PID 文件和存活查询会在入口清除上次调用遗留的 XRT 错误；析构走内部释放路径，不主动清掉待处理的业务错误。失败后可修正参数并重试。

PID 文件使用 XRT 的 UTF-8 路径、持有期文件对象和安全删除判定。Windows 使用 XRT 字节区间锁，锁定位于数据区之外以允许正常读取 PID；POSIX 使用 `flock`，避免 `fcntl` 进程级锁在同进程读文件关闭时意外释放。锁仅提供协作进程间的排他性；不提供机器重启后的存活保证，也不防御同一用户的恶意路径替换。`isAlive` 只表示查询时可见的进程状态，结果可能立即过时。
Linux C 门禁还验证：已锁定文件改名后，原路径出现的新文件不会被旧对象删除；对象析构执行真实解锁和删除时，不清掉调用方已有的错误。
Linux 包级脚本还运行中央和包内 XLang 用例，并把 C/XLang 范例中不同的进程号
归一化后逐字比较输出；C 范例只启用实际使用的 XRT 模块。

## 包级验证

```powershell
pwsh -NoProfile -File release/extlibs/daemon/test/check.ps1
./release/xl.exe run test/cases/extlib_daemon.xl
./release/xl.exe run release/extlibs/daemon/test/ffi_boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package daemon -Source release/extlibs/daemon/test/test_daemon.c
./release/xl.exe run release/examples/extlibs/daemon.xl
./release/xl.exe run release/examples/extlibs/daemon_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package daemon -Source release/examples/extlibs/daemon_c.c
wsl bash release/extlibs/daemon/test/check.sh
```

`test/check.ps1` 与 `test/check.sh` 还执行包外编译负例：
私有写入、状态读取、释放代理与未导出的原生构造必须被拒绝。
公开的 `pid`、`name`、`stopRequested`、`holdsPidFile`、
`releasePidFile()` 和 `requestStop()` 保持可用。
包内 `test/check.ps1` 还检查 manifest 模块宏下的严格 GCC 编译，并对照
C/XLang 前台服务范例输出（仅将不同进程的 PID 归一化）。
默认还构建运行同一 XLang 范例 AOT，并归一化 PID 后与 C 输出比较；
共享 AOT 阻断期间可用 `-SkipAot` 单独复验 C/JIT，不代表发布签收。

2026-10-17：原生服务名（最多 255 字节）与 PID 路径（最多 4096 字节）
原先在校验上限前用无界 `strlen`。ASan 负例复现 256 字节未终止名称越界；
现两条路径共用有界扫描。255 字节名称可创建，超长名称和路径在文件操作前
拒绝；Windows 包级 C/XLang/双语门禁及 Linux ASan/UBSan 均通过。
公开 C 接口仍要求有效 NUL 结尾字符串；完整 AOT 阻断未变。

2026-10-17：修复读取 PID 文件时嵌入 NUL 可使数字前缀被错误接受的问题；
新增实际文件长度内 NUL、符号前缀和多余换行的 C 负例，并验证正常 PID。
Linux ASan/UBSan 与 Windows C/XLang/双语包级门禁通过；完整 AOT 阻断未变。

2026-10-18：Linux 包级脚本原只执行 C sanitizer；现补中央/包内 XLang 用例
及 C/XLang 范例同输出。严格 GCC 构建发现 C 范例的 `XRT_MODULE_ALL`
与裁剪测试宿主不一致，已改为 PATH_SYSTEM 和 STRING_FORMAT 的直接模块，
测试宿主同步启用 PATH_SYSTEM。Windows 与 Linux C/XLang 联合门禁通过；
带输出 AOT 仍受共享主线缺符号阻断。

2026-10-18：服务对象的释放、PID、名称、停止状态和 PID 文件状态原生代理
原可由包外直接调用，现统一设为私有；公开属性、`releasePidFile()` 和
`requestStop()` 不变。四个包外编译负例纳入 Windows/Linux 门禁，C/XLang
范例同输出，Linux C ASan/UBSan 通过。当前完整范例 AOT 重试仍缺
`xl6ValueDataSnapshotV1`，因此未签收完整发布门禁。

同日补 `service.name` 生命周期用例：从局部服务对象返回名称，
对象销毁后仍能读取原字符串。Windows 包级 C/XLang、Linux C ASan/UBSan
与 XLang 双语对照门禁通过；实现无需修改，完整 AOT 阻断不变。

2026-10-20：Windows 单入口原只覆盖 C/JIT，现默认增加完整范例 AOT
构建、运行和 C 输出对照；构建产物使用独占目录并在失败后定向清理。
`-SkipAot` 的严格 C、语言边界、四项访问负例和双语对照通过；默认
在相同检查后因共享 `xl6ValueDataSnapshotV1` 缺符号于范例构建处退出，
尚未执行 AOT 输出对照。脚本按真实退出码报告原始 TCC 诊断；包实现
未改，整包不签收发布级。
