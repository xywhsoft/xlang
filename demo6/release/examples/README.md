# DEMO6 范例入口

本目录收集 demo6 当前可运行或可验证的示例。范例只描述当前实现能力，不作为语言设计来源；语言设计仍以 `docs` 目录为准。

## 推荐入口

- `tutorial/README.md`：按学习顺序组织的小型教学范例。
- `feature_suite/README.md`：按语言特性组织的完整功能范例套件。
- `apps/README.md`：把语言特性和标准库组合成小工具的复合应用范例。

## 根目录语言范例

- `hello.xl`：最小程序、CLI 基础阶段示例；回归：`lex`、`parse`、`check`、`cgen`、`ir`。
- `literals.xl`：数字、字符串、time 等字面量基础示例；回归：`lex`、`parse`。
- `containers.xl`：容器字面量、索引和基础操作；回归：`check`、`run`。
- `anonymous_functions.xl`：匿名函数与函数值基础示例；回归：`check`。
- `generic_identity.xl`：泛型函数最小示例；回归：`check`。
- `generics.xl`：泛型函数、泛型调用和泛型容器示例；回归：`check`。
- `generic_constraints.xl`：泛型约束、class 继承约束和 cloneable 约束示例；回归：`check`、`run`。
- `generic_class_iter.xl`：泛型 class 的 `__iter__` / `__next__` 迭代协议示例；回归：`run`。
- `runtime_type.xl`：运行时 `is` 判断、`as` checked downcast、函数签名和泛型容器类型示例；回归：`run`。
- `class_point.xl`：class 字段和方法基础示例；回归：`check`。
- `class_field_persistence_attrs.xl`：字段级 `@json` / `@xson` / `@persist` 持久化注解示例；回归：`run`。
- `object_protocol.xl`：object 标准基类、动态字段、索引、运算符和可调用协议示例；回归：`check`。
- `oop_protocol_overload.xl`：OOP protocol overload, multi-argument index, callable and contains example；回归：`check`、`run`。
- `oop_property_get_let.xl`：OOP property get/let, zero-dimensional and multi-dimensional property index example；回归：`check`、`run`。
- `oop_static_access.xl`：OOP static field/static method and private member example；回归：`check`、`run`。
- `oop_protected_access.xl`：OOP protected field and method access from derived class example；回归：`check`、`run`。
- `oop_virtual_dispatch.xl`：OOP virtual/override/final method and property dispatch example；回归：`check`、`run`。
- `copy_clone.xl`：`derive copy/clone`、`copy()` 和 `clone()` 示例；回归：`check`。
- `class_persistence.xl`：`derive value`、class 与 object 的 json/xson 持久化 roundtrip 示例；回归：`run`。
- `class_custom.xl`：class 与 native-backed class 组合示例；回归：`check`。
- `class_native_bridge.xl`：native-backed class 对接 C opaque handle 的声明示例；回归：`check`。
- `custom_counter.xl`：native-backed class 句柄类示例；回归：`check`。
- `second_stage.xl`：第二阶段基础语义组合示例；回归：`check`、`run`。
- `concurrency.xl`：并发、通道和异步相关示例；回归：`run`。
- `task_api.xl`：高层 task API 的 spawn、all、race 和 cancel 示例；回归：`run`。
- `async_generator.xl`：`async func ... as generator<T>`、`func ... as async generator<T>`、`await + yield`、`for await`、保存型/单次消费、错误传播、空流、双目标索引与 `break`/`continue` 示例；回归：`run`。
- `union.xl`：C 风格 union 示例；回归：`run`。

## C 混编与导出范例

- `c_hello.xl`：调用 C/CRT 风格函数的最小示例；回归：`run`。
- `c_variables.xl`：extern C 变量示例；回归：`check`。
- `c_export.xl`：xlang 导出 C ABI 和 ABI dump 示例；回归：`abi`。
- `class_c_export.xl`：class 成员函数导出 C ABI wrapper、头文件和 ABI dump 示例；回归：`cgen`、`abi`。

## 标准库范例

标准库范例文件名统一为 `stdlib_*.xl`，由 `tools/check_stdlib_examples.ps1` 批量 `check` 和 `run`，并由 `tools/check_stdlib_coverage.ps1` 参与函数覆盖校验。

- `stdlib_math.xl`
- `stdlib_string.xl`
- `stdlib_time.xl`
- `stdlib_time_c.c`：与 `stdlib_time.xl` 对照的 C/XRT 时间库范例，主回归会比较两者输出。
- `stdlib_random.xl`
- `stdlib_random_c.c`：与 `stdlib_random.xl` 对照的 C/XRT 随机库范例，展示全局随机源和独立随机对象。
- `stdlib_path_file.xl`
- `stdlib_path_file_c.c`：与 `stdlib_path_file.xl` 对照的 C/XRT 路径、文件和目录范例，主回归会比较两者输出。
- `stdlib_json.xl`
- `stdlib_xson.xl`
- `stdlib_print.xl`
- `stdlib_codec.xl`
- `stdlib_hash.xl`
- `stdlib_crypto.xl`
- `stdlib_id_net.xl`
- `stdlib_url.xl`
- `stdlib_query.xl`
- `stdlib_regex.xl`
- `stdlib_process.xl`
- `stdlib_http_server.xl`
- `stdlib_http_server_c.c`：与 `stdlib_http_server.xl` 对照的 C/XRT `xweb` HTTP server 范例，展示 GET/POST/ANY 路由和静态文件。

## 复合应用范例

复合应用范例位于 `apps` 子目录，由 `test_apps.bat` 批量 `check` 和 `run`。这些范例优先展示“用户实际会写的小工具”，每个范例都自造输入数据、输出到 `release/tmp/examples_apps`，并用 `assert` 校验结果。

- `apps/asset_manifest.xl`：文件扫描、hash 和 JSON 清单。
- `apps/cli_args.xl`：`main(...)` 命令行参数解析。
- `apps/config_loader.xl`：JSON 配置加载和默认值。
- `apps/csv_sales_report.xl`：CSV 文本解析与汇总。
- `apps/file_backup.xl`：文件复制和备份元数据。
- `apps/log_summary.xl`：日志级别统计。
- `apps/static_site_builder.xl`：静态 HTML 页面生成。
- `apps/template_mail_merge.xl`：f-string 模板合并。
- `apps/todo_json_store.xl`：class value JSON 持久化。
- `apps/url_batch_builder.xl`：URL 和 query string 批量构造。

## DLL 宿主范例

以下 C 文件用于 `xl.dll` 嵌入式运行库验证：

- `xl_dll_embed_string.c`
- `xl_dll_link_host.c`
- `xl_dll_loadlib_host.c`
- `xl_dll_null_context.c`
- `xl_dll_threaded_host.c`
- `xl_dll_xrt_exports.c`
- `xl_run_demo_win32.c`
- `xlc_api_embed.c`
- `stdlib_math_c.c`：与 `stdlib_math.xl` 对照的 C/XRT 数学库范例，主回归会比较两者输出。
- `stdlib_string_c.c`：与 `stdlib_string.xl` 对照的 C/XRT 字符串库范例，主回归会比较两者输出。
- `stdlib_time_c.c`：与 `stdlib_time.xl` 对照的 C/XRT 时间库范例，主回归会比较两者输出。
- `stdlib_random_c.c`：与 `stdlib_random.xl` 对照的 C/XRT 随机库范例，主回归会比较两者输出。
- `stdlib_path_file_c.c`：与 `stdlib_path_file.xl` 对照的 C/XRT 路径、文件和目录范例，主回归会比较两者输出。
- `stdlib_http_server_c.c`：与 `stdlib_http_server.xl` 对照的 C/XRT `xweb` HTTP server 范例，展示 GET/POST/ANY 路由和静态文件。

## 回归入口

在 `demo6` 目录下运行：

```bat
test_tutorial.bat
test_feature_suite.bat
test_apps.bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_examples.ps1
```

完整主回归使用：

```bat
test.bat
```
