# crontab

2026-10-29：本轮不改变四条公开 API、ABI、调度算法或字符串所有权，
集中补验收缺口。新增有长度输入的 169 项 C 检查：无 NUL 哨兵、
刚好 1024/1025 字节、逐位置内嵌 NUL、数字/步长与分隔符、正常
拒绝不残留旧错误，以及三条描述分支的 OOM/恢复和内存平衡。
语言分配用例补一般字段摘要，专用生成 C 夹具检查完整返回链不
出现正文复制；门禁还提取四条实际生成原型与正式 C 头严格编译。

Linux 新增独立 Python `zoneinfo` 对照，不复制产品的字段解析器或
跳步逻辑：用独立条件核对 `matches`，逐秒枚举下一本地整分钟，
覆盖 UTC 的负/零时间戳和 2038 年后日期、纽约历史秒级偏移/DST、
Lord Howe 半小时 DST、Kathmandu 的时差调整。它与原生 ASan/UBSan
分开执行，不能把未插桩的 Python/动态库对照当作内存安全检查；
更不代表所有 IANA 时区、历史转换或性能均已认证。

默认 Windows/Linux 门禁分别覆盖八/十条独立程序，Linux 多出的两条
是历史秒级和半小时 DST 的语言 AOT。每条构建与执行有超时，前后
检查输入指纹，成功只删本次工作目录、失败保留现场，报告留在目录
之外。Windows JSON 包含命令、状态、原始输出和本地时区快照；
Linux `.report.txt` 包含逐阶段命令/输出/退出值，另存输入指纹。
测试运行库只开时间、本地时间及内存诊断，不再全开 XRT。

2026-10-26 新版 Windows/Linux 发布物验收：两平台均从当前源码重建
`xl` 后运行默认包门禁，全部退出 0。`xcron_describe` 返回的 XRT
分配由 `@owned string` 直接接管；Windows 验证严格 C、边界、
分配故障、C/XLang 同输出与核心/范例/边界/分配/C 范例独立 EXE。
Linux 另以 ASan/UBSan 验证 C，覆盖纽约及 Lord Howe 夏令时、历史
秒级时差、远期日期与相同的语言/AOT 路径。此前的发布物不识别
`@owned`、快照失配等记录属于旧构建；本轮仍只签收本文的五字段
语法、8 年搜索窗口与已测时区，不宣称所有历史时区均正确。

2026-10-26 源码级更新：`xcron_describe` 的 C 返回类型明确为 `char*`，
XLang 外部声明以 `@owned ... as string` 直接接管同一 XRT 堆内存，
删去仅负责 `point` 转交的包装分支。生成 C 代码直接保存指针并扫描
一次长度；普通字符串后续操作仍遵守原有值语义。
Windows 严格 C/边界/长时域/C 范例通过；当前源码编成的公共驱动
运行中央、边界、分配故障与公开范例，后面三项的独立 EXE 也通过，
公开范例输出与 C 路径及 C 单文件独立 EXE 一致。当时的 `release/xl.exe`
不认识 `@owned`，新发布构建被 XRT/xruntime 快照失配挡住；该阻断已由
上方的新发布物构建和双平台门禁关闭。以下双平台记录对应迁移前的发布物。

此前 2026-10-26 发布物复验：Windows 默认 `test/check.ps1` 与 Linux
默认 `test/check.sh` 均退出 0。两平台验证 C/XLang 范例同输出、
语言核心/边界/分配/公开范例独立 EXE，并新增 C 单文件范例 AOT
与 GCC 输出对照；Linux C 边界、远期日期、纽约及 Lord Howe DST
继续经 ASan/UBSan，历史秒级时差与半小时 DST 有 XLang JIT
用例。本轮只完善门禁，算法、API 与主线未改。验收限本文的五字段
语法、8 年搜索窗口及已测时区样例，不能外推全部历史时区。
以下旧版 AOT 缺符号记录均为历史状态。

2026-10-24 当前 Windows 发布版默认包门禁通过：严格 C、远期时间
边界、XLang 分配故障、双语范例，以及核心、公开范例、时间边界和
分配故障四份独立 EXE 的 AOT 构建与执行。新增两份 AOT 用例与同次
JIT 输出一致。旧 `xl6ValueDataSnapshotV1` 缺符号记录仅属先前
发布物；Linux DST 和历史时区仍以独立门禁为准，不能从 Windows
结果推断全部时区规则正确。

五字段本地时间 cron：`minute hour day month weekday`。各字段支持 `*`、单个数字、范围 `N-M`、步长 `*/S` / `N/S` / `N-M/S` 和逗号列表。不支持秒、年份、英文月份/星期名或 `@daily` 等宏。分钟范围 0–59，小时 0–23，月内日 1–31，月份 1–12，星期 0–7（0 和 7 均为星期日）。表达式最长 1024 字节，拒绝第六字段、内嵌 NUL 和非法数字，不静默截断。

```xlang
import crontab

int after = 1700000000
int nextRun = crontab.next("*/15 * * * *", after)
assert nextRun > after
assert crontab.matches("*/15 * * * *", nextRun)
```

`matches` 以进程**本地时区**解释 Unix 秒，并按所在分钟匹配，秒数本身不参与比较。`next` 返回严格晚于输入时间的下一次分钟边界；无效表达式、时间越界或 8 年窗口内无匹配时返回 `-1`。夏令时跳过的本地分钟不会被返回，重复的本地分钟可分别返回。月内日和星期都不以 `*` 开头时，两者采用 cron 常见的 **OR** 语义；任一字段以 `*` 开头时，两者按各自实际位图取 **AND**。因此 `*/2` 仍按步长约束，而不是忽略该字段；`1-31` / `0-7` 即使覆盖全部数值，也不等于语法上的 `*`。`isValid` 对非法表达式返回 `false`，`describe` 返回独立字符串，非法输入为 `"invalid"`。描述字符串是字段摘要，不承诺自然语言释义。

`next` 按本地时间的秒字段对齐整分钟，不假设历史时区偏移一定是整分钟。时差切换时还检查两个候选点之间的短区间，避免漏掉切换瞬间提前出现的本地整分钟。

双语范例展示 `isValid`、`next`、`matches`、`describe` 四条公开入口；
其中 `next` 的结果会再交给 `matches` 验证，失败时范例直接退出。
包内 `examples/crontab.c` 可直接 `xl run` 单文件运行；仅该路径由 TCC
编入原生实现，普通 C 构建仍按包清单分别链接。双平台门禁核对两种
C 构建与 XLang 范例逐字输出一致。

语法合法但日历上不可能出现的规则（例如 `0 0 31 2 *`）仍由 `isValid` 接受，`next` 返回 `-1`。当星期字段以 `*` 开头、月内日字段不以 `*` 开头时，`next` 先检查所选月份是否存在所选月内日，避免逐分钟搜索完整 8 年窗口；两字段都受限时保留 OR 语义，不做这一提前判定。

## C 接口

`include/crontab_native.h` 中各入口传入表达式指针和真实字节长度，不使用 `strtok` 或共享静态状态。时间分解使用 XRT 的 `xrtTimeFromUnix` / `xrtTimeLocal`，查询按本地时区运行。`xcron_describe` 返回 XRT 分配的独立 `char*`；C 调用方用 `xrtFree` 释放，XLang 的 `@owned` 声明直接接管。`xcron_matches` / `xcron_is_valid` 返回 C `bool`，与 XLang extern ABI 对齐。
四个公开 C 入口均在调用开始时清除本线程旧的 XRT 错误；成功及按合同返回的正常未匹配结果不会带上一次调用的错误状态。
`describe` 分配失败时 C 返回 `NULL` 并保留 `XERR_MEMORY`，XLang 抛
`MemoryError`；清除故障后，同一表达式可正常生成描述，包括非法表达式的
`"invalid"` 结果。包的生产模块清单不包含测试专用的内存故障注入模块。

包清单显式声明 `XRT_MODULE_TIME` 和 `XRT_MODULE_TIME_LOCAL`；AOT 构建不必因为这个原生源文件回退到完整 XRT。无输出核心 AOT 在 Windows 上的实测文件大小由 3,533,312 字节降至 584,704 字节，且测试仍通过。显式混编额外 C 文件时，编译器仍可能按独立规则使用完整 XRT。

## 验证

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/crontab/test/check.ps1
```

该入口覆盖严格 manifest 宏 GCC 编译、C/XL 边界、169 项长度/诊断/分配检查、远期日期、实际 ABI、直接接管、双语范例同输出和八条 AOT。`-SkipAot` 仅用于快测，不用于签收。单项命令：

```powershell
.\release\xl.exe run test\cases\extlib_crontab.xl
.\release\xl.exe run test\cases\extlib_crontab_boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package crontab -Source release\extlibs\crontab\test\test_crontab.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package crontab -Source release\extlibs\crontab\test\test_long_horizon.c
.\release\xl.exe build release\extlibs\crontab\test\aot_core.xl -o build\test\crontab_aot_core.exe
.\build\test\crontab_aot_core.exe
.\release\xl.exe run release\examples\extlibs\crontab.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package crontab -Source release\extlibs\crontab\examples\crontab.c
```

测试覆盖严格字段解析、内嵌 NUL、DOM/DOW 的 OR/AND 与步长位图、不可达日期、本地闰日和跨 2100 年的 8 年搜索窗口。远期可达规则仍按原路径搜索；不可达的月/日组合提前返回。默认 Windows 门禁构建并执行中央核心、中央边界、分配、接管夹具、无输出核心、新 C 长度边界、C 范例及 XLang 范例八条路径；Linux 另外执行两条历史时区 AOT。旧版共享链接阻断不适用于本文固定基线的通过记录。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/crontab/test/check.sh`，需 GCC、Python 3.9+ 和 `/usr/share/zoneinfo` 下的 UTC、America/New_York、Australia/Lord_Howe、Asia/Kathmandu；缺少依赖会失败，不跳过。可用 `CC` / `PYTHON` 选择工具。脚本以严格 GCC、ASan/UBSan 复验 C 边界和跨 2100 年的远期闰日搜索；纽约 2024 年春季不存在的 `02:30` 被跳过、秋季重复的 `01:30` 各触发一次。它还运行独立 Python 时区对照、中央及包内 XLang 用例，并逐字节比较 C/XLang 范例输出。通用用例在 UTC 下执行，专用用例显式设置 TZ；时区数据、编译器与 Python 程序也加入前后指纹。建议在核对过源件哈希的 Linux 原生快照中执行，避免 DrvFS 干扰。

同一 Linux 门禁还使用 IANA `Australia/Lord_Howe` 验证 2024 年半小时 DST：回拨日的 `01:45` 分别命中两次，前跳日的 `02:15` 不存在、`02:30` 正常命中。C sanitizer 与 XLang 用例使用同一组独立确定的 Unix 时间点；这仍不是对所有时区的完整认证。

2026-10-15 补四个公开 C 入口的失败后成功错误槽回归；首个 `is_valid` 用例先复现旧错误残留，修复后 Windows C、Linux ASan/UBSan + DST 和 XLang 边界均通过。完整 AOT 的共享链接阻断未变。

2026-10-16 补 `describe` 对有效和无效表达式的 C/XLang 分配失败、错误分类
及恢复测试，并将语言用例登记到包目录 smoke。Windows 包级 C/XL、同输出
范例及无输出 AOT 核心、Linux ASan/UBSan + DST、目录 smoke 2/2 均通过；
完整带输出 AOT 仍待共享符号修复。

同日 Linux 包门禁追加远期闰日、中央/包内 XLang 与 C/XLang 范例对照；
完整脚本实跑通过。时间算法未改，完整 AOT 共享阻断不变。

2026-10-17：Linux `America/New_York` 1880 年的秒级时差先复现 `next`
返回错误的分钟秒数；1883 年切换到标准时又复现漏掉当天 `12:00:00`。
搜索现按 XRT 本地秒字段对齐，仅在 UTC 偏移变化时检查相邻候选点之间的
不足一分钟区间。两条回归先红后绿；Linux C ASan/UBSan、原有夏令时与远期
测试、XLang JIT/双语范例，Windows 严格 C/XLang 和核心 AOT 均通过。
完整带输出 AOT 的共享缺符号阻断不变。

同日增加 Linux 专用的 `test/historical_timezone.xl`，与 C 回归使用相同的
1880 年午夜和 1883 年切换时间戳；`test/check.sh` 在
`TZ=America/New_York` 下运行它。XLang 定向用例及完整 Linux 包级门禁通过。
此测试依赖 IANA 历史时区数据，不代表 Windows 或所有时区的历史规则验收。

同日新增四条固定规则、每条 16 个起点的 `next` 独立对照。测试借助 XRT
分解本地时间，但由测试自身判断字段匹配并逐分钟枚举最早触发点，再核对
`next` 与 `matches`。覆盖分钟步长、工作日、月日/星期 OR 及小时步长；
Windows 包级和 Linux ASan/UBSan、DST、双语门禁通过。它不证明所有时区
或历史规则均正确；完整带输出 AOT 的共享缺符号阻断仍在。

2026-10-24：独立逐分钟枚举对照扩至八条规则、每条 16 个起点，新增
`N/S`、分钟与小时的 `N-M/S`、月内日 `*/2` 与星期的 AND，以及受限
月内日与星期的 OR。Windows `-SkipAot` 严格 C/XLang、双语门禁和 Linux
ASan/UBSan、两组 DST、历史时区门禁通过；本次只扩测试，完整 AOT
仍受共享缺符号阻断，不作发布签收。

2026-10-20：Windows `test/check.ps1` 增加 `-SkipAot` 聚焦模式；默认
先完成 C/XLang 同机输出对照，再构建/运行无输出核心与带输出范例 AOT，
范例结果仍与同机 C 输出比较，不硬编码依赖时区的时间戳。AOT 产物
放在每次独占目录，退出时校验并只清理本次目录。快测通过，默认
门禁核心 AOT 通过、完整范例在共享 `xl6ValueDataSnapshotV1` 缺符号
处失败；旧的固定 `build/test/crontab` 目录未动。此前仅核心通过
的默认全绿不能作为发布级签收。
