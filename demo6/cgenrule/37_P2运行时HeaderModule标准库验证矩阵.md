# P2 运行时、Header、Module、标准库验证矩阵

本文继续细化 `34_规则测试范例交叉索引.md`，覆盖以下 P2 规则文档：

- `14_P2_标准库wrapper细则.md`：20 条
- `15_P2_header与needs细则.md`：19 条
- `17_P2_module输出顺序细则.md`：20 条
- `18_P2_runtime_helper细则.md`：35 条

合计 94 条规则。本文件不新增 `CGEN-*` 规则，只记录这些运行时/产物级规则当前由哪些测试、范例和工具脚本证明，以及剩余非阻断增强项。

## 标准库 Wrapper

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-STD-001` wrapper 注入总规则 | `tools/check_stdlib_coverage.ps1`、`tools/check_stdlib_test_wiring.ps1`、`tools/check_stdlib_examples.ps1`、`stdlib_*.xl` | 覆盖脚本 + 运行行为检查 | 公开标准库函数必须同时出现在 docs/tests/examples 中，测试入口会先跑覆盖脚本。 |
| `CGEN-STD-002` crypto SHA HEX | `stdlib_crypto.xl`、`examples/stdlib_crypto.xl`；`test.bat` 检查 `xrtSHA256`、`xl6_xrt_sha512_hex` | 直接生成检查 + 运行行为检查 | SHA wrapper 与 XRT 原型均有覆盖。 |
| `CGEN-STD-003..005` url part/flag/port/resolve/normalizePath | `stdlib_url.xl`、`stdlib_error_paths.xl`、`examples/stdlib_url.xl`；`test.bat` 检查 `xrtUrlParseViewN`、`xrtUrlResolveTo`、`xl6_xrt_url_resolve`，并检查 invalid base fallback | 直接生成检查 + 运行行为检查 + 错误路径快照 | URL 解析、属性、resolve、路径规范化有覆盖；非法 URL、非法 base resolve、含空格 path normalize 的空串收束已有集中错误路径测试。 |
| `CGEN-STD-006..007` query encode/decode/valid/pair/append | `stdlib_query.xl`、`stdlib_error_paths.xl`、`examples/stdlib_query.xl`；`test.bat` 检查 `xrtPercentEncodeTo`、`xrtFormUrlEncodedDecodeTo`、`xl6_xrt_query_append`，并检查 decode 失败释放 `outText` 后返回空串 | 直接生成检查 + 运行行为检查 + 错误路径快照 | query 编解码与拼接 wrapper 有覆盖；空 key invalid 与非法 percent decode 失败收束已有集中错误路径测试。 |
| `CGEN-STD-008` regex wrapper | `stdlib_regex.xl`、`examples/stdlib_regex.xl`；`test.bat` 检查 `xrtRegexCreate`、`xl6_xrt_regex_find` | 直接生成检查 + 运行行为检查 | regex 创建、匹配、find helper 有覆盖。 |
| `CGEN-STD-009..010` process exec/success/exitCode | `stdlib_process.xl`、`stdlib_error_paths.xl`、`examples/stdlib_process.xl`；`test.bat` 检查 `xrtExecCapture`、`xl6_xrt_process_exec`、`xl6_xrt_process_exit_code`，并检查 exec/success 结果清理 | 直接生成检查 + 运行行为检查 + 错误路径快照 | 进程执行、输出、退出码 wrapper 有覆盖；非 0 exit code 的 success/exitCode/exec 行为和 result cleanup 已有集中错误路径测试。 |
| `CGEN-STD-011` dir list/files/dirs | `stdlib_path_file.xl`；`test.bat` 检查 `xrtDirScanEx`、`xl6_xrt_dir_list`、`__xl_std_dir_files`、`__xl_std_dir_dirs` | 直接生成检查 + 运行行为检查 | 目录扫描和动态容器结果构造有覆盖。 |
| `CGEN-STD-012..013` string startsWith/endsWith/split | `stdlib_string.xl`、`examples/stdlib_string.xl`、`cgen_string_*` | 直接生成检查 + 运行行为检查 | 字符串 wrapper 与基础字符串 runtime 组合有覆盖。 |
| `CGEN-STD-014` path join | `stdlib_path_file.xl`、`examples/stdlib_path_file.xl`；`test.bat` 检查 `xrtPathJoin`、`xl6_xrt_path_join`、`__xl_std_path_join` | 直接生成检查 + 运行行为检查 | path.join runtime helper 和标准库 helper 均有覆盖。 |
| `CGEN-STD-015..019` http.server 类型、handler 生命周期、request bridge、create/start/port/stop/destroy | `stdlib_http_server.xl`、`stdlib_http_server_host.c`、`examples/stdlib_http_server.xl`；`test.bat` 检查 `xrtHttpdCreate`、`xl6_xrt_http_server_create` 并运行宿主请求 | 直接生成检查 + C 宿主运行 | HTTP server wrapper、function handler 桥接、端口读取和销毁有强覆盖。 |
| `CGEN-STD-020` 标准库 wrapper 级联 runtime 依赖 | 全部 `stdlib_*.xl` cgen + 编译运行；覆盖脚本检查 DLL 符号 | 直接生成检查 + 运行行为检查 | wrapper 触发相关 runtime 原型与 helper 的路径有批量覆盖；缺少更细的一对一 needs 快照。 |

## Header 与 Needs

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-HDR-001..005` header 外壳、UTF-8 编码、字符串符号、类型别名、C 类型映射 | `export_*_header_host.c`、`export_c_abi_types.xl`、`export_alias.xl`、`cgen_unicode_identifiers.xl` | 直接生成检查 + C 宿主运行 | header 基础格式、别名展开、C ABI 类型映射和 UTF-8 符号有覆盖。 |
| `CGEN-HDR-006..008` union/class/inline union 字段与依赖 | `export_union.xl`、`export_union_class_field.xl`、`export_class.xl`、`export_class_inherit.xl`、`export_class_union_field.xl` | 直接生成检查 + C 宿主运行 | union、class、继承、class/union 字段依赖排序有覆盖。 |
| `CGEN-HDR-009..010` export 函数与变量声明 | `export_add.xl`、`export_bool.xl`、`export_multi_return.xl`、`export_vars.xl` | 直接生成检查 + C 宿主运行 | 函数原型、变量声明、多返回 wrapper 和 C 宿主调用有覆盖。 |
| `CGEN-HDR-011` header 输出顺序 | `xl cgen --emit-header`、`xl cgen --dump-dir`、export header host 系列 | 直接生成检查 + C 宿主运行 | 依赖先于引用输出的路径由多类 export header 编译证明。 |
| `CGEN-NEEDS-001..003` module/basic/stdlib needs 收集 | `examples/hello.xl --dump-dir`、`stdlib_*.xl`、`cgen_*` 生成文本检查 | 直接生成检查 | module needs 与标准库 wrapper 触发有覆盖。 |
| `CGEN-NEEDS-004..008` string/convert/function-var/generic 需求扫描与级联依赖 | `cgen_string_concat.xl`、`cgen_type_conversions.xl`、`cgen_function_named_value.xl`、`cgen_generic_*.xl`、`cgen_var_runtime_objects.xl`、`cgen_minimal_runtime.xl` | 直接生成检查 + 运行行为检查 + 负向快照 | string、转换、function var、泛型实例和 xvalue 级联 runtime 需求有覆盖；最小程序不注入 function/xvalue/std wrapper 的负向快照已补。 |

## Module 总装配

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-MODULE-001..003` 入口校验、预收集、needs 聚合 | `examples/hello.xl --dump-dir`、`import_metadata.xl`、generic/class/function value 系列 | 直接生成检查 + 运行行为检查 | 入口校验、导入元数据不生成 main、匿名/泛型/runtime type 预收集有覆盖。 |
| `CGEN-MODULE-004..007` 固定序言、基础 runtime、xvalue/type desc/main arg、标准库 wrapper 注入 | `hello.xl --dump-dir`、`cgen_main_entry.xl`、`cgen_var_runtime_objects.xl`、`stdlib_*.xl` | 直接生成检查 + 运行行为检查 | 生命周期、main 参数、xvalue、runtime type desc、标准库 wrapper 注入有覆盖。 |
| `CGEN-MODULE-008..012` union/class/custom、多返回、顶层函数/extern/export/async、泛型函数/class 方法原型顺序 | export/class/custom/generic/multi-return 系列 | 直接生成检查 + C 宿主运行 | 声明顺序通过生成片段和 C 宿主编译验证。 |
| `CGEN-MODULE-013..016` function wrapper/匿名函数体、class/custom 方法体、普通/generator/async/export 函数体、泛型实例函数体 | `cgen_function_named_value.xl`、`cgen_closure.xl`、`cgen_class*.xl`、`cgen_custom*.xl`、`cgen_generator.xl`、`cgen_async.xl`、`cgen_generic_*.xl` | 直接生成检查 + 运行行为检查 | 主体输出顺序覆盖充分。 |
| `CGEN-MODULE-017..019` xlang main、脚本 main、异常退出路径 | `cgen_main_entry.xl`、`examples/hello.xl`、`cgen_loop_exception_propagation.xl`、`cgen_exception_uncaught.xl`；`test.bat` 检查未捕获异常 stderr 中包含 `uncaught Error: uncaught` | 直接生成检查 + 运行行为检查 + 运行负例 + 负例诊断检查 | xlang main wrapper、顶层脚本 main、异常退出和未捕获错误诊断均有覆盖。 |
| `CGEN-MODULE-020` 生成状态汇总与上下文资源释放 | 大量 `xl cgen` 成功/失败路径、invalid/sem 用例 | 间接覆盖 | 主要通过稳定运行与失败诊断间接覆盖；建议后续引入内存/资源泄漏巡检。 |

## Runtime Helper

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-RTH-001..006` approximate/time/string/int pow/range helper | `cgen_approx.xl`、`cgen_time_literals.xl`、`cgen_string_concat.xl`、`cgen_string_compare.xl`、`cgen_pow.xl`、`cgen_in_range.xl` | 直接生成检查 + 运行行为检查 | 基础 helper 原型、调用点和运行结果有覆盖。 |
| `CGEN-RTH-007..011` header 生命周期声明、内嵌生命周期、`xlInitOptions`/`xlInit`/`xlUnit`、DLL attach/detach | export/shared build、`cgen_main_entry.xl`、`test_dll.bat` | 直接生成检查 + DLL/API 测试 | 生命周期 C ABI、初始化/反初始化命名和 DLL 动态执行路径有覆盖。 |
| `CGEN-RTH-012..016` argv 到 `arg`、xlang main wrapper、退出码与错误处理 | `cgen_main_entry.xl`、`cgen_main_uncaught.xl`；`test.bat` 运行 `alpha beta` 期望 argc=3，并检查 `xl6_argv_to_arg`、`xlInitOptions(argc, argv)`、`xl_user_main(__xl6_arg)`、`xlUnit()`；显式 main 未捕获错误负例检查 `xl6_error_print_uncaught` 与 stderr 诊断 | 直接生成检查 + 运行行为检查 + 负例诊断检查 | main 参数桥、初始化、退出码、反初始化和显式 main 未捕获错误可见诊断路径有强覆盖。 |
| `CGEN-RTH-017..020` 类型转换、临时字符串、toString、xvalue toString | `cgen_type_conversions.xl`、`cgen_implicit_methods.xl`、`cgen_for_in_containers.xl` | 直接生成检查 + 运行行为检查 | 转换 runtime、隐性方法和动态字符串化有覆盖。 |
| `CGEN-RTH-021..025` xvalue runtime 类型、类型码、动态 `type(value)`、type desc 绑定、typed item 空值兜底 | `cgen_var_values.xl`、`type_values.xl`、`cgen_var_runtime_objects.xl`、typed container 系列 | 直接生成检查 + 运行行为检查 | xvalue 类型系统、动态 type、type desc 绑定和 typed item 装箱有覆盖。 |
| `CGEN-RTH-026..027` typed container 转动态容器与深复制 | `cgen_typed_container_box_to_var.xl`、`cgen_typed_container_copy_assign.xl`、typed container object/function 系列 | 运行行为检查 | 静态容器进入 var、复制、对象/function 元素生命周期有覆盖。 |
| `CGEN-RTH-028..033` 动态索引、长度、比较、二元、一元、`in` | `cgen_index_access.xl`、`cgen_safe_index.xl`、`cgen_var_values.xl`、`cgen_in_containers.xl`、`invalid_*operator.xl` | 运行行为检查 + 负例语义检查 | 动态读写、truthy、比较、运算、包含判断和非法运算有覆盖。 |
| `CGEN-RTH-034..035` 动态 array/list/set/dict 构造 helper | `cgen_containers.xl`、`cgen_for_in_containers.xl`、`cgen_varargs.xl`、`call_named_vararg.xl` | 直接生成检查 + 运行行为检查 | 动态字面量、var 前缀 typed literal fallback、`...`/`***` 打包有覆盖。 |

## 剩余风险与后续增强

| 缺口 | 建议补法 |
|---|---|
| runtime needs 的“简单程序不注入重型 runtime”已由 `cgen_minimal_runtime.xl` 覆盖。 | 后续可继续把负向快照细化到单个 wrapper/单个 helper 级别。 |
| `CGEN-MODULE-020` 上下文资源释放主要靠稳定运行间接证明。 | 后续可增加 ASAN/valgrind 或 XRT 内存统计模式，用于 cgen 成功/失败路径资源巡检。 |
| 标准库 wrapper 错误分支已补 regex/url/query/process 的集中失败输入和生成快照。 | 后续可继续为 http.server create 失败注入、极端 buffer 溢出保护和 XRT 内存统计模式补更细资源巡检。 |

## 阶段结论

运行时、Header、Module、标准库 wrapper 已完成矩阵级整理；符号、泛型、调用解析与类型推断矩阵已落在 `38_P2符号泛型调用类型推断验证矩阵.md`。
