# fix

`fix` 是 FIX 4.x 文本报文的**帧编解码器**，不是交易会话引擎。它自动生成
`8=BeginString`、`9=BodyLength`、`10=CheckSum`，按传入顺序保留应用字段，
也保留重复标签。它不连接交易所、不管理序号/重发，也不验证场所数据字典。

```xlang
import fix

var fields = [fix.field(35, "D"), fix.field(49, "SENDER"), fix.field(56, "TARGET")]
string message = fix.build("FIX.4.4", fields)
assert fix.validate(message)
var decoded = fix.parse(message)
```

`field(tag, value)` 创建有 `tag` 与 `value` 的对象；`build(beginString, fields)`
接受这些有序对象，也接受同结构的对象，其值可为字符串、有符号或无符号整数、浮点或布尔值。
浮点字段使用 XRT 写出区域无关的十进制文本，小数点固定为 `.`，
不受进程 `LC_NUMERIC` 影响。
应用字段不可使用保留标签 8、9、10，文本值不可含 SOH (`\x01`) 或 NUL。
XLang 的 `field`、`build`、`parse` 在交给 C 字符串接口前拒绝内嵌 NUL；
`validate` 对这类输入返回 `false`，不会把后缀静默丢弃。C 公开操作
会清理前一次操作遗留的 XRT 错误状态。
单帧限制 16 MiB、最多 4096 个**应用字段**；8、9、10 是额外的帧字段，
所以最大合法报文的解析结果含 4099 项。`parse` 返回 `{valid, fields}`，
其中 `fields` 包含 8、9、10；输入无效时抛 `ValueError`。
`validate` 对无效报文返回 `false`，不留下预期解析错误；内存分配失败
不是无效报文，C 侧保留 `XERR_MEMORY`，XLang 侧抛 `MemoryError`。
`field`、`build`、`parse` 的原生分配失败在 XLang 侧同样抛 `MemoryError`，
格式或参数错误仍抛 `ValueError`。C 可通过 `xfix_has_error` 和
`xfix_memory_error` 在失败后检查本线程的 XRT 错误状态。
构建和解析都要求 `8=BeginString` 非空；校验和即使正确，空 `8=` 也不是有效帧。
原生 C 的 `build` BeginString 与 `field` 文本入口在读取 NUL 终止符时也按
16 MiB 限界扫描；超过上限返回 `XERR_RANGE`，不会先无界扫描或分配字段对象。
单独的 `field` 文本可恰好为 16 MiB；完整报文仍须为 8、9、10 字段留出空间，
因此该极限字段不能构建成符合单帧上限的报文。

对应的 C 声明在 `fix_native.h`。`xfix_build` 返回独立的 XRT 缓冲区，
C 调用方用 `xrtFree` 释放；`xfix_field`/`xfix_parse` 返回的 `xvalue*`
用 `xrtValueRelease` 释放。XLang 包装会处理这些所有权。
`examples/fix_c.c` 可在 `demo6/release` 下直接运行：

```powershell
./xl run extlibs/fix/examples/fix_c.c
```

范例为单文件运行显式包含 `fix_native.c`；正式扩展库仍通过包清单将
`fix_native.c` 作为独立 C 源文件编译，不应在同一构建中重复加入它。

在 `demo6` 目录运行定向测试：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/fix/test/check.ps1 -SkipAot
bash release/extlibs/fix/test/check.sh
```

去掉 `-SkipAot` 可增加独立可执行文件验证。C 与 XLang 的下单报文范例
在同一脚本中逐字对照，并由独立 Python 检查器按原始字节重算示例
报文的 `9=BodyLength`、`10=CheckSum`，核对字段顺序；此检查不调用
本包的 `validate`，Windows/Linux 包级门禁均需 Python 3。
当前 Windows 严格 GCC、C/XLang 边界、双语范例及 Linux 严格 GCC +
ASan/UBSan C 边界通过；4096/4097 应用字段及最大字段数报文回读也已覆盖。
四个公开入口的首次分配失败、错误分类与恢复由 C 和 XLang 专项测试覆盖。
当前完整 AOT 在 `xl6ValueDataSnapshotV1` 缺符号处
阻断，见 `dev/extlib_blockers/text_snapshot_link.md`，尚未签收完整发布门禁。
新增 `test/aot_core.xl` 独立检查构建帧与校验的 AOT 主路径；包 manifest
只声明原生源码使用的 XRT 缓冲、值容器与浮点数字模块。同一核心产物从
3,595,776 缩至 641,024 字节，执行通过；完整边界 AOT 的共享阻断不变。

2026-10-15：修复逗号小数点区域设置下浮点字段被写成 `44=1,5` 的问题；
Windows C 真实报文负例先红后绿，C/XLang 包级门禁及无输出核心 AOT 通过。

2026-10-17：Linux `test/check.sh` 在 C ASan/UBSan 测试后运行中央主用例、
包内边界和分配失败用例，并逐字节对照 C/XLang 范例输出；两份包内
XLang 用例均登记到 `coverage.tests`。整套门禁通过。
完整 AOT 的共享链接阻断不变。

2026-10-17：原生 C `xfix_parse` 原在检查 16 MiB 单帧上限前无界
`strlen`。16 MiB 加 1 字节的未终止输入由 ASan 复现越界读取，现最多
探测上限再加一个字节，超限仍返回 null 值和 `XERR_ARGUMENT`。
Linux C ASan/UBSan、XLang/双语联合门禁及 Windows `-SkipAot` 包级门禁
通过。C 接口仍要求有效 NUL 结尾字符串，完整 AOT 阻断未变。

2026-10-18：补齐另外两处 C 文本入口的相同边界。`build` 的 BeginString
原先无界 `strlen`，超限误报 `VALUE`；`field` 先无界 `strchr/strlen`，
还能构造超过单帧上限的字段。两条 C 负例先红后绿，且对无终止符
缓冲区按限额返回 `RANGE`。Windows 严格 GCC/C/XLang/双语及 Linux
ASan/UBSan 与 XLang 对照门禁通过；完整 AOT 的主线缺符号阻断不变。

2026-10-19：C 字符串扫描原在恰好 16 MiB 的终止符处提前报超限；
新增红绿回归后接受该长度的独立 `field` 文本，继续拒绝再长一字节和
上限附近无终止符输入。Windows 严格 C/XLang 包级门禁与 Linux C
ASan/UBSan、XLang、双语范例门禁通过；完整 AOT 的共享阻断不变。
当前发布版直接构建完整 XLang 范例时，首个缺失符号为
`xl6ValueDataSnapshotV1`；旧 `xl6ValueIndexReadCopyV1` 记录是历史快照。

2026-10-19：修复 C 范例单独 `xl run` 缺 `xfix_parse` 等实现符号的问题。
Windows 严格 GCC、单文件 TCC、C/XLang 同输出及 Linux C ASan/UBSan、
Linux TCC/XLang 同输出门禁通过；正式包构建仍分离编译原生实现，
完整 AOT 的共享缺符号阻断不变。

2026-10-20：`build` 原先拒绝 XRT `XVALUE_UINT`，与公开的“整数字段值”
口径不一致。C 以 `UINT64_MAX` 红测复现后，原生字段写出器增加无符号
十进制分支；XLang `uint64` 字段值同步回归。Windows 严格 C/XLang 与
双语范例、Linux C ASan/UBSan 和 XLang 联合门禁通过。完整 AOT 本轮
未重验，仍按已登记的共享缺符号阻断处理。

同日包级 Windows 门禁改为每次使用独占构建目录并重新编译 XRT，结束时
仅清理本次目录，不复用固定路径下的对象文件。`-SkipAot` 的 C/XLang
边界与双语范例通过；默认门禁的核心 AOT 也通过，但边界用例构建缺
共享 `xl6ValueIndexReadCopyV1`，失败后目录已清理。前述完整范例缺
`xl6ValueDataSnapshotV1` 是不同构建目标的记录，二者均未关闭。

`build` 与 `parse` 的 C 回归现逐点注入前 128 个可能分配位置，要求确实
触发多个失败点；每次内存失败不得交出报文或半成品对象，且故障解除后
原报文仍可解析。当前 Windows 严格 C/XLang 包门禁与 Linux C
ASan/UBSan、XLang 双语门禁通过；未发现需修改的包内编解码逻辑。
