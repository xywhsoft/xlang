# crontab

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

`include/crontab_native.h` 中各入口传入表达式指针和真实字节长度，不使用 `strtok` 或共享静态状态。时间分解使用 XRT 的 `xrtTimeFromUnix` / `xrtTimeLocal`，查询按本地时区运行。`xcron_describe` 返回 XRT 分配的独立内存，由调用方用 `xrtFree` 释放。`xcron_matches` / `xcron_is_valid` 返回 C `bool`，与 XLang extern ABI 对齐。
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

该入口覆盖严格 manifest 宏 GCC 编译、C/XL 边界、远期日期、双语范例同输出和无输出 AOT。单项命令：

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

测试覆盖严格字段解析、内嵌 NUL、DOM/DOW 的 OR/AND 与步长位图、不可达日期、本地闰日和跨 2100 年的 8 年搜索窗口。远期可达规则仍按原路径搜索；不可达的月/日组合提前返回。Windows 严格 GCC、Linux ASan/UBSan、C/XL 范例逐行同输出和无输出 AOT 核心通过；完整带输出范例 AOT 仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。发布级验收暂不签收。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/crontab/test/check.sh`。脚本以严格 GCC、ASan/UBSan 复验 C 边界和跨 2100 年的远期闰日搜索；在安装了 IANA `America/New_York` 时区数据的环境下验证 2024 年春季不存在的 `02:30` 被跳过、秋季重复的 `01:30` 各触发一次。它还运行中央及包内 XLang 用例，并逐字节比较 C/XLang 范例输出。DST 测试仅证明该时区和这两次转换，不代表所有历史时区规则已验收。

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
