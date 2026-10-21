# ical

`ical` 提供 iCalendar 2.0 的 **VEVENT/VTODO 常用字段值映射**，用于简单日程的读取与生成。`parse(text)` 返回 `{prodid, events, todos}`，其中事件、待办是对象数组；`build(calendar)` 生成独立的 UTF-8 文本。日期与规则仍是原始字符串，本库不计算时区、重复周期或提醒触发时间。

```xlang
import ical

var event = {
    "uid": "meeting@example",
    "dtstamp": "20260801T000000Z",
    "dtstart": "20260817T013000Z",
    "summary": "团队例会",
    "rrule": "FREQ=WEEKLY;BYDAY=MO,WE"
}
string text = ical.build({"events": [event]})
var back = ical.parse(text)
assert back["events"][0]["summary"] == "团队例会"
```

## 映射边界

日期伴随键的后缀严格区分大小写：只接受 `dtstartTzid`、`dtstartValue` 等精确拼写；例如 `dtstartTZID`、`dtstartVALUE` 会报错，不会静默丢失参数。显式提供的 `Tzid` / `Value` 不能为空；省略伴随键才表示不输出该参数。

支持 `uid`、`dtstamp`、`dtstart`、`dtend`、`summary`、`description`、`location`、`organizer`、`status`、`rrule`、`due`、`priority`、`categories`、`url`、`created`、`lastModified`、`sequence`、`transp`。`prodid`、`uid`、`summary`、`description`、`location` 按 TEXT 转义；日期、URI、规则及 `categories` 保留属性值原文。重复字段读为字符串数组，生成时逐项输出。`DTSTART`、`DTEND`、`DUE` 的 `TZID` 和 `VALUE` 分别映射为 `dtstartTzid` / `dtstartValue` 等伴随键。

读取时展开折行，要求单个 `VCALENDAR`、`VERSION:2.0`、非空 `PRODID` 和正确闭合；`BEGIN` / `END` 只能作为无参数的组件界定行，带参数的同名行会被拒绝，不能作为未知属性忽略。嵌套 `VALARM` / `VTIMEZONE` 的属性不会污染父事件。生成时顶层只接受 `version`、`prodid`、`events`、`todos`；每个事件/待办都必须有 `uid`、`dtstamp`，事件还必须有 `dtstart`。未知对象键直接报错，避免把拼错的 `event` 当作空日历成功写出。

**这不是无损 iCalendar DOM 或完整 RFC 5545 验证器。** 未映射属性、未知参数、组件顺序及嵌套组件不会往返；`RRULE` 只保留原值，不求值；`CATEGORIES` 保留原始逗号分隔与转义写法，不拆分为分类数组。生成的 `TZID` 参数不会自动附带 `VTIMEZONE` 定义，因此需要独立交换的日历建议使用 UTC 日期，或由调用方负责时区定义。日期字符串的合法性也由调用方负责。输入/输出限制 8 MiB；生成超限时 C 接口报告 `XERR_RANGE`，不返回截断结果。拒绝非法 UTF-8、内嵌 NUL、非法 ASCII 控制字符与畸形包络，包含未知属性的输入行也须满足字符约束。TEXT 字段中的内存换行在生成时转义，HTAB 允许使用；物理输出行按最多 75 字节折叠且不拆开 UTF-8 字符。

## C 接口


`include/ical_native.h` 提供 `xi_parse(const char *text, size_t size)` 与 `xi_build(xvalue *calendar)`。解析结果由调用方 `xrtValueRelease`；生成的字符串由调用方 `xrtFree`。结果互相独立，没有静态共享槽。文件 IO 交给 XRT，本库仅处理内存文本。

## 验证

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release\extlibs\ical\test\check.ps1
.\release\xl.exe run test\cases\extlib_ical.xl
.\release\xl.exe run test\cases\extlib_ical_boundaries.xl
.\release\xl.exe run release\extlibs\ical\test\control_characters.xl
.\release\xl.exe run release\extlibs\ical\test\allocations.xl
.\release\xl.exe run release\extlibs\ical\test\text_identifiers.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package ical -Source release\extlibs\ical\test\test_ical.c
.\release\xl.exe build release\extlibs\ical\test\aot_no_index.xl -o build\test\ical_guard\aot_no_index.exe
.\build\test\ical_guard\aot_no_index.exe
.\release\xl.exe run release\examples\extlibs\ical.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package ical -Source release\extlibs\ical\examples\ical.c
.\release\xl.exe run release\extlibs\ical\examples\ical.c
```

在 Linux/WSL 的 `demo6` 目录另运行 `bash release/extlibs/ical/test/check.sh`。脚本从自身位置定位包和 XRT，在临时目录复用编译对象，运行 C ASan/UBSan、三条 XLang 用例及 C/XLang 同输出范例；生成的日历文件逐字节比较，并用 Python 标准库独立检查 UTF-8、CRLF、75 字节折行、字段与长中文描述。

当前发布物执行上述 Windows 默认门禁时，完整范例 AOT 构建会因共享符号缺失而失败；
局部复验 C/JIT 和线格式时可在 `check.ps1` 后加 `-SkipAot`，不能以快测替代发布验收。

C/XL 测试覆盖事件/待办、RRULE、TZID/VALUE、嵌套提醒隔离、重复分类、长中文字段、折行、异常输入、控制字符拒绝、内存故障与独立所有权；Windows 严格 GCC、Linux ASan/UBSan、双语同输出和无索引核心 AOT 通过。完整 AOT 仍受主线 `xl6ValueIndexReadCopyV1`、`xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/`，因此暂不签收正式发布级验收。

Windows 包级脚本在本次专属构建目录运行严格 GCC 的三个独立实现单元、
C 边界、中央及包内 XLang 用例、双语范例、Python 标准库线格式检查和
无索引核心 AOT，并继续构建运行完整范例 AOT、比较控制台输出与 `.ics`
字节及线格式；`-SkipAot` 仅供局部开发。C 教学范例显式包含三个
实现单元，可不带额外参数直接 `xl run`；正式包与 C 边界测试仍分别
编译实现文件，避免让范例的单文件写法改变发布构建结构。

`test/allocations.xl` 对 `parse/build` 各扫描前 48 个分配点，验证语言侧内存异常类别、成功路径和失败后中文字段往返；已登记包清单并加入 Linux 包级门禁。

另以独立的 [Python icalendar](https://pypi.org/project/icalendar/) 7.3.0 解析 C/xlang 范例和 3000 个中文字符的折行样例；两种语言生成的范例文件逐字节一致，长字段完整读回。`icalendar` 仅是可选测试依赖，不进入扩展库或发布物。从 `demo6` 目录运行：

```powershell
python -m venv build\test\ical\venv
.\build\test\ical\venv\Scripts\python.exe -m pip install icalendar==7.3.0
.\release\xl.exe build release\extlibs\ical\examples\ical.c release\extlibs\ical\ical_native.c release\extlibs\ical\ical_parse.c release\extlibs\ical\ical_build.c --inc-path lib\xrt --inc-path release\extlibs\ical\include -o build\test\ical_example.exe
.\build\test\ical_example.exe build\test\ical_example_c.ics
.\release\xl.exe run release\examples\extlibs\ical.xl -- build\test\ical_example_xl.ics
.\release\xl.exe build release\extlibs\ical\test\test_ical.c release\extlibs\ical\ical_native.c release\extlibs\ical\ical_parse.c release\extlibs\ical\ical_build.c --inc-path lib\xrt --inc-path release\extlibs\ical\include -o build\test\ical_test.exe
.\build\test\ical_test.exe build\test\ical_long.ics
.\build\test\ical\venv\Scripts\python.exe release\extlibs\ical\test\check_external.py build\test\ical_example_c.ics build\test\ical_example_xl.ics build\test\ical_long.ics
```

可选输出路径只用于保留测试产物；不传路径时 C/xlang 范例原有输出不变。外部解析成功不代表本包是完整 RFC 5545 验证器，能力边界仍以上文为准。

2026-10-14：生成器补顶层未知键拒绝，C 与 XLang 的 `event` 拼写错误先复现静默丢弃、修复后转为显式失败。正常双语范例、无索引 AOT 核心及 Linux ASan/UBSan 复验通过；完整 AOT 的共享符号阻断不变。

2026-10-15：审计原生实现的 XRT 调用，清单声明 buffer、Unicode 和 value 容器模块。同一无索引核心 AOT 从 3,610,624 字节降至 655,360 字节，裁剪前后均构建运行通过；Windows C/XLang 边界与控制字符测试、C/XLang 同输出范例、Linux ASan/UBSan 已复跑通过。完整边界 AOT 当前仍缺共享符号 `xl6ValueDataSnapshotV1`，不据核心用例签收完整 AOT。

2026-10-15：补生成结果超过 8 MiB 的 C 回归，先复现泛化的值错误，再在写入处统一报告 `XERR_RANGE`；Windows C 测试、XLang 边界用例、双语范例、无索引 AOT 核心与 Linux ASan/UBSan 复验通过。完整 AOT 的共享符号阻断不变。

2026-10-16：将包内控制字符用例登记为 `coverage.tests`，供目录 smoke
自动发现。当前发布物复跑该用例、两条中央 XLang 用例、C 测试及
C/XLang 同输出范例均通过；未重跑完整 AOT，既有主线阻断不变。

2026-10-17：Linux 包级门禁加入现有 XLang 边界、C/XLang 控制台及日历文件逐字节对照，新增无需额外安装的独立线格式检查；C 范例的失败路径统一清理值与输出缓冲。C ASan/UBSan、三条 XLang 用例、长中文折行检查同次通过；完整 AOT 主线阻断不变。

2026-10-17：修复公开头和内部头在清单预定义 XRT 模块宏时的重复定义；Linux 门禁新增清单宏严格编译。C ASan/UBSan、XLang 边界、线格式和双语输出复验通过；完整 AOT 共享阻断不变。

2026-10-18：`PRODID` 与 `UID` 属于 RFC 5545 的 TEXT 值；原实现将它们按原始线值处理，带逗号、分号或换行时写出错误格式，解析也不解码转义。C 线格式回归先红后绿，现复用 TEXT 解码/编码路径；`URL`、`RRULE` 保持原值。新增 XLang 精确线格式用例并接入门禁，完整 AOT 主线阻断不变。

同日增加一份含中文事件和嵌套提醒的完整 CRLF 日历，对其每个严格前缀执行长度感知解析：未写完最终 `END:VCALENDAR` 均必须失败并设置错误，完整输入必须成功。Windows C 定向测试、Linux C ASan/UBSan、XLang、线格式与双语联合门禁通过；这只覆盖固定样本的截断边界，不代替任意日历 fuzz 或完整 AOT 验收。

同日修复嵌套根组件：解析器原将根内的 `BEGIN:VCALENDAR` 当作未知子组件忽略，导致双层根的畸形日历被接受。C 负例先红后绿，XLang 负例纳入包清单和 Linux 门禁；Windows C/JIT、XLang JIT 与 Linux 严格 GCC、ASan/UBSan、线格式及双语同输出通过。正常的嵌套 `VALARM` 保持可解析；完整 AOT 共享缺符号阻断不变。

2026-10-19：C 教学范例原先不能单文件 `xl run`，首报缺少包头文件；
现显式包含包内三个实现单元，正式包与 C 边界仍分别编译。新增 Windows
一键门禁，组合严格 GCC、C 边界、两种 C 运行方式、全部现有 XLang
用例、双语范例、独立线格式检查及当前发布物的无索引核心 AOT，全部通过。
Linux C ASan/UBSan、XLang 与线格式联合门禁亦通过；完整边界 AOT 的
共享主线缺符号阻断不变。

2026-10-20：Windows 默认门禁在无索引核心 AOT 后增加完整教学范例
AOT 构建、运行、C 输出比较和独立 `.ics` 线格式/字节检查。`-SkipAot`
的严格 C、全部 XLang JIT、双语文件与线格式检查通过；默认门禁核心
AOT 构建运行通过，完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺
符号失败，后续 AOT 结果检查尚未执行。本次独占目录已清理，包实现
未改，不签收整包发布级。

同日补齐组件界定行的参数拒绝：此前 `BEGIN;X=1:...`、`END;X=1:...`
会被当作未知属性忽略，畸形日历因此可能通过解析。C/XLang 负例先红
后绿；Windows `-SkipAot` 严格 C、边界、双语范例与独立线格式检查，
Linux C ASan/UBSan、XLang 与线格式门禁均通过。完整范例 AOT 的共享
缺符号阻断不变，不据此签收发布级。
