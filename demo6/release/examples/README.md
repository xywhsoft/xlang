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
- `container_methods.xl`：typed/dynamic 容器成员 API 示例；回归：`check`、`run`。
- `container_ops.xl`：typed 容器拼接、集合运算和复合赋值示例；回归：`check`、`run`。
- `anonymous_functions.xl`：匿名函数与函数值基础示例；回归：`check`。
- `generic_identity.xl`：泛型函数最小示例；回归：`check`。
- `generics.xl`：泛型函数、泛型调用和泛型容器示例；回归：`check`。
- `generic_constraints.xl`：泛型约束、class 继承约束和 cloneable 约束示例；回归：`check`、`run`。
- `generic_class_iter.xl`：泛型 class 的 `__iter__` / `__next__` 迭代协议示例；回归：`run`。
- `runtime_type.xl`：运行时 `is` 判断、`as` checked downcast、函数签名和泛型容器类型示例；回归：`run`。
- `class_reference_model.xl`：统一堆 class、自引用可空字段，以及默认/`byval` 受管理引用语义；回归：`run`。
- `typed_point.xl`：`point<T>` 静态 opaque pointer 标签和 `null` 规则；回归：`run`。
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
- `class_native_backed.xl`：class 与 native-backed class 组合示例；回归：`check`。
- `class_native_bridge.xl`：native-backed class 对接 C opaque handle 的声明示例；回归：`check`。
- `native_counter.xl`：native-backed class 句柄类示例；回归：`check`。
- `second_stage.xl`：第二阶段基础语义组合示例；回归：`check`、`run`。
- `top_level_main.xl`：顶层语句与 `main(...)` 共存示例，验证顶层语句先于 `main` 执行；回归：`run`。
- `error_model.xl`：分类异常、异常层级捕获、`AssertionError` 和 future catch 回调示例；回归：`run`。
- `concurrency.xl`：并发、通道和异步相关示例；回归：`run`。
- `task_api.xl`：高层 task API 的 spawn、all、race 和 cancel 示例；回归：`run`。
- `async_task_wait.xl`：future wait、waitTimeout、state/error、cancel 和 task.whenAny 示例；回归：`run`。
- `async_generator.xl`：`async func ... as generator<T>`、`func ... as async generator<T>`、`await + yield`、`for await`、保存型/单次消费、错误传播、空流、双目标索引与 `break`/`continue` 示例；回归：`run`。
- `union.xl`：C 风格 union 示例；回归：`run`。
- `script_embed_tcc.xl`：内嵌 TCC 脚本运行、Context 和动态 VFS 示例；回归：`run`。
- `script_embed_xlang.xl`：内嵌 xlang 脚本运行和 `XLangContext` 示例；回归：`run`。

## C 混编与导出范例

- `c_hello.xl`：调用 C/CRT 风格函数的最小示例；回归：`run`。
- `c_vararg_printf.xl`：通过 `c_vararg` 调用 C 可变参数函数并显式保持 ABI 类型；回归：`run`。
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
- `stdlib_buffer.xl`
- `stdlib_buffer_c.c`：与 `stdlib_buffer.xl` 对照的 C/XRT xbuffer 范例，主回归会比较两者输出。
- `stdlib_path_file.xl`
- `stdlib_path_file_c.c`：与 `stdlib_path_file.xl` 对照的 C/XRT 路径、文件和目录范例，主回归会比较两者输出。
- `stdlib_os.xl`
- `stdlib_os_c.c`: C/XRT counterpart for `stdlib_os.xl`; covers args, cwd, env, shell execution, process start and chain helpers.
- `stdlib_json.xl`
- `stdlib_json_c.c`: C/XRT counterpart for `stdlib_json.xl`; covers validation, parse/stringify and file read/write.
- `stdlib_file_async.xl`
- `stdlib_file_async_c.c`: C/XRT async file counterpart; main regression compares both outputs.
- `stdlib_future.xl`
- `stdlib_future_c.c`: C/XRT counterpart for `stdlib_future.xl`; covers `then`, `catch`, result-preserving `finally`, and Future ownership.
- `stdlib_special_collections.xl`
- `stdlib_special_collections_c.c`: C/XRT counterpart for the specialized stack, AVL tree, SPSC/MPSC/MPMC queue and wait queue APIs.
- `stdlib_tls_session.xl`
- `stdlib_tls_session_pair.xl`
- `stdlib_tls_session_pair_c.c`: C/XRT counterpart for raw TLS client/server sessions, handshake, encrypted transport and close lifecycle.
- `stdlib_xson.xl`
- `stdlib_xson_c.c`: C/XRT counterpart for `stdlib_xson.xl`; covers strict validation, ignore-unsupported validation, stringify and file read/write.
- `stdlib_print.xl`
- `stdlib_dict.xl`
- `stdlib_codec.xl`
- `stdlib_codec_c.c`：与 `stdlib_codec.xl` 对照的 C/XRT 编解码范例，覆盖 HEX、Base64、百分号编码和 form-url 编解码。
- `stdlib_encoding.xl`
- `stdlib_encoding_c.c`：与 `stdlib_encoding.xl` 对照的 C/XRT 编码转换范例，展示探测、UTF-16 转换和字节视图 API。
- `stdlib_hash.xl`
- `stdlib_hash_c.c`：与 `stdlib_hash.xl` 对照的 C/XRT 非加密哈希范例，覆盖 seed 和 32/64 位变体。
- `stdlib_crypto.xl`
- `stdlib_crypto_c.c`: C/XRT counterpart for `stdlib_crypto.xl`; covers hash, HMAC, HKDF, AEAD, ChaCha20, Ed25519, X25519, X448 and RSA guard validation.
- `stdlib_id_net.xl`
- `stdlib_id_net_c.c`：与 `stdlib_id_net.xl` 对照的 C/XRT XID 和本机网络信息范例。
- `stdlib_net_tcp_udp.xl`
- `stdlib_net_tcp_udp_c.c`：与 `stdlib_net_tcp_udp.xl` 对照的 C/XRT TCP/UDP 范例，主回归会比较两者输出。
- `stdlib_url.xl`
- `stdlib_url_c.c`：与 `stdlib_url.xl` 对照的 C/XRT URL 解析、解析片段、resolve 和路径归一化范例。
- `stdlib_query.xl`
- `stdlib_query_c.c`：与 `stdlib_query.xl` 对照的 C/XRT query/form-url 编解码范例。
- `stdlib_log.xl`
- `stdlib_log_c.c`：与 `stdlib_log.xl` 对照的 C/XRT logger 范例，展示默认 logger、独立 `Logger`、文件 appender 和滚动文件。
- `stdlib_template.xl`
- `stdlib_template_c.c`：与 `stdlib_template.xl` 对照的 C/XRT template 范例，展示预编译模板、控制流、定界符配置和文件模板渲染。
- `stdlib_regex.xl`
- `stdlib_regex_c.c`：与 `stdlib_regex.xl` 对照的 C/XRT 正则库范例，展示懒加载 match object 和 regex set。
- `stdlib_process.xl`
- `stdlib_process_c.c`：与 `stdlib_process.xl` 对照的 C/XRT process 库范例，展示结构化执行、pipeline 和长生命周期进程。
- `stdlib_signal.xl`：`signal` 标准库范例，展示信号常量、普通回调、一次性回调、channel 路由、任务组取消桥接和默认行为恢复。
- `stdlib_http_client.xl`：`http` 客户端范例，展示 GET、POST、通用 request、请求对象、响应对象、header、body 和超时配置。
- `stdlib_http_client_c.c`：与 `stdlib_http_client.xl` 对照的 C/XRT HTTP client 范例。
- `stdlib_http_server.xl`：`http.server` 基础服务示例，展示简化路由、对象化路由、静态文件、流式响应、流式请求体上传、HTTPS 创建、错误处理器和统计接口。
- `stdlib_http_server_c.c`：与 `stdlib_http_server.xl` 对照的 C/XRT `xweb` HTTP server 范例，展示 GET/POST/ANY 路由、静态文件和流式请求体 route。
- `stdlib_http_server_app.xl`：`http.server` app、vhost、reload 和 `appError` 示例，展示默认 app 替换、虚拟主机、错误处理器和 app 句柄释放后的 server 持有语义。
- `stdlib_tcc_xlang_embed.xl`：TCC C 脚本、动态 VFS、编译输出和内嵌 xlang 运行范例。

## 复合应用范例

复合应用范例位于 `apps` 子目录，由 `test_apps.bat` 批量 `check` 和 `run`。这些范例优先展示“用户实际会写的小工具”，每个范例都自造输入数据、输出到 `build/test/examples_apps`，并用 `assert` 校验结果。

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
- `stdlib_buffer_c.c`：与 `stdlib_buffer.xl` 对照的 C/XRT xbuffer 范例，主回归会比较两者输出。
- `stdlib_encoding_c.c`：与 `stdlib_encoding.xl` 对照的 C/XRT 编码转换范例，主回归会比较两者输出。
- `stdlib_path_file_c.c`：与 `stdlib_path_file.xl` 对照的 C/XRT 路径、文件和目录范例，主回归会比较两者输出。
- `stdlib_net_tcp_udp_c.c`：与 `stdlib_net_tcp_udp.xl` 对照的 C/XRT TCP/UDP 范例，主回归会比较两者输出。
- `stdlib_log_c.c`：与 `stdlib_log.xl` 对照的 C/XRT logger 范例，主回归会比较两者输出。
- `stdlib_template_c.c`：与 `stdlib_template.xl` 对照的 C/XRT template 范例，主回归会比较两者输出。
- `stdlib_regex_c.c`：与 `stdlib_regex.xl` 对照的 C/XRT 正则库范例，主回归会比较两者输出。
- `stdlib_http_client_c.c`：与 `stdlib_http_client.xl` 对照的 C/XRT HTTP client 范例。
- `stdlib_http_server_c.c`：与 `stdlib_http_server.xl` 对照的 C/XRT `xweb` HTTP server 范例，展示 GET/POST/ANY 路由、静态文件和流式请求体 route。

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
