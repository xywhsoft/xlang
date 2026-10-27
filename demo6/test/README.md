# DEMO6 测试资产入口

本目录保存 demo6 的回归测试资产。`test/cases` 下的 `.xl` 文件会被递归纳入用例 wiring 守门，由 `test.bat`、`test/suites/*.bat`、范例批处理和工具脚本接线；`.c`/`.h` 宿主 fixture 由 C fixture 守门检查；`.json`/`.xlp` 数据 fixture 由 test data 或分类套件守门检查。

## 主要入口

- `tools/check_stdlib_time_core.ps1` / `tools/check_stdlib_time_core.sh`：时间核心
  的完整整数参数、UTC 微秒、Unix 毫秒、日历增减与显式容差；严格 C、
  声明合同变异、JIT/AOT、真实错误分配失败和语言 OOM 账本。Linux 原生
  adapter + XRT 使用 ASan/UBSan/leak；语言 libxl 本身未做 sanitizer。
  `cases/stdlib_time_core.xl` / `cases/stdlib_time_core_only.xl` 与五个非法 ABI
  声明固定纳入本门，不代表 time_local/time_text 或 53 模块全部完成。

- `test.bat`：主回归调度入口，先完成构建、清理和工具链准备，再按顺序运行 `test/suites` 下的分类套件。
- `test_smoke.bat`：快速冒烟入口，覆盖构建、核心文档/源码/工具接线、CLI 基础阶段、ABI、source import/package、容器运行和一个标准库运行样本。
- `test_all.bat`：完整回归入口，串联主回归、tutorial 和 DLL 测试。
- `test_tutorial.bat`：教学范例回归。
- `test_feature_suite.bat`：功能范例套件回归。
- `test_dll.bat`：`xl.dll` 嵌入式运行库回归。
- `cases/stdlib_release_string_abi.xl`：双字段 string 跨转换、动态值/nullable 投影、稀疏列表读取与容器 take/pop、内嵌 NUL 字典键/动态字段、product move、Future 名称及异步结果的综合回归；Windows 核心套件和 Linux JIT/AOT 都运行。

## 分类套件

`test/suites/common.bat` 是分类套件的公共初始化入口。单独运行某个套件时，它会切换到 demo6 根目录，设置 `XLANG_PACKAGE_PATH=release\extlibs;test\fixtures`，在缺少 `release\xl.exe` 或 `release\xlc.dll` 时自动构建，并准备 TCC bootstrap 与 XRT object。

可以按需运行：

```bat
test\suites\02_language_frontend.bat
```

常用环境变量：

- `XL6_SUITE_FORCE_BUILD=1`：运行套件前强制执行 `build.bat`。
- `XL6_SUITE_CLEAR_TMP=1`：运行套件前清理 `build\test` 顶层文件。

当前分类：

- `00_static_smoke.bat`：文档、工具、批处理、源码、编码、临时目录、DLL/API、标准库覆盖等静态守门，以及 CLI dump/save-temps 基础冒烟。
- `01_import_package_source.bat`：source import、package、同名 C companion、`.xlp`、依赖版本、私有依赖和 JSON 诊断。
- `02_language_frontend.bat`：词法、语法、字面量、布局、运算符、泛型约束、协议、derive、字段属性和第二阶段基础示例。
- `03_cgen_runtime_core.bat`：IR/C 生成、运行时基础、main/varargs/closure/function、容器、字符串、f-string、time、runtime type、标准库 math/string/time/random/buffer/path/file/dir 核心路径。
- `04_oop_abi_export.bat`：控制流 lowering、OOP/class/object/native-backed class、重载、ABI、C export、混编和链接路径。
- `05_semantics_exceptions_concurrency_base.bat`：类型语义负例、异常/#doc、sync/thread/async 基础运行与生成形态守门。
- `06_task_api_spawn.bat`：future/task API、task.spawn、task.all/race/cancel、任务边界和重复运行稳定性。
- `07_async_generator_channel_docs.bat`：async class await、async generator、generator、channel、并发负例、文档注释和 f-string/语法负例。
- `08_stdlib_extended.bat`：JSON/XLON、codec/hash/crypto、IO/Console、URL/query、log/template/regex/process 等扩展标准库；HTTP client/server 当前随 XRT 网络层更新暂缓，默认跳过，设置 `XL6_ENABLE_DEFERRED_HTTP=1` 可显式运行。
- `09_artifact_closure.bat`：负例诊断产物和 `build\tmp` 文本产物收口检查，通常作为 `test.bat` 全量调度的最后一步运行。
- `10_format_extlibs.bat`：gzip/bz2/zip/tar、CSV/INI/TOML/plist/netrc/XML 的 xlang/C 双语测试、Python 双向互操作、严格编译、私有依赖裁剪与按需链接守门。

## Manual probes

`tools/check_stdlib_text_values.ps1`（已接入 `03_core_base_09.bat`）覆盖 B1—B9 文本、数字、Unicode/HTML、编码与闭合原生 product：Windows 1040 项冻结输入检查，35 正向/42 负例，严格 C/JIT/AOT、确定性、ABI/私有访问/默认参数、真实 OOM 与回归。`tools/check_xlir_artifact_runtime.ps1` 本轮核对 60 组件、3600 对依赖、384 导出，并完成 B9 八个实际闭包；B7/B8 的闭包仅保留历史证据，不混为本轮重新验收。Linux 的 `check_stdlib_number_values.sh` / `check_stdlib_unicode.sh` / `check_stdlib_encoding.sh` 均接入 `test_linux.sh`，分别保留数字 253、Unicode/HTML 148、编码 B8/B9 216 项专项；sanitizer 只覆盖原生探针，受管层使用逻辑账本，均核对实际闭包与冻结输入。最小 encoding meta/转码/repair/guess 不引入其他文本 resident 或 Buffer；guess 不带转换/元信息 resident 和 charset 根，猜测不是验证。通用三整数 product 的普通函数/函数值与独立 C 提供者回归通过。整个 53 模块目标仍未完成，专项不能代替全仓 RC0。

`tools/check_stdlib_io_console.ps1` 批量验证二进制流、可选长度、结构化读写协议、独立行值、192 个分配故障探针、EOF 与非法 UTF-8、封闭 XLIR ABI 负例、确定性生成 C、严格 C 与 JIT/AOT。真实终端事件和正常返回/异常/finally 清理使用独立的隐藏 Windows 控制台，不改动用户终端；POSIX PTY 与 sanitizer 验证位于 XRT 仓库。这不包含尚未实现的全局 `exit(code)` 清理。

`tools/check_xlir_conditional_nullable_join.ps1` 验证三元表达式的可空类型汇合、分支局部收窄、惰性表达式的确定赋值，以及字符串/类/闭包汇合的所有权。门禁包含语义正负例、IR 变异拒绝、确定性 C/header、严格 C 内存平衡和 C-host JIT/AOT；它不代表全部 `out` 参数异常路径已完成证明。

`tools/check_xlir_managed_storage_inputs.ps1` 验证普通存储、闭包 cell 写入前的独立所有权取得，以及托管 `match` 的显式存储汇合、支配使用的 MOVE、临时字段投影与 checked 转换的生命周期。包含 IR 变异拒绝、严格 C 内存平衡和 C-host JIT/AOT；它尚不等于所有 RuntimeOp 输入的全局单次消费证明。

`tools/check_xlir_owner_consumption.ps1` 在此基础上验证托管 SSA 的路径敏感单次消费：统一 RuntimeOp/调用/返回/存储的输入效果，区分互斥分支、循环重新取得和透明别名，并禁止省略 STORE 的物理类型。包含已复现的跨块重复 STORE 反例、同路径重复消费变异、独立 CFG 单元测试及确定性 C/header；这仍不替代借用存活、存储重新激活与部分 product 字段生命周期的完整证明。

`test/manual_probes.txt` lists interactive probes that require a local GUI, hardware, or human observation. These files remain checked test assets, but they must not be wired into automated suites. Every probe path is explicit and must use an `_probe_` filename.

## fixtures

- `fixtures/libprobe`：package/import 测试用裸包名 fixture，用于验证 `XLANG_PACKAGE_PATH=release\extlibs;test\fixtures` 下的非发布物搜索路径。

## 用例前缀索引

以下前缀来自 `test/cases` 下递归发现的 `.xl` 用例。新增新前缀时，必须同步更新本节，避免测试资产变成只有批处理知道。

<!-- case-prefix-index:start -->
- `char`：Unicode 单字符值、整数字面量与字符串索引回归。
- `syntax`：已移除参数修饰语法的拒绝，以及原关键字作为普通名称的正例。
- `index`: indexed container read/write and temporary ownership regressions.
- `runtime`: runtime type and dynamic value cases.
- `bad`: focused negative regression cases for rejected source forms.
- `contextual`: contextual keyword parsing and member-name cases.
- `anonymous`：匿名函数基础用例。
- `async`：异步函数、异步生成器和异步生命周期用例。
- `c`：C ABI 类型用例。
- `call`：调用参数、默认参数和命名参数用例。
- `cgen`：代码生成、运行时 lowering 和生成 C 快照用例。
- `class`：class/native-backed class 基础语义用例。
- `cli`：CLI link/loadlib 用例。
- `companion`：同名 C companion 用例。
- `comments`：注释词法语法用例。
- `container`：容器成员 API 和运算符补全用例。
- `containers`：容器基础语义用例。
- `control`：控制流用例。
- `definite`：确定赋值数据流用例。
- `enum`：数值枚举、带载荷枚举和 match 用例。
- `export`：xlang 导出 C ABI 用例。
- `extlib`：随发布包扩展库的 xlang 回归用例。
- `extern`：C extern 函数/变量用例。
- `function`：function 类型基础用例。
- `future`：future 值、等待、取消和生命周期用例。
- `generic`：泛型类型基础用例。
- `generics`：泛型语法基础用例。
- `hir`：历史文件名前缀；当前覆盖 typed XLIR 类型与 lowering 边界。
- `import`：source import/package 导入用例。
- `invalid`：无效运算和无效 range 负例。
- `layout`：布局和语句分隔用例。
- `member`：成员与索引用例。
- `nullable`：可空类型、空值收窄和安全访问用例。
- `operators`：运算符基础用例。
- `overload`：可变参数重载用例。
- `overloads`：重载基础用例。
- `parse`：解析器负例和边界诊断用例。
- `perf`：长进程、递归分配和大 JSON/AST 生命周期性能回归。
- `protocol`：用户协议、约束、见证表和动态派发用例。
- `sem`：语义诊断负例。
- `stdlib`：标准库函数与错误路径用例。
- `type`：type 值和类型别名用例。
- `typed`: typed container, nullable TypeDesc and ownership matrix cases.
- `varargs`：可变参数基础用例。
- `weak`：弱引用生命周期和升级用例。
- `xlang`：xlang 嵌入、嵌套运行和脚本上下文用例。
- `xl`：`xl run` 参数用例。
- `xlir`：统一 XLIR 直接 AST 构建与确定性 C 发射用例。
- `xruntime`：xruntime 类型协议、动态字段和语言桥接用例。
<!-- case-prefix-index:end -->

## 守门脚本

- `tools/check_test_case_wiring.ps1`：所有 `test/cases` 下递归发现的 `.xl` 必须按相对路径接入回归入口，且本 README 的前缀索引必须与当前用例一致。
- `tools/check_test_data_wiring.ps1`：所有 JSON 数据 fixture 必须有效，并通过完整相对路径或 package/import 语义被引用。
- `tools/check_c_fixture_wiring.ps1`：所有 C/H fixture 必须被测试、范例、DLL 或同 package 目录内 metadata 接线。
- `tools/check_stdlib_test_wiring.ps1`：所有 `test/cases/stdlib_*.xl` 必须按完整相对路径接入主回归。

### B10 整数常量与组件集合

`tools/check_stdlib_text_values.ps1` 在原 B1—B9 门上追加最小有符号常量、
四个整数越界负例、七个存量整数回归，以及同一生产组件集合内核的严格 GCC
与 TinyCC 检查。`tools/check_xlir_scalar_component_kernel.sh` 接入 Linux
入口，验证 10 个人口规模至 257、全部双根与独立 Floyd 结果、真实组件图、
Future/网络 IR 依赖、确定性 C、JIT/AOT 与损坏常量在 C/header 输出前拒绝。
Linux ASan/UBSan 范围仅为集合内核和有符号常量的生成 C 宿主，不能把所链接
但未 instrument 的共享库算为 sanitizer 覆盖。整数回归只声称其实际执行
路径；此批不替代全仓 RC0 或 53 模块完成验收。

### B11 字符串存量与格式合同

`tools/check_xlir_string_join.ps1` 为当前源码冻结输入主门；旧 bridges/repeat
入口委托同一矩阵。覆盖 10 个声明负例、typed array/char/bytes、精确 NUL
视图、2000 次多线程 join、600 repeat、语言故障 catch、确定性 C/H/IR、
严格 C/JIT/AOT、char 插值及其 IR 操作损坏拒绝。插值宿主对实际逻辑分配
逐点失败，和可恢复的缓存预分配区别开来。Linux 主门
`tools/check_xlir_string_join.sh` 已接入 test_linux.sh：原生 selected XRT
和文本 provider 用 ASan/UBSan，链接共享库的生成宿主用逻辑账本；两者
覆盖范围不能混同。额外独立产物门核对实际闭包，不以全运行库宿主链接代替
裁剪验收。此批不是全部 53 模块或全仓 RC0 完成。

### B12 有界字面量与嵌套插值

`xlir_literal_span_contract.c` 对精确缓冲区、截断前缀、128 层限制、CRLF/
UTF-8 token 位置和实际 lexer 做 101301 项检查；另跑前端 fuzz 10000 次。
`stdlib_text_literals_nested.xl` 覆盖同引号/嵌套/raw/三引号、临时 array/list、
拥有副本及复合赋值。主门还核对 10 个插值负例/历史诊断，禁止错误输入
产生 C/H/IR；新有效程序的全部逻辑分配点必须传播 MemoryError 并平衡
退役账本。`xlir_text_value_contract.c` 禁止旧 aborting array/list string-copy
操作并损坏 owning projection payload 验证拒绝。Linux 门真正 instrument
扫描器和实际 lexer/parser/fuzz；共享库宿主仍只声称逻辑账本覆盖。

### B13 Regex 编译与独立匹配

`check_xlir_regex_values.ps1` 接入中央参数化场景调度器；
`check_stdlib_regex.sh` 接入 test_linux.sh。覆盖 `stdlib_regex_core_only.xl`、
`stdlib_regex_match.xl`、`stdlib_regex_errors.xl`，以及错误 ABI 的返回值、
原生所有者、构造参数数、nullable 捕获合同四个负例。复用
`xlir_text_value_contract.c` 损坏并拒绝所有新声明的 ABI/效果/所有权信息，
复用 `xlir_string_interpolation_host.c` 完整逐点语言 OOM/退役账本。
`xlir_regex_host.c` 与 `test/support/regex_native.c` 是原生真实依赖探针，
检查 NUL、空组/未匹配、独立寿命、1000 次跨线程读取和 68 个分配失败点。
Linux sanitizer 覆盖原生 XRT/adapter；共享库生成宿主仍只计逻辑账本。
另验三个实际 AOT 原生闭包与按需 CORE/MATCH；不是 all/replace/split/set/
Pattern 或全部 53 模块完成证明。

### B17b PatternBuilder 事务与快照

Regex 专项门新增 `stdlib_pattern_builder.xl`、
`stdlib_pattern_builder_lifetime.xl`、`stdlib_pattern_builder_cleanup.xl`，
验证批量/实例键、取消与陈旧键、缓存快照、对象标签寿命及提交后析构错误。
生命周期正例逐点注入真实语言分配失败并检查首个 no-hit 与退役账本。
四个新增 ABI 负例拒绝错误标签表种类、整数宽度、批量标签类型与键所有者，
另加 Builder private 负例。`xlir_pattern_builder_host.c` 和
`test/support/pattern_builder_native.c` 检查准备/应用/键/编译实际 OOM、COW
标签快照、孤立编辑、创建线程与取消键永久失效。Linux 对该原生 host
执行 ASan/UBSan/leak 检查，语言共享库宿主仍只声称逻辑账本覆盖。

### B97 拥有型 Array / 动态索引读取

`tools/check_stdlib_array_value_read.py` 验证 `stdlib_array_value_read.xl`
及 continuation 的真实 IR、C/header 拒绝、C/IR 确定性、严格 O0/O2、
模块逐点 OOM、JIT/AOT。contract 抽取真实函数与 TypeId，原生 projection
host 用其调用生成函数，检查 char、nullable、NULL 失败、BUSY、finally
和拥有账本。声明头另作无 XRT 实现的独立 C11 编译。
`check_stdlib_array_primitive_admission.py --reads` 重用 11 个真实物理闭包，
检查 strict/safe/keyed native/wire 读取、codec、NUL、寿命和完整分配前缀。
Array / Stack 父门已纳入这些测试；不表示源 Array carrier 已迁移完成。

### B98 Sequence 构造配方 / Array 显式导入

`tools/check_stdlib_array_import.py` 对九类实际构造闭包执行 O0/O2、
wire/native 导入、真实拥有边、BUSY 和完整 OOM 前缀。
`check_stdlib_array_primitive_admission.py --imports` 对 11 类实际 codec
验证精确 text/NUL/nullable、同 codec 无装箱复制、不同 codec 转换和
失败清理；Linux native lanes 用 ASan/UBSan/leak 插桩选定闭包全源。
`check_stdlib_sequence_recipe_emit.py` 使用真实 Cgen 私有渲染器及实际
语言类型；每配方 15 次损坏均在输出前拒绝，将九类 native target 构造
实际编译并运行 100 轮。Array/Stack 父门接入以上子门。源 Array
仍为 Value-v1；此门不能冒充整个语言 Array producer/carrier 已迁移。

`check_stdlib_sequence_schema_language.py` 验证完整递归 schema 工厂在
真实语言程序中的严格 C、JIT/AOT、确定性和十条当前线程 OOM 前缀。
原生嵌套准入门另外验证同布局错域/孙级策略拒绝、64 层独立元数据、
BUSY、foreign/unprofiled 拒绝、循环拥有图及深层构造完整失败前缀。
schema 只拥有准入元数据，不缓存源代码地址或子容器存储；没有切换
源 Array carrier。已接入 Stack 父门；新增接线不等于父套件重跑通过。

### B99 Array resident 核心 / 查询方法

`tools/check_stdlib_array_methods.py` 在九类实际 native/wire 闭包执行
严格 O0/O2、拥有边、未绑定/BUSY/错误角色拒绝及完整 OOM 前缀。
Linux native lanes 用 ASan/UBSan/leak 插桩选定闭包全源；shared lanes
只插桩宿主，不声称共享 runtime 全源插桩。
`tools/check_stdlib_array_methods_language.py` 检查
`test/cases/stdlib_array_methods.xl` 和
`test/cases/stdlib_array_methods_continuation.xl` 的真实 IR 错误合同、
C/header 输出前拒绝、确定性、严格 O0/O2、finally、模块 OOM、
JIT/AOT；抽取真实函数和 TypeId 后，以 native int/双字段 string Array
调用生成函数，检查 NUL、顺序、别名和拥有账本及逐点 OOM。
`test/cases/stdlib_array_methods_core_only.xl`、
`test/cases/stdlib_array_methods_insert_only.xl`、
`test/cases/stdlib_array_methods_find_only.xl` 用于实际 artifact 组件裁剪。
Array/Stack 父门接线；pop/take、resize 默认值、动态分派及整个源 carrier
切换不属于本批完成范围。

### B100 Array resident 默认值 / 扩容

`tools/check_stdlib_array_methods.py --defaults` 使用
`test/cases/stdlib_array_resident_default_host.c` 和独立声明头
`test/cases/stdlib_array_default_header.c` 验证 12 类真实原生槽位，
覆盖别名/快照、nullable、禁止默认构造、BUSY/未绑定/错误角色，
容量足够与重新扩容两类完整 OOM 前缀。Linux 插桩选定 native 闭包全源。
`tools/check_stdlib_array_defaults.py` 继续验证 wire carrier 的逻辑默认值。

`tools/check_stdlib_array_methods_language.py --defaults` 检查
`test/cases/stdlib_array_resident_defaults.xl` 与
`test/cases/stdlib_array_resident_defaults_continuation.xl` 的真实 IR、
严格 O0/O2、JIT/AOT、async/generator、finally 和完整模块 OOM；
抽取真实函数/TypeId 后以原生 int/string Array 调用生成函数。
完整模块 OOM host 对显式 main 选择 USER_ENTRY，并要求首个 no-hit
为非零，防止只执行 module_init 而没有实际执行用户程序的空通过。
`test/cases/stdlib_array_default_commit_host.c` 验证提交后析构错误、
回调重入 BUSY 拒绝及保留首错；Linux 单独插桩此原生闭包全源，
不将共享 runtime 的宿主插桩说成全源插桩。
`test/cases/stdlib_array_resize_only.xl` 用于真实 artifact 组件裁剪。
Array/Stack 父门接入新 native 子门，Stack 父门同时接入语言子门。
pop/take、动态分派和全部 Array source carrier 切换仍需后续完成。

自有字符串专项：`extern_c_variadic_text.xl` 配合 `extern_c_variadic_text_host.c` 覆盖 no_raise 实参准备与 C 默认提升；`extern_owned_string_public.xl`、`extern_owned_string_public_host.c`、`extern_owned_string_public_consumer.in` 由 `tools/check_extern_owned_string_public.py` 验证独立 C 头文件及 120 个 OOM 前缀。

`extern_c_variadic_value.xl` 验证原生可变参数函数值的传参、重绑定、worker、默认提升和 32 个 OOM 前缀；`bad_extern_owned_variadic_tail.xl` 与 `bad_extern_owned_variadic_signature.xl` 验证无物理 ABI 的尾参与无固定参数签名被拒绝。

`xlir_native_variadic_contract.c` 使用实际可变参数函数值程序构建 XLIR，扰动入口、签名、元数据偏移、捕获、回调擦除与调用实参，要求六种错误在 C/头文件输出之前被拒绝。

- `xlir_native_variadic_oom.c`: source-object fault injection for all four native variadic metadata arrays and the first parameter name, plus partial-module disposal; run by `tools/check_extern_owned_string_public.py`.

- `extern_borrowed_string_failure.xl` / `extern_borrowed_string_failure_host.c`: borrowed C return admission, no-raise producer plus copy OOM, direct/fixed/native-variadic/protocol/getter/dynamic paths, and unreadable failed payloads.

- `extern_owned_task_scalars.xl` / `extern_owned_task_scalars_host.c`: all 20 exact C scalar task arguments, platform limits, ordinary/static extern callables, nullable/void results, failed nonterminated payloads, worker OOM and 32 caller allocation prefixes. `xlir_task_c_scalar_contract.c` rejects six corrupted frame/signature contracts, including byref/out storage, before C/header emission. The owned-string gates also reject `bad_extern_owned_task_variadic.xl`.
- `extern_owned_task_defaults.xl` / `extern_owned_task_composition_host.c`: top-level/static extern arity adapters, native and ordinary defaults, per-invocation evaluation and default failure skipping the producer. `extern_owned_task_nullable.xl` covers aliases and non-null branch proofs; `bad_extern_owned_task_nullable.xl` and `bad_extern_owned_task_nullable_invalidated.xl` require a current proof before task capture.
- `extern_owned_task_generic.xl`: concrete generic spawn/all/race/whenAny, including literal and stored-array aggregates, native unsigned limits, managed strings and producer failure/reuse. `extern_owned_task_all_scalars.xl` checks all 20 native result representations (including the existing shared bool singleton), platform limits and floating special bits. `xlir_task_composition_contract.c` rejects nine damaged task/payload/proof contracts before C/header output. Both owned-string gates include these regressions.

- `extern_owned_static_callable.xl` / `extern_owned_static_callable_host.c`: static C/ordinary/async function values, exact owned pointer and allocation witnesses, overload/default/inherited/raw-callback paths and 32 OOM prefixes; negative static private/ambiguous/instance cases reject before C emission. The extern_owned_string package fixture also exports Factory.Text for imported static binding.

- `extern_owned_task_optional.xl`, `extern_owned_task_optional_scalars.xl`: nullable task inputs, flow-refined argument injection, null string aggregation and all 20 optional C scalar payloads (including floating special bits). `extern_owned_task_continuations.xl` covers concrete generic then/catch/finally source and result contracts. The owned-string public compiler gate runs JIT, AOT and emitted-C checks for these fixtures.

- `extern_owned_task_generic_callable.xl`: explicitly closed generic named functions as local values, task callbacks, returned callbacks, nullable and asynchronous signatures; nested array types and reordered type parameters share the ordinary monomorphic adapter. `xlir_task_optional_contract.c` rejects ten damaged nullable, continuation and generic-reference contracts before C/header emission.
