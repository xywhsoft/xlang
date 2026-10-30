# xhttp.multipart

`xhttp.multipart` 是 XRT multipart 协议底座的 xlang 对应包。它依赖
`xhttp.protocol`，同时覆盖完整正文解析、form-data 与原始 Part 写出、增量
Reader 和安全随机 boundary。

## 设计边界

- `MultipartBoundary` 拥有已解码 boundary，并公开 `nativeHandle()`。
- `MultipartParser.parse()` 复制正文后解析，适合默认安全路径。
- `MultipartParser.borrow()` 不复制正文；解析器和 Part 会保活来源 `Buffer`，
  但调用方在解析和 Part 使用期间不得修改该 Buffer。
- `MultipartWriter` 的 `fieldHead()`、`fileHead()`、`partEnd()`、`close()` 可与
  原始正文分段发送，上传大文件时不需要构建完整封包。
- `MultipartReader` 不持有固定接收缓冲。`read()` 返回的 `consumed` 表示可从
  当前输入头部移除的字节，`PART` 和 `DATA` 事件借用并保活本次输入 Buffer。
- `MultipartPart.data()`、`MultipartEvent.data()` 和所有 `nativeHandle()` 保留
  C/XRT 级底层能力；`body()`、`buffer()`、`headers()` 是拥有型便利路径。

## 完整正文

```xlang
MultipartBoundary boundary = MultipartBoundary.parse("form-boundary")
MultipartWriter writer = MultipartWriter(boundary)
Buffer body = Buffer()
body.append(writer.fieldText("name", "value"))
body.append(writer.close())

MultipartParser parser = MultipartParser.parse(body, boundary)
MultipartPart? part = parser.next()
```

## 分段发送

```xlang
send(writer.fileHead("file", "large.bin", "application/octet-stream"))
send(fileBuffer)
send(writer.partEnd())
send(writer.close())
```

这里的 `send` 由上层 TCP、HTTP 客户端或服务器选择，multipart 层不强迫正文
进入对象树，也不绑定网络实现。
