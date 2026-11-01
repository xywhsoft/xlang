# cli

## 2026-11-02 复用结果字典查询旗标

解析前已经把全部声明旗标初始化到结果字典，长旗标和隐式单字符短
旗标现直接查询该字典，不再每遇一个参数就线性扫描声明数组。
字典中的 `false` 也是已声明的非空值，不能用真假值判断名称是否存在。
别名/名称/冲突校验、首错即停、默认值、结果所有权及公开API/ABI不变；
不增加索引对象、缓存、副本或分配，生产净增一行说明注释。
这是内部优化，没有把旧解析正确的结果误报为BUG。

新 `test/flag_lookup.c` 固定0/1/2/8/64/256个声明和八种输入，共48组：
未选择的false、首尾/逆序/重复命中、直接短旗标组合、中文名称、未知
长短名、非法旗标赋值及首错后的旗标不生效。三条路径枚举83个分配
失败点，立即恢复、旧结果/释放全部输入后的寿命与七项内存计数通过；
旧新48组解析分配次数逐项相同，GCC/TCC/独立程序也按同表核对。
原36组默认值的882故障点和完整C边界仍保留。

新双语 `examples/flag_selection.xl` / `flag_selection_c.c` 展示大小写
精确选择、直接短旗标、中文、未选择状态和首错，匹配同一五行golden，
默认无文件、无子进程。XLang示例读取JSON配置值；这不代替原语言
数组/字典字面量的必过正例，更没有在包中增加容器投影兼容层。

```text
release/xl.exe run release/extlibs/cli/examples/flag_selection.xl
release/xl.exe run release/extlibs/cli/examples/flag_selection_c.c
```

Linux将 `xl.exe` 换为 `xl`。本轮Windows完整门禁62阶段中54符合要求，
Linux原生61阶段中53符合，均剩同八条M1运行正例失败；没有把默认门禁
的退出1改成通过。两平台各十条独立程序成功构建，六条运行过，四条
原字面量/投影运行仍失败；新双语和故障测试独立程序均过。Windows
50输入工具指纹、Linux47文件/三工具及boot保持，Linux C另含
ASan/UBSan/泄漏检查。manifest补记原basic/options及新范例的coverage，
不只登记可运行范例来隐藏旧红项。当前包保持M，M7不回退、M1不关闭。

单独 `test/bench_flags.c` 是固定200次完整parse/验证/结果释放的局部
基准，使用同平台GCC-O2与同一带MEMORY_DEBUG的测试运行库，三轮交替
旧/新。32旗标/512参数中位数Win54→16ms、Linux60.626→14.126ms；
256/512为466→105、405.265→99.649。单参数不宣称收益：Win256/1
为96→97、Linux32/1为4.424→4.549ms；其他原始数据也保留。数字来自
各平台CRT clock，仅作平台内部比较，不外推真实命令行、发布程序或
所有负载。声明重复校验、结果构建和必要输出仍存在，没有零开销承诺。
原生对象text Win减少28B、Linux减少156B，data/bss不变；这不是最终
程序大小。完整记录见仓库 `docs/reports/extlib-cli-flags-20261102.json`。
IDE资料候选仍按独立发布绑定门验收，不用包内结果替代整版交付。

## 2026-11-01 当前状态

原M7缺符号已解除：两平台的无包os.args、CLI最小导入和独立程序通过，
两种DLL/SO的两个上下文A/B/A交替调用也通过。UTF-8、空串、空格参数及
返回数组独立性已补查，证据见`docs/reports/extlib-cli-os-args-20261101.json`。

按现行标准库规范，`os.args()`返回宿主进程向量，含argv[0]；脚本main
的arg是独立的调用参数。`parseOs`读取前者，不自动切换成后者。
旧阻断记录中假定两者必须相同的建议已纠正；差异本身不是BUG。

本包仍为M：原数组/对象声明传给真实解析器的M1在两平台仍重现，C正
对照成功不替代完整语言正例。本次没有修改CLI实现或xlang主线，也不
把导入恢复直接写成整包发布完成。以下旧日期结果是历史冻结证据。

2026-10-30：默认值只能是字符串，现用 XRT 标量复制入口替代图深拷贝，
并从公开头/manifest 移除 VALUE_GRAPH 依赖。旧接口对字符串本来也只是
增加引用，不宣称减少字符串副本或分配；36 组空/中文/8KiB 默认值组合的
882 个逐点失败在修改前后均通过，包含立即恢复、共享默认值和旧结果寿命。
两平台默认各 39 阶段、29 满足要求，10 条语言路径仍真实失败；不导入本包语言
模块的真实解析器最小源，在避开 M7 后仍遇 M1 容器投影。不能把修复 os.args
等同于 CLI 已可运行，不能按 C 通过签收整包；复现见下方验证状态。

同一 basic C 消费者、同一冻结 XRT、同平台 GCC -O2/函数数据分段/gc-sections：
Windows GNU size 的 text 为 234804→223608（减少 11196）字节，Linux 为
89087→75615（减少 13472）字节；Linux data 2088→1960，Windows data 不变。
旧图复制入口的 GCC 静态栈帧分别为 3184/3168 字节；含默认值的解析不再进入
该图路径，basic 消费者也不再携带该实现。这里只量这个 C 消费者，不把 size
text 等同于文件大小或精确机器码，
也不外推 XLang 产物、总栈上限、分配减少或端到端性能。

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

var spec = json.parse(r"""{"flags":["quiet","Quiet","q","Z","开关","pending"]}""")
var args = json.parse(r"""["--Quiet","-qZ","--开关"]""")
var result = cli.parse(args, spec)
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
默认值是不可变 XRT 字符串，结果保留其拥有型引用，不做正文深拷贝；
替换 spec 的默认项或释放全部输入不会改变已返回结果。通过 C 的
`xrtValueClone` 直接完成同样的标量操作，不需要声明 VALUE_GRAPH。

支持 `--name`、`--name=value`、`-a`、组合短旗标、短选项附加值和 `--`
之后的位置参数。旗标不能使用 `--name=value`；不会丢弃等号后的内容。
同一选项多次出现时最后一次生效；`--name=` 是有效的空字符串值，
会覆盖默认值。`--` 后的 `--name=value` 和 `-a` 都不再作为选项解析。
长选项值、声明名称及其错误说明均不设固定缓冲上限，受可用内存约束。
短选项名只接受可打印 ASCII；`-中` 等输入返回固定的 ASCII 错误文本，
不会把 UTF-8 的单个字节写进错误消息。短选项的附加值仍可使用 UTF-8，
例如 `-o中文.txt`。

## 验证状态

两平台 `test/check_c.ps1` / `check_c.sh` 保留入口文件名，但现在不只检查 C：
默认还执行原始中央用例、两份 XLang 范例、无包 os.args、独立容器投影
的 check/JIT/AOT，以及真实生成的 xcli_parse 原型。各正例仍要求退出 0，
多个语言失败独立留档后聚合退出，不能按已知错误改成预期通过。
全部外部进程有超时，记录实际 CLI/DLL/源码/编译器的前后哈希；成功才
清理独占目录，失败保存现场。Windows 报告在工作目录及旁路 `.json`，
Linux 保留每步 command/stdout/stderr/exit、phases.tsv 和旁路 `.report.txt`。
Linux C 同次启用 ASan/UBSan/泄漏检查；测试消费者明确不启用图模块。

新 `examples/options.xl` / `options_c.c` 共用六行 expected：显式短别名、
组合旗标、附加中文值、覆盖默认值、`--` 与首错后停止。C 可直接 xl run/build；
XL 范例为共享层修复后的验收资产，当前不能声称已运行或双语一致。
当前公开语言导入仍有 M7；单独调用真实 C 解析器的
`dev/extlib_blockers/cli_args_projection.xl` 在 Windows/Linux JIT/AOT 均报
`parse requires an args array and a spec object`，是独立 M1 复现，
不是供用户绕开 import 的替代 API。本任务不识别私有 handle、不把字面量
改写成 json.parse 来换绿灯。修复建议与平台结果见
[os.args 阻断](../../../dev/extlib_blockers/cli_os_args_symbol.md)及
[容器投影记录](../../../dev/extlib_blockers/foreign_var_container_projection.md)。
当前完整门禁中，五份语言 check/cgen 都通过；中央、两份公开例及无包
os.args 的 JIT/build 共八项仍缺 `xl6_xrt_os_args`；独立投影的 JIT/AOT
共两项仍在真实解析器的数组/对象类型检查失败，不归咎于原生选项算法。
Windows 39 项输入、Linux 38 项输入在执行前后变化为零；Linux 还核对
51 份复制源（含旧对照包）与原件，C 含 sanitizer，编译警告为零。
旧/新两份消费者均通过同样的 36 组合/882 故障，结果与存活内存一致；
原型错改返回类型的隔离负例在 LLP64/LP64 均拒绝。PS5/PS7 的真实静默
编译器故障保存 raw37、外层1；Linux 缺编译器保存原始127，不丢失败现场。

在仓库根目录运行 `release/extlibs/cli/test/check_c.ps1`；Linux 运行
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
