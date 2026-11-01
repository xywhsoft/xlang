# BSON 扩展库

## 2026-11-01 主目录现行发布物复验

当前状态为 **R：本体和完整语言链路已恢复，XRT 文件失败回滚仍有阻断**。
原最小字典、嵌套数组/字典、全部六份包级语言测试、完整双语文件往返和
15 份独立程序均已实跑，不再将旧 M1 当作本包当前状态；也不由此推断
其他包的通用数组/字典投影已经修好。本轮没有修改主线或替换 XRT。

- 将三条文本返回直接声明为 `@owned string`，去掉三个 `void*` 跳板和
  `takeCstr` 中转。生成 C 的三个实际返回函数直接接管 `char*`，没有正文
  复制；旧路径本来也已接管正文，因此本轮不声称消除了不存在的复制。
  一次长度扫描、FFI 错误作用域、输入装箱等成本仍存在。
- 原负毫秒 Date 的测试/范例仍按旧微秒纪元判断，数值回归出现36项失败；
  现统一使用 AD 毫秒与 Unix 毫秒的正确换算，1080向量/72逐点 OOM全部过。
  新56条四入口日期记录由 Python 整数/日期/小端模型独立核对，包含极值和
  换算溢出；语言另验12个时间往返。`numeric_fields` 两份范例输出完全一致。
- 综合教学范例不再依赖或写入固定 `build/` 文件，改用 UTF-8 独占临时
  目录和 `finally` 清理；空工作目录运行也实际验收，不留下调用方文件。
- Windows普通完整73阶段、Linux最终75阶段（含ASan/UBSan/泄漏检查）通过。
  扩大到Windows共享拒绝发布 + OOM后，最终默认76阶段仅该新增必过正例
  失败，其余75阶段符合预期；不能把前面的绿跑作为最终发布签收。
- 固定 PyMongo 4.10.1 实跑两平台双向互通：C/XL写出相同字节、反向读取
  UTF-8/嵌套/二进制/负毫秒Date，14个Date线格式与PyMongo双向一致。
  Windows使用其C后端，Linux使用同版本纯Python后端；不连接数据库服务器。

普通保存的Windows25组、Linux17组分配矩阵均无文件残留。然而Windows
禁止替换的目标叠加分配故障时仍会留下临时文件：TCC与GCC的真实BSON
保存均在14组中失败1组；不编译BSON的直接XRT对照12组也失败1组。
旧目标和原错误保持、内存平衡，不是BSON数据损坏。修复责任与最小入口见
[原子保存阻断](../../../dev/extlib_blockers/bson_atomic_save_cleanup.md)。
完整摘要见 `docs/reports/extlib-bson-20261101.json`；此问题按主线约定
记录并转交，不在包中重复实现原子写入或降低门禁标准。

## 历史冻结工作树记录

2026-10-29 当时冻结 Windows 发布版默认门禁前36阶段满足预期：严格
C、边界/OOM、实际生成原型、双语数值范例及五个独立程序；第37阶段
原中央 XLang 字典编码报 `bson: encode requires a table (var dict)`；
最小复现在当时 Windows/Linux 均失败，见
`dev/extlib_blockers/bson_encode_var_dict_projection.xl`。当时首要阻断是
共享 foreign `var` 字典投影，不是下面旧发布物的 AOT 缺符号；
不在包内绕过，整包尚未签收发布级。
具体双平台报告以[发布验收矩阵](../../../docs/普通扩展库发布验收矩阵_2026-10-28.md)
和任务台账为准。下方带旧日期的结果是历史证据，不能覆盖本文最新状态。

## 数值读取与错误边界

Double、Int32、Int64、Date 的载荷必须完整读取后才创建值对象。
旧路径在读取失败后继续用零值调用工厂，产生无效分配；该分配若失败，
会把已经确定的格式错误覆盖为内存错误。现在读取失败立即停止，
不改变正常线格式、类型映射、精度、公开 API/ABI 或所有权。
数值宽度与小端布局依据 [BSON 1.1 规范](https://bsonspec.org/spec.html)。

`test/numeric_reads_c.c` 观察三个公开 XRT 值工厂，不改生产接口：
四种类型的每个截断位置、根/嵌套文档/数组、四条 C 解码入口以及
前置合法文档的流，共1008个截断/错误后故障组合；另72个合法对照
和72条逐分配点 OOM/立即恢复路径。检查数值、错误类别、工厂调用
次序和活动分配/非法释放计数。此定向矩阵不是完整 BSON fuzz。
正常预算下的截断仍报 `XERR_VALUE`；真正的容器、值或错误对象分配
失败仍可报 `XERR_MEMORY`，不把一切坏输入强行归为不分配的错误。

语言侧 `test/numeric_reads.xl` 验证28个截断长度分别经过 decode 与
decodeAll 的静态 bytes 入口，共56项，并扫描合法 Int64 解码的分配
故障，要求 `MemoryError` 且解除后立即恢复。原语言字典编码正例不删，
不能用外部字节成功解码代替 M1 所代表的完整双向能力。

`examples/numeric_fields.xl` 与 `examples/numeric_fields_c.c` 读取同一份
独立固定线格式，展示 Int32、Int64、Double 与负毫秒 Date，五行输出
对照 `numeric_fields.expected.txt`。该参照不是由本库编码器自生成。

从仓库根目录运行：

```powershell
./release/xl.exe run release/extlibs/bson/examples/numeric_fields.xl
./release/xl.exe run release/extlibs/bson/examples/numeric_fields_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/bson/test/check.ps1
bash release/extlibs/bson/test/check.sh
```

两平台门禁先检查新的数值通路、原 C/文件/故障用例、15个真实生成
原型和五个独立程序，再验收原语言字典正例及完整双语文件往返。
失败保留独占目录、命令/真实输出和输入指纹，成功只清理本次目录；
所有子进程有限时，缺少 `CC` 指定的编译器明确失败，不作跳过。
Windows 报告在 `build/test/bson_guard`，Linux 在本次 `TMPDIR` 下。
测试运行库只带生产五模块加路径操作和内存调试，生产依赖未扩大。

`bson` 将 XLang 的 `var` 字典编码为 BSON 文档，也能解码 BSON 文档或连续文档流。C 调用方可直接使用 `bson_native.h` 中的 `xvalue*` 接口。

```xlang
import bson

var encoded = bson.encode({"name": "xlang", "count": 7})
var document = bson.decode(encoded)
println(document["name"])
```

## 映射和边界

| XLang `var` | BSON |
| --- | --- |
| `dict` | Document |
| `array`、连续的 `list` | Array |
| `string` | UTF-8 String |
| `bytes` | Binary subtype 0 |
| `int` | Int32 或 Int64（按数值范围） |
| `float` | Double |
| `bool`、`null`、`time` | Boolean、Null、Date |

根值必须是字典。`encode` 和 `concat` 返回的原始 BSON 是 `bytes`，`decode`、`decodeAll`、`toHex`、`docLength` 只接受 `bytes`，不接受用字符串伪装的二进制。BSON String 映射为 UTF-8 `string`，Binary subtype 0 映射为任意内容的 `bytes`，后者可原样往返。ObjectId 解码为 24 位十六进制字符串，仅是值投影，再次编码不会保留 ObjectId 类型。其余特殊 BSON 类型（包括 regex、timestamp、undefined、MinKey/MaxKey）明确拒绝，不能把本库当作全类型 BSON 驱动使用。未知类型、重复键、非连续数组索引、非法 UTF-8、截断或尾随数据都会拒绝，不会静默变成 `null`。单个输入或输出上限 64 MiB，嵌套深度上限 64；整份文档的字段/数组项合计最多 100 万，编解码均执行该预算。

`toHex` 的输出是输入字节数的两倍，因此它最多接受 32 MiB 输入；
再多一个字节就会在分配输出前报告 `XERR_RANGE` / `RangeError`。
恰好 32 MiB 输入可生成 64 MiB 十六进制文本。

深度按文档层数计算，根文档是第 1 层：63 层子文档可编解码，第 64 层子文档会被编解码两侧共同拒绝。编解码、`concat` 和 `toHex` 的资源预算超限报告 C `XERR_RANGE` / XLang `RangeError`；畸形 BSON 和不支持的值类型仍是值错误。`docLength` 是不抛异常的外层长度查询，仅检查输入类型、总长度、头部长度和结束字节；这些检查不通过时返回 0。它不验证正文的字段类型或嵌套结构，不可信文档仍需调用 `decode` 验证。

UTF-8 字符串**值**可包含内嵌 NUL，编解码按真实字节长度保留；文档**键**是 BSON C 字符串，不能包含 NUL。两种情况均有 C/XLang 回归，不能把值的可往返性推广到键或文件路径。

BSON Date 是 Unix Epoch 有符号毫秒；当前 XRT `time` 是 AD Epoch 有符号毫秒，二者纪元相差 `62135596800000` 毫秒，不再进行微秒整除或精度截断。使用 `time.fromUnixMs` / `time.toUnixMs`（C 为 `xrtTimeFromUnixMs` / `xrtTimeToUnixMs`）明确转换。原始时间不人为限制为四位年份，但加减纪元发生 int64 溢出时拒绝：不可表示的线格式解码报告值错误，不能表示为 BSON Date 的原始时间编码报告范围错误，不静默回绕。

`decodeAll` 读取连续 BSON 文档，整个连续流的字段/数组项合计也最多 100 万；`concat` 拼接两个文档。`concat` 只核对外层长度、终止字节与总量，不做完整结构解析或累计项数校验；拼接后的流仍可能被 `decodeAll` 按预算拒绝。不可信输入应先用 `decode` 验证。`toHex` 用于调试。`objectId` 生成 4 字节秒时间戳加 8 字节安全随机数的 12 字节 ID；不保证顺序递增。时间戳来自 XRT 时钟，超出 32 位无符号秒数范围时返回范围错误，不会静默回绕。`oidTime` 提取该时间戳。`load`/`save` 使用 XRT 文件接口，支持 UTF-8 路径。`load` 在读取前及读取中文件增长时执行 64 MiB 限额，超限保留 XRT 的 `XERR_RANGE` 错误。
`save` 使用 XRT 原子写入；写入中途失败不会截断已有 BSON 文件。
保存链路的编码与原子写入分配失败在 C 端保留 `XERR_MEMORY`，
XLang `save()` 抛出 `MemoryError`；故障解除后可正常保存并读回。

`objectId` 读取系统时钟失败时不生成 ID，保留 XRT 的 `XERR_IO`；下一次时钟正常时可以直接重试。Linux 包级门禁使用一次性时钟故障注入固定这一错误路径。

`oidTime` 在 XLang 中返回 `time`，C 底层 `xbson_oid_time` 返回 Unix 秒数。`load`、`save` 的路径及 `oidTime` 的十六进制文本拒绝内嵌 NUL，避免传给 C 字符串接口时静默截断。
原生 `xbson_oid_time` 最多探测 25 字节以确认精确的 24 位长度，C 调用方仍须传入有效 NUL 结尾字符串。

C 接口直接依赖公开的 XRT `xvalue`，不需要 xlang 私有运行时头文件。返回的 `xvalue*` 由调用方 `xrtValueRelease`，`xbson_to_hex` 和 `xbson_gen_oid` 返回的 `char*` 由调用方 `xrtFree`。三个整数结果使用 `int64_t`，与 XLang `int` 的跨平台 ABI 一致。失败时动态值接口返回 null 值并设置 XRT 错误；字符串接口返回 `NULL`。
`encode` 的结果始终独立拥有：小文档从写入器的栈缓冲复制到结果值，
超过内联缓冲的文档由结果值直接接管 XRT 堆缓冲，不再整段复制一次。
成功调用 `toHex`、`objectId`、`docLength` 或 `oidTime` 后，C 端不会保留前次操作的 XRT 错误状态。

`examples/bson_c.c` 可在 `release` 下直接运行：

```powershell
./xl run extlibs/bson/examples/bson_c.c
```

该单文件范例包含 `bson_native.c`；正式扩展库构建仍通过包清单独立编译
这个源文件，同一命令中不要再重复加入它。
成对范例还展示 `toHex`、`concat/decodeAll`、`objectId/oidTime`；
传入输出路径时两侧都执行 `save/load` 并核对读回内容。C 范例使用
统一释放出口，失败时不遗留已持有的 BSON 值与原生字符串。
包级门禁把 C、C 单文件直跑与 XLang 输出对照
`examples/basic.expected.txt`，并逐字节比较 C/XLang 保存的文件。

运行包级验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/bson/test/check.ps1
bash release/extlibs/bson/test/check.sh
```

Linux 脚本使用 GCC ASan/UBSan 与泄漏检查，运行原生 C 烟测、畸形/随机字节、时钟故障与文件边界。两平台均保留全部原语言正例、完整双语保存/读取比较、15个实际原型和JIT/AOT；当前仅Windows新增的锁定发布组合仍失败。`coverage.tests` 登记原四份测试及新增拥有型文本/日期测试。`-SkipAot` 仅用于 Windows 快测，不代表完整验收。早期共享索引缺符号是历史记录，不是本包当前首阻断。C/XL范例位于 `examples/`。
包 manifest 仅带入原生实现使用的值容器、整文件、时间、Unicode 与安全随机模块；
同一 `test/aot_core.xl` 产物从 3,646,464 缩至 743,424 字节且运行通过。
上面的体积数值属于历史构建，不代表本次产物尺寸；当前15份AOT已实际通过，文件失败回滚的剩余阻断不能据此关闭。

2026-10-22：C 文件 API 的逐点分配故障测试增加活动分配数/字节数
基线核对；`load` 新增有效文件的读缓冲与解码结果故障扫描，确认内存
错误分类、结果释放和故障清除后正常读取；`save` 同时核对旧目标不变
和释放平衡。Windows 严格 C/XLang、双语范例、核心 AOT 与 Linux C
ASan/UBSan、XLang 门禁通过。完整边界 AOT 仍受共享缺符号阻断。

2026-10-21：补齐此前未覆盖的保存链路逐点分配故障。C 回归从编码到
原子落盘检查内存错误与旧文件保持；XLang `save()` 检查 `MemoryError`
分类、故障清除后写入和读回。Windows 严格 GCC、包级 C/XLang 与核心
AOT 快测，以及 Linux C ASan/UBSan、XLang/双语门禁均通过；未发现
需要修改的包内实现，完整 AOT 共享缺符号阻断未变。

2026-10-19：新增 C/XLang 对照边界，固定含 NUL 的字符串值往返后仍为
3 字节，同时拒绝含 NUL 的文档键。Windows C/XLang 定向用例及 Linux
C ASan/UBSan、XLang、双语范例联合门禁通过；编码器无需修改，完整 AOT
主线阻断未变。

2026-10-17：`xbson_oid_time` 原用无界 `strlen` 检查 24 位 ObjectId；
25 字节未终止输入的 ASan 负例先复现越界读取，现限界探测并保留原错误语义。
Windows 包级 C/XLang/双语及核心 AOT、Linux ASan/UBSan 联合门禁通过；
完整边界 AOT 阻断未变。

同日新增 XLang `encode/decode` 逐点分配故障用例，两条路径各自验证
`MemoryError`、未触发故障时成功以及清除故障后正常解码。用例已登记
`coverage.tests` 并接入 Windows/Linux 包级门禁；严格 C、Linux
ASan/UBSan、语言边界及双语范例同次通过。未发现新的包内错误，
完整 AOT 的共享索引缺符号阻断不变。

2026-10-16：原生接口移除对 xlang 私有 `xl_runtime.h` 的依赖，改由 XRT 单头的值容器 API 提供 `xvalue`；三个整数结果统一为 `int64_t`。Windows/Linux 严格 C、包级 C/XLang 边界与双语范例通过，Linux XLang 从项目根目录和 `release` 目录导入均可用。完整 AOT 阻断不变。
2026-10-15：Linux 原生 sanitizer 门禁已落为包内可复跑脚本，两个 C
测试与 C 范例通过。另补 BSON Date 精度检查：正负非整毫秒时间拒绝编码；
正负整毫秒时间 C/XL 往返，PyMongo 生成的负毫秒 Date 由两种语言解码验证。

2026-10-15：Linux 限制文件大小的故障用例先复现 `save` 写坏旧文件，
改用 XRT 原子写入后旧内容保持不变。Windows 严格 GCC、C/XLang 边界、
双语范例、裁剪 AOT 核心及 Linux ASan/UBSan 通过；完整 AOT 阻断未变。

2026-10-15：编码分配失败原覆盖 XRT 的 `XERR_MEMORY` 为 `XERR_VALUE`，
逐点故障测试先红后绿；编码缓冲扩容、结果复制及 `concat` 分配现在保留
内存错误。嵌套解码、编码和拼接的逐点故障与恢复测试在 Windows 严格 GCC、
C/XLang 包级门禁及 Linux ASan/UBSan 下通过；完整 AOT 阻断不变。

2026-10-15：`decodeAll` 在结果数组创建失败后仍尝试解码、追加，空句柄错误
覆盖 `XERR_MEMORY`。两个连续文档的逐点故障测试先红后绿；数组创建失败
立即返回，未生成半成品。Windows C/XLang 包门禁、双语范例、裁剪核心 AOT
及 Linux ASan/UBSan 通过；完整 AOT 仍缺 `xl6ValueIndexReadCopyV1`。

2026-10-16：给 BSON 展开增加累计 100 万项预算。原先 64 MiB 输入可容纳
数百万字段并触发大量值对象分配；恰好达限的嵌套数组可编解码，再增一项
编码报 `XERR_RANGE`，两份各自合法的文档拼接后超限则由 `decodeAll` 拒绝。
Windows C/XLang、双语范例、核心 AOT 与 Linux ASan/UBSan 通过；完整 AOT
仍缺上述共享索引符号。

与 [MongoDB 官方 PyMongo BSON 实现](https://www.mongodb.com/docs/languages/python/pymongo-driver/current/data-formats/bson/) 互操作的可选验证已在本次两平台实跑，包括14个全int64范围Date线格式；其中两个日期虽然是合法BSON，却不能换算为XRT时间，本包明确拒绝。PyMongo固定4.10.1，只安装到 `build/test` 隔离环境，不进入发布包。从仓库根目录运行：

```powershell
python -m venv build\test\bson\venv
.\build\test\bson\venv\Scripts\python.exe -m pip install -r release/extlibs/bson/test/requirements-external.txt
.\release\xl.exe run release\extlibs\bson\examples\bson.xl -- build\test\bson_example_xl.bson
.\release\xl.exe build release\extlibs\bson\examples\bson_c.c --inc-path lib\xrt --inc-path release\include -o build\test\bson_example.exe
.\build\test\bson_example.exe build\test\bson_example_c.bson
.\release\xl.exe run release\extlibs\bson\test\date_epochs_c.c | Set-Content -Encoding utf8 build\test\bson_dates.tsv
.\build\test\bson\venv\Scripts\python.exe release\extlibs\bson\test\check_external.py build\test\bson_example_c.bson build\test\bson_example_xl.bson build\test\bson_python.bson build\test\bson_dates.tsv
.\release\xl.exe run release\extlibs\bson\test\external_python.xl -- build\test\bson_python.bson
.\release\xl.exe build release\extlibs\bson\test\external_python_c.c release\extlibs\bson\bson_native.c --inc-path lib\xrt --inc-path release\include -o build\test\bson_external_c.exe
.\build\test\bson_external_c.exe build\test\bson_python.bson
```

范例的文件路径参数只用于保留互操作产物；不传路径时原有输出不变。此验证只覆盖本文公开的 BSON 类型子集，不意味着支持全部 BSON 扩展类型。

原始实现曾参考 [cloudwu/lua-bson](https://github.com/cloudwu/lua-bson)（MIT）；上游授权正文保留在 `LICENSE.lua-bson`。当前 BSON 格式依据 [BSON 规范](https://bsonspec.org/spec.html)限定上述公开范围。

2026-10-18：C 边界测试新增合法嵌套文档的逐长度截断和逐字节替换矩阵，
覆盖子文档、数组、布尔和字符串的深路径。每次解析必须返回完整对象且无错误，
或返回 null 值并设置错误；Linux ASan/UBSan 和 Windows 严格 C、XLang 包级
门禁均通过。此固定矩阵补充原有随机噪声测试，不代表完整 fuzz 认证；
完整 AOT 仍受共享索引符号阻断。

2026-10-18：原始 BSON 和 Binary subtype 0 从字符串迁移到 XRT `bytes`。
解码合法字节流、拒绝字符串伪装、`0x00ff` 二进制字段逐字节往返已由
C/XLang 边界覆盖。Windows 包级门禁及 Linux ASan/UBSan 通过；
PyMongo 4.18.3 双向互操作也通过：C/XLang 写出的文件逐字节一致，
PyMongo 可读，PyMongo 写出的 `0x00ff` 字段由 C/XLang 读为 `bytes`。
完整带输出 AOT 阻断未变。

2026-10-19：C 范例改为直接 `xl run` 可运行，保留正式包的原生源码
独立编译。Windows 严格 GCC/TCC、C/XLang 同输出与核心 AOT 已通过；
完整边界 AOT 的共享缺符号阻断不变。

2026-10-20：包内 NUL 路径负例原直接写删固定
`build/test/bson_nul_boundary.bson`；现改为独占临时目录和 `finally`
清理，保持“带 NUL 的路径不能命中既有前缀文件”断言。Windows 严格
GCC、C/XLang 边界、分配故障、双语范例和核心 AOT，以及 Linux C
ASan/UBSan、XLang 门禁均通过。BSON 编解码实现未改；完整带输出
AOT 的共享阻断不变，不签收整包发布级。

2026-10-21：C 边界测试的原子保存失败和超大文件限额用例，原先分别
写入、删除固定 `build/test/bson_guard` 下的文件；现要求测试程序接收
本次门禁的独占工作目录，文件路径由 XRT 拼接。Windows 门禁传入自己的
独占构建目录，Linux 门禁使用 `mktemp` 目录，不再预先创建固定父目录。
Windows `-SkipAot` 的严格 C、XLang 边界、双语范例和核心 AOT，以及
Linux C ASan/UBSan、XLang/双语联合门禁均通过。旧固定目录中的历史
产物未改动；完整带输出 AOT 的共享阻断仍在，不签收整包发布级。

2026-10-23：`encode` 在写入器已使用堆缓冲时直接把该缓冲移交给
`xvalue` bytes，省去结束时的同尺寸分配和整段复制；栈内小文档仍
分配独立结果。600 字节字段的 C 故障扫描新增完整解码往返，验证
释放与错误恢复。Windows `-SkipAot` 的严格 C/XLang、双语与核心 AOT，
Linux C ASan/UBSan、XLang 联合门禁通过；完整 AOT 仍待共享主线缺符号修复。
