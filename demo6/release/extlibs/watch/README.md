# watch

2026-10-25：`Watcher.poll()` 改为返回拥有型 `cstring`，直接接管
`xwatch_poll` 的独立事件分配；调用方仍可赋给普通 `string`。
`path()` 的 C 结果是借用指针，保持原有独立字符串语义，不参与这次接管。
当前 Windows 默认包门禁的严格 C、目录竞态、语言边界与故障注入、
双语范例和两份 AOT 均通过；生成 C 检查只有事件出口调用
`xl6CStringTakeV1`，没有旧 `takeCstr` 桥接。Linux 语言侧仍待
更新到识别 `cstring` 的发布物后复验。

2026-10-24 当前发布物复验：Windows 默认 `test/check.ps1` 全部通过，
包含严格 GCC、目录竞争、分配故障、C/XLang 双语固定输出、边界 AOT
以及带输出范例 AOT。Linux `test/check.sh` 的 C ASan/UBSan、目录竞争、
XLang JIT 边界和双语输出也通过。旧版
`xl6ValueDataSnapshotV1` 缺符号已不是当前 Windows 阻断；本轮没有
用新 Linux 编译器构建并运行独立 EXE，不能外推为 Linux AOT 验收。
以下早期记录的“当前缺符号”均指当时的旧发布版。

`Watcher` 轮询 UTF-8 路径元数据，可监视文件与目录。`add(path)` 返回稳定索引；`poll()` 每次返回一个 `CREATED:path`、`MODIFIED:path`、`DELETED:path` 事件，或在无变化时返回空串；`remove(index)` 释放该项，后续 `add` 可重用索引。监视时路径可以尚不存在，此时后续创建会产生事件。`watch.exists(path)` 可单独检查路径。

`Watcher` 是导出类型，可写 `watch.Watcher watcher = watch.create()`；现有 `auto` 写法仍可使用。

相对路径在 `add` 时按当时的工作目录固定为绝对监视目标；之后进程切换工作目录不会改变目标。事件文本和 `path(index)` 仍保留传入的原路径写法，不返回内部绝对路径。

目录监视是**非递归**的：扫描直接子项的名称和元数据以发现子项增删、重命名及常见文件修改；事件仍指向被监视的目录，不提供每个子项的独立事件。每次 `poll` 的代价与监视路径数及目录直接子项数相关；它不是操作系统通知接口，也无法还原两次轮询之间的全部中间状态。实例由使用方持有，不允许多个线程并发操作同一实例。`poll` 使用轮转游标，避免前面的活跃路径长期占用返回机会。
比较会使用 XRT 可用的文件身份、创建时间、修改时间、大小等元数据；
同大小且保留修改时间的原子替换因此仍可被识别。目录直接子项的摘要
也包含可用的身份和创建时间，但受文件系统实际提供的元数据限制，
不保证识别所有“内容变了而元数据完全相同”的情况。
目录若恰好在元数据查询后、子项枚举前消失或变成文件，`add` / `poll`
会重新取样一次，把变化作为缺失状态或事件处理；权限、内存等错误不被
当作普通变化吞掉。连续变化超过这次有界重试时仍可能返回文件系统错误。

原生 API 声明在 `watch_native.h`：`xwatch_poll` 返回的事件由 C 调用方用 `xrtFree` 释放；XLang 的 `poll()` 以 `cstring` 接管它。`xwatch_path` 是借用路径，添加其他监视项或条目表扩容时仍有效，在删除对应项或销毁实例后失效；XLang 的 `path()` 返回独立字符串。XLang 包装层将失败转为异常。底层路径、元数据、内存与目录枚举均使用 XRT，Windows 不走 ANSI 文件 API。
包清单显式声明目录、路径与 64 位哈希三个 XRT 模块；同一无输出边界 AOT 在 Windows 上由 3,781,632 字节降至 881,664 字节，事件测试仍通过。编译时另行混编 C 源文件可能触发编译器的完整 XRT 路径。
公开操作从本次调用开始清理本线程旧的 XRT 错误；成功调用不会继承上一次失败的诊断。`destroy` 不清错，避免析构时抹去调用方待处理的错误。
XL 的 `add` 拒绝内嵌 NUL 的路径，`exists` 对该路径返回 `false`，
避免 C 字符串接口静默截断路径。
`Watcher` 的 `*Native` 成员仅供类内部使用，包外应通过公开方法完成路径校验并接管事件文本；`test/private_native.xl` 是预期编译失败的可见性负例。
`watch.create()` 在底层分配失败时抛出 `MemoryError`，不会返回带空原生句柄的实例。C/XLang 测试注入首次分配失败并验证清除故障后可正常创建。

双语范例位于 `release/extlibs/watch/examples`；更完整的 XLang 范例位于 `release/examples/extlibs/watch.xl`。在 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/watch/test/check.ps1 -SkipAot` 做 Windows 包级测试，运行 `bash release/extlibs/watch/test/check.sh` 做 Linux C 侧 ASan/UBSan 与 XLang 边界测试；Windows 脚本不带 `-SkipAot` 时加测 AOT。Windows 包门禁的 C/XL 边界测试与 Linux C 测试均在本次运行的独立工作目录内创建临时文件，正常结束后清理。Linux 包门禁也从隔离目录运行三份 XLang 边界用例，并比较 C/XLang 范例输出；C 范例从 `demo6` 根目录运行，XLang 范例从 `release` 目录运行，各自使用对应的相对路径。
成对入门范例除索引生命周期外，还在独占临时目录内创建、修改、删除文件，逐条验证并展示 `CREATED`、`MODIFIED`、`DELETED` 事件；门禁核对固定八行输出。
其中 C 范例已自包含包实现，可从 `demo6` 根目录直接运行
`release/xl.exe run release/extlibs/watch/examples/watch_c.c`；包门禁也将这一路径的输出与 GCC、XLang 对照。C 范例添加监视目标失败时会销毁已创建的实例。
三份 XLang 包内用例已登记到 `coverage.tests`；`tools/smoke_extlib_catalog.ps1 -Package watch` 会自动执行它们。Windows 包级脚本从隔离目录运行边界用例，并从 `release` 目录运行 XLang 范例。
当前 Windows 严格 GCC、C/XL 边界、双语示例、边界及带输出范例 AOT
均通过。Linux 包门禁运行跨平台的
`test/boundaries.xl`；中央 `test/cases/extlib_watch.xl` 使用 Windows 反斜杠路径，
不作为 Linux 门禁。
2026-10-16 移除 C 源对私有 `xl_runtime.h` 的依赖，改用公开 `xrt.h`；
Windows C/XL 与 Linux C ASan/UBSan 加 XLang 边界门禁复跑通过。

2026-10-14：修复相对路径在切换工作目录后指向另一目标的问题。C/XLang cwd 切换负例先红后绿；新增绝对路径分配的逐点故障回归、Windows 包级 C/XLang 门禁、Linux ASan/UBSan 和无输出 AOT cwd 用例通过。带输出完整 AOT 的共享缺符号阻断不变。

2026-10-14：修复无效 `add` 后有效 `add`、无效 `poll` 后有效 `poll` 残留旧 XRT 线程错误的问题。C 连续调用先红后绿，XLang 连续调用、Windows 包级门禁、Linux ASan/UBSan 与当前无输出边界 AOT 复跑通过；带输出 AOT 主线阻断不变。

2026-10-15：以链接包装稳定复现目录在 stat 与打开之间删除的竞争；
`add` 的旧错误先红后绿，`poll` 的删除事件、目录变文件及真实 I/O 错误
不被吞掉均纳入 Windows GCC 和 Linux ASan/UBSan 门禁。普通 C/XLang
边界与双语范例复跑通过；带输出 AOT 的共享符号阻断不变。

2026-10-16：新增 `poll` 分配故障扫描，覆盖取样和事件字符串创建阶段；
每次注入失败后再次轮询仍能收到原来的 `CREATED` 事件，不会把未交付的
变化提前标记为已消费。Windows C/XLang 包门禁与 Linux ASan/UBSan 通过；
实现无需改动，带输出 AOT 仍待共享符号修复。

同日包级门禁将 C 测试、目录竞争测试和 XLang 边界测试移入每次运行的
隔离目录，避免两份门禁争用 `build/test/watch_guard` 中同名测试文件。Windows `-SkipAot`
全套门禁与 Linux ASan/UBSan 重跑通过；完整 AOT 阻断未变。
不带 `-SkipAot` 的实测中，边界 AOT 构建与运行均通过，带输出范例构建
仍缺 `xl6ValueDataSnapshotV1`；失败退出后本次隔离目录也已清理。

2026-10-17：新增 XLang `create/add/poll` 首次分配故障用例，验证三条公开路径
均抛 `MemoryError`，失败的 `add` 不增加监视项，清除故障后可继续添加及轮询。
Windows `-SkipAot` 包级门禁和 Linux C ASan/UBSan 加 XLang 门禁通过；这不是
全部分配点扫描，也不替代仍被共享符号阻断的带输出 AOT 验收。

2026-10-18：包外 `addNative` 原可绕过 NUL 路径检查，编译负例先确认可达；
五个原生方法现设为私有，Windows 严格 C/XLang/目录竞争与 Linux
ASan/UBSan 包级门禁通过。带输出 AOT 的共享阻断不变。

2026-10-18：Linux 包级门禁补跑已有的工作目录切换与分配故障 XLang 用例，
并对照 C/XLang 范例的完整输出。严格 GCC 静态分析和 Linux
ASan/UBSan、三份 XLang 边界、双语同输出门禁通过；本轮未改包实现，
带输出 AOT 的共享阻断不变。

2026-10-20：`test/boundaries.xl` 与 `test/cwd_snapshot.xl` 不再写删固定的
`build/test/watch_guard`；各自建立独占系统临时目录，并在 `finally` 清理。
两份用例从 `demo6` 根目录单独运行通过，Windows `-SkipAot` 包门禁和
Linux C ASan/UBSan、XLang、双语对照门禁均通过。带输出 AOT 的共享缺符号
阻断仍在，不据此签收整包发布级。

2026-10-21：两个 C 边界测试原依赖门禁外层切换工作目录，单独运行时
会使用调用方的固定 `build/test/watch_guard` 路径。现由测试共用的
`test/workdir.h` 创建独占系统临时目录，运行原有文件、目录竞争与故障
注入用例后恢复工作目录，只清理本次夹具。Windows `-SkipAot` 包门禁
和 Linux C ASan/UBSan、XLang/双语联合门禁通过。仓库中已有的历史
`build/test/watch_guard` 产物未动；本轮未从该目录直跑，也未改监视器实现。
完整带输出 AOT 的共享主线阻断仍在。

2026-10-22：XLang `create/add/poll` 分配故障由只测首点扩为各前 16 点；
每点使用独立监视器，核对故障是否确实触发、异常类别、失败时计数不变、
清除故障后的添加/轮询恢复，并要求三条路径都覆盖失败与正常完成。
Windows `test/check.ps1 -SkipAot` 的 C/目录竞态/XLang/双语门禁和 Linux
`test/check.sh` 的 C ASan/UBSan、XLang、双语门禁通过。实现/API 未改；
完整带输出 AOT 的共享主线缺符号仍未签收。

同日 C/XLang 入门范例补齐 `path(index)`、`remove(index)` 与删除后索引复用，
两边均断言计数和索引状态；当时 Windows/Linux 包门禁核对固定五行输出。
Windows `-SkipAot`、Linux C ASan/UBSan 与双语门禁通过；默认 Windows
边界 AOT 通过，带输出范例仍缺共享 `xl6ValueDataSnapshotV1`，不签收整包。

2026-10-23：C/XLang 边界测试增加持续变化的双监视项：第一项事件交付后
立即再次修改它，下一次 `poll` 仍须先交付第二项待处理事件，随后才交付
第一项的新变化。Windows `-SkipAot` 和 Linux C ASan/UBSan、XLang、
双语门禁通过；监视器实现无需修改。Windows 完整门禁再次确认边界 AOT
通过，公开范例 AOT 仍缺共享 `xl6ValueDataSnapshotV1`，整包未签收发布级。

2026-10-24：XLang 边界新增返回字符串的生命周期回归：保存 `path()` 结果，
移除对应监视项后仍须保留原 UTF-8 路径；保存 `poll()` 的事件文本，后续
交付其他事件后原结果仍不变。Windows `test/check.ps1 -SkipAot` 的 C、
目录竞态、XLang 与双语门禁，以及 Linux `test/check.sh` 的 C ASan/UBSan、
XLang 和双语门禁通过。完整带输出 AOT 阻断未变。
