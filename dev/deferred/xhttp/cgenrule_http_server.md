# xhttp 延后迁移的服务端生成规则

本文件保存从 xlang 核心规则文档移出的高级 HTTP 服务端生成规则。对应实现位于
`dev/deferred/xhttp/src`，目前仅作为迁移 xhttp 扩展包时的设计资产，不属于核心编译器契约。

## CGEN-STD-015 http.server runtime 类型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.create(port)` |
| 生成规则描述 | http server wrapper 生成 XRT `xweb` 公共 typedef，并定义 `XL6_Http_Server` 与 `XL6_Http_Route`。server 只保存 `xwebserver*`、已注册 route 链表和 running 状态；每条 route 单独保存被 retain 的 `XL6_Function`。 |
| 延后实现 | `dev/deferred/xhttp/src/xl6_cgen_http_server_contract.c` |
| 对应生成的 C 代码 | `typedef struct XL6_Http_Server { xwebserver* server; XL6_Http_Route* routes; int running; } XL6_Http_Server;` |
| 中间形态伪代码 | `HttpServerHandle = {xwebserver, route_list, running}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要创建 xwebserver，并在销毁 server 时遍历释放所有 route handler。 |
| 额外依赖的封装代码 | HTTP server runtime typedef、XRT xweb 原型、function runtime。 |

## CGEN-STD-016 http handler function 生命周期

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.get(server, "/users/{id}", handler)` / `http.server.post(...)` / `http.server.any(...)` |
| 生成规则描述 | 注册路由时为 handler 分配 `XL6_Http_Route`，把 function value 保存到 route 并 retain。若 `xrtWebServerRoute` 注册失败，立即 release handler 并释放 route；server destroy 时遍历 route 链表统一 release。 |
| 延后实现 | `dev/deferred/xhttp/src/xl6_cgen_http_server_contract.c` |
| 对应生成的 C 代码 | `route->handler = handler; xl6_http_server_retain_function(&route->handler); ... xl6_http_server_release_function(&route->handler);` |
| 中间形态伪代码 | `retain handler on route register; release handler on route register fail or server destroy` |
| 实现同等功能手写的 C 代码 | 手写 C 需要遵守 `XL6_Function` retain/release ABI。 |
| 额外依赖的封装代码 | function runtime / function-var runtime / route 链表释放 helper。 |

## CGEN-STD-017 http request 到 function value 调用桥

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | handler 接收 `method, path, query, body` 并返回响应文本 |
| 生成规则描述 | XRT `xweb` 回调中，生成器把 method/path/query/body 创建为 xvalue text，然后调用 `xl6_function_call_var(route->handler, 4, ...)`。返回值按 xvalue text 读取，作为 `text/plain; charset=utf-8` 响应体；handler 不存在时返回 500 空响应。 |
| 延后实现 | `dev/deferred/xhttp/src/xl6_cgen_http_server_contract.c` |
| 对应生成的 C 代码 | `result = xl6_function_call_var(route->handler, 4, method, path, query, body); xrtWebResponseText(resp, text, "text/plain; charset=utf-8");` |
| 中间形态伪代码 | `request -> xvalue args -> handler(args) -> text response` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己构造 xvalue 参数并调用 function value。 |
| 额外依赖的封装代码 | `xl6_http_server_text_value`、function-var runtime、xvalue runtime。 |

## CGEN-STD-018 http.server create/route/static 失败收束

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.create(port)` / `http.server.get(server, pattern, handler)` / `http.server.static(server, mount, root)` |
| 生成规则描述 | create 检查端口和 backlog，建立或借用 `xnetengine`，创建任务池、默认 router，并保存当前 `xhttpserverconfig`；TLS 版本额外加载 `xtlsidentity`。route/static 注册先写入可变 router。start 时冻结 router，构建 host mux，再调用 `xrtHttpServerMuxStart` 或 `xrtHttpServerMuxStartTls`。创建与启动分离，允许启动前完整配置路由、多主机和 TLS。 |
| 延后实现 | `dev/deferred/xhttp/src/xl6_cgen_http_server_contract.c`、`dev/deferred/xhttp/src/xl6_cgen_http_server_lifecycle.c` |
| 对应生成的 C 代码 | `server = xl6_xrt_http_server_create(...)` / `xl6_xrt_http_server_create_tls(...)`，配置完成后调用 `xl6_xrt_http_server_start(server)`。 |
| 中间形态伪代码 | `validate; create engine/pool/router; register routes; freeze router; build mux; start HTTP or TLS server` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护 engine/task pool、router/mux、TLS identity、route handler 和失败回滚的完整生命周期。 |
| 额外依赖的封装代码 | 当前 XRT HTTP server/router/mux、TLS identity、task pool 和 function retain/release helper。 |

## CGEN-STD-019 http.server start/port/stop/destroy

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.start(h)` / `http.server.port(h)` / `http.server.stop(h)` / `http.server.destroy(h)` |
| 生成规则描述 | start 检查 handle/server 并调用 `xrtWebServerStart`，成功设置 running。port 返回 xweb bound port，空句柄返回 0。stop 调用 `xrtWebServerStop` 并清 running。destroy 调用 `xrtWebServerDestroy`，然后遍历 route 链表 release handler，最后释放 self。 |
| 延后实现 | `dev/deferred/xhttp/src/xl6_cgen_http_server_contract.c` |
| 对应生成的 C 代码 | `xrtWebServerStart(self->server); xrtWebServerStop(self->server); xrtWebServerDestroy(self->server);` |
| 中间形态伪代码 | `server lifecycle methods operate on opaque handle` |
| 实现同等功能手写的 C 代码 | 手写 C 需要按 xweb server、route handler、server handle 顺序释放。 |
| 额外依赖的封装代码 | HTTP server runtime 类型和 route 链表释放 helper。 |
