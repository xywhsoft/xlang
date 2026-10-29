# httpreq — HTTP 请求测试器（curl 简化版）

用 xlang 编写的命令行 HTTP 客户端，验证 `http` 标准 API（`HttpClientRequest` / `HttpClientResponse`）。

## 用法

```
httpreq <method> <url> [body] [options]
```

- `method`：`get` / `post` / `put` / `delete` / `patch` / `head` / `options`（任意大小写）
- `options`：
  - `-h <Name:Value>`  自定义请求头，可重复（`-H` 同义）
  - `-t <ms>`          超时毫秒，默认 `10000`
  - `--ct <type>`      body 的 Content-Type，默认 `text/plain; charset=utf-8`

## 示例

```
httpreq get http://httpbin.org/get
httpreq post http://httpbin.org/post "{\"hello\":\"world\"}"
httpreq get http://httpbin.org/get -h X-Custom:abc -h Token:secret
httpreq get http://example.com -t 5000
httpreq put http://httpbin.org/put "data" --ct application/octet-stream
```

## 输出格式

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 66
Date: Tue, 07 Jul 2026 17:35:27 GMT
----------------------------------------
{"url":"...","headers":{...}}
```

网络层失败（连接被拒绝 / DNS 失败 / 超时）时打印错误并返回退出码 1：

```
Error: request failed (no HTTP response received).
  url   : http://127.0.0.1:1/get
  method: GET
  timeout: 2000ms
  likely cause: connection refused, DNS failure, or timeout exceeded
```

## 设计要点

- **统一走 `HttpClientRequest` 对象化 API**（不用 `http.get`/`http.post` 便捷函数），因为后者不支持自定义请求头。
- **网络失败判据**：`req.execute()` 在网络层失败时返回 `handle==NULL` 的 `HttpClientResponse`，`status()` 返回 `0`（HTTP 合法状态码是 1xx-5xx，0 可靠表示"没拿到响应"）。所有访问器对 NULL handle 都做了空值兜底，无崩溃。
- **响应头无法枚举**：API 只支持 `resp.header(name)` 按名查询，不提供遍历所有响应头的能力。本工具固定查询 `Content-Type` / `Content-Length` / `Server` / `Date` 四个常见头，存在才输出。

`HttpClientRequest` / `HttpClientResponse` 是 native-backed class，按 class 的默认引用
参数语义传递；例如 `printResponseHeader(HttpClientResponse resp, string name)` 直接接收
响应对象，不复制底层 native handle。对象由运行时生命周期协议统一释放。

## 本地验证（无外网时）

`tinyserver.xl` 是用 xlang `http.server` 写的临时本地对端，监听 `127.0.0.1:18099`，提供 `/get`（返回 JSON）和 `/echo`（回显 POST body 与 `X-Test` 头）。验证步骤：

```bat
cd /d D:\GIT\x-lang\release
:: 启动本地对端（后台 60 秒）
start /b xl.exe run projects\dev\httpreq\tinyserver.xl

:: run 路径
xl.exe run projects\dev\httpreq\httpreq.xl -- get http://127.0.0.1:18099/get
xl.exe run projects\dev\httpreq\httpreq.xl -- post http://127.0.0.1:18099/echo payload -h X-Test:client

:: build 路径
xl.exe build projects\dev\httpreq\httpreq.xl -o projects\dev\httpreq\httpreq.exe
projects\dev\httpreq\httpreq.exe get http://127.0.0.1:18099/get

:: 错误路径（连接被拒绝）
xl.exe run projects\dev\httpreq\httpreq.xl -- get http://127.0.0.1:1/x -t 2000
```
