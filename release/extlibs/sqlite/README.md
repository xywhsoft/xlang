# SQLite

2026-10-29 延迟列转换补测：原有149个 SQLite 分配点主要覆盖打开、
编译和执行，并不代表列访问转换已测完。新增五类当前行访问，实际
命中 int64/float 转文本、UTF-16 存储转 UTF-8 字符串/Buffer 的四条
SQLite 分配故障路径；均保留 `MemoryError` / `SQLITE_NOMEM`，不返回
假成功或半成品，释放语句后连接仍可执行，最终 SQLite 内存归零。
XRT 活动分配数量/字节也回到基线。`zeroblob(4096)` 另外核对全部正文：
当前固定 vendor 在 step 阶段展开它，列访问阶段没有 SQLite 分配，
因此不把这项描述为“新增的列 OOM 点”。既有 XRT 输出故障矩阵仍运行。

新增成对范例 `examples/column_storage.xl` / `column_storage_c.c`：
UTF-16 数据库存储、UTF-8 结果、完整 int64 十进制文本、4096字节 BLOB，
以及语句和连接释放后继续使用独立拥有的文本/Buffer。C 拥有型文本
使用 `xrtFree`，Buffer 使用 `xrtBufferDestroy`，不释放 SQLite 借用指针。
从发布目录执行 `./xl run extlibs/sqlite/examples/column_storage.xl`
或同名 `_c.c` 即可，均输出：

```text
text: 你好
integer: 9223372036854775807
utf8 bytes: 6
zero blob: 4096
```

两平台默认完整门禁通过；Windows 为61项检查、44个生成原型与45个
常量，Linux 使用原生文件系统隔离快照及 C ASan/UBSan。新旧成对
范例的 GCC、C/TCC、XLang JIT 和各自独立 EXE 输出一致，输入指纹
前后相同。本轮未改包实现、vendor、主线或 XRT；没有发现需要修复
的列转换缺陷，也不据本次通过关闭历史 Linux 无诊断退出风险。

2026-10-28 拥有型文本全链路复核：旧 `@owned string?` 结果进入
`__sqlite_take_owned` 后又被编译成 `xlir_string_copy`，因此“接管处不复制”
不代表整条返回链无额外副本。现删除可空中转；C 已把 SQL NULL 规范化为
独立空串，原生失败同时发布 XRT 错误，六个公开字符串函数可以直接
返回 `@owned string`，保持普通 string API、C 物理 ABI 与释放责任不变。

新增 16 条薄接口/对象路径 × 两个分配预算，共 32 组检查：准备当前行后，
允许一次 XRT 输出分配时成功，禁止该次分配时保持 `MemoryError`；包含
SQL NULL、空串、UTF-8、元数据、错误消息、rowid 与连接关闭后持有结果。
Windows/Linux 八份语言用例/范例 JIT/AOT 均通过，44 个生成原型严格 C
检查通过；新用例已进入双平台默认门禁。此阶段只改语言返回链，未重复
编译未变的 SQLite C/算法/分配故障矩阵，沿用上一阶段的完整 C 证据。

同一基础范例生成 C 从 621,789 降至 611,811 字节；这是本机固定发布物
下的生成源码差异，不是最终 EXE 体积或性能认证。SQLite 借用文本仍必须
复制一次以获得独立寿命，边界也仍有 `strlen`、错误作用域与释放检查，
不宣称整条 SQL 调用零复制或零开销。历史 Linux 无诊断退出风险不变。

2026-10-28 输入侧 NUL 边界：先红后绿确认 SQL 会执行 NUL 前的有效
前缀，普通文本绑定也会把 `A\0B` 静默存成 `A`。现在 XLang 的路径、
VFS 名、SQL、普通 TEXT/BLOB 文本绑定在调用零结尾 C API 前统一检查，
含 NUL 抛 `ValueError`；对象方法、构造器和公开 `sqlite3_*` 薄接口共用
同一检查，不复制正文，不改 C ABI。错误 SQL 不执行前缀、不会提前编译
PRAGMA，失败绑定也不替换旧绑定。26 组拒绝、正常空 SQL/空文本和恢复
用例已加入 coverage 与双平台 JIT/AOT。

新增 `examples/text_bytes.xl` / `text_bytes_c.c`：将同一组三字节分别
绑定为 TEXT 和 BLOB，修改原输入后读回，关闭连接后继续使用拥有型
Buffer；C/XLang JIT/AOT 与 GCC 固定输出一致。C 按其语言规则使用
零结尾字符串，不能从 `char*` 检测未知长度的 NUL 后缀；需要完整数据
使用现有 `xsqlite3_bind_text_bytes_native` / `xsqlite3_bind_blob_native`。

本次 Windows 完整门禁 49 项、44 个生成原型与 45 个常量通过；Linux
完整门禁通过 C ASan/UBSan、149 个 SQLite 分配故障点、线程、新旧
语言 JIT/AOT 与双语范例。Linux 使用逐文件相同的原生文件系统快照，
保持原超时和冻结发布物。两平台失败保留现场；既有 Linux 无诊断退出
仍未定位，本包保持 R，不因这一轮通过而关闭旧风险。

2026-10-28：修复单语句 `prepare` 为检查尾部而再次编译 SQL 的问题。
被拒绝的尾部 PRAGMA 原可提前修改连接，C/XLang 红例已复现 `query_only`；
同类测试同时覆盖 `foreign_keys/recursive_triggers`。现只扫描尾部空白、空语句和注释，不编译额外 SQL，
也不为尾部检查另行分配。C 授权回调验证不再收到尾部 PRAGMA，正常第一条
PRAGMA 的原生语义不变；另有 510 组单字节尾部与上游对照和注释/BOM 边界。
成对 C/XLang 范例展示拒绝额外语句后连接仍可写，固定输出不变。

最终 Windows 默认门禁 37 项通过，44 个生成原型与 45 个常量通过；Linux
默认完整门禁通过 C ASan/UBSan、149 个分配故障点、线程、语言 JIT/AOT 和
双语同输出。此前一次 Linux 门禁无诊断退出且旧脚本删除了现场，根因仍未知。
脚本现保留失败命令/行号、原始状态、管道状态和已有日志，并记录 `xl/libxlc.so`
指纹；无输出退出 37 的诊断探针通过。包在当前验收矩阵中暂列 R，详见
[Linux 门禁待定位记录](../../../dev/extlib_blockers/sqlite_linux_silent_gate_20261028.md)，
不把最终复跑通过视为关闭该风险。

2026-10-26 当前发布物复验：Linux 包级门禁现覆盖 C ASan/UBSan、
149 个分配故障点、线程、中央/边界/文件 XLang JIT 与独立 EXE AOT、
两份公开 XLang 范例及 C 范例的 `xl run`/`xl build`。成对范例与
独立 GCC 程序的固定输出一致，输入文件前后哈希一致。Windows
专项门禁也已重新运行。此处的 Linux 证据使用当前 `release/xl`，
不再沿用下面 2026-10-25 的旧构建阻断结论；断电恢复、网络文件
系统、跨进程 WAL 故障矩阵、32 位/ARM 和长期压力仍未验收。
当前 Win/Linux x64 已公开包装能力按上述门禁签收发布候选范围。

2026-10-25 C 薄接口收紧：`open/open_v2` 的 `NULL` 文件名立即报告
`XERR_ARGUMENT`；`exec` 的 `NULL` SQL 返回 `SQLITE_MISUSE`，不再作为
成功的空操作；`prepare` 对 `NULL` SQL 明确报告参数错误。合法的空字符串
仍交给 SQLite 按其自身语义处理，内存库继续显式使用 `:memory:`。
Windows 37 项专项门禁（含 JIT/AOT 与 C/XLang 范例）及 Linux C
ASan/UBSan、分配故障和线程门禁通过；新版 Linux XLang 发布构建仍受
主线源清单链接缺项阻断，本次不签收那一路径。

2026-10-25 当前 Windows 发布版专项门禁通过 37 项：严格 C、
SQLite/XRT 分配故障、线程、XLang JIT 与独立 EXE AOT、生成原型
及 45 个常量、C/XLang 成对范例。Linux 独立 C 门禁的 ASan/UBSan、
149 个 SQLite 分配故障点、XRT Buffer/临时文本分配及线程测试通过。
两个门禁现使用每次独占的构建目录；Windows 留存独立 JSON 结果报告，
测试结束仅清理本次构建产物，不覆盖旧的 `build/test/sqlite_guard` 文件。
本次未使用新 Linux 发布版验证 XLang JIT/AOT，也未覆盖断电恢复、
网络文件系统或所有并发故障。

内置 SQLite 源码的本地数据库包，无需另外安装数据库 DLL。推荐使用 `Database`
和 `Statement`；需要精确状态码或 C 互操作时使用 `sqlite3_*` 薄接口。
XLang 的列文本、列名、错误消息和行号文本直接接管 C 层一次性分配的
`char*`；C 调用方使用对应 `*_owned_native` 后须以 `xrtFree` 释放。
旧的 `*_point_native` 透传层不再提供。

```xlang
import sqlite

auto db = sqlite.memory()
assert db.execute("create table people(name text)")
auto insert = db.prepare("insert into people values(?)")
assert insert.bind(1, "O'Reilly") == sqlite.SQLITE_OK
assert insert.done()
insert.finalize()
var rows = db.query("select name from people")
println(rows[0]["name"])
```

## 对象 API

- `memory()`：独立内存库。`open(filename)`：读写打开，不存在则创建。
- `open_v2(filename, flags, vfs)`：显式打开模式；空 `vfs` 使用 SQLite 默认 VFS。
- `Database()`：空对象；`Database(filename)`：打开文件；`Database(point)`：接管
  同一份 SQLite 实例创建的连接，不增加引用，调用方不得再次关闭原始句柄。
- `valid()`、`close()`、`close_v2()`：查询/释放连接。重复 close 返回 `SQLITE_OK`。
- `execute(sql)` 返回成功布尔值；`exec_code(sql)` 返回精确 SQLite 状态码。
- `last_error()`：当前连接的 SQLite 诊断，不是进程全局字符串。
- `prepare(sql)`：创建拥有型 `Statement`，只接受一条语句；空语句、多语句和非法
  尾部被拒绝。允许结尾空白、分号和注释；多语句执行使用 `execute`/`exec_code`。
  尾部检查不编译第二条语句，注释按 SQLite 规则不嵌套。该限制不是只读检查或
  事务回滚承诺：第一条语句仍按 SQLite 自身语义编译，部分 PRAGMA 可在
  `prepare` 时生效，不能把 `prepare` 当作无副作用的 SQL 校验器。
- `query(sql)`：全部行组成的动态数组，每行为列名到值的字典。执行失败抛出异常，
  不返回空成功或部分结果；语句在 `finally` 中释放。大结果集应使用 Statement 逐行读。
- `changes()`、`total_changes()`、`last_insert_rowid()`：64 位整数；行计数使用
  SQLite 的 `changes64`/`total_changes64`，不经 32 位返回值截断。
- `last_insert_rowid_text()`：rowid 的十进制文本。
- `busy_timeout(milliseconds)`：设置 SQLite 锁等待上限；0 表示不等待。
- `begin()`、`commit()`、`rollback()`：对应事务 SQL，返回成功布尔值。

Statement：

- `Statement()`：空对象；`Statement(point)`：接管同一 SQLite 实例的语句。
- `valid()`、`finalize()`：释放后对象不可再操作；重复 finalize 是成功的空操作。
- `bind(index, int|float|string|Buffer)`、`bind_null(index)`：参数从 **1** 开始。
  int 保留 64 位；Buffer 写入 BLOB。SQLite 复制参数，不借用调用方内存。
- `bind_text_bytes(index, Buffer)`：按明确字节数写 TEXT，允许内嵌 NUL。
- `bind_blob_text(index, string)`：将普通文本的 UTF-8 字节写入 BLOB，不是任意二进制 API。
- `step()`：返回 `SQLITE_ROW`、`SQLITE_DONE` 或错误码。
- `row_ready()`、`done()`：各自推进一次；错误时抛出 RuntimeError，不静默返回 false。
- `last_error()`、`reset()`、`clear_bindings()`：诊断/重新执行/清空参数。reset 保留
  绑定；执行失败后的 reset/finalize 可以返回前次 SQLite 执行错误码。
- `column_count()`、`column_name(column)`：列元数据，列号从 **0** 开始。
- `column_type`、`column_int`、`column_float`、`column_text`、`column_blob`、
  `column_text_bytes`、`column_blob_text`：读当前行；必须先得到 `SQLITE_ROW`。
- `value(column)`：INTEGER/FLOAT/TEXT/BLOB/NULL 分别得到 int/float/string/Buffer/null。
- `row()`：当前行字典。重复列名会覆盖此前同名列，请使用 SQL `AS` 指定唯一列名。

## 数据与错误合同

- 普通文本使用 UTF-8；文件路径直接交给 SQLite 的 UTF-8 文件 API。
- XLang 的文件名、VFS 名、SQL、`bind(string)` / `bind_blob_text` 的
  文本不能含 NUL，构造器与 `sqlite3_*` 薄接口同样抛 `ValueError`，
  不会把前缀当作完整输入。NUL 是语言参数错误，不是 SQLite SQL 执行状态。
  含 NUL 的 TEXT 使用 `bind_text_bytes` / `column_text_bytes`，BLOB 使用 Buffer。
- BLOB 通过 `Buffer` 保存全部字节。SQL NULL、空 TEXT 与零长度 BLOB 是不同值。
  明确调用 `column_text`/`column_blob` 读取 NULL 会得到空值，区分 NULL 应先看
  `column_type`，或者使用 `value`/`row`。
- `column_blob`/`column_text_bytes` 返回拥有型 Buffer，语句和数据库关闭后仍有效。
- 普通 `column_text` 和 `column_blob_text` 遇到内嵌 NUL 抛出 ValueError，不能静默
  截断；这类数据改用 `column_text_bytes`/`column_blob`。
- 列越界、参数收窄超出 C int 范围：RangeError。没有当前行、使用已关闭对象：
  RuntimeError。创建/prepare 失败通过 XRT 错误传播，内存失败保持 MemoryError。
- `execute`、`exec_code`、`step`、bind、reset、finalize、close 保留其布尔/状态码
  合同。SQL 执行错误用连接或语句的 `last_error()` 读取；模块级 `last_error()` /
  `sqlite3_last_error()` 仅访问当前 XRT 错误，不是最近任意 SQLite 返回码的描述。
- 对象不声明 copy/clone 能力；普通赋值共享对象身份，不复制数据库句柄。
  不得用同一原始指针创建两个拥有对象，也不得从 C 端关闭已经交给对象的句柄。
- 未释放 Statement 时，`close()` 返回 `SQLITE_BUSY` 并保留连接；`close_v2()` 和
  Database 析构采用 SQLite 延迟释放规则。语句可先于或后于数据库对象析构。
- 每个连接/语句的多步操作由调用方串行化。独立连接可并发；不承诺并发关闭同一
  对象、读列时另一线程 step/reset/finalize，或共享连接的事务序列自动隔离。

## C 接口

`include/sqlite_native.h` 是桥接 ABI；SQLite 的全部原始 C 声明见
`third_party/sqlite3.h`。句柄必须来自同一份 SQLite 库，不能混用其他 DLL 的句柄。
借用文本副本保存在 XRT 临时区，寿命到该区 clear/rewind；C 调用方需要长期保存时
自行复制。返回的 Buffer 由 C 调用方 `xrtBufferDestroy`，xlang 则由 Buffer 对象释放。
`*_owned_native` 文本入口返回一份 `xrtFree` 负责释放的独立副本；xlang 直接接管
这份内存，列值和诊断不再经过临时区与第二次字符串复制。原有 C 入口保持借用合同。
当前语言声明以 `@owned string` 直接调用这些 `char*` 入口，不提供额外的
`*_point_native` 转接层。

底层状态码入口不把 SQLite 业务失败变成 XRT 异常；没有状态码的创建、prepare、
读列入口使用 XRT 错误对象。裸 C 句柄不检测悬空指针，释放后复用属于 C 调用方错误。
普通文本绑定采用零结尾字符串；精确长度文本和任意二进制使用 bytes 接口。

## 验证与范围

门禁：`tools/check_extlib_sqlite.ps1`，Linux C 与双语 AOT：`tools/check_extlib_sqlite.sh`。
覆盖 ABI/常量、64 位值、UTF-8、二进制/NUL、状态与生命周期、事务/文件锁、分配故障、
独立线程，以及 C/xlang 的 JIT/AOT/GCC 范例。每次通过仅证明对应固定输入与平台。
Linux 失败现场由 `[sqlite] FAIL exit=... artifacts=...` 报告；`failure.txt` 保存
原始退出码、脚本行号、失败命令及管道状态，原有 stdout 日志和输入/编译器记录
一并保留。只有成功时自动清理该次独占目录；排查结束后再手动清理失败现场。

2026-09-27 专项：37 项 Windows 检查、44 个生成原型与 45 个常量通过；SQLite
工作流注入 149 个分配失败点，XRT Buffer/临时文本另外验证 2/1 个失败点及恢复。
Linux 独立 C ASan/UBSan/泄漏检查通过。原始证据保存在 docs 的
`extlib_sqlite_2026-09-27.*.results.json`。

在仓库根目录运行成对范例：

```powershell
release/xl.exe run release/examples/extlibs/sqlite_basic.xl
release/xl.exe run release/extlibs/sqlite/examples/basic.c
powershell -NoProfile -File tools/run_extlib_c.ps1 -Package sqlite -Source release/extlibs/sqlite/examples/basic.c
```

C 范例在 TCC 单文件路径先包含 SQLite amalgamation，再包含包内薄封装；
独立 GCC 构建仍分别链接 `sqlite_native.c`、`third_party/sqlite3.c`
与一份启用了对应模块的 XRT 实现。先包含独立 `sqlite3.h` 会使
amalgamation 的 `sqlite3_version` 定义被头文件保护宏跳过。

- 本批不宣称验证 32 位/ARM、断电恢复、网络文件系统、跨进程 WAL
  故障矩阵或吞吐性能。上游数据库引擎不是本包装逐行重审的对象。
- 包装不提供数据库加密、异步线程池、自动事务重试、备份/增量 BLOB、高级回调或
  用户自定义 SQL 函数。这些上游能力不能仅因 vendor 源码存在就算作 xlang API 已开放。
- 成对范例：`release/examples/extlibs/sqlite_basic.xl` 与 `examples/basic.c`；两者
  使用同一份预期输出。完整接口演示仍在 `release/examples/extlibs/sqlite.xl`。

## 历史验证记录

下列“当前阻断”仅描述记录当日的发布物，旧 XLIR/point 透传层不作为当前
实现依据；现行接口及待定位风险以本文开头和数据/错误合同为准。

2026-10-19：C 范例的数据库操作改为显式检查，不再依赖 `assert` 执行；
失败路径释放语句、连接和 Buffer。Linux C 门禁使用 `-DNDEBUG` 并在
ASan/UBSan 下通过；Windows 严格 GCC `-DNDEBUG` 与 TCC 范例输出和黄金文件一致。
当前 Windows 完整门禁仍在 `test/cases/extlib_sqlite.xl:550` 的主线
`XLIR C emitter runtime operation ABI is unresolved` 停止，不能据此签收整包。
2026-10-20：`test/file.xl` 的文件/锁测试已改用独占临时目录，避免删除固定路径上的已有数据库；
单独运行该测试仍在执行前遇到同一 XLIR ABI 错误，路径清理与文件行为尚待主线修复后验证。
2026-10-21：完整接口演示改用两份独立的 SQLite 内存库，不再创建或删除固定
`examples_out/extlibs` 数据库；文件和锁语义仍由 `test/file.xl` 覆盖。当前
`xl check release/examples/extlibs/sqlite.xl` 通过，`xl run` 与文件测试均在
既有 XLIR ABI 主线错误处停止，运行行为需待主线修复后重新验收。

2026-10-21：四线程 C 测试原把数据库调用、语句释放和线程等待
写在 `assert` 内，定义 `NDEBUG` 会删掉实际操作。测试单元现保持
断言启用，仅线程测试在两平台门禁中以 `-DNDEBUG` 编译执行；
预处理确认 `step` 调用仍在。Windows 三组严格 C 与 C JIT 通过，
随后仍在 `extlib_sqlite.xl:550` 的既有 XLIR ABI 错误处停止。
Linux C ASan/UBSan、149 个分配故障点及四线程隔离门禁通过，
未签收整包发布级。

2026-10-24：列文本、列名、连接/语句诊断和行号文本的 xlang 返回路径改为
一次复制后直接接管；C 测试验证副本在 `step/finalize/close` 后仍有效、含 NUL
列继续拒绝。当前发布版的 xlang 运行门禁仍在既有 XLIR ABI 检查处停止。

同日以当前源码驱动构建公开范例 AOT 时，发现生成的 `point` 返回原型
为 `void*`，而原 C 头声明的五个拥有型文本入口返回 `char*`，TCC 将其判为
重定义冲突。现新增五个无复制的 `void*` ABI 入口，保留原 C `char*` API；
C 测试逐一检查结果内容及独立释放。当前源码驱动下，中央语言用例的 JIT、
AOT 和公开范例 AOT 均运行通过，配对范例输出匹配黄金文件；Windows
严格 C 边界/分配/线程测试与 Linux C ASan/UBSan 通过。正式 `xl.exe`
仍在旧 XLIR runtime operation ABI 处停止，因此正式包级门禁尚未签收。

2026-10-24：当前发布版 `xl.exe` 已越过上述 XLIR ABI 阻断。随后
边界用例暴露 `Database` 和 `Statement` 在 `destroy()` 释放原生句柄后
写回正在退役的对象字段，触发 XRT 状态错误；析构不再写字段，显式
`close`/`finalize` 仍会清空句柄并保持幂等。新增不手动 finalize 的
语句清理回归，并保留语句逃逸后由 `close_v2` 延迟关闭连接的回归。
Windows 完整门禁 37 项通过，包含 44 个生成原型、45 个常量、
中央/边界/文件 JIT/AOT 与 C/XLang 对照。Linux C ASan/UBSan、
149 个分配故障点和四线程隔离通过；Linux XLang JIT/AOT、长期
压力及跨进程 WAL 故障矩阵不在本次验收范围。自动析构不返回 SQLite
状态码，需要获知失败原因时仍应显式调用 `finalize` 或 `close`。

语义依据：[列结果](https://www.sqlite.org/c3ref/column_blob.html)、
[关闭连接](https://www.sqlite.org/c3ref/close.html)、
[参数绑定](https://www.sqlite.org/c3ref/bind_blob.html)、
[PRAGMA 的编译阶段行为](https://www.sqlite.org/pragma.html)、
[SQL 注释规则](https://www.sqlite.org/lang_comment.html)。

## 模块级签名

```xlang
available() as bool
version() as string
last_error() as string
sqlite3_last_error() as string
sqlite3_open(string filename) as point
sqlite3_open_v2(string filename, int flags, string vfs) as point
sqlite3_close(point db) as int
sqlite3_close_v2(point db) as int
sqlite3_exec(point db, string sql) as int
sqlite3_prepare_v2(point db, string sql) as point
sqlite3_step(point stmt) as int
sqlite3_finalize(point stmt) as int
sqlite3_reset(point stmt) as int
sqlite3_clear_bindings(point stmt) as int
sqlite3_bind_int(point stmt, int index, int value) as int
sqlite3_bind_int64(point stmt, int index, int value) as int
sqlite3_bind_double(point stmt, int index, float value) as int
sqlite3_bind_text(point stmt, int index, string value) as int
sqlite3_bind_blob_text(point stmt, int index, string value) as int
sqlite3_bind_blob(point stmt, int index, Buffer value) as int
sqlite3_bind_text_bytes(point stmt, int index, Buffer value) as int
sqlite3_bind_null(point stmt, int index) as int
sqlite3_bind_parameter_count(point stmt) as int
sqlite3_column_count(point stmt) as int
sqlite3_column_type(point stmt, int column) as int
sqlite3_column_int(point stmt, int column) as int
sqlite3_column_int64(point stmt, int column) as int
sqlite3_column_double(point stmt, int column) as float
sqlite3_column_text(point stmt, int column) as string
sqlite3_column_blob_text(point stmt, int column) as string
sqlite3_column_blob(point stmt, int column) as Buffer
sqlite3_column_text_bytes(point stmt, int column) as Buffer
sqlite3_column_bytes(point stmt, int column) as int
sqlite3_column_name(point stmt, int column) as string
sqlite3_errmsg(point db) as string
sqlite3_stmt_errmsg(point stmt) as string
sqlite3_errstr(int rc) as string
sqlite3_extended_errcode(point db) as int
sqlite3_changes(point db) as int
sqlite3_total_changes(point db) as int
sqlite3_last_insert_rowid_text(point db) as string
sqlite3_last_insert_rowid(point db) as int
sqlite3_busy_timeout(point db, int milliseconds) as int
sqlite3_libversion() as string
sqlite3_libversion_number() as int
open(string filename) as Database
open_v2(string filename, int flags, string vfs) as Database
memory() as Database
```
