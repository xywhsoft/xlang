# WebSocket 标准库实施与验收

范围：已批准的 16 个 XRT WebSocket 模块。协议算法只复用 XRT，语言公开面
采用 OOP 和封闭驻留 ABI；不在 Cgen 中生成协议、操作码分派或桥接实现。

## 开发顺序与验收状态

1. 原生拥有权、活动调用、checked 退役、HTTP 初始缓冲交接：已实现；Windows GCC/TCC 与 Linux GCC 普通拥有权用例通过。
2. 帧、Close、握手、消息状态、扩展和压缩协商：驻留实现已完成开发用例，待整体验收。
3. 压缩／解压、不可变 NetBytes 保留发送、成功后消耗 Buffer：已实现，待产品验收。
4. TCP／TLS 事件流、升级、取消／期限及 Future：Windows 开发驱动回环通过，待产品验收。
5. 移除旧 Cgen 与 HTTP 声明岛、迁移标准库消费者：已实现，待产品验收。
6. Windows／Linux 普通产品、JIT／模块／AOT、生命周期／OOM、裁剪和
   同版本等价 C 成本比较：待验收。

上述状态描述实现，不将角色清单或符号登记等同于执行覆盖或发布验收。

## 必须保持的合同

- string／bytes 使用精确字节长度。适用输入有 size=-1，全长或截断到实际
  输入边界；其他负值拒绝。文本前缀仍须满足 UTF-8，协议限额不能截断绕过。
- More、背压、关闭、超时和取消与 checked 失败分别表示。
- 原生回调短期视图不逃出同步调用；普通语言回调数据拥有存储，可保存。
  写入既有 Buffer 的出口复用容量，只有实际可保留的块才能使用引用路径。
- 流、传输、回调闭包及发送数据完整进入拥有图；不伪装为叶对象。
- Upgrade 接管连接、HTTP 已预读余量及 EOF。提交前失败保留来源；成功后禁止
  原 HTTP 读取器与 WebSocket 竞争读取，重复接管拒绝。
- 发送受理、队列排空和对端应用收到不混同。转移仅在成功受理后消耗来源。
- 显式关闭保持已受理发送；自动释放未关闭连接则中止。析构不吞掉收尾错误。
- permessage-deflate 保留窗口、方向、上下文接管和输出限额，不能用普通
  gzip 包装替代。新接口不升级固定快照的微秒／Until 时间 ABI。
- 帧、Close、确定性握手与密钥生成分别裁剪，后端沿用 NetEngine。
  接管接受运行时协商的 Upgrade，因此 TCP/TLS 接管包含原生 PMD 编解码；
  不将只发送明文的数据路径等同于不链接压缩代码。
- 测试仅使用普通离线向量、自有本机回环、生命周期与逻辑分配失败。
  不进行公网探测或网络安全专项测试。

## 基线缺口

使用 HTTP 已验收的隔离候选复核：旧 core 可运行，TCP／TLS stream
夹具仍依赖退休的 peek／consume／nativeHandle；Close 原因 a\0b 返回时
丢失 NUL 后缀；WsUpgrade 选择旧 HTTP 声明岛，与 HttpHeaders 冲突。
本轮须同时消除这些消费者与桥接缺口。

协议依据：[RFC 6455](https://www.rfc-editor.org/rfc/rfc6455.html)、
[RFC 7692](https://www.rfc-editor.org/rfc/rfc7692.html)。

## 第一批开发验证

22 个协议／配置／变换／受管流类、276 个精确驻留声明，来自单一元数据表
`tools/gen_stdlib_websocket.py`。7 个适配翻译单元通过 GCC C11
`-Wall -Wextra -Werror` 语法检查；开发用源码驱动的 values、extensions、
codecs 三例通过，覆盖 NUL 原因与负载、帧头 More／掩码、子协议、参数
有值／无值、压缩协商、上下文复用和直通消息。MessageState.activeSize
表示活动消息大小，消息完成后按 XRT 合同归零，不缓存第二份累计状态。

默认配置使用明确的无参／带配置重载，避免把可空原生 class 参数伪装成
point 或动态值；当前通用直接 C ABI 尚无该参数合同。默认构造的冷路径
适配调用也计入成本，不据此声称零桥接开销。原生流、扩展消费者和普通
发布产物尚未完成，开发验证不能替代最终验收。

## 受管流开发合同与证据

上游原生提交：`7d27d412`（预读块链）、`25f57ebf`（拥有权与 TLS 独立事件
拥有者）、`4e74ef9e`（原生 NetBytes 发送图）、`930aa281`（精确发送预算）。
同步工具仅应用这些 WebSocket/TLS 差异，保持固定快照的微秒时间合同。

资源先完成语言侧登记、原生句柄装箱和语言对象构造，再通过返回 void 的
原生绑定提交 TCP/TLS 接管。异步入口连同任务、绑定方法和 Future 也提前
准备；不在接管完成后再分配返回对象。登记被拒绝或准备失败时，原连接
及 HTTP 余量仍属于调用方。物理接管完成后取消交付不能还原已消费的字节，
此时由新资源的拥有权规则中止并清理。成功后原 TcpStream/TlsStream
原生槽位被消费；HTTP handoff 再以原子状态拒绝重复使用。TLS 保留原会话
配置拥有者，并独立持有 WebSocket 事件拥有者。

`attachAsync`、异步发送及 Buffer 配置复用 NetEngine 的拥有型任务和实际
连接 Worker。Worker 内可直接使用 `attachOnWorker`、发送和流控制入口。
未在所属 Worker 的操作产生 checked 状态错误。可写、排空和关闭等待是
原生 Promise，取消只投递合并的就绪任务，期限使用一个最早到期计时器。
没有每连接线程、每等待协程栈或定时轮询。每流最多 1024 个未完成订阅；
已取消的 Future 在下一次注册、流事件或终结时清除，不会无限累计。

事件拥有其字节、Close 原因及 `stream`，可跨回调保存；发送方引用和闭包
循环进入真实拥有图。`receiveIntoAsync(Buffer)` 使负载直接追加到已有
Buffer，默认不为每块分配 data 事件；显式传入 `true` 可接收不复制负载的
data 事件，`pending`／`dataSize` 表示本次追加字节数。Buffer 绑定期间须由
调用方保证外部访问同步。
不可变 NetBytes 在明文服务端保留原生负载引用；客户端掩码、TLS 记录和
压缩仍执行协议要求的复制或变换。可变 Buffer 的 sendTake 在复制发送
成功后释放来源，失败保持来源；不把它计为零复制。

开发驱动用例：values、extensions、codecs、upgrade、stream、handoff、tls、
cycle、callback_error；
额外复测 HTTP/1 core 与 TCP transport。覆盖 NUL、严格长度、协商回调、
TLS＋permessage-deflate、失败交接重试、预读数据、暂停／恢复、Buffer
输出、可写／排空／关闭、取消、实际计时器及模块清理。普通发布产物、
逻辑 OOM、裁剪和成本验收仍在后续阶段。
