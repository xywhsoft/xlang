# dotenv

2026-10-29 所有权补测：八组C测试现在逐组核对存活分配及非法释放、
重复释放、上下溢指标；覆盖构造/解析/更新/环境OOM、解析变异与恢复。
新增同一条目的借用键/值回传、自有配置文本作为parse输入、失败后
借用地址保持，以及自别名更新OOM。两平台均通过，无需修改包本体。

新增 `examples/reload.xl` / `examples/reload_c.c`，展示同对象安全重载：
坏配置保留旧值、成功重载整体替换、已保存字符串跨修改/clear存活。
两者同一黄金输出，覆盖GCC、C直跑及C/XLang独立EXE；C显式复制
要跨修改保留的借用文本，XLang普通string结果独立持有内容。
C对照范例不与XL共用文件基名，以免成为语言设计规定的C companion。

新增顶层配置对象另确认 **M2主线阻断**：Windows/Linux JIT退出0，
独立EXE均打印PASS后于退出期报状态错误、退出1。默认门禁保留此项，
当前不作完整发布签收；不移动全局对象、不提前销毁或吞异常。
复现与恢复方向见 `dev/extlib_blockers/global_native_class_aot_fini.md`。

Windows最终完整42项，40个正向阶段及一项预期私有访问拒绝符合预期，
仅最后生命周期子门禁失败，输入变动0；13个真实生成原型与C定义
在同一编译单元严格核对，旧边界和新旧双语九条AOT通过。报告为
`build/test/dotenv_guard_97a7c3343c524705951cd7678c14dcfd.json`。
Linux原生快照7mIqXx同样先过C ASan/UBSan、全部新旧JIT/AOT和黄金
对照，最后在新顶层AOT报红，内外输入一致并保留父/子现场。
门禁现在记录输入指纹、编译器、命令/退出码与原始输出，失败保留
产物；C侧只编译一次XRT对象供测试和范例复用，不影响产品运行路径。

2026-10-28 从当前工作树重建 Windows/Linux 发布版后，默认包门禁均退出 0。
Windows 覆盖严格 C、语言边界与分配故障、双语范例和独立 EXE；Linux
另覆盖 C ASan/UBSan 与同样的 JIT/AOT 路径。Linux 门禁现将语言
编译与运行需要的发布二进制、包、源码和一份固定夹具复制到本次
独占的原生临时目录，结束时统一清理；包实现和公开 API 未改。
在 WSL Windows 挂载盘上，核心 EXE 构建曾约百秒后无诊断返回 1，
同一输入在原生临时目录约 10 秒构建且运行通过。此结果只签收
本包门禁范围，不外推为其他包或整套编译器的 Linux 发布验收。

2026-10-27 从已提交源码 `f715e057e` 在独立工作树重建 Linux 发布版，
默认 `test/check.sh` 退出 0：严格 C ASan/UBSan、语言 JIT、中央与
边界/故障注入及公开范例的独立 EXE、C/XLang 输出对照均通过。
脚本只规范化黄金文本的 CRLF 行尾，以便 Windows 工作树在 Linux
运行时不因检出方式失败；库实现与公开 API 未改。此前的图提供者
缺符号见下方历史记录，不再是当前源码的发布阻断。

2026-10-27 旧 Linux 发布物复验：C ASan/UBSan、语言 JIT、中央
独立 EXE 均通过；默认门禁在构建带 `println` 的 `test/limits.xl`
时因 `xl6DictGraphProviderV1` 未解析而退出 1。无包最小复现见
`../../../dev/extlib_blockers/linux_aot_print_graph_provider.md`。
这属于当前 Linux AOT 产物边界，不能把 Windows 门禁或部分 Linux
通过项当作整包双平台签收；本任务不在 dotenv 内规避输出调用。

2026-10-27 当前 Windows 发布版复验：默认 `test/check.ps1` 退出 0，
覆盖严格 C、C/XLang 范例、限额、进程环境写入与构造分配故障、
配置替换，以及对应的独立 EXE。下文旧版缺共享符号或头文件顺序
错误的记录均为历史状态；本次未重建 Linux 发布物，不据此签收
新 Linux AOT 或长期进程环境压力。

UTF-8 `.env` 配置库。C 使用 `include/xrt_dotenv.h`，XLang 使用 `import dotenv`。
每份 `Config` 独立持有键值；`parse`、`load` 成功后才替换现有内容，不会把半份
配置暴露给调用方，也不会默认改写进程环境。

## XLang

```xlang
import dotenv

auto config = dotenv.load("config.env")
string host = config.get("HOST", "localhost")
config.set("PORT", "8080")
config.apply()       // 显式写入进程环境，默认不覆盖已有值
```

`dotenv.create()` 返回空 `Config`；`dotenv.parse(text, maxBytes=4194304)` 和
`dotenv.load(path, maxBytes=4194304)` 返回新对象。对象提供 `get`、`has`、
`set`、`clear`、`count`、`apply(overwrite=false)`；也可调用
`config.parse(text, maxBytes=4194304)` 或 `config.load(path, maxBytes=4194304)`
在同一对象上整体替换配置，失败时保留旧条目。格式错误抛 `ConfigError`，
超过输入上限抛 `RangeError`，文件错误抛 `FileError`。错误后可读取
`errorCode`、`errorLine` 和 `errorText()`。
底层内存分配失败抛 `MemoryError`，包括 `apply` 查询或写入进程环境时
的分配失败；普通环境操作失败仍抛 `RuntimeError`。
传给 C 字符串接口的键、值与路径若含内嵌 NUL 会被拒绝，不会静默截断；
`get`/`has` 与 `load` 抛 `ValueError`，`set` 抛 `ConfigError`。
三个构造入口的底层对象分配失败均抛 `MemoryError`，不会返回带空原生句柄的 `Config`，也不会误报为配置解析或文件错误。
`parse` 对非空输入先检查 `maxBytes`，超限立即报告 `RangeError`，不扫描超出配额的内容；限额内的 NUL 仍按非法输入拒绝。
XLang 的 `maxBytes` 必须为正数；C 的 `xrtDotenvParse/Load` 接受 0 作为严格的零字节上限，非空输入返回 `LIMIT` 且原配置不变。
`*Native` 原生方法仅在 `Config` 内部可见，包外不能自行传入文本长度或绕过上述校验；`test/private_native.xl` 是预期编译失败的访问权限负例。

键名必须符合 `[A-Za-z_][A-Za-z0-9_]*`，大小写敏感；重复键采用最后一个值。
键和值按插入顺序保存在 XRT `xmap`；重复键替换原位置的值，查询和解析不再逐项线性扫描。`clear()` 同时释放条目与桶容量。
接受 UTF-8 BOM、LF/CRLF、行首 `export`、空行和 `#` 注释；孤立 CR 是格式错误，
不会并入值或被当作行结束。单引号保留原样，
双引号解析 `\\n`、`\\r`、`\\t`、`\\"`、`\\\\`；未加引号的值会
去掉末尾空白，空白后的 `#` 开始行内注释。不做变量插值，也不隐式读取
进程环境，避免配置加载依赖外部可变状态。坏行不会被静默跳过。

## C 与所有权

`xrtDotenvCreate` / `xrtDotenvDestroy` 管理对象；`xrtDotenvParse`、
`xrtDotenvLoad`、`xrtDotenvSet` 返回成功状态。`xrtDotenvGet` 返回借用的
字符串，修改或销毁对象后失效；XLang 的 `string` 返回值独立持有内容。
`xrtDotenvErrorCode`、`xrtDotenvErrorLine`、`xrtDotenvErrorText` 可检查失败。C 故障注入逐点检查创建失败的 `XERR_MEMORY` 分类及清除故障后的恢复。
`get` 的原生结果由Config拥有，不能以`@owned`接管；需要跨修改保存时，
C调用方显式`xrtStrDup`并以`xrtFree`释放。重载对照范例验证了这一点，
不把保护生命周期所需的复制改成悬空借用。
公开 C 操作开始时清理本线程旧 XRT 错误，成功调用不残留之前的失败；对象错误查询与析构不会擦除诊断。
同一对象的并发修改需由调用方同步；不同对象互不共享存储。

`xrtDotenvApply` 显式写入 XRT 管理的进程环境，`overwrite=false` 时保留已有
变量，包括已有的空字符串。多键写入不提供事务回滚；需要原子地配置进程
环境时，应由宿主统一协调。文件路径由 XRT UTF-8 文件 API 处理。

## 定向验证

在 DEMO6 目录运行：

```powershell
./release/xl.exe run release/extlibs/dotenv/examples/basic.c
./release/xl.exe run release/examples/extlibs/dotenv.xl
./release/xl.exe run release/extlibs/dotenv/examples/reload_c.c
./release/xl.exe run release/extlibs/dotenv/examples/reload.xl
release/extlibs/dotenv/test/check.ps1
bash release/extlibs/dotenv/test/check.sh
```

双语范例各自创建独占的系统临时目录，写入 UTF-8 配置文件，成功或失败均清理；
不覆盖工作目录中现有的 `.env`。C 教学范例包含包内实现，可直接 `xl run`，
正式扩展包仍按清单分别编译 C 源。范例还演示同对象 `parse` 整体替换、
`has`/`count`、`apply` 默认保留与显式覆盖，以及 `clear`；进程环境变量
`XL_DEMO6_DOTENV_EXAMPLE` 在退出前恢复原值或移除，C/XLang 输出对照同一
黄金文件。只有显式 `apply` 才会写入进程环境。

2026-10-25 使用当前 Windows 发布版 `xl.exe` 复验：默认 `check.ps1`
完整通过，包括严格 C、C/XLang 范例、语言边界、中央用例和公开范例 AOT。
Linux `check.sh` 也通过 C ASan/UBSan、语言 JIT、中央用例及四个包内
边界程序的独立 EXE AOT、公开范例 AOT，以及 C/JIT/AOT 范例输出对照。
此前的 `xl6ValueDataSnapshotV1` 缺符号记录仅描述旧发布二进制，现已解除。
Linux 脚本需要 `release/xl`，检查 C 侧 XRT
解析/环境边界、XLang 中央与包内边界，构建运行六条独立 EXE 路径，
并将 C/XLang 范例同黄金输出逐字对照。
这两项包级门禁通过不代表整个 xlang 发布版已经签收。

2026-10-26 再次运行 Windows/Linux 默认门禁均通过。Windows 新增
`limits`、`apply_oom`、`create_oom`、`replace` 四组独立 EXE，
以及 C 教学范例通过 `xl build` 生成 EXE 并与黄金输出对照；
Linux 保持 C ASan/UBSan、语言 JIT 与同组 AOT 全覆盖。
这次使用的是现有发布版；尝试从更新后的工作树重建发布版时，
主线 `XRT/xlang capability matrix is stale` 快照校验在编译前退出，
故新二进制仍需待主线快照同步后重新验证。

以下为历史验证记录；其中“当前阻断”均指当时的旧发布二进制。

2026-10-24 编译器产物分层复验：现有 `release/xl.exe` 的默认包门禁仍在
公开范例 AOT 链接时报缺少 `xl6ValueDataSnapshotV1`；同一包的
`-SkipAot` 严格 C、C/XLang 范例黄金输出及语言边界全过。此前从源码
构建的 `public_source.exe` 则把公开范例、中央用例和包内 `limits`、
`apply_oom`、`create_oom`、`replace` 均编为独立 EXE，逐一运行退出 0；
公开范例与 `examples/basic.expected.txt` 逐字一致，内存运行亦通过。
这表明旧发布二进制的缺符号不能直接归为 dotenv 包实现缺陷，但不构成
当前工作树的发布签收：重新构建源码驱动时，并行主线修改触发脚本的
`Build inputs changed` 一致性检查，未得到新的可信产物。待主线稳定并
同步发布版后，须重跑不带 `-SkipAot` 的完整包门禁。

2026-10-21：C 与 XLang 补齐空文本、仅 UTF-8 BOM 文本的替换回归：
两者都应成功清空原对象的旧条目；C 的零字节限额也接受真正的空文本。
Windows `-SkipAot` 严格 C/XLang 与双语门禁、Linux C ASan/UBSan 和
XLang 门禁均通过。Windows 默认门禁的核心 AOT 通过，带输出范例构建
仍缺共享 `xl6ValueDataSnapshotV1`，故整包尚未完成发布级验收。

2026-10-23：C/XLang 回归补齐双引号内 `\\t`、`\\n`、`\\r`、转义引号与
反斜杠，以及单引号内 `\\q` 原样保留；非法双引号转义拒绝后旧配置
保持不变，下一次有效解析仍可恢复。Windows `-SkipAot` 严格 C、语言
用例和双语范例，以及 Linux C ASan/UBSan、XLang 联合门禁通过。
解析器无需修改；完整 AOT 的共享阻断仍未解除。

2026-10-24：XLang 回归保存 `get()` 返回的字符串，再覆盖对应键并清空配置，
确认先前结果仍独立可读。Windows `test/check.ps1 -SkipAot` 与 Linux
`test/check.sh`（含 C ASan/UBSan、XLang 和双语对照）通过。C 的 `get()`
仍返回对象内部借用地址，不能直接移交给语言字符串；完整 AOT 阻断未变。

2026-10-14：修复缺文件后成功解析仍残留旧 XRT 错误的连续调用问题，并验证 `set`、`get`、`has`、`count`、`clear`、`apply`、`load` 的相同边界。Windows 包级 C/XLang 门禁与 Linux ASan/UBSan 通过；AOT 阻断状态不变。

2026-10-15：新增解析 17 项与更新已有键的逐点分配失败矩阵，覆盖扩容、
旧配置/旧值保留、错误分类及同一对象恢复调用；Windows 包级 C/XLang
与 Linux ASan/UBSan 通过。实现未改；完整 AOT 主线阻断仍在。

2026-10-15：中央 `test/cases/extlib_dotenv.xl` 的文件限额用例改为从
DEMO6 根目录访问 `test/data/dotenv_test.env`，与正式测试运行器的工作
目录一致。中央用例 JIT 和 AOT 均构建运行通过；包实现与完整包级
AOT 阻断状态未变。

2026-10-15：包级 Windows 门禁同步在 DEMO6 根目录运行该中央用例，
不再从 `release` 误读固定夹具。C 与 XLang JIT、中央用例 AOT 均通过；
完整教学范例 AOT 仍缺共享 `xl6ValueDataSnapshotV1`，未作发布签收。

2026-10-16：将顺序键值数组替换为 XRT `xmap`，保留插入顺序和覆盖语义，消除大量键时解析与查询的平方复杂度。`test/bench_dotenv.c` 在本机 Windows GCC `-O2`、2 万个固定键下，改动前解析/反向查询分别为 0.753/0.674 秒，改动后为 0.008/0.003 秒；这只是同机定向基线，不是跨平台性能承诺。分配故障扫描扩至 96 个位置；另以 C/XLang 故障注入确认 `apply` 的内存失败分类、环境不变与清除故障后的恢复。Windows C/XLang、Linux ASan/UBSan、目录 smoke 3/3 和中央无输出 AOT 构建运行通过。完整带输出范例 AOT 仍缺共享 `xl6ValueDataSnapshotV1`。

2026-10-16：Linux 门禁加入 XLang 中央用例、包内限额和分配故障用例，
并与 C 范例及黄金输出逐字比较；完整 Linux C ASan/UBSan + XLang
包级脚本通过。
完整 AOT 的共享缺符号阻断不变。

2026-10-19：C/XLang 范例改用独占临时目录，消除固定路径覆盖现有文件的风险；
C 范例可直接 `xl run`。Windows C/语言边界、Linux C ASan/UBSan 与双语
黄金输出复验通过。未修改 dotenv 解析实现；完整 AOT 主线阻断仍在。

2026-10-17：C/XLang 负例复现 `A=1\rB=2` 被误收为单个值，及文件末尾
孤立 CR 被误当作 CRLF。行扫描现只接受 LF/CRLF，异常输入报告所在行且
保留原配置；Windows 包级 C/JIT 与 Linux ASan/UBSan、双语范例门禁通过。
完整带输出 AOT 仍受共享缺符号阻断。

2026-10-18：收紧十个原生方法的包外可见性，`parseNative` 的任意长度调用
由编译负例先复现，修复后需在语义检查阶段拒绝。模块工厂复用新增的
`Config.parse/load` 成员；XRT 的事务替换和错误映射保持不变。新增同对象
替换、失败保留及恢复用例，Windows 严格 C/JIT、Linux ASan/UBSan 与
C/XLang 同输出范例通过；完整 AOT 共享缺符号阻断不变。

2026-10-18：从合法多行配置生成逐长度截断与逐字节替换矩阵，固定验证
成功解析不残留错误，失败解析保留旧配置；Windows 包级 C/XLang 和 Linux
C ASan/UBSan + XLang 门禁通过。此矩阵不是完整 fuzz。当前完整范例 AOT
重试已变为 `xl6_channel_language_abi.h:7` 的头文件声明顺序错误；
包脚本现在完整报告失败诊断。复现与修复方向见
`dev/extlib_blockers/runtime_channel_header_cycle_aot.md`，未签收发布级。

2026-10-24 当前 Windows 发布版复测：默认 `test/check.ps1` 全部通过，
包括严格 C 测试与范例、C/XLang 输出对照、限额/内存失败/对象替换
边界，以及中心用例和完整教学范例的 AOT 构建与执行。旧记录中的
`xl6ValueDataSnapshotV1` 缺符号及 channel 头声明顺序错误在本次路径
均未复现。Linux XLang 门禁尚未使用当前源码重建的发布物复验，
构建阻塞见 `demo6/dev/extlib_blockers/linux_release_x509_sources.md`；
本条不代表 Linux AOT、长期压力或跨平台发布级签收。
