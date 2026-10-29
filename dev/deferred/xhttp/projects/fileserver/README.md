# fileserver - HTTP server 深度验证工具

启动一个多功能 HTTP server，自发自收跑 21 组 / 50 项断言，
覆盖 `http.server` 全部公开能力。两条路径（`xl run` 与 `xl build`）均通过。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\fileserver\fileserver.xl
xl build projects\dev\fileserver\fileserver.xl -o projects\dev\fileserver\fileserver.exe
projects\dev\fileserver\fileserver.exe            # self-test
projects\dev\fileserver\fileserver.exe serve 18221 # 常驻模式
```

## 验证范围（与 DEMO6功能清单 §14 对齐）

| # | 能力 | 路由 / 入口 |
|---|---|---|
| 1 | 基本路由 get/post/any + getRequest/postRequest | `/`, `/api/items`, `/any` |
| 2 | 路由模式 静态 / `{id}` / 尾部 `*` | `/api/items/{id}`, `/status/{code}`, `/wildcard/*` |
| 3 | 静态文件 ETag / 304 / Range / 206 | `/static/*` (static_root/index.txt) |
| 4 | HttpRequest method/path/query/header/cookie/body/bodyBytes | `/headers`, `/query`, `/cookieread`, `/echo-bytes` |
| 5 | HttpResponse status/header/cookie/text/jsonText/redirect/file | `/status/{code}`, `/cookieset`, `/redirect`, `/file` |
| 6 | app 体系 app()/setApp/reloadApp/host | `vhost.local` 虚拟主机 |
| 7 | 流式响应 start/write/finish | `/stream` |
| 8 | 错误处理器 error | `/error`, `/no-such-path` |
| 9 | MIME mimeByPath | 启动时打印 4 种扩展名 |
| 10 | 连接统计 requestCount/connectionCount | 启动/结束各打印一次 |

## 结果

```
passed: 50 / 50
failed: 0
post-test requestCount=22
```

## 设计要点

- **资源作用域清晰**：请求与响应包装对象只在拥有对应句柄的 handler 或测试块内使用。
  self-test 用 `if true then ... end` 为每个 case 建立独立生命周期，完成请求后立即释放资源。
- **结构化 JSON**：反射路径参数、请求头、查询字符串、Cookie 和错误信息时，先构造字典再
  `json.stringify`，不会因为外部文本包含引号而生成非法 JSON。
- **无 bare block scope**：xlang 只有 `do ... while cond end` 后置循环，没有独立的 `do ... end` 块；
  用 `if true then ... end` 替代。
- **self-test 端口 = 0**：让系统自动分配空闲端口，避免端口冲突；
  server 启动后用 `http.server.port(server)` 取实际端口构造 client URL。
- **静态文件目录**：`static_root/index.txt` + `index.html`，运行时相对于
  `release/` 工作目录解析（`xl run` / `xl build` 默认在 release 下执行）。

## 发现的小问题（非阻塞，不记入 BUGS.md）

### DOC-1: `HttpClientRequest.body` 参数类型文档不明确

- `DEMO6功能清单.md` 第 14 节写的是 `body(data, size, contentType)`，
  没有标注 `data` 的类型。
- 实际绑定（`xl6_stdlib_data.c:526`）：
  `extern "xl6_xrt_http_client_request_body" func body(string data, c_size size, string contentType) as bool`
- 即 `data` 是 `string` 而非 `point`。若想发原始二进制（含 NUL），目前
  无法通过 `HttpClientRequest.body` 完成（NUL 会截断 string）；要发原始
  字节流，需要走 `Buffer.data()` + 自行 TCP/HTTP 编码路径。
- 建议：在功能清单里补 `data` 类型为 `string`，并提示 NUL 截断限制。
