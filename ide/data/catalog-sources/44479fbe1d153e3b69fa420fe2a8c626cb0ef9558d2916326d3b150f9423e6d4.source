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

方法、URL、目标、主机、原因短语和 Header 文本优先转交原生 Buffer 的分配；
含内嵌 NUL 的结果保留按长度转换，响应正文不走零结尾文本快路。

定向 C 测试：

```text
pwsh -NoProfile -File tools\run_extlib_c.ps1 -Package xhttp.client -Source release\extlibs\xhttp.client\test\buffer_transfer.c
```

当前 XLang `check` 仍先被依赖包 `xhttp.multipart` 的 `Buffer.appendBytes` 语义错误
阻断；通过上述 C 边界测试不等于客户端全链路验收。
