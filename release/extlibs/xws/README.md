# xws

`xws` 是 xlang 的 WebSocket 扩展对象层。XRT 与 xlang 内建 `WebSocket`
继续保留无额外抽象的单连接快速路径；本包在同一套 XRT 实现上增加长期连接、
分片 Writer、任意线程 Future、连接组和批量广播能力。

## 能力边界

- `WsConnectionConfig`：角色、协议、消息与帧上限、背压预算、异步队列和
  `permessage-deflate` 协商结果。
- `WsConnection`：TCP/TLS 接管、直接回调、同步和异步发送、move 路径、关闭与
  结构化错误。
- `WsWriter`：Text/Binary 分片消息、压缩分片和 Buffer 所有权移交。
- `WsGroup`：线程安全成员管理、稳定快照、广播、批量等待和逐成员结果。
- `nativeHandle()`：所有关键对象保留底层句柄入口，供需要完整 XRT 能力的代码使用。

HTTP Upgrade 仍由 XRT 核心的 `WsUpgrade` 完成。`WsConnectionConfig.fromUpgrade()`
复制已校验的角色、子协议和压缩协商结果，然后由 `attachTcp()` 或 `attachTls()`
接管传输。包不重复实现 HTTP 客户端、服务器或路由器；这些高级流程由后续
`xhttp`/`xws` 组合层提供。

## 回调契约

连接回调签名为：

```xlang
func(int event, int opcode, int flags, int value, point data, int size)
```

消息数据 `data` 只在当前回调期间有效，处理路径不经过复制队列。需要跨回调保留时，
调用方应立即复制到 `Buffer`。语言回调使用 xlang `int`，C/XRT 快速路径仍保留
`uint64_t` 和 `size_t` 的完整宽度。回调抛出的 xlang 异常会转换为带原始 cause 的
XRT 结构化错误、清理 Worker 线程的语言异常状态并中止当前连接。

## 测试

```text
release\xl.exe run release\extlibs\xws\test\xws_test.xl
release\xl.exe run release\extlibs\xws\test\xws_loopback_test.xl
release\xl.exe run release\extlibs\xws\test\xws_tls_loopback_test.xl
```

测试覆盖配置与边界、TCP/WSS Upgrade、同包首帧、`permessage-deflate`、闭包回调、
分片 Writer、Buffer 所有权移交、同步和异步发送、Future、活连接 Group 广播、
Ping/Pong、关闭握手以及回调异常跨线程传播。三组用例同时支持 `xl run` 动态执行和
`xl build` 静态产物。
