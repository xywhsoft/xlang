# xhttp.client

`xhttp.client` 提供 HTTP 客户端的请求对象与后续传输层。当前第一阶段已经完成
拥有型 `Request`：方法、URL、Header、Trailer、正文、查询参数、urlencoded
表单和 `multipart/form-data` 使用同一套失败原子契约。

`Request.headers()` 与 `trailers()` 返回独立快照；`body()` 返回新的不可变正文
引用。设置 `Body` 或 FormData 时不强制把流式正文拼接到连续缓冲，保留 XRT 的
高性能组合路径。

```xlang
import xhttp.client
from xhttp.client import Request

Request request = Request.make("POST", "https://example.test/items")
request.setHeader("Accept", "application/json")
request.setBody("{\"name\":\"xrt\"}", "application/json")
```
