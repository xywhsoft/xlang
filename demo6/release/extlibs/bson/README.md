# BSON 扩展库

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

BSON Date 精度为毫秒，XRT `time` 精度为微秒。编码仅接受微秒值恰好为 1000 的整数倍的时间；否则报错，不做截断或四舍五入。解码超出 XRT `time` 可表示范围的 BSON Date 也会报错。

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

`examples/bson_c.c` 可在 `demo6/release` 下直接运行：

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
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/bson/test/check.ps1 -SkipAot
bash release/extlibs/bson/test/check.sh
```

Linux 脚本使用 GCC ASan/UBSan 与泄漏检查，运行原生 C 烟测、畸形/随机字节边界及固定输出 C 范例；Windows/Linux 同时运行 XLang 边界、编解码与中文路径集成测试，并逐字节核对 C/XLang 范例输出。`coverage.tests` 登记这两份 XLang 测试。去掉 Windows 脚本的 `-SkipAot` 可检查完整 AOT；当前数组/字典索引涉及主线缺失符号，详见 `dev/extlib_blockers/value_index_link.md`。C/XL 范例位于 `examples/`。
包 manifest 仅带入原生实现使用的值容器、整文件、时间、Unicode 与安全随机模块；
同一 `test/aot_core.xl` 产物从 3,646,464 缩至 743,424 字节且运行通过。
完整边界 AOT 仍受上述主线缺符号阻断，不能据此签收发布级别。

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

与 [MongoDB 官方 PyMongo BSON 实现](https://www.mongodb.com/docs/languages/python/pymongo-driver/current/data-formats/bson/) 互操作的可选验证：C/xlang 范例保存的文件字节一致，PyMongo 可解码；PyMongo 写出的 64 位整数、嵌套文档、数组及含 NUL/`0xFF` 的二进制值，C/xlang 都可读回。PyMongo 只安装到 `build/test` 下的隔离环境，不进入发布包。从 `demo6` 目录运行：

```powershell
python -m venv build\test\bson\venv
.\build\test\bson\venv\Scripts\python.exe -m pip install pymongo==4.18.3
.\release\xl.exe run release\extlibs\bson\examples\bson.xl -- build\test\bson_example_xl.bson
.\release\xl.exe build release\extlibs\bson\examples\bson_c.c --inc-path lib\xrt --inc-path release\include -o build\test\bson_example.exe
.\build\test\bson_example.exe build\test\bson_example_c.bson
.\build\test\bson\venv\Scripts\python.exe release\extlibs\bson\test\check_external.py build\test\bson_example_c.bson build\test\bson_example_xl.bson build\test\bson_python.bson
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
