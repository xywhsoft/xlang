# singleinstance

`singleinstance` 为当前用户提供非阻塞实例锁。同名锁在另一进程或
同一进程的另一包实例持有时，`acquire(name)` 返回 `false`；输入或 IO
失败会抛异常，不会伪装为“另一实例正在运行”。一个包实例只能持有一个锁，
再次 `acquire` 是状态错误；不同包实例可持有不同名称。
`release()` 幂等；进程异常退出时操作系统会释放锁。

```xlang
import singleinstance

if not singleinstance.acquire("my_app") then
    println("Another instance is already running!")
    return 1
end
// 处理应用工作，然后释放；异常路径可用 finally。
singleinstance.release()
```

名称是 1 至 128 字节有效 UTF-8，不能含控制字符。XLang 入口还会拒绝
内嵌 NUL，避免 `"service\0other"` 静默争用 `"service"` 的锁；C 入口
仍接受普通 NUL 结尾字符串。名称以 XRT Base64URL
编码成文件名，避免路径分隔符和 Unicode 目录问题。锁文件位于当前用户
主目录的 `.xlang/singleinstance/`，新目录权限为 `0700`，新文件权限为
`0600`（Windows 依账户 ACL）。**释放时保留锁文件**，避免 unlink 后
另一个进程重新创建同名文件产生两个锁域；残留文件不代表仍有实例持锁。
已有目录不会被静默更改权限：POSIX 上 `.xlang` 和 `singleinstance`
必须是实际目录，且组/其他用户不可访问；不符合要求时 `acquire`
报权限错误。Windows 同样拒绝目录重解析点，但继承的账户 ACL 仍需
由部署环境管理，本包不声称检查了完整 Windows ACL。
Windows 使用 XRT `xrtFileLock`；POSIX 使用同一个 XRT 文件对象的原生
`flock`，因为 POSIX `fcntl` 锁按进程计，两个 JIT 模块会错误地同时取得
同名锁，关闭其中一个文件描述符还可能释放另一个模块的锁。
两个平台均由系统内核在进程退出时回收锁。
建议在应用主路径中持锁，直到退出，并在卸载脚本上下文前显式 `release`；
`acquire` / `release` 可从不同线程调用，包内用 XRT 的 once/mutex 保护
当前包实例状态。`isAcquired` 只查询当前包实例，不枚举进程里的其他上下文。

C API 在 `include/singleinstance_native.h`：`xsi_acquire` 返回 1=成功、
0=另一包实例持有、-1=错误；`xsi_release` 返回操作是否成功；
`xsi_is_acquired` 查询当前包实例状态。C 范例在
`examples/singleinstance_c.c`，XLang 范例在
`demo6/release/examples/extlibs/singleinstance.xl`。
在 `demo6` 目录可直接执行 `release/xl run release/extlibs/singleinstance/examples/singleinstance_c.c`，
也可用 `release/xl build` 构建该 C 范例，无须手动追加包源码。示例会在当前用户主目录留下锁文件；
包门禁使用隔离的主目录，不触碰日常用户配置。

`release` 和 `isAcquired` 进入时清理同一线程的旧 XRT 错误；成功或合法未持锁状态不会携带先前 `acquire` 的错误。

聚焦验证（在 `demo6` 目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/singleinstance/test/check.ps1 -SkipAot
```

Windows 门禁独立编译严格 GCC 程序，验证 ASCII 与中文 UTF-8 名称的
跨进程互斥、释放后重新获取和持有进程退出后的自动解锁；同时运行中央
和包内 XLang 用例，并逐字比较 C/XLang 范例输出。门禁临时设置
`USERPROFILE`，使保留的锁文件只留在本次测试目录，结束后恢复环境。
现在还将单文件 C 范例的直接运行、直接构建后执行与 GCC/XLang 输出逐字比较。
Linux GCC +
ASan/UBSan 门禁也覆盖这些跨进程场景。`xl run` 的 C 测试只能覆盖当前
进程，因为它运行在 `xl.exe` 内。去掉 `-SkipAot` 可重试完整构建；
当前 XL AOT 构建仍缺少共享
主线符号 `xl6ValueDataSnapshotV1`，详见
`demo6/dev/extlib_blockers/text_snapshot_link.md`；暂不标记发布验收完成。
Windows 深 `USERPROFILE` 配合 128 字节名称还可能在 XRT 文件打开路径
失败，短隔离目录通过；复现及主线修复建议见
`demo6/dev/extlib_blockers/singleinstance_windows_long_home.md`。

Linux 聚焦门禁：`sh release/extlibs/singleinstance/test/check.sh`。
它增加了两个独立静态包实例的同进程互斥及释放隔离检查，
覆盖原 POSIX `fcntl` 错误；所有普通用例运行在临时 HOME，且使用
独立隔离目录检查已有目录的宽权限和符号链接拒绝。门禁还运行中央和
包内 XLang 用例，并逐字比较 C/XLang 范例输出。真实两个
`XL_Context` 同时执行的 DLL 验收
受主线运行边界阻断，最小有界复现与建议见
`demo6/dev/extlib_blockers/singleinstance_context_execution.md`。

2026-10-17：Linux 门禁加入中央 XLang 用例与 C/XLang 范例输出对照，
并将普通锁文件限制在临时 HOME；严格 GCC、ASan/UBSan、双包实例和
双语门禁通过。双上下文及完整 AOT 阻断未变。

同日补 XLang 名称内嵌 NUL 的红绿回归：原来会以截断后的前缀取得
另一名称的锁，现在 `acquire` 在原生调用前抛 `ValueError`。随机锁名
用例、Windows C/严格 GCC、Linux ASan/UBSan、双包实例和双语门禁
通过；完整 AOT 与双上下文阻断不变。

同日修复 C 名称校验先用无界 `strlen`、后检查 128 字节上限的问题：
129 字节未终止输入的 ASan 用例先复现越界读取，现最多探测 129 字节即拒绝。
128 字节合法名称仍可持锁，129 字节名称报参数错误；Windows C 与 Linux
ASan/UBSan、XLang/双语包级门禁通过。C 接口仍要求有效 NUL 结尾字符串。

2026-10-19：新增 Windows 独立 GCC 一键门禁，并在 Windows/Linux 子进程
验证中文 UTF-8 名称的争用、释放后重获；子进程只接收 ASCII 锁名前缀，
自行构造中文名称，避免 Windows C 启动参数编码影响测试结果。Windows
C/XLang/双语门禁和 Linux C ASan/UBSan/双包实例/双语门禁通过；Windows
门禁现隔离 USERPROFILE，避免测试锁文件留在真实主目录。完整 AOT
及真实双 `XL_Context` 阻断未变；深主目录仍需 XRT 收口。

2026-10-16：包内与中央 XLang 用例的锁名改为每次运行独有，避免
并行门禁互相占锁；包内连续两次、中央一次运行通过，包内用例
已登记到 `coverage.tests`。
真实双上下文与完整 AOT 阻断状态不变。

同日 C 用例的中文锁名加入本次进程 PID，避免并行门禁争用固定名称；
Windows TCC C 与 Linux 严格 GCC、ASan/UBSan 及双模块门禁通过。
仍未重跑完整 AOT 或真实双 `XL_Context` 门禁。

2026-10-15：失败后成功的 C 连续调用回归先红后绿，`release/isAcquired` 不再保留旧错误。包清单声明原生文件锁、目录、路径、Base64URL、Unicode、once 和 mutex 的 XRT 模块，核心无输出 AOT 从 3,527,680 降至 597,504 字节并运行通过。Windows C/XLang、双语范例和 Linux ASan/UBSan 的跨进程/双模块门禁复跑通过。完整带输出 AOT 仍缺 `xl6ValueDataSnapshotV1`；真实双 `XL_Context` 仍按上述独立阻断处理，未签收发布级。

2026-10-15：已存在的宽权限锁目录原会被直接使用；隔离 HOME 负例先红后绿。创建锁文件前现在检查 `.xlang` 与 `singleinstance` 两级目录，拒绝符号链接及 POSIX 组/其他用户权限。Linux sanitizer 的正常互斥、宽权限两级目录与链接门禁通过；Windows 完整 ACL 审计仍不在此实现范围内。

2026-10-21：Linux 双包实例测试中的取得、释放和子进程检查原写在 `assert` 表达式里，`-DNDEBUG` 会删除整个操作。测试单元现保持断言生效，包级 C 门禁带 `-DNDEBUG` 运行；预处理确认取得锁调用仍在，ASan/UBSan、双实例、XLang 边界与双语对照通过。只修复测试可信度；深主目录、完整 AOT 和真实双 `XL_Context` 的既有阻断不变。

同日 Windows 门禁也编译两份独立的原生模块实例，验证同进程锁争用、释放后重获及反向争用；测试单元带 `-DNDEBUG` 构建，断言中的操作仍会执行。Windows 聚焦门禁及 Linux ASan/UBSan、双模块门禁均通过。此测试不替代真实双 `XL_Context` 验收，完整 AOT 与深主目录阻断仍在。
