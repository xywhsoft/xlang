# xhttp.protocol

`xhttp.protocol` 是可独立裁剪的 HTTP 协议对象层，不依赖 `xhttp` 的 URL/Query
基础包，也不引入客户端、服务器或网络运行时。

当前包含：

- `Headers`：拥有型、可重复、大小写不敏感查询的 Header 容器；迭代器持有快照。
- `Cookies`：拥有 Cookie 请求字段原文和连续 `xcookiepair` 视图。
- `SetCookie`：按用户代理接收算法解析，保留原文、已知属性和未知扩展属性。
- `MediaType`：严格 Content-Type 解析、参数、结构化后缀和可压缩性判断。
- `Mime`：按扩展名或路径查询内置 MIME 类型。

每个拥有型对象都公开 `nativeHandle()`；原始 Header/Cookie 文本也可以直接读取或
写出。语言对象只负责所有权和常用操作，不替代 XRT 的无分配 C API。

## 同步

```text
python tools\sync_xhttp_protocol_package.py --sync --xrt-root <xrt-root>
python tools\sync_xhttp_protocol_package.py --xrt-root <xrt-root>
```

## 测试

```text
release\xl.exe run release\extlibs\xhttp.protocol\test\xhttp_protocol_headers_test.xl
release\xl.exe run release\extlibs\xhttp.protocol\test\xhttp_protocol_cookie_test.xl
release\xl.exe run release\extlibs\xhttp.protocol\test\xhttp_protocol_mime_test.xl
pwsh -NoProfile -File tools\run_extlib_c.ps1 -Package xhttp.protocol -Source release\extlibs\xhttp.protocol\test\buffer_transfer.c
```

文本结果优先转交原生 Buffer 的分配，含内嵌 NUL 的字段仍按长度复制，
不将二进制字段截成 C 字符串。
