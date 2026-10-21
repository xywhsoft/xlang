# floatEnv

`floatEnv` 操作**当前 OS 线程**的浮点环境：舍入模式、IEEE 异常标志和状态快照。它不改变其他线程，也不控制编译器对浮点表达式的常量折叠。快照不能跨线程恢复；在 `await`、任务迁移和回调跨线程场景，不要把快照当作进程级状态使用。

## XLang API

- `getRounding()` / `setRounding(mode)`：`RoundingMode.nearest`、`downward`、`upward`、`towardZero`。
- `getFlags()` / `clearFlags(flags = 31)` / `raiseFlags(flags)`：`ExceptionFlag` 位标志可用 `|` 组合；非法掩码抛 `RangeError`。
- `capture()`：保存当前线程环境；`Snapshot.restore()` 可在原线程恢复，也可重复恢复。
- `hold()`：保存环境并清除标志、屏蔽异常陷阱；必须显式调用 `restore()` 才能恢复原状态。

`Snapshot` 是拥有原生句柄的 class；不要把它伪装为可跨线程迁移的环境令牌。与 C 一样，状态变更会影响随后在**同一线程**运行的浮点代码。推荐保存旧环境、进行局部操作、恢复旧环境；异常路径也应执行恢复。
接管句柄的 `Snapshot(point)` 已声明私有，但当前编译器仍允许包外构造；
见 `demo6/dev/extlib_blockers/float_env_private_adopt_constructor.md`。
在主线修复前，本包不能签收完整发布级访问安全。
`hold()` 先创建并持有快照，再清除当前线程的异常标志；快照分配失败时不会改变浮点环境。
`restoreNative()` 只供 `Snapshot.restore()` 在类内使用；包外不能跳过跨线程和
CRT 失败诊断直接调用原生状态码入口。模块级 `hold()` 经未导出的 C 声明
执行就地 hold；`Snapshot` 不公开 `holdNative()`，仍先持有句柄再改变环境。
包外对模块级 `__holdSnapshot` 的访问也由只编译负例固定为
`no such exported symbol`；负例绝不执行裸句柄调用。

## C 接口

声明见 `include/float_env_native.h`。舍入模式使用整数 0..3；标志位分别为 1、2、4、8、16。`xfenv_capture` / `xfenv_hold` 返回独立快照，使用后调用 `xfenv_snapshot_destroy`。`xfenv_snapshot_restore` 返回 1 表示成功、0 表示 CRT 拒绝、-1 表示调用线程与创建线程不一致。无效模式或标志返回 0。C ABI 中快照是 `void*`，与 XLang `point` 严格一致。
已有快照可通过 `xfenv_snapshot_hold(snapshot)` 就地保存并屏蔽当前线程环境，返回值同样为 1/0/-1；跨线程调用不修改调用线程的状态。持有该快照的调用方仍负责销毁它。

## 验证与范例

- XLang 范例：`release/examples/extlibs/floatEnv.xl`
- 同输出 C 范例：`release/extlibs/floatEnv/examples/float_env_c.c`
- 定向测试：包内 `test/boundaries.xl`、`test/allocations.xl`、`test/test_float_env.c`，以及 `demo6/test/cases/stdlib_crt_float_env.xl`

在 `demo6` 目录运行 Windows 包级门禁：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/floatEnv/test/check.ps1
```

Linux 严格 GCC、C ASan/UBSan 与 XLang 双语门禁：

```sh
bash release/extlibs/floatEnv/test/check.sh
```

Windows 门禁还逐行比较 C/XLang 范例输出并构建、运行无输出 AOT 核心。本包仅覆盖平台 CRT/TCC 可提供的浮点环境能力，不承诺改变不遵守动态舍入模式的优化表达式。
Linux 门禁复用同一份按需 XRT 运行时编译 C 边界与范例，运行包内
XLang 边界，并逐字节比较 C/XLang 范例输出；它不执行完整 AOT。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、XL JIT、C/XL 双语对照及 `test/cases/stdlib_crt_float_env.xl` 无输出核心 AOT。`xl build extlibs/floatEnv/test/boundaries.xl -o ../build/test/float_env_xl_test.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 补工作线程更改舍入模式与异常标志后主线程保持原状态的 C 回归，以及 C/XLang 同一快照重复恢复的边界；定向测试通过。

2026-10-15：manifest 与源码重复声明线程模块曾使外部严格 GCC `-Werror` 构建失败；改为条件声明并纳入包级门禁。Windows C/XLang、双语逐行输出与无输出 AOT 核心，Linux ASan/UBSan 均通过。完整边界 AOT 的共享快照缺符号仍未解除。

2026-10-16：Linux 包级门禁纳入 XLang 边界和 C/XLang 范例逐字节对照；测试运行时独立编译一次供 C 用例与范例共用。严格 GCC、C ASan/UBSan 与双语门禁同次通过。完整 AOT 的共享缺符号阻断不变。

2026-10-17：XLang `hold()` 改为先持有 `Snapshot`、再调用原生就地 hold，避免包装对象分配失败时留下已改变的环境和未托管的句柄。C 跨线程拒绝/恢复、XLang 分配故障后状态不变，以及 Windows C/JIT/核心 AOT、Linux ASan/UBSan/双语门禁通过；完整带输出 AOT 仍受共享符号阻断。

2026-10-18：包外调用 `Snapshot.restoreNative()` 和 `holdNative()` 的只编译
负例原均返回 `ok`；前者现设为私有，后者改为模块内 C 声明，导入方只能
通过检查返回状态的 `restore()` 与安全工厂 `hold()` 使用。Windows
C/XLang、双语范例、核心 AOT 与 Linux ASan/UBSan 门禁通过；原生句柄接管
构造器的主线访问控制缺陷仍在，完整发布级验收未签收。

2026-10-19：新增包外访问模块级 `__holdSnapshot` 的只编译负例，
Windows/Linux 均确认该裸指针入口未导出；现有 C/XLang 边界、双语范例、
Windows 无输出 AOT 核心和 Linux ASan/UBSan 门禁通过。
`Snapshot(point)` 的主线可见性缺陷及完整 AOT 阻断未解除。

2026-10-19：C 教学范例现在可从 `demo6` 直接执行
`release/xl.exe run release/extlibs/floatEnv/examples/float_env_c.c`。
仅 TCC 单文件运行路径内含本包原生实现；正式包和 GCC 仍独立编译。
Windows 门禁比较包运行器、单文件 C 与 XLang 的输出，Linux 门禁比较
GCC、单文件 C 与 XLang 的输出；两平台已通过，Linux 含 ASan/UBSan。
私有构造器与完整边界 AOT 的主线阻断不变，不签收整包发布级。

2026-10-20：`test/allocations.xl` 新增 `capture()` 的快照分配失败用例，
确认当前原生构造路径直接抛 `MemoryError`，并保留调用前的舍入模式与
异常标志；既有 `hold()` 故障用例继续通过。Windows 严格 C、C/XLang
边界、双语范例和无输出 AOT 核心，以及 Linux C ASan/UBSan 与 XLang
门禁均通过。此轮未修改包实现；完整边界 AOT 和私有构造器的主线阻断不变。

2026-10-20：新增包外调用 `__getRoundNative()` 的只编译负例，与原有
`__holdSnapshot` 负例分别确认模块内部原生入口没有进入导入表。
Windows 包门禁与 Linux C ASan/UBSan、XLang 对照门禁通过；
`Snapshot(point)` 私有构造器的主线访问控制缺陷仍未解除。
