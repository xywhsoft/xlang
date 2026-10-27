# cli

2026-10-28 最小复验：不导入 CLI、只调用 `os.args()` 的源文件也在
Windows run/build 与 Linux run 报 `xl6_xrt_os_args` 未解析，Windows
`check` 则通过。最小源、运行时符号注册位置和修复验收边界见
[专项记录](../../../dev/extlib_blockers/cli_os_args_symbol.md)。本包不
为标准库入口增加替代实现；以下 C 通过记录不等于完整语言签收。

`cli` 将参数数组按声明解析为旗标、选项和位置参数。XLang 使用 `import cli`；
C 使用 `include/cli_native.h`，调用方以 `xrtValueRelease` 释放 `xcli_parse`
返回的结果。解析不读取进程参数；只有显式调用 `cli.parseOs(spec)` 才读取。

```xlang
import cli

var spec = {"flags": ["verbose"], "options": {"output": "o"},
    "positional": ["src"]}
var result = cli.parse(["--verbose", "--output=out.txt", "in.txt"], spec)
assert result["ok"] == true
```

`spec.flags` 和 `spec.positional` 是非空且内部不重复的名称数组；`spec.options` 将长名称
映射到短名字符串，`spec.shorts` 可将旗标长名称映射到短名字符串，
`spec.defaults` 只为 `spec.options` 中已声明的选项提供默认值；每个默认值必须是
无内嵌 NUL 的字符串，与命令行解析得到的选项值保持同一类型。拼错或未声明的
选项名属于无效 spec，不会悄悄进入解析结果。名称不得为空、以 `-` 开头，或包含空白、
`=`、控制字符；短名字符串中的每个字符代表一个别名，限可打印单字节
ASCII，且不能是 `-`、`=`。选项、旗标别名和单字符旗标名之间不得重复；
`spec.shorts` 的长名称必须已在 `spec.flags` 声明，旗标也不能与长选项同名。
长选项名和位置参数名可使用有效 UTF-8 文本，但不得包含 Unicode 空白、
C0/C1 控制字符、常见零宽格式字符、双向文本控制字符或 BOM；短别名仍须是上述 ASCII 字节。
`parse` 的 spec 顶层只接受 `flags`、`options`、`shorts`、`defaults`、
`positional`；拼错字段名会报告声明错误，不会被忽略。
冲突或无效声明报错，不依赖映射遍历顺序选择其中一个。解析结果有 `ok`、
`error`、`flags`、`options`、`positional`、`rest`。用户参数错误返回
`ok=false` 和说明；错误的 spec 或含 NUL 的参数使 C 入口返回 `NULL`，
设置 XRT 线程错误。分配失败同样返回 `NULL`，保留 `XERR_MEMORY`，
不会伪装成参数类型错误。遇到第一条用户参数错误即停止解析，后续
旗标与选项不会生效，错误说明也不会被后续参数覆盖。XLang 包装层将
`NULL` 转为异常。

解析期间只借用 `args` 和 `spec` 的字符串视图，不修改输入，也不为每个
参数分配临时 C 字符串；结果中的选项、位置参数和剩余参数仍是独立拥有
的字符串，释放输入不会使解析结果失效。

长选项错误说明先在 XRT 内存中组装，再由结果对象直接接管；不再
额外复制整条消息。短固定错误说明仍从静态文本建立独立结果。
其余借用文本直接由 `xrtValueString` 建立拥有型结果，不再先分配
临时 C 字符串再交给 `xrtValueStringTake`；结果释放方式不变。

支持 `--name`、`--name=value`、`-a`、组合短旗标、短选项附加值和 `--`
之后的位置参数。旗标不能使用 `--name=value`；不会丢弃等号后的内容。
同一选项多次出现时最后一次生效；`--name=` 是有效的空字符串值，
会覆盖默认值。`--` 后的 `--name=value` 和 `-a` 都不再作为选项解析。
长选项值、声明名称及其错误说明均不设固定缓冲上限，受可用内存约束。
短选项名只接受可打印 ASCII；`-中` 等输入返回固定的 ASCII 错误文本，
不会把 UTF-8 的单个字节写进错误消息。短选项的附加值仍可使用 UTF-8，
例如 `-o中文.txt`。

## 验证状态

在 `demo6` 目录运行 `release/extlibs/cli/test/check_c.ps1`；Linux 运行
`bash release/extlibs/cli/test/check_c.sh`。它们验证严格 C 编译、解析边界、
C 范例与固定输出；Linux 还启用 ASan/UBSan。`examples/basic.xl` 可通过
`xl check`，但当前发布版执行 `xl run test/cases/extlib_cli.xl` 仍在主线
缺少 `xl6_xrt_os_args` 时链接失败，见 `dev/extlib_blockers/cli_os_args_symbol.md`。
因此本包的 XLang 运行、双语同输出和 AOT **尚未签收**；本任务不在包内
为该符号增加兼容回退。

2026-10-20：C 范例 `examples/basic_c.c` 仅在 TCC 单文件路径包含包内
解析器，可直接 `xl run` 或 `xl build`；正式包继续分离编译。Windows
严格 GCC/单文件构建运行及 Linux ASan/UBSan/单文件运行均与黄金输出一致。
这只收口 C 教学入口，不解除上述 XLang 运行时符号阻断。

同日修正 C/XL 同名范例的伴随源冲突：`basic.xl` 原会自动带入
`basic.c`，与包清单中的解析器重复定义 `xcli_parse`。C 范例改为
`basic_c.c`，两种语言的范例现在独立选择自己的入口。XLang 路径
仍由上述 `xl6_xrt_os_args` 主线缺符号阻断，不记为本包运行通过。

2026-10-16 补短别名声明校验：重复别名、非 ASCII/保留字节、未声明的
旗标目标及旗标/长选项同名均拒绝。C 红绿用例、Windows 严格 GCC 和
Linux ASan/UBSan 门禁通过；XLang 运行阻断未变。
同日修复长选项错误说明被 160 字节缓冲截断：未知选项、缺少值和旗标
误带值的 200 字符名称用例先红后绿；Windows C 范例及 Linux ASan/UBSan
复验通过。XLang 运行阻断未变。
同日补 C 逐点分配故障扫描，覆盖普通/声明位置参数、长/短选项取下一参数
和长选项错误说明。参数副本 OOM 原被覆盖为 `XERR_VALUE`，现四处调用
保留 `XERR_MEMORY`；故障清除后均可再次解析。Windows 严格 GCC、
Linux ASan/UBSan 通过，XLang 运行阻断未变。
2026-10-18 补短选项错误后的停止边界：原解析器会继续处理后续参数，
使后来的长选项错误覆盖第一条错误，且可能设置后续旗标。C 回归先红后绿；
Windows 严格 GCC/C 范例及 Linux ASan/UBSan 复验通过，XLang 运行阻断未变。
同日补默认值声明边界：`spec.defaults` 中未声明的选项原会被静默加入结果；
现按无效 spec 拒绝，并覆盖已声明默认值仍正常生效的正例。
重复的位置参数名原会覆盖较早位置的值；现在 `positional` 和 `flags` 均
拒绝重复名称，C 负例先红后绿。

同日补 spec 顶层字段校验：`flagz` 等未知字段原被静默忽略，空参数解析
甚至报告成功。C 负例先红后绿；Windows 严格 C/范例和 Linux
ASan/UBSan 门禁通过。当前 `xl run test/cases/extlib_cli.xl` 仍因
`xl6_xrt_os_args` 未解析失败，XLang 运行与完整 AOT 继续待主线修复。

2026-10-18 再补默认值类型边界：整数默认值原会使同一选项在有无命令行输入时
分别成为整数或字符串，含 NUL 的默认文本也会原样进入结果。C 负例先红后绿，
现在两者均按无效 spec 拒绝；合法字符串默认值保持可用。Windows 严格 GCC/C
范例及 Linux ASan/UBSan 门禁通过，XLang 运行与 AOT 的主线阻断不变。

2026-10-19：新增 UTF-8 长选项名和位置参数名的 C 边界用例，分别通过
`--名称=值` 和 ASCII 短别名传值，核对名称与中文值完整保存。Windows 严格
GCC/C 范例与 Linux C ASan/UBSan 包门禁通过。当前 XLang `xl run` 仍缺
`xl6_xrt_os_args`；未把 C 侧通过当作双语或 AOT 发布验收。

2026-10-20：C 负例确认声明名称原可包含不换行空格、全角空格、下一行
控制符或无效 UTF-8，与公开的 UTF-8/禁空白合同不符。名称校验现复用
XRT UTF-8 解码并逐标量拒绝 Unicode 空白和 C0/C1 控制字符；中文正例
继续通过。Windows 严格 GCC/C 与 Linux ASan/UBSan 门禁通过；当前
`xl run test/cases/extlib_cli.xl` 仍缺 `xl6_xrt_os_args`，语言运行、
双语同输出和完整 AOT 继续待主线修复。
Windows/Linux C 门禁现在各用本次独占构建目录，结束时只清理该目录，
不覆盖并行门禁的对象或已有固定目录中的文件。

2026-10-20：名称校验补拒绝零宽空格、双向覆盖/隔离及 BOM 等不可见字符；
C 负例先复现接受，再确认拒绝，普通中文名称仍可解析。完整 XLang 运行与
AOT 的共享主线阻断不变。

2026-10-21：C 边界补 `--` 终止选项后的组合用例：其后的 `-v` 进入声明的
位置参数，`--output=x` 与单独 `-` 进入剩余参数；旗标不会被误设置。
Windows `test/check_c.ps1` 的严格 C、固定范例与 C 单文件 JIT/AOT，
以及 Linux `test/check_c.sh` 的 C ASan/UBSan 均通过。当前发布版运行
`test/cases/extlib_cli.xl` 仍缺 `xl6_xrt_os_args`，故本轮没有验证 XLang
运行或双语同输出，也不签收完整 AOT。

2026-10-23：长选项错误消息的已拥有内存直接交给 `xvalue` 字符串，
避免组装后再次整段复制。未知、缺值、旗标误带值三条长消息内容保持；
未知选项的逐点分配失败扫描增加活动分配基线。Windows 严格 C 门禁
与 Linux C ASan/UBSan 门禁通过。当前 `xl run test/cases/extlib_cli.xl`
仍缺 `xl6_xrt_os_args`，本轮不声称 XLang 运行或完整发布验收通过。

2026-10-25：C 解析主循环改用借用的长度视图，去掉逐参数及位置名称的
临时分配，长选项拆分不写回输入。长值、短值、组合旗标、`--`、默认值
和分配故障仍由 Windows 严格 C 与 Linux ASan/UBSan 门禁覆盖；新增
原输入保持不变的断言。XLang 运行继续受 `xl6_xrt_os_args` 阻断。
