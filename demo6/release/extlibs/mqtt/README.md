# mqtt

MQTT 3.1.1 QoS 0/1 的二进制报文编解码、TCP 逐帧传输和一次性握手。
C 与 xlang 使用同一份实现，内存、UTF-8、hex、错误及 TCP 操作复用 XRT。

它不是完整 MQTT 客户端或 broker。没有自动心跳、重连、重发、消息去重、
订阅路由、持久会话、Will、UNSUBSCRIBE、QoS 2 或 MQTT 5。
构造 QoS 1 报文不等于提供完整的 QoS 1 会话交付保证。
当前网络入口接受 TcpStream，不是 TLS/MQTT-over-WebSocket 入口。

## 快速开始

```xlang
import mqtt

func main(...) as int
    Buffer frame = mqtt.publishFrame(1, "home/temp", "22.5", 42)
    var message = mqtt.parseFrame(frame)
    println(message["topic"])
    println(message["payload"])
    println(mqtt.frameToHex(frame))
    return 0
end
```

二进制正文使用 Buffer 重载与 `payloadBytes`，不要将包含 NUL 或无效 UTF-8
的内容转换成 C 字符串。文本重载通过 C 字符串 ABI 传递，不保留嵌入的 NUL。

## xlang API

### 构造与转换

| 函数 | 返回及规则 |
| --- | --- |
| `connectFrame(string clientId, string username = "", string password = "", int keepalive = 60, bool cleanSession = true)` | 独立 Buffer；keepalive 单位为秒，0 保持为 0，不自动发送心跳 |
| `connectFrame(string clientId, string username, Buffer password, int keepalive = 60, bool cleanSession = true)` | 二进制密码重载；其他规则相同 |
| `publishFrame(int qos, string topic, string payload, int packetId = 0, bool retain = false, bool duplicate = false)` | 独立 Buffer；qos 为 0 或 1 |
| `publishFrame(int qos, string topic, Buffer payload, int packetId = 0, bool retain = false, bool duplicate = false)` | 二进制正文重载，不接管或修改输入 |
| `pubackFrame(int packetId)` | PUBACK Buffer，id 为 1..65535 |
| `subscribeFrame(int packetId, string topic, int qos = 0)` | 单个过滤器 SUBSCRIBE Buffer，id 为 1..65535；qos 为 0/1 |
| `pingreqFrame()` / `disconnectFrame()` | 控制帧 Buffer；不隐式操作 TCP |
| `hexToFrame(string hex)` | 严格 hex 解码为 Buffer；这是字节转换，不验证 MQTT 内容 |
| `frameToHex(Buffer frame)` | 小写 hex；空缓冲对应空字符串 |
| `frameSize(Buffer prefix)` | 固定头未收齐返回 0；收齐后返回第一帧总字节数，不要求正文已齐 |
| `parseFrame(Buffer frame)` | 返回 var 字典；输入必须恰好是一帧，拒绝截断、尾随数据和无效字段 |
| `payloadBytes(Buffer frame)` | 严格验证 PUBLISH 后，返回正文的独立 Buffer 副本 |

`connect`、`publish`、`puback`、`subscribe`、`pingreq`、`disconnect`
分别是同名 `*Frame` 的 hex 便捷形式，参数与文本重载一致，返回 string。
`parse(string hex)` 等价于 `parseFrame(hexToFrame(hex))`。
构造、解析、范围和 hex 错误抛 `ValueError`，不会返回不完整的假成功字典。

### 结构化解析结果

所有结果含 `type` 与 `remaining`，分别为控制报文类型编号和 Remaining Length。

| 报文 | 附加字段 |
| --- | --- |
| CONNECT (1) | `clientId`、`cleanSession` (bool)、`keepalive`、`username`、`password` |
| CONNACK (2) | `returnCode`、`sessionPresent` (整数 0/1) |
| PUBLISH (3) | `topic`、`payload`、`qos`、`retain` (bool)、`dup` (bool)；仅 QoS 1 含 `packetId` |
| PUBACK (4) | `packetId` |
| SUBSCRIBE (8) | `packetId`、`subscriptions` 数组；各项含 `topic` 和 `qos` |
| SUBACK (9) | `packetId`、`granted` 数组；允许协议返回码 0/1/2/128，解析码 2 不代表本库实现 QoS 2 |
| PINGREQ (12)、PINGRESP (13)、DISCONNECT (14) | 无附加字段 |

解析结果复制内容，不借用输入 Buffer。C 的长度型 xvalue 字符串能够保留二进制，
但 xlang 读取 `payload` / `password` 作为文本并不保证二进制无损；正文请用 `payloadBytes`。
解析不会跟踪已发送的 packetId，也不会自动验证 PUBACK/SUBACK 是否对应此前请求。

### TCP

| 函数 | 行为 |
| --- | --- |
| `sendFrame(TcpStream stream, Buffer frame, int timeoutMs = 5000, point cancel = null)` | 检查单帧固定头与总长度，分块发送并等待队列排空；不重复解析正文 |
| `recvFrame(TcpStream stream, int timeoutMs = 5000, int packetLimit = 1048576, point cancel = null)` | 独立 Buffer；仅取当前帧，保留 TCP 中的后续报文；用 parseFrame 检查正文语义 |
| `handshake(TcpStream stream, string clientId, string username = "", string password = "", int keepalive = 60, int timeoutMs = 5000, point cancel = null)` | 发送 Clean Session CONNECT 并等待成功 CONNACK；返回解析字典，不接管 stream |

- `timeoutMs=-1` 不限时，0 立即超时，正数是整个操作的总预算，不是每个片段重新计时。
  握手发送与接收共用一个截止时间。C API 接受 XRT 单调微秒绝对截止时间。
- `cancel` 为借用的 XRT `xcancel*`；null 不取消。它必须存活到调用返回。
  不接受任意指针或其他对象句柄。
- 调用方串行化同一连接的所有读取，以及所有写入；握手期间独占双向操作。
  阻塞入口不可在负责驱动该连接的 XRT worker 回调上调用。
- 空连接等待超时/取消不消费数据，可以重试。收到半帧后超时、取消、断连、
  分配失败或协议错误会终止连接，不能把残余字节解释成新包。
- 发送开始后失败会终止连接；部分数据可能已经抵达对端。成功只说明发送队列排空，
  不表示对端业务确认，不能据此自动重发非幂等操作。
- 握手的 CONNECT 已发送之后，若应答失败，即使尚未读取 ACK 也会终止连接。
- packetLimit 范围为 2..268435460 字节，默认 1MiB；包含固定头。在申请正文缓冲前检查。
  未开始 I/O 的参数失败不改变连接。网络入口抛 `IOError`，保留底层诊断。

## 协议范围

- 主题和文本字段验证 UTF-8、NUL 和 65535 字节上限；发布主题不能包含 `+` / `#`。
  过滤器 `+` 必须独占一个层级，`#` 必须独占末级。
- keepalive 为 0..65535；cleanSession=false 时 clientId 不可空。
  客户端标识的最终接受策略仍由 broker 决定。
- 构帧的空 username/password 表示不携带对应字段；非空密码要求非空 username。
  当前构造入口不能表达“存在但长度为零”的独立认证字段。
- QoS 0 的 packetId 必须为 0，且不能置 DUP；QoS 1 的 packetId 为 1..65535。
- Remaining Length 最多四个字节。增量检查允许同一个值的非最短编码，
  构帧只生成最短编码；严格解析仍要求输入总长度与报文长度完全一致。

字段依据 [OASIS MQTT 3.1.1 Errata 01](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)。

## C API 与拥有权

公开头：`include/xrt_mqtt.h`、`include/xrt_mqtt_net.h`。
编解码只需 `mqtt_frame.c`、`mqtt_build.c`、`mqtt_parse.c`；网络再加 `mqtt_net.c`。
链接一份 XRT 即可独立使用，不需要 xlang 编译器或 `mqtt_native.c`。

| C 入口 | 合同 |
| --- | --- |
| `xrtMqttConnect`、`xrtMqttPublish`、`xrtMqttPuback`、`xrtMqttSubscribe`、`xrtMqttPingreq`、`xrtMqttDisconnect` | 返回拥有型 xbuffer*；失败 NULL；用 xrtBufferDestroy 释放 |
| `xrtMqttInspect` | 返回 ERROR/MORE/READY；MORE 不设置新错误，固定头完整时填充 PacketSize |
| `xrtMqttPublishView` | bool；Topic/Payload 借用输入，不复制；原帧销毁或修改后视图失效 |
| `xrtMqttParse` | 拥有型 xvalue*；失败 NULL；用 xrtValueRelease 释放 |
| `xrtMqttSendFrame` | bool；借用 stream、frame、cancel；失败 false |
| `xrtMqttRecvFrame` | 拥有型 xbuffer*；失败 NULL |
| `xrtMqttHandshake` | 拥有型 xvalue*；二进制密码为 xbytesview；失败 NULL |

具体参数及类型以公开头为准。失败详情读取当前线程 `xrtGetError()`；成功不会清空
之前的错误槽，必须根据本次返回值判断失败。`include/mqtt_native.h` 的 16 个 `xmq_*`
函数是语言 ABI 适配入口，门禁将编译器生成的全部声明与此头一起交给 GCC 校验。

## 范例与验证

在发行目录中直接运行，无需网络账户或外部 broker：

```text
xl run examples/extlibs/mqtt.xl
xl run examples/extlibs/mqtt_c.c
xl run examples/extlibs/mqtt_tcp.xl
xl run examples/extlibs/mqtt_tcp_c.c
```

前两份 15 行输出一致；后两份本机真实 TCP 交换的 4 行输出一致。
默认均无外部服务连接。它们也可以使用 `xl build` 编译为独立程序。

在源码 `demo6` 目录运行：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_mqtt.ps1
bash tools/check_extlib_mqtt.sh
```

Windows 门禁：严格 GCC、C/xlang JIT/AOT、16 个生成原型、两组范例与黄金输出，
协议负例与逐字节分包、粘包、发送背压、异常 ACK、超时/取消、分配失败。
Linux 门禁：独立 C 的 ASan/UBSan 与两份 C 范例，不代表 Linux xlang JIT/AOT 已验证。
报告分别在 `build/test/mqtt_guard` 与 `build/test/mqtt_linux_guard`，包含源码哈希。
OOM 注入在独立 C 进程执行，不修改 xl.dll 宿主分配策略。

截至 2026-09-26 的本地验证不覆盖真实 broker 矩阵、长时会话、TLS 集成或公网故障环境；
不能由这些测试推导未实现的完整客户端能力。
