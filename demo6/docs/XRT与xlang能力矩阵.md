# XRT 与 xlang 能力矩阵

此文件由 `tools/gen_xrt_xlang_matrix.py` 从仓内快照、TCC 符号表和当前语言实现生成，请勿手工修改。

## 契约结论

- XRT 核心公共函数：3168。
- xruntime 产品公共函数：3626，其中扩展自有 458。
- 低层能力：3626 个函数全部进入 TCC 符号面，可通过显式 `extern "xrt..."` 使用。
- 头文件内联辅助函数单独记录于 `headerInlineReferences`，不增加外部符号数量；未声明的新引用仍报错。
- 语言内核或标准库已直接使用：1774 个函数。其余 1852 个保留为低层能力，不视为功能丢失。
- 已迁出核心但尚待扩展库适配的旧引用：0，仅包含 xhttp=0、xws=0、xregex=0。

逐 API 详情可执行 `python tools/gen_xrt_xlang_matrix.py --report build/reports/xrt_xlang_api_matrix.json` 生成。

## 模块矩阵

| 产品 | 公共头 | API | 语言抽象/内核使用 | 仅低层 |
| --- | --- | ---: | ---: | ---: |
| xrt | `include/xrt/array.h` | 50 | 0 | 50 |
| xrt | `include/xrt/asn1.h` | 27 | 0 | 27 |
| xrt | `include/xrt/atomic.h` | 29 | 17 | 12 |
| xrt | `include/xrt/avl.h` | 43 | 0 | 43 |
| xrt | `include/xrt/buffer.h` | 23 | 20 | 3 |
| xrt | `include/xrt/cancel.h` | 15 | 12 | 3 |
| xrt | `include/xrt/channel.h` | 42 | 14 | 28 |
| xrt | `include/xrt/charset.h` | 72 | 40 | 32 |
| xrt | `include/xrt/codec.h` | 20 | 8 | 12 |
| xrt | `include/xrt/compress.h` | 18 | 0 | 18 |
| xrt | `include/xrt/console.h` | 22 | 22 | 0 |
| xrt | `include/xrt/core.h` | 16 | 10 | 6 |
| xrt | `include/xrt/coroutine.h` | 51 | 0 | 51 |
| xrt | `include/xrt/crypto.h` | 122 | 4 | 118 |
| xrt | `include/xrt/environment.h` | 4 | 4 | 0 |
| xrt | `include/xrt/error.h` | 29 | 25 | 4 |
| xrt | `include/xrt/error_format.h` | 1 | 0 | 1 |
| xrt | `include/xrt/executor.h` | 10 | 0 | 10 |
| xrt | `include/xrt/file.h` | 100 | 94 | 6 |
| xrt | `include/xrt/file_async.h` | 34 | 28 | 6 |
| xrt | `include/xrt/future.h` | 74 | 58 | 16 |
| xrt | `include/xrt/future_bridge.h` | 10 | 0 | 10 |
| xrt | `include/xrt/hash.h` | 9 | 9 | 0 |
| xrt | `include/xrt/html.h` | 3 | 1 | 2 |
| xrt | `include/xrt/http.h` | 59 | 0 | 59 |
| xrt | `include/xrt/http1.h` | 25 | 13 | 12 |
| xrt | `include/xrt/http1_net.h` | 2 | 0 | 2 |
| xrt | `include/xrt/http1_tls.h` | 2 | 0 | 2 |
| xrt | `include/xrt/http_connection.h` | 5 | 0 | 5 |
| xrt | `include/xrt/http_decode.h` | 10 | 8 | 2 |
| xrt | `include/xrt/http_encoding.h` | 12 | 0 | 12 |
| xrt | `include/xrt/http_expect.h` | 8 | 0 | 8 |
| xrt | `include/xrt/http_te.h` | 10 | 0 | 10 |
| xrt | `include/xrt/http_trailer.h` | 6 | 0 | 6 |
| xrt | `include/xrt/http_upgrade.h` | 10 | 0 | 10 |
| xrt | `include/xrt/io.h` | 46 | 35 | 11 |
| xrt | `include/xrt/json.h` | 30 | 28 | 2 |
| xrt | `include/xrt/jsonl.h` | 12 | 11 | 1 |
| xrt | `include/xrt/list.h` | 28 | 0 | 28 |
| xrt | `include/xrt/logger.h` | 80 | 55 | 25 |
| xrt | `include/xrt/map.h` | 62 | 0 | 62 |
| xrt | `include/xrt/math.h` | 20 | 19 | 1 |
| xrt | `include/xrt/memory.h` | 13 | 5 | 8 |
| xrt | `include/xrt/memory_debug.h` | 11 | 0 | 11 |
| xrt | `include/xrt/memory_stats.h` | 4 | 0 | 4 |
| xrt | `include/xrt/net.h` | 191 | 39 | 152 |
| xrt | `include/xrt/net_file.h` | 4 | 0 | 4 |
| xrt | `include/xrt/net_frame.h` | 9 | 0 | 9 |
| xrt | `include/xrt/net_interface.h` | 12 | 0 | 12 |
| xrt | `include/xrt/number.h` | 18 | 14 | 4 |
| xrt | `include/xrt/path.h` | 32 | 29 | 3 |
| xrt | `include/xrt/pattern.h` | 47 | 34 | 13 |
| xrt | `include/xrt/pem.h` | 7 | 3 | 4 |
| xrt | `include/xrt/pool.h` | 58 | 0 | 58 |
| xrt | `include/xrt/process.h` | 37 | 27 | 10 |
| xrt | `include/xrt/proxy.h` | 23 | 0 | 23 |
| xrt | `include/xrt/queue.h` | 46 | 0 | 46 |
| xrt | `include/xrt/random.h` | 39 | 25 | 14 |
| xrt | `include/xrt/regex.h` | 54 | 38 | 16 |
| xrt | `include/xrt/set.h` | 34 | 0 | 34 |
| xrt | `include/xrt/signal.h` | 20 | 16 | 4 |
| xrt | `include/xrt/slot_map.h` | 16 | 0 | 16 |
| xrt | `include/xrt/spin.h` | 7 | 0 | 7 |
| xrt | `include/xrt/stack.h` | 74 | 0 | 74 |
| xrt | `include/xrt/string.h` | 85 | 53 | 32 |
| xrt | `include/xrt/sync.h` | 48 | 34 | 14 |
| xrt | `include/xrt/task.h` | 43 | 23 | 20 |
| xrt | `include/xrt/task_net.h` | 6 | 0 | 6 |
| xrt | `include/xrt/tcp.h` | 63 | 39 | 24 |
| xrt | `include/xrt/tcp_server.h` | 15 | 0 | 15 |
| xrt | `include/xrt/temp.h` | 17 | 1 | 16 |
| xrt | `include/xrt/template.h` | 32 | 4 | 28 |
| xrt | `include/xrt/thread.h` | 23 | 12 | 11 |
| xrt | `include/xrt/time.h` | 58 | 46 | 12 |
| xrt | `include/xrt/tls.h` | 140 | 4 | 136 |
| xrt | `include/xrt/tls_client.h` | 10 | 7 | 3 |
| xrt | `include/xrt/tls_identity.h` | 13 | 5 | 8 |
| xrt | `include/xrt/tls_resume.h` | 7 | 2 | 5 |
| xrt | `include/xrt/tls_server.h` | 9 | 6 | 3 |
| xrt | `include/xrt/tls_session.h` | 29 | 15 | 14 |
| xrt | `include/xrt/tls_stream.h` | 53 | 27 | 26 |
| xrt | `include/xrt/tls_verify.h` | 8 | 4 | 4 |
| xrt | `include/xrt/udp.h` | 73 | 25 | 48 |
| xrt | `include/xrt/value.h` | 143 | 120 | 23 |
| xrt | `include/xrt/vfs.h` | 31 | 23 | 8 |
| xrt | `include/xrt/wait.h` | 3 | 3 | 0 |
| xrt | `include/xrt/websocket.h` | 57 | 17 | 40 |
| xrt | `include/xrt/websocket_stream.h` | 38 | 25 | 13 |
| xrt | `include/xrt/websocket_upgrade.h` | 9 | 6 | 3 |
| xrt | `include/xrt/x509.h` | 89 | 6 | 83 |
| xrt | `include/xrt/xid.h` | 11 | 8 | 3 |
| xrt | `include/xrt/xlon.h` | 36 | 34 | 2 |
| xrt | `include/xrt/xlonl.h` | 12 | 11 | 1 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_call.h` | 25 | 25 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_convert.h` | 6 | 6 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_field.h` | 40 | 40 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_object.h` | 20 | 20 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_object_graph.h` | 9 | 0 | 9 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_type.h` | 60 | 60 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_type_future.h` | 1 | 1 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_type_string.h` | 2 | 2 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/runtime_value.h` | 28 | 28 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_array.h` | 34 | 34 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_dict.h` | 30 | 30 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_list.h` | 30 | 30 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_queue.h` | 54 | 54 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_set.h` | 33 | 33 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_stack.h` | 21 | 21 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_tree.h` | 33 | 33 | 0 |
| xruntime | `extlibs/xruntime/include/xrt/typed_value.h` | 32 | 32 | 0 |

## xruntime 语义能力契约

直接出现 C 函数名只表示实现证据，不再等同于语言抽象。每个 xruntime 模块必须归入语言类型、内核语义或显式底层能力，并绑定可执行测试或边界文档。

| 公共头 | 语言能力 | 归属 | 后端状态 | API | 直接调用 | 见证 |
| --- | --- | --- | --- | ---: | ---: | --- |
| `extlibs/xruntime/include/xrt/runtime_call.h` | function values, call frames, and dynamic invocation | kernel | direct | 25 | 16 | `test/cases/cgen_function_named_value.xl` |
| `extlibs/xruntime/include/xrt/runtime_convert.h` | checked runtime conversion for is/as and var | kernel | direct | 6 | 1 | `test/cases/cgen_runtime_is_as.xl` |
| `extlibs/xruntime/include/xrt/runtime_field.h` | DynamicFields and class dynamic-field storage | language-type | direct | 40 | 22 | `test/cases/xruntime_dynamic_fields.xl` |
| `extlibs/xruntime/include/xrt/runtime_object.h` | class instance identity, payload, and weak ownership | kernel | direct | 20 | 11 | `test/cases/weak_class_lifecycle.xl`, `test/cases/weak_async_lifecycle.xl`, `test/cases/async_class_receiver_lifetime.xl` |
| `extlibs/xruntime/include/xrt/runtime_object_graph.h` | custom runtime object graph traversal | explicit-low-level | public-extern | 9 | 0 | `docs/xruntime语言桥接契约.md` |
| `extlibs/xruntime/include/xrt/runtime_type.h` | type identity, descriptors, registry, and protocols | kernel | direct | 60 | 27 | `test/cases/cgen_runtime_type_baseline.xl` |
| `extlibs/xruntime/include/xrt/runtime_type_future.h` | future<T> runtime type identity | kernel | direct | 1 | 1 | `test/cases/cgen_future_task_wait.xl` |
| `extlibs/xruntime/include/xrt/runtime_type_string.h` | stable language type names | kernel | direct | 2 | 2 | `test/cases/cgen_runtime_type_baseline.xl` |
| `extlibs/xruntime/include/xrt/runtime_value.h` | var lifecycle, boxing, comparison, and cloning | kernel | direct | 28 | 14 | `test/cases/runtime_var_type_storage_matrix.xl` |
| `extlibs/xruntime/include/xrt/typed_array.h` | array<T> | language-type | direct | 34 | 28 | `test/cases/cgen_typed_container_functions.xl` |
| `extlibs/xruntime/include/xrt/typed_dict.h` | dict<T> | language-type | direct | 30 | 14 | `test/cases/cgen_typed_container_functions.xl` |
| `extlibs/xruntime/include/xrt/typed_list.h` | list<T> | language-type | direct | 30 | 11 | `test/cases/cgen_typed_container_functions.xl` |
| `extlibs/xruntime/include/xrt/typed_queue.h` | typed SPSC/MPSC/MPMC queues | language-type | direct | 54 | 0 | `test/cases/stdlib_special_collections.xl` |
| `extlibs/xruntime/include/xrt/typed_set.h` | set<T> | language-type | direct | 33 | 20 | `test/cases/cgen_typed_container_functions.xl` |
| `extlibs/xruntime/include/xrt/typed_stack.h` | Stack<T> | language-type | direct | 21 | 12 | `test/cases/stdlib_special_collections.xl` |
| `extlibs/xruntime/include/xrt/typed_tree.h` | AvlTree<K,V> | language-type | direct | 33 | 19 | `test/cases/stdlib_special_collections.xl` |
| `extlibs/xruntime/include/xrt/typed_value.h` | typed values used by generic containers | kernel | direct | 32 | 27 | `test/cases/cgen_typed_container_functions.xl` |

## 扩展延期边界

延期清单保存在 `tools/xrt_xlang_deferred_apis.txt`。该清单是精确快照，不按前缀宽泛放行；xhttp、xws、xregex 分组当前均为空，出现新的旧 API 引用会直接失败。xregex 已正式接回，xhttp/xws 只延期非快速路径的高级语言包装，不允许旧符号回流核心快照。
