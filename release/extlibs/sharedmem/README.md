# sharedmem

> 2026-10-24 当前 Windows 发布版复验：C 跨进程/分配、XLang 边界、
> 私有成员负例、双语 JIT 与核心 AOT 通过。默认门禁新增公开范例
> AOT 对照后，范例先输出正确结果，再于退出时报
> `RuntimeError: operation is not valid in the current state`。
> `test/global_teardown_repro.xl` JIT 通过、独立 EXE 失败；
> `test/local_teardown_control.xl` 两条路径均通过，属已记录的
> [顶层原生类退役问题](../../../dev/extlib_blockers/global_native_class_aot_fini.md)。
> 另一个独立阻断是包外仍可调用 `private extern create(point)`：
> `xl check test/private_constructor_negative.xl` 返回 `ok`。
> 默认门禁现将这条只编译负例纳入最终签收检查，绝不执行它。
> 本包不能据此前较窄的门禁通过记录签收发布级。

`sharedmem` 提供 Windows/Linux 的命名共享内存。它保存原始字节，不隐式
序列化对象，也不提供进程间锁。适合事先约定偏移、长度和同步方式的进程。

```xlang
import sharedmem

auto owner = sharedmem.create("my_exchange", 1024)
auto peer = sharedmem.open("my_exchange")
owner.writeText(0, "hello")
println(peer.readText(0, 5))
peer.writeInt(16, 42)
assert owner.readInt(16) == 42
```

`SharedMemory` 提供 `name`、`size`、`write(offset, Buffer)`、
`read(offset, length) as Buffer`、`writeText`、`readText`、
`writeInt`、`readInt` 和 `zero()`。`read` 返回独立副本；`writeInt/readInt`
按固定小端序读写有符号 64 位整数。文本按 UTF-8 字节写入；
`readText` 的长度也是**字节数**，不是字符数，也没有自动尾零协议。
它返回独立的 `cstring` 快照：原生层只复制一次并验证 UTF-8，语言层
直接接管这块内存。合法 UTF-8 中的 NUL 会按显式长度保留；任意二进制
数据仍请用 `Buffer`。C 的 `xshm_read_text` 返回值由调用方用 `xrtFree`
释放，读取长度由调用方保留，尾部另有一个 NUL 方便常规 C 字符串使用。

名称是 1 至 128 字节的有效 UTF-8，不能含 NUL、斜线、反斜线或控制字符。
XLang 的 `create/open` 在进入 C ABI 前拒绝内嵌 NUL，不会意外访问前缀名称。
原生 C 名称校验最多探测 129 字节，超长名称在访问共享内存前拒绝；
C 接口仍要求有效的 NUL 结尾字符串。
大小必须在 1 至 64 MiB 之间。创建采用独占名称，重复创建失败；
`open` 不会静默创建。Windows 使用当前会话的 `Local\xlang.` 命名空间，
最后一个句柄关闭时名称消失。POSIX 使用权限 `0600` 的 `/xlang.` 名称；
创建者关闭时 unlink，已打开的句柄仍可使用，但不能再通过名称打开。
C API 的 `xshm_unlink` 可用于 POSIX 显式移除名称；Windows 不支持该操作。
语言侧重复 `create` 的原生 `XERR_EXISTS` 映射为 `ValueError`，打开不存在的名称时 `XERR_NOT_FOUND` 映射为 `LookupError`；无效大小也属于 `ValueError`。XLang 的 extern 调用会先传播原生错误，不能只根据包装函数中的备用 `throw` 分支推断异常类型。C 调用方则直接检查 `xrtErrorKind(xrtGetError())`。
显式移除后可以同名重建；旧创建者关闭时会核对共享段身份，不会误删
已经重建的名称。名称的并发移除与重建仍需由应用自行同步，身份核对
不是跨进程的原子改名或锁。
异常退出可能在 POSIX 留下名称，可由进程或运维按需清理。

不同进程可同时访问同一映射，但本库**不保证并发读写原子性或顺序**。
应用必须自行选择同步原语和数据协议。头文件为
`include/sharedmem_native.h`，C 调用方用 `xshm_close` 释放句柄、
`xrtBufferDestroy` 释放 `xshm_read` 返回的缓冲。XLang 的
`SharedMemory` 管理句柄生命周期。
原生 `*Native` 方法只供类内部调用，公开读写接口负责检查参数并接管
返回缓冲。`xl check extlibs/sharedmem/test/private_native_negative.xl`
应报告访问错误。原生 `create(point)` 虽标记为私有，目前主线仍允许
导入方调用；`private_constructor_negative.xl` 仅用于编译检查，绝不可运行。
详见 `demo6/dev/extlib_blockers/sharedmem_private_native_constructor.md`，
此问题修复前本包不签收完整发布级验收。
公开 C 查询、读写、清零及 unlink 操作在调用开始清除本线程旧错误；失败时
可由 `xrtGetError()` 读取本次诊断。析构操作不清除正在传播的错误。
`xshm_size(NULL)` 和 `xshm_name_of(NULL)` 分别返回 0 与空串，并设置
`XERR_ARGUMENT`；不要把这两个哨兵当成有效共享段信息。

2026-10-17：补齐这两个 C 查询入口的空句柄诊断，并验证失败后立即查询
有效句柄会清除旧错误；Windows C 边界和 Linux ASan/UBSan、XLang、
双语对照门禁通过。带输出 AOT 的共享缺符号阻断不变。

定向验证（在仓库根目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/sharedmem/test/check.ps1 -SkipAot
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package sharedmem -Source release/extlibs/sharedmem/test/test_sharedmem.c
release/xl.exe run release/extlibs/sharedmem/examples/sharedmem_c.c
release/xl.exe run release/extlibs/sharedmem/test/boundaries.xl
release/xl.exe run test/cases/extlib_sharedmem.xl
release/xl.exe run release/examples/extlibs/sharedmem.xl
bash release/extlibs/sharedmem/test/check.sh
```

2026-10-17：原生校验原在 128 字节上限前使用无界 `strlen`；
129 字节未终止输入的 ASan 用例先复现越界读取，现改为限界探测。
128 字节名称可实际创建、打开；129 字节名称在创建、打开和 unlink
入口均报参数错误。Windows C 边界与 Linux ASan/UBSan、跨进程、
XLang/双语包级门禁通过；带输出 AOT 的共享阻断不变。

Windows 严格 GCC 独立程序验证真实子进程访问；Linux GCC + ASan/UBSan
验证 `fork` 子进程、边界、释放与同名重建。Linux 脚本还会执行中央及包内
XLang 用例，并逐字比较 C/XLang 范例输出；可在 WSL 的仓库目录内运行。
独立 C 分配故障用例扫描 `create/open/read` 的前 8 个分配位置，检查内存错误分类、创建失败后名称不可打开、读结果与原始字节不变、解除故障后的恢复及 XRT 活跃内存回归；两平台包级门禁均执行，Linux 同时启用 ASan/UBSan。
Windows 门禁每次独立构建 XRT 与原生测试对象，只清理本次独占目录；
不会复用 `build/test/sharedmem_guard` 中的历史文件。
C 范例在 `examples/sharedmem_c.c`，两个范例均使用独立随机名称，避免
并行运行和异常退出后的名称冲突。
旧发布物的中央无输出核心 AOT 曾构建运行；当前 `release/xl.exe` 默认
门禁在同一核心用例首先缺 `xl6NativeHandleTraceLeaf`，最小复现见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`。带输出范例还可能
遇到 `xl6ValueDataSnapshotV1`；主线修复后须重跑默认门禁和完整范例，
不能沿用旧 AOT 结果签收发布级。

2026-10-17：Linux 门禁加入中央与包内 XLang 用例、C/XLang 双语范例
输出对照；严格 GCC + ASan/UBSan 与全部语言入口通过。范例改用独立
随机名称，不再共享固定命名映射。AOT 阻断不变。

2026-10-16：包内与中央 XLang 用例改用独立随机名称，避免并行测试或异常
退出后的同名资源干扰；包内用例连续两次、中央用例一次从仓库根目录
工作目录运行通过，包内用例登记到 `coverage.tests`。

同日 C 子进程用例的共享段名称加入 XRT 安全随机后缀，中文名称使用同一
独立前缀；Windows TCC C 用例与 Linux 严格 GCC、ASan/UBSan 门禁通过。
仍未重跑带输出 AOT。

2026-10-15：修复 `create/open` 的名称 NUL 前缀误访问和成功读写残留旧
XRT 错误，C/XL 负例先红后绿。中央旧 API 用例已迁移为当前公开接口。
清单声明 buffer/Unicode 依赖后，同一核心 AOT 从 3,654,144 降至
675,328 字节，构建运行通过；C/XL 边界、双语同输出范例及包内 Linux
ASan/UBSan 门禁复跑通过。完整范例 AOT 阻断不变。

2026-10-19：C 范例现在可直接以
`release/xl.exe run release/extlibs/sharedmem/examples/sharedmem_c.c` 执行；
仅 TCC 单文件路径包含包实现，正式包与 GCC 仍独立编译。
新增 Windows `test/check.ps1`：独立 GCC 子进程验证、中央/包内 XLang
边界、原生私有入口负例，以及 GCC/单文件 C/XLang 三路径同输出；
`-SkipAot` 门禁通过。Linux C ASan/UBSan、XLang 与三路径对照也通过。
默认 AOT 核心用例在当前发布物首先缺 `xl6NativeHandleTraceLeaf`，
且私有原生构造器的主线缺陷仍在；不签收整包发布级。

2026-10-23：XLang 对 `writeText/create/open/read/readText` 各扫描前 32 个
分配点，核对 `MemoryError` 分类、失败写入不改原字节、创建失败后名称
不可重新打开，以及解除故障后的恢复。`writeText` 在本地 Buffer 追加
直接返回失败的备用分支也统一抛 `MemoryError`，不再误报普通运行时错误。
Windows `-SkipAot` 的跨进程 C、分配故障、语言边界与双语门禁，以及
Linux C ASan/UBSan、XLang 和双语门禁均通过。私有构造器访问控制及
完整 AOT 的既有主线阻断仍未解除，整包不据此签收发布级。

2026-10-25：`readText` 改为原生层单次复制、在独立快照上验证 UTF-8，
再由 `cstring.take` 接管；C 和 XLang 均保留明确字节长度。新增无效 UTF-8、
内嵌 NUL、空文本、越界、快照独立性与分配故障检查。Windows 严格 C
跨进程/分配门禁、XLang 边界与分配用例、双语范例 JIT、核心 AOT 均通过。
完整包脚本从仓库根目录运行时仍受主线 VFS 头文件查找问题阻断；从
`release` 工作目录的定向 JIT/AOT 则通过。私有构造器与全局原生类退役
问题仍使整包不能签收完整发布级。
