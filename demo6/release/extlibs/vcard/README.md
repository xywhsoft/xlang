# vcard

2026-10-22：生成器现在将 XRT 缓冲直接移交给调用方，去掉写出完成后
额外分配和复制整张名片的步骤；返回值仍由 C 调用方 `xrtFree`，4 MiB
内容限额不变。Windows 聚焦 C/XLang、分配故障和双语门禁，以及 Linux
C ASan/UBSan、XLang 和双语门禁通过。默认 Windows 门禁的完整范例 AOT
仍缺共享 `xl6ValueDataSnapshotV1`，未签收整包发布级。

轻量的 vCard 3.0/4.0 **常用字段值映射**，适合通讯录导入、展示和导出。`parse(text)` 返回对象：字段名为小写，单值为字符串，重复字段自动成为字符串数组；`build(card)` 反向生成同版本文本，未指定 `version` 时默认 3.0。支持 `fn`、`n`、`email`、`tel`、`org`、`title`、`adr`、`note` 等常用字段，以及 UTF-8、转义和折行。

```xlang
import vcard

var contact = {"fn": "王小明", "email": ["home@example.com", "work@example.com"]}
string text = vcard.build(contact)
var back = vcard.parse(text)
assert back["email"].length == 2
```

这是**值提取器，不是无损 vCard DOM**：解析时忽略未知属性与参数（例如 `TEL;TYPE=CELL` 的 `TYPE`）；构建时拒绝未知键，不把它们静默写丢。`N`、`ADR`、`ORG` 保留调用方提供的结构化线值；`URL`、`IMPP`、`PHOTO`、`LOGO`、`GEO` 保留 URI/原始线值，不施加 TEXT 的逗号、分号转义；`FN`、`NOTE`、`EMAIL` 等文本字段仍按 TEXT 转义。`TEL` 按默认 TEXT 值处理，`TEL;VALUE=uri` 的参数不往返。仅接受单张 3.0/4.0 名片，必须有 `BEGIN:VCARD`、`VERSION`、非空 `FN` 和 `END:VCARD`。为兼容实际数据，3.0 读取/生成允许缺少 `N`，这比 [RFC 2426](https://www.rfc-editor.org/rfc/rfc2426.html) 宽松；4.0 的 `VERSION` 与 `FN` 要求见 [RFC 6350](https://www.rfc-editor.org/rfc/rfc6350.html)。不宣称复杂参数/分组或原始文件无损往返。

输入和输出均限制为 4 MiB，生成超限时 C 接口报告 `XERR_RANGE`；拒绝内嵌 NUL、非法 UTF-8、非法 ASCII 控制字符、畸形包络及不合法字段类型，不进行静默截断。4.0 的 `VERSION:4.0` 必须紧随 `BEGIN:VCARD`，中间不能有其他属性或空行；3.0 仍允许 `VERSION:3.0` 出现在 `FN` 后。解析时未知属性也要满足内容行的字符约束；TEXT 内存换行由生成器转义，HTAB 允许使用。生成器在不拆开 UTF-8 字符的前提下把物理行折至最多 75 字节；多值字段逐项输出。所有错误都可由 xlang 捕获，C 侧失败返回 `NULL`。
3.0 的 `VERSION` 不允许属性参数；4.0 允许如 `VERSION;VALUE=text:4.0` 的写法，但本值映射器不保留参数，重新生成时写作普通 `VERSION:4.0`。
`VERSION` 是整张卡片的结构属性，不接受 `item1.VERSION` 这样的分组前缀；`item1.BEGIN` / `item1.END` 也不能伪装成未知字段绕过包络校验。普通分组字段如 `item1.EMAIL` 仍可读取其值，分组名不会保留。

超过 4 MiB 的解析输入会在读取内容前返回 `XERR_RANGE`；XLang 的
`parse` 与 `build` 对容量超限均抛 `RangeError`。空输入、非法 UTF-8
与畸形结构仍属值错误。超限失败后可继续解析其他名片。

## C 接口

`include/vcard_native.h` 公开 `xv_parse(const char*, size_t)` 和 `xv_build(xvalue*)`。解析结果由调用方 `xrtValueRelease`，生成文本由调用方 `xrtFree`；二者不共享静态结果槽。文件读写交给 XRT `xrtFile*`，本库只处理文本。

## 验证

Windows 从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/vcard/test/check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/vcard/test/check.ps1 -SkipAot
```

联合验证严格 GCC、C/XLang 边界与范例控制台输出，逐字节比较两份
生成的 `.vcf` 文件，并检查写入目录路径时失败且不输出成功标记。
默认门禁另构建运行无输出核心与带输出完整范例 AOT，将后者的控制台
输出和 `.vcf` 字节与 C 范例比较；`-SkipAot` 仅用于包侧聚焦验证，
不能视为发布签收。AOT 产物只写入本次独占临时目录并在结束时清理。
C/XLang 范例还各自断言完整的 3.0 文本、CRLF 行尾，以及读回后的姓名与两条邮箱；双语一致不能代替这些固定预期。

在 `demo6/release` 可直接运行 `xl run extlibs/vcard/examples/vcard.c`；
普通 C 编译仍按包清单分别链接三个实现单元。可选文件路径通过
`xl run extlibs/vcard/examples/vcard.c -- contact.vcf` 传入。

从 `demo6` 目录运行：

```powershell
.\release\xl.exe run test\cases\extlib_vcard.xl
.\release\xl.exe run test\cases\extlib_vcard_boundaries.xl
.\release\xl.exe run release\extlibs\vcard\test\control_characters.xl
.\release\xl.exe run release\extlibs\vcard\test\allocations.xl
.\release\xl.exe run release\extlibs\vcard\test\uri_values.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package vcard -Source release\extlibs\vcard\test\test_vcard.c
.\release\xl.exe build release\extlibs\vcard\test\aot_core.xl -o build\test\vcard_aot_core.exe
.\build\test\vcard_aot_core.exe
.\release\xl.exe run release\examples\extlibs\vcard.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package vcard -Source release\extlibs\vcard\examples\vcard.c
```

在 Linux/WSL 的 `demo6` 目录另运行 `bash release/extlibs/vcard/test/check.sh`；脚本在临时目录编译运行，不写入发布目录。它同时执行 C ASan/UBSan、四组 XLang JIT 用例，并逐字节比较 C/XLang 范例的输出和生成的 `.vcf`。此门禁不覆盖完整 AOT。

C/XL 测试覆盖 3.0/4.0 常用字段、重复字段、中文结构化字段、转义/反转义、9 KiB 长字段、75 字节折行、畸形包络、NUL/非法 UTF-8、非法控制字符、4 MiB 限额、独立结果和分配失败。Windows 严格 GCC、Linux ASan/UBSan、双语同输出及无输出 AOT 核心已通过。完整带输出范例 AOT 仍受主线 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`，因此尚未签收正式发布级验收。

解析重复字段与生成文本另扫描前 64 个分配故障位置：注入确实覆盖失败和成功分支，失败返回空结果并保留 `XERR_MEMORY`，清除故障后完整往返仍可用。Windows TCC C 测试、严格 GCC 语法检查和 Linux ASan/UBSan 包级门禁通过。

另以独立的 [vobject](https://pypi.org/project/vobject/) 0.9.9 解析 C/xlang 生成的 vCard 3.0 范例、3000 字中文折行样例，以及 C 测试生成的 vCard 4.0 样例；双语 3.0 范例文件逐字节一致。`vobject` 是可选测试依赖，不进入发布包。独立验证只覆盖这些固定样本，不代表完整 4.0 互操作认证。从 `demo6` 目录运行：

```powershell
python -m venv build\test\vcard\venv
.\build\test\vcard\venv\Scripts\python.exe -m pip install vobject==0.9.9
.\release\xl.exe build release\extlibs\vcard\examples\vcard.c release\extlibs\vcard\vcard_native.c release\extlibs\vcard\vcard_parse.c release\extlibs\vcard\vcard_build.c --inc-path lib\xrt --inc-path release\extlibs\vcard\include -o build\test\vcard_example.exe
.\build\test\vcard_example.exe build\test\vcard_example_c.vcf
.\release\xl.exe run release\examples\extlibs\vcard.xl -- build\test\vcard_example_xl.vcf
.\release\xl.exe build release\extlibs\vcard\test\test_vcard.c release\extlibs\vcard\vcard_native.c release\extlibs\vcard\vcard_parse.c release\extlibs\vcard\vcard_build.c --inc-path lib\xrt --inc-path release\extlibs\vcard\include -o build\test\vcard_test.exe
.\build\test\vcard_test.exe build\test\vcard_long.vcf build\test\vcard_four.vcf
.\build\test\vcard\venv\Scripts\python.exe release\extlibs\vcard\test\check_external.py build\test\vcard_example_c.vcf build\test\vcard_example_xl.vcf build\test\vcard_long.vcf build\test\vcard_four.vcf
```

可选输出路径只用于保留测试产物；不传路径时双语范例原有输出不变。
2026-10-16 从当前 C 源码重建 4.0 名片，独立解析器读回版本、姓名和电话 URI 通过；3.0 双语样本与长中文备注也重新通过。Windows C 定向测试和 Linux ASan/UBSan 门禁通过；完整 AOT 的共享缺符号阻断不变。

2026-10-15：包清单显式声明实际使用的 XRT Buffer、Unicode 与 value 容器模块，原生头文件改为条件定义模块宏。相同无输出 AOT 核心从 3,549,184 字节降至 652,800 字节，构建运行通过；C/XLang 边界、双语逐字同输出和 Linux ASan/UBSan 门禁复跑通过。完整带输出 AOT 仍缺共享 `xl6ValueDataSnapshotV1`，不据此签收发布级别。

2026-10-15：已有 4 MiB 生成限额用例原只验证失败，补 `XERR_RANGE` 断言后先红后绿；公共写入函数统一设置范围错误，并避免容量减法下溢。Windows C、XLang 边界、双语范例、无输出 AOT 核心及 Linux ASan/UBSan 通过；完整 AOT 共享符号阻断不变。

2026-10-15：新增 XLang 入口的前 24 个分配故障点测试，分别验证 `parse`/`build` 抛出 `MemoryError`、未误报格式错误，并在清除故障后恢复。`coverage.tests` 已登记它和控制字符负例；`tools/smoke_extlib_catalog.ps1 -Package vcard` 自动运行 3 项且全部通过。完整 AOT 阻断不变。

2026-10-17：Linux 包级门禁补上正式 `release/xl` 的主用例、边界、控制字符和分配故障用例；C 范例在 ASan/UBSan 下运行，并与 XLang 范例逐字节对照标准输出和 `.vcf` 文件。全部通过；完整 AOT 仍受共享文本快照缺符号阻断。

2026-10-18：URI 字段原被当作 TEXT 转义，生成 `URL:https://e.test/a\\,b\\;c=1` 等非原始 URI。C 线格式负例先红后绿；`URL` 在 3.0/4.0、`IMPP/PHOTO/LOGO/GEO` 在 4.0 均保留原值，`FN` 的 TEXT 转义保持不变。新增双语边界用例并接入包级门禁；完整 AOT 主线阻断不变。

同日增加固定 vCard 4.0 样本的逐位置单字节变异矩阵：每次解析若失败须有错误；若成功则生成结果必须可再次解析，且已映射字段的值与首次解析结果递归相等。Windows C 定向测试与 Linux C ASan/UBSan、XLang、双语门禁通过，未发现解析/生成自洽性缺陷。矩阵只覆盖当前字段和七种替换字节，不等于全格式 fuzz 或 4.0 原始文本无损往返；完整 AOT 阻断不变。

同日修复单卡包络：卡片内部的 `BEGIN:`/`END:` 原被当成未知属性忽略，可使重复 `BEGIN:VCARD` 或游离 `END` 的畸形输入通过解析。C 负例先红后绿，XLang 负例进入包清单和 Linux 门禁；Windows C/JIT、XLang JIT、Linux 严格 GCC、ASan/UBSan、C/XLang 双语同输出及目录 smoke 5/5 通过。完整 AOT 共享缺符号阻断不变。

2026-10-19：新增 Windows 包级 `test/check.ps1`，严格 GCC、C 边界、
六组 XLang 用例、包运行器 C/单文件 C/XLang 范例同输出以及 `.vcf`
生成字节对照均通过。使用独立系统临时目录并测试目录写入失败路径；
本轮未重跑既有 Linux ASan/UBSan 门禁，完整带输出 AOT 仍被共享符号阻断。

2026-10-20：补齐 4.0 包络顺序校验。C/XLang 负例先复现 `FN`、未知属性及空行插在 `BEGIN` 和 `VERSION:4.0` 之间仍被接受，再收紧解析；保留 3.0 的晚置 `VERSION` 正例。Windows 包级门禁和 Linux C ASan/UBSan、XLang JIT、双语范例门禁复跑通过；完整带输出 AOT 的共享缺符号阻断不变。

2026-10-21：C/XLang 负例确认 `VERSION;X=1:3.0` 原被错误接受；根据 vCard 3.0 的 VERSION 参数约束，仅在识别出 3.0 时拒绝参数。4.0 的 `VERSION;VALUE=text:4.0` 保持可解析。Windows 包级门禁与 Linux C ASan/UBSan、XLang JIT 和双语范例均通过；完整带输出 AOT 未在本轮重测，仍待主线缺符号修复。

同日：C/XLang 负例先复现 `item1.VERSION:4.0` 被当成整卡版本；现在 3.0/4.0 的分组 `VERSION` 均拒绝，普通 `item1.EMAIL` 仍可提取，4.0 的参数正例保留。Windows 包级门禁与 Linux C ASan/UBSan、XLang JIT、双语范例通过。完整带输出 AOT 的共享缺符号阻断不变。
