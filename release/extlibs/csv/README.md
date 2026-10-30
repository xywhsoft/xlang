# csv

基于 XRT 流与 UTF-8 检查的 CSV/TSV 读写。提供逐行、字典行、内存及文件接口；
解析与写入使用同一组 C 核心，xlang 的字典层只负责表头和行形状。

## 当前验证范围

2026-10-30，冻结工具链的 Windows/Linux 综合门禁分别执行 88/77 个阶段，
77/71 个符合要求（含私有方法的预期拒绝）。八组基础 C 测试、20 组首错组合、
454 项 Python 互通、四线程往返、19 个真实原型和新增流式双语 JIT/AOT 通过；
Linux C 含 ASan/UBSan/泄漏检查。两平台仍有 M5 嵌套数组、M3 私有构造器和
M2 顶层对象 AOT 退役阻断；Windows 另有关闭态事务回滚 OOM 残留，Linux 同例通过。
门禁均真实退出 1，不将 PASS 输出或局部 main 对照视为整包发布签收。
下列带旧日期的缺符号/通过说明仅为历史记录；当前核心 AOT 已通过，不能再套用
旧的 native-handle 缺符号诊断。逐项现场与修复责任见发布验收矩阵及任务台账。

2026-10-24 当前 Windows 发布版复验：七组独立 GCC C 测试及 454 项
Python CSV 互通通过；默认综合门禁随后在中央 XLang 用例的
`array<array<string>>.append(array[...])` 处报运行时 `TypeError`。
不导入 CSV 的最小文件在 `check` 通过后于 JIT/EXE 同样失败，见
`../../../dev/extlib_blockers/nested_array_append_runtime.md`。这次
完整语言门禁未通过，也未走到 CSV AOT 验收；下列较早的通过记录
均指当时的发布物，不代表当前构建已发布签收。

2026-10-16：当前 Windows 包级严格 GCC、C/XL JIT、17 个生成原型、双语范例
与文档代码块通过；Linux C ASan/UBSan、454 项 Python 独立互操作，以及
Linux XLang 中央、包内边界、文件和范例 JIT 通过。默认 Linux XLang 门禁的
精简 AOT 构建仍报共享符号 `xl6NativeHandleTraceLeaf` 未解析；独立 Buffer
最小程序同样复现，见 `../../../dev/extlib_blockers/native_handle_trace_aot.md`。
当前不能将 C/JIT 通过视作全平台 RC 验收。

2026-10-18 原生访问边界：`Reader`/`Writer` 的 `*Native` 方法已设为私有，
包外应通过公开方法让包装层检查字段与接管结果。但当前编译器仍允许包外
调用导出类标成 `private extern create` 的原生构造器，可能绕过指针/长度及
选项检查；只编译、不运行的复现见 `test/private_constructor.xl` 和
`../../../dev/extlib_blockers/csv_private_native_constructor.md`。在主线修复并
完成回归前，本包**不能发布签收**；不要把私有构造器标记误认为已经生效。

2026-10-17：`tools/check_extlib_csv.sh` 已将 Linux C ASan/UBSan、454 项 Python
互操作、中央及包内 XLang JIT 用例、三份语言范例和两组 C/XLang 黄金输出
逐字节对照合为一次固定输入门禁，完整运行通过。实际运行的 XL 源码与
`release/xl` 已纳入输入快照；AOT 的共享缺符号阻断不变。

## 快速开始

```xlang
import csv

csv.Writer output = csv.create()
output.write(["name", "city"])
output.write(["Alice", "上海"])
Buffer data = output.finish()

csv.DictReader rows = csv.openDict(data)
while rows.next()
    dict<string> row = rows.row()
    println(row["name"] + "|" + row["city"])
end
rows.close()
```

输出 `Alice|上海`。`open("data.csv")` 的字符串是**文件路径**；解析文本使用
`parse("a,b\n1,2\n")`。不要把文本传给 `open(string)`。

## 函数

表内“读选项”“写选项”不是额外语法，表示后续可选参数，准确顺序见下一节。

| 函数 | 结果 | 用途 |
| --- | --- | --- |
| `open(Buffer data, 读选项)` | `Reader` | 保存输入快照，之后修改原 Buffer 不影响读取 |
| `open(string path, 读选项)` | `Reader` | 打开 UTF-8 路径的文件，逐行读取 |
| `parse(string text, 读选项)` | `array<var>` | 一次读完；每个元素为一行字符串数组 |
| `create(写选项)` | `Writer` | 内存输出 |
| `createFile(string path, 写选项)` | `Writer` | 事务式文件输出 |
| `stringify(array<array<string>> rows, 格式选项)` | `string` | 一次写完；使用默认资源限额 |
| `openDict(Buffer data, 读选项)` | `DictReader` | 第一行作为表头 |
| `openDict(string path, 读选项)` | `DictReader` | 从文件读取字典行 |
| `createDict(array<string> headers, bool writeHeader=true, 写选项)` | `DictWriter` | 按指定列顺序生成字典行 |
| `createDictFile(string path, array<string> headers, bool writeHeader=true, 写选项)` | `DictWriter` | 字典行文件输出 |

全部字段为 `string`，不会猜测数字、日期或布尔类型。需要数字时在应用层明确转换。
`parse`/`stringify` 会持有整份结果；大文件使用 `open`/`createFile`。

## 选项与默认值

读选项顺序：`delimiter, quote, strict, maxInput, maxOutput, maxField, maxRows, maxFields`。

写选项顺序：`delimiter, quote, newline, quoteAll, writeBOM, maxInput, maxOutput, maxField, maxRows, maxFields`。

`stringify` 的格式选项仅为 `delimiter, quote, newline, quoteAll, writeBOM`。

| 参数 | 默认值 | 合同 |
| --- | --- | --- |
| `delimiter` | `","` | 单个可打印 ASCII 或 TAB，与 quote 不同；TSV 用 `"\t"` |
| `quote` | `"\""` | 单个可打印 ASCII；内部同字符通过重复转义 |
| `strict` | `true` | 拒绝未闭合引号、未引用字段中的引号、闭合引号后的杂项 |
| `newline` | `"lf"` | 输出只能是 `"lf"` 或 `"crlf"` |
| `quoteAll` | `false` | 强制引用所有字段；否则按内容自动引用 |
| `writeBOM` | `false` | 输出开头写 UTF-8 BOM |
| `maxInput` | `268435456` | 读取时计原始输入字节；写入时计字段 UTF-8 字节总数 |
| `maxOutput` | `536870912` | 读取时计解码字段字节；写入时含引号、分隔符、换行、BOM |
| `maxField` | `16777216` | 单字段 UTF-8 字节上限，不是字符数 |
| `maxRows` | `1000000` | 累计行数，字典读写包含表头 |
| `maxFields` | `10000000` | 累计字段数，不是每行字段数 |

限额 `0` 明确取消该项限制，负数抛 `ValueError`。这些是数据配额，不是全部工作内存、
CPU 时间或文件系统空间的上限。`strict=false` 只放宽引号语法，不允许非法 UTF-8 或 NUL。
`parse(string)` 在复制输入前按 UTF-8 字节数检查 `maxInput`；流式读取仍应使用 `open`。

读取接受 LF、CRLF、CR，并保留引号内的换行字节；只跳过输入开头的 UTF-8 BOM。
不支持多字节分隔符、反斜杠转义方言、自动编码猜测或 Excel 公式防护。
原生 C 方言入口只检查分隔符/引号的前两个字节，要求恰好一个可用字符；
C 调用方仍须传入有效 NUL 结尾字符串。
空文件为零行；空行是零字段行；`""` 是一个空字符串字段，两者有区别。

## 对象

| 对象 | 成员 | 说明 |
| --- | --- | --- |
| `Reader` | `next() as bool` | 移动到下一行；false 只代表正常 EOF，错误抛异常 |
| `Reader` | `field(int index) as string` | 当前行字段副本，索引从 0 开始 |
| `Reader` | `row() as array<string>` | 当前行独立数组，后续 next 不影响结果 |
| `Reader` | `eof / rowNumber / count` | EOF、当前行号、字段数；没有当前行时后两项为 -1 |
| `Reader` | `close()` | 关闭并清除当前行；可重复关闭 |
| `Writer` | `write(array<string> fields)` | 写一整行，不保留调用者数组 |
| `Writer` | `close()` | 完成输出；文件只有成功 close 才提交 |
| `Writer` | `finish() as Buffer` | 内存模式下完成并取走结果，只能取一次；文件模式不可用 |
| `Writer` | `finishText() as string` | 内存模式下直接取走文本；只能取一次，与 `finish()` 二选一 |
| `DictReader` | `next(), row(), close(), eof, rowNumber` | row 返回新的 `dict<string>`；首行为表头，第一条数据行编号为 1 |
| `DictReader` | `headers as array<string>` | 返回表头副本 |
| `DictWriter` | `write(dict<string> values), close(), finish(), finishText()` | 缺失键写空字符串，多余键抛 CsvError；文本结果可直接取得 |
| `DictWriter` | 构造时的 `headers` | 固定输出列顺序；当前不提供读取 headers 的属性 |

`finishText()` 直接接管内存写入器的 XRT 缓冲分配，避免再通过 `Buffer.text()` 复制整份结果；为补上 C 字符串终止字节，缓冲容量不足时仍可能扩容。它只适用于内存模式，且与 `finish()` 共用一次性取结果状态。C 侧调用 `xcsv_writer_take_text` 后须用 `xrtFree` 释放返回值。

字典输入必须有表头，重复表头、数据列数不符都会抛 `CsvError`。空表头名称允许，
但仍不得重复。读取到最后一条不带换行的记录时，`next()` 可返回 true 而 `eof` 已为 true，
所以遍历应以 `while rows.next()` 为准，不用 `while not rows.eof`。

同一个 Reader/Writer 需要串行使用；不同对象可以独立并发。底层自定义流回调可以操作
其他对象，不得重入当前 CSV 对象的 next/write/close/destroy；重入使该对象失败。

## 文件提交

```xlang
import csv

csv.DictWriter output = csv.createDictFile("report.csv", ["name", "qty"])
output.write({"name": "甲", "qty": "2"})
output.close()

csv.DictReader input = csv.openDict("report.csv")
while input.next()
    dict<string> row = input.row()
    println(row["name"] + ":" + row["qty"])
end
input.close()
```

目标父目录必须已存在。写入使用目标同目录的私有临时文件；成功 close 才替换目标。
发生错误或未 close 就析构时，不提交半份文件，清理临时文件并保留原目标。
这不是断电持久化承诺；权限变化、介质故障等仍可能阻止清理，不保证远程文件系统的原子性。
普通 C 外部输出流不具备文件事务，失败时已经写出的前缀无法回滚。

## 错误与文本边界

- `ValueError`：选项、负限额、路径等非法参数。
- `RangeError`：数据配额超限；字段索引越界为 `IndexError`。
- `CsvError`：格式、UTF-8、表头和行形状不合法。
- `RuntimeError`：无当前行、关闭后使用、重复提取结果等状态错误。
- `IOError`、`MemoryError`：底层读写或分配失败，保留首个错误及原因。

C 流已经报告 IO/内存错误后，CSV 位置诊断的二次分配若失败，直接发布原始错误，
不把已经发生的 IOError 改写为 MemoryError。此时 XRT 错误的 domain/code 属于
原始流，CSV 错误码和数据位置仍从 Reader/Writer 的 `Error()` 结构查询。
正常分配时仍发布带 CSV 位置的包装错误，并通过 cause 保留底层详情。

原始 CSV 中的 NUL 不合法，Buffer 输入会按真实长度校验并拒绝。语言封装层也会在
`parse(string)`、字段、表头、路径和方言选项进入 C 字符串接口前拒绝内嵌 NUL，
不会把 NUL 后面的内容静默丢弃。任意二进制输入仍应使用 Buffer，而不是 string。
普通 Unicode 文本按 UTF-8 处理。
CSV 错误位置是数据的行/列/偏移，不是 xlang 源码位置。

## C API

公共头：`include/xrt_csv.h`。实现为 manifest 中的四个 C 编译单元，不重复编译第二份 XRT。

| API | 合同 |
| --- | --- |
| `xrtCsvDialectInit / xrtCsvErrorInit` | 初始化版本、大小、保留位和默认选项；五项有效 C 限额继承 XRT 默认值，不承诺与 XL 默认值相同 |
| `xrtCsvReaderCreate(stream, takeOwnership, dialect)` | 使用 XRT xreader；接管时即使构造失败也消费流，否则只借用、不关闭 |
| `xrtCsvReaderOpenMemory / OpenFile` | C 内存入口借用字节，必须保持有效不变；文件入口拥有输入流 |
| `xrtCsvReaderNext / EOF / Error` | false 需区分 EOF 和错误；行、字段指针在下次 Next/Close/Destroy 后失效 |
| `xrtCsvReaderClose / Destroy` | 显式关闭与最终释放；Destroy(NULL) 安全 |
| `xrtCsvWriterCreate / CreateMemory / OpenFile` | 外部流、内存、事务文件；接管规则与 Reader 一致 |
| `xrtCsvWriterWrite` | 输入为 NUL 结尾 UTF-8 字符串；NULL 字段当空串，不保留输入 |
| `xrtCsvWriterClose / TakeBuffer` | C 必须先 Close；内存 Buffer 仅可取一次，调用方负责 `xrtBufferDestroy` |
| `xrtCsvWriterError / Destroy` | 查询错误、释放资源；析构不自动提交文件 |

方言只使用输入字节、输出字节、单字段字节、行数和总字段数五项资源限额。
写入器检查字段时，若设置了单字段或累计输入字节限额，会在超过剩余配额的
第一个字节停止扫描；刚好达到限额的字段仍可写入。C 入口仍要求有效的
NUL 结尾字符串，限额不能保护无效指针。
深度、压缩倍率、资源标志及保留字段必须为零；显式设置这些无效配置会在
reader/writer 创建时报告参数错误，不会默默忽略。

创建失败既可能返回 NULL，也可能返回带错误的对象。因此创建后检查
`reader != NULL && xrtCsvReaderError(reader)->iCode == XRT_CSV_OK`，Writer 同理；
之后每一步仍须检查返回值，必要时结合 `xrtGetError()`，最后无条件 Destroy。
借用的错误结构随对象销毁失效。C 字典行可按表头列序映射，不另建第二套解析器。

## 范例与复验

发行目录中的 xlang 范例：

```text
xl run examples/extlibs/csv_basic.xl
xl run examples/extlibs/csv_files.xl
```

C 对照为 `extlibs/csv/examples/basic.c`、`files.c`。两份教学范例显式包含
三个 CSV 核心源，可直接 `xl run`；正式扩展包仍按清单分别编译四个 C 源。
基础范例还将同一组含中文、逗号和引号的行经 XLang `stringify` 与 C Writer
编码，分别核对完整 CSV 字节文本，再读取并展示行内容。
`extlibs/csv/examples/streaming.xl` 与 `streaming_c.c` 展示逐行汇总、跨推进与关闭的
独立快照、带换行的中文字段、文本结果所有权和超限后的失败状态；两语共用六行固定输出。
C 的行字段只借用，因此显式复制要保留的字段；XL `row()` 已返回独立数组。
仓库根目录可执行：

```powershell
./release/xl.exe run release/extlibs/csv/examples/basic.c
./release/xl.exe run release/extlibs/csv/examples/files.c
./release/xl.exe run release/extlibs/csv/examples/streaming.xl
./release/xl.exe run release/extlibs/csv/examples/streaming_c.c
tools/run_extlib_c.ps1 -Package csv -Source release/extlibs/csv/examples/basic.c
tools/run_extlib_c.ps1 -Package csv -Source release/extlibs/csv/examples/files.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_csv.ps1 -SkipAot
wsl bash tools/check_extlib_csv.sh
bash release/extlibs/csv/test/check_xlang.sh
```

Windows/Linux 综合门禁默认使用各自独占的临时构建目录，仅成功时清理；
失败保留产物，报告另存为 `<工作目录>.json` / `<工作目录>.report.txt`。
Windows 报告保留每步原始状态与退出码；Linux 另保留 command/stdout/stderr/exit
及 phases.tsv。两端均校验实际编译器、CLI/DLL 和源码输入前后哈希，外部命令有超时。
需要保留成功产物时，Windows 显式传 `-WorkDirectory`，Linux 设置 `CSV_WORK_DIR`；
Linux 的显式目录必须尚不存在，由脚本创建、调用方管理。
语言正例失败后继续检查其余独立用例，最后聚合失败；不会把已知主线失败改成预期成功。

`test/check_xlang.sh` 是较窄的历史语言辅助入口，`--skip-aot` 显式仅复跑 JIT；
不传参数仍要求其核心 AOT。当前发布收口应运行 tools 下的完整综合门禁，
它继续检查 M2/M3/M5，不能用辅助入口或历史缺符号记录代替当前结果。

两种语言的文件范例都通过 XRT 排他创建临时目录，只在目录内写入 `报表.csv`，
验证未提交不覆盖、成功提交及读取汇总后清理目录；不会触碰工作目录中已有的同名文件。
XLang 范例原使用固定 `build/test/csv-files-example/报表.csv`，现已移除该写入路径。
两组范例各有固定 expected.txt，C/XL 输出逐字对照。
基础 C 范例的写入、读取和格式错误检查也使用显式条件；`-DNDEBUG` 构建不会跳过 CSV 操作。
六个使用 `assert` 的 C 边界测试在测试单元内保持断言启用；即使使用
`-DNDEBUG` 编译，文件、分配、线程等测试操作也不会被预处理器删除。

门禁覆盖八组 C 测试、短读短写、状态/重入、限额、分配失败、文件事务、中文路径、
四线程 1000 次独立往返、19 个实际生成的 C 原型、六组 XL JIT/AOT 及三组双语范例。
新增首错测试在读、读关闭、写、刷新、写关闭五个阶段，分别组合 IO/内存首错与
位置诊断分配成功/失败，共 20 组；检查错误身份、系统码、详情、重复操作、关闭次数和内存平衡。
严格 GCC 的 `-Wmissing-prototypes` 同时防止已实现的原生入口遗漏头文件声明。
`-SkipAot` 只跳过 AOT 构建与执行，仍检查 C、互操作、XL JIT、生成声明、双语范例
和文档代码块；它不会跳过嵌套数组与私有构造器问题，也不能据此签收发布级别。
独立 Python 标准库 csv 对照 454 项，包含四种分隔符、两种引号、换行、BOM、Unicode、
空字段和拒绝非法输入。Linux `tools/check_extlib_csv.sh` 同次启用 C ASan/UBSan/泄漏检查与 XLang JIT/双语对照，
可通过 `CSV_DECLARATIONS` 再核对 Windows XL 生成声明在 LP64 上的原型。

2026-10-30：修复 CSV 位置诊断 OOM 覆盖已发生的 IO/内存首错；补齐已有
`xcsv_reader_open_text` 的 C 头文件声明。公开 API、函数签名、分配器与正常解析路径不变。
新增流式双语范例，Windows GCC、XL JIT 与直接 C 运行已核对六行输出一致；
完整平台/模式结果和 M5、M3 阻断以[发布验收矩阵](../../../docs/普通扩展库发布验收矩阵_2026-10-28.md)为准。
当前另有 M2 顶层对象 AOT 退役问题：四份原始文档代码完整输出后退出 1，
相同代码进入局部 main 后退出 0。门禁保留原文，并增加 `test/global_lifecycle.xl`
作为要求零退出的顶层正例，不用局部改写换绿灯。

Windows 还复现关闭态事务回滚的 RFS 缺口：目标改名自然失败后句柄已关闭，
析构删除临时文件时若 UTF-8/UTF-16 路径分配再失败，会留下临时文件；原目录和
标记文件未损坏，活动内存与首错保持。`test/file_rollback_closed.c` 调用真实文件 API，
要求无残留，失败返回 1 并保留自己的独占目录，不把 Windows 缺口当成预期成功。
最小复现与 XRT 修复建议见[关闭态回滚记录](../../../dev/extlib_blockers/csv_closed_transaction_oom_cleanup.md)。

`test/check_xlang.sh` 要求先构建 Linux `release/xl`，并验证中央与包内语言用例、
双语范例的固定输出；默认还要求无输出内存读写核心 AOT。核心 AOT 在当前冻结
Windows/Linux 发布物均已通过；完整包仍受上列独立阻断，不能沿用旧符号问题作为
当前失败解释，也不能从核心通过推断所有语言能力通过。
尚不代表正式 CLI/DLL 更新、Linux 全路径、32 位/ARM、TSan、长期负载或性能基准已验收。

2026-10-19：两份 C 教学范例改为可单文件运行，Windows/Linux 门禁新增
直接 `xl run`，并与严格 GCC、XLang 范例的固定输出逐字对照。
正式包的四单元独立编译和既有 C/语言边界测试仍通过；完整 AOT 的共享
缺符号阻断未解除，不据此签收发布级。

2026-10-20：包内 `test/files.xl` 也不再写固定的
`build/test/csv-xl-files/中文.csv`；测试用 XRT 独占临时目录并在 `finally`
清理，初始夹具写入显式检查。单独运行通过，Windows `-SkipAot` 的
七组 C 测试、454 项 Python 对照、语言/双语门禁和 Linux C ASan/UBSan、
XLang 联合门禁均通过。完整 AOT 的主线阻断仍在，不签收整包发布级。

2026-10-21：六个 C 边界测试修正了 `-DNDEBUG` 下有副作用的断言会被
整段跳过的问题。Windows 常规 `-SkipAot` 门禁与六个 `-DNDEBUG` 严格
GCC 测试通过；Linux C ASan/UBSan、XLang JIT 与双语门禁也通过。
完整 AOT 仍受共享主线缺符号阻断。

同日补 Writer 流回调的内存失败分类：在刷新和关闭阶段分别注入
`XERR_MEMORY`，要求 CSV 错误码保持 `MEMORY`、保留底层原因，重复关闭
不覆盖首错，销毁时仍关闭所拥有的流。Windows 严格 GCC、七组 C 测试、
454 项 Python 互通、XLang JIT/双语范例及 Linux ASan/UBSan 联合门禁
均通过；本轮只补包内测试，核心实现未改。完整 AOT 仍未签收。

同日继续补 Reader 的对称边界：读取中与关闭时的底层 `XERR_MEMORY`
均须保持 CSV `MEMORY` 分类、原因链和首次失败；销毁仍关闭所拥有的流。
Windows 包运行器执行及严格 GCC 语法检查、Linux 包级 ASan/UBSan、
454 项互通与 XLang/C 对照通过。Windows 完整门禁未在此改动后重跑，
完整 AOT 阻断不变。
