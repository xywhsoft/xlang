# WebSocket 标准库实施与验收

范围：已批准的 16 个 XRT WebSocket 模块。协议算法只复用 XRT，语言公开面
采用 OOP 和封闭驻留 ABI；不在 Cgen 中生成协议、操作码分派或桥接实现。

## 开发顺序与验收状态

1. 原生拥有权、活动调用、checked 退役、HTTP 初始缓冲交接：已实现；Windows GCC/TCC 与 Linux GCC 普通拥有权用例通过。
2. 帧、Close、握手、消息状态、扩展和压缩协商：已实现，双端产品验收通过。
3. 压缩／解压、不可变 NetBytes 保留发送、成功后消耗 Buffer：已实现；统一默认输出预算为 64 MiB。
4. TCP／TLS 事件流、升级、取消／期限及 Future：已实现；含循环清理、错误保留和取消投递故障回归。
5. 移除旧 Cgen 与 HTTP 声明岛、迁移标准库消费者：已完成，6 个旧文件共 2,358 行移除。
6. Windows／Linux 普通产品、JIT／模块／AOT、生命周期／OOM、裁剪和
   同版本等价 C 成本比较：已完成本组专项验收。

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
  Keygen 使用 XRT 的握手辅助依赖，确定性握手不拉入随机源。
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

## 第一批开发验证（历史记录）

22 个协议／配置／变换／受管流类、276 个精确驻留声明，来自单一元数据表
`tools/gen_stdlib_websocket.py`。7 个适配翻译单元通过 GCC C11
`-Wall -Wextra -Werror` 语法检查；开发用源码驱动的 values、extensions、
codecs 三例通过，覆盖 NUL 原因与负载、帧头 More／掩码、子协议、参数
有值／无值、压缩协商、上下文复用和直通消息。MessageState.activeSize
表示活动消息大小，消息完成后按 XRT 合同归零，不缓存第二份累计状态。

默认配置使用明确的无参／带配置重载，避免把可空原生 class 参数伪装成
point 或动态值；当前通用直接 C ABI 尚无该参数合同。默认构造的冷路径
适配调用也计入成本，不据此声称零桥接开销。原生流、扩展消费者和普通
发布产物当时尚未完成；最终结论以文末产品验收为准。

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
发送、暂停／恢复和接收配置要求所属 Worker，错误线程调用会 checked 拒绝；
查询及 abort 可从其他线程调用。可写、排空和关闭等待是
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
输出、可写／排空／关闭、取消、实际计时器及模块清理。该阶段尚未覆盖的
普通发布产物、逻辑 OOM、裁剪和成本检查已在最终验收中补齐。

## 公开面与使用规则

完整声明见 [API 表](websocket-language-api.json)，16 模块的对应关系见
[模块角色表](websocket-module-coverage.json)。角色表是功能归属，不冒充运行覆盖；
运行结果由 `tools/check_stdlib_websocket.py` 的收据单独记录。

| 领域 | 公开类／入口 |
| --- | --- |
| 帧与 Close | WsFrameConfig、WsFrame、WsClose |
| 握手与随机 key | WsHandshake；无共同子协议返回 null |
| 消息状态 | WsMessageConfig、WsMessageState、WsMessageInfo |
| 扩展与 PMD 协商 | WsExtensions、WsExtension、WsDeflateOffer、WsDeflateResponse、WsDeflateDirection |
| 消息编解码 | WsInflaterConfig／WsInflater、WsDeflaterConfig／WsDeflater |
| HTTP 升级 | WsClientOptions、WsServerOptions、WsUpgrade；TCP/TLS handoff.webSocketAsync |
| 受管连接 | WsStreamOptions、WsStream、WsEvent；NetStatus、Future、取消和期限 |

Inflater／Deflater 的无参构造与显式默认 Config 都使用 64 MiB 输出预算。
`reset()` 恢复语言默认配置；保留自定义窗口、方向及预算时用 `reset(config)`。
析构只释放资源，`endInto`／Close 是显式完成协议的操作。

事件先按 `kind` 分支。messageData 的 `dataSize` 是本次交付的清文字节数，
`offset` 是当前消息已交付的清文字节偏移；`payloadSize` 来自消息开始时的
帧元数据，不是分片或压缩消息的最终清文总长度。非相应事件的字段为零／空。
Buffer 模式的 `pending` 是本块写入量，背压／可写事件的 `pending` 按原生队列
含义解释。需要最终总长度时使用 End 事件偏移或接收 Buffer 的长度。

正常范围比较的成本需分层：原生帧适配保存一份定长头状态；普通语言对象
还需要 native handle、对象装箱和 checked 边界；拥有事件复制负载并保留发送方；
Async 方法创建语言闭包、拥有任务与 Future，并取得 Engine 语言对象。
这些成本不等同于算法或 native adapter 成本，不能将原生微基准解释为语言端零开销。
高吞吐收包使用已有 Buffer；Worker 回调／批量任务内可直接调用同步发送入口。

示例见 [WebSocket 示例](../release/examples/websocket/README.md)。

旧 `release/extlibs/xws` 的 Writer／Group 高层包装不属于这 16 个 XRT 模块。
它仍依赖已移除的 TCP/TLS 裸句柄入口，须单独迁移；本轮标准库不依赖该包，
也不将其已有并行修改纳入默认库验收。

## 最终产品验收：2026-10-29

实现源码 `843f43a0`，验收执行器 `6e986356`；独立冻结输入后从源码构建
TCC、XRT、CLI 和两份 DLL／SO。未使用开发驱动、替换提供方或工作区旧产品。
完整来源、哈希、阶段与成本见 [机器收据](websocket-acceptance-2026-10-29.json)。

| 平台 | 正例程序 | 验收阶段（含负例） | 预期拒绝 | 严格 GCC／堆账本运行 |
| --- | ---: | ---: | ---: | ---: |
| Windows x64 | 23 | 207（含预期拒绝） | 5 | 24 |
| Linux glibc x64 | 23 | 204（含预期拒绝） | 5 | 24 |

所有正例分别通过 JIT、真实模块、C 输出、AOT 构建、导入检查和移位后的
独立可执行文件运行。AOT 不依赖 xl／xlc DLL/SO；Linux 不带 RPATH/RUNPATH。
12 个程序分别用 GCC C11 `-O0`、`-O2` 和严格警告编译，再执行完整堆账本检查。
每个平台、每种优化各验证 361 次语言逻辑分配失败，均为 checked 失败且分配释放平衡。
两份产品各验证 22 类工厂、46 个完整分配前缀，其中 24 次故障；没有无效释放、
重复释放或 UAF 记录。默认构造／reset 的 64 MiB＋1 边界在输出前拒绝，堆平衡。

流测试包含正常 TCP、TLS＋PMD、HTTP 预读交接、拆开的首帧头、存储事件、
发送方访问、Buffer 输出、NetBytes 引用发送、Buffer 成功消费与失败保留、
Ping/Pong、暂停／恢复、真实期限／取消、回调抛错及仍打开连接的循环拥有图清理。
取消投递 OOM 用例关闭真实节点缓存以命中分配器；同一用例在修复前版本因
错误被吞掉而失败，修复后 Future 和 `throwIfFailed` 都报告 checked 错误。
两端的协议示例与本机回显示例也用新 CLI 在独立工作目录运行通过。

帧、Close、确定性握手和 keygen 的实际预处理宏已检查；不拉入网络、TLS、
Inflate/Deflate。keygen 保留 XRT 的握手辅助依赖。配置单独使用只选原生 Stream
配置所在模块，不拉入 Upgrade、TLS 或编解码；运行时协商的接管仍保留 PMD 能力。

原生帧微基准：100,000 次相同输入、相同拥有布局，同一产品 XRT 和对应 C 编译器。

| 平台／编译器 | 借用 C | 拥有 C | 拥有适配入口 |
| --- | ---: | ---: | ---: |
| Windows GCC -Os | 2,797 μs | 16,204 μs | 22,235 μs |
| Linux GCC -O2 | 2,318 μs | 16,636 μs | 18,525 μs |

这是 memory-debug=1、并行验收机器上的观测值。拥有入口每次分配一个定长帧记录，
不复制负载；默认适配入口还初始化配置并调用 getter，手写 C 可复用配置及直读字段。
普通语言装箱、事件、Async 调度等额外成本如上，不包含在这组 native 数字中。
另有 Windows TCC 调用互操作测试，不将它标成与 GCC 提供方同编译器的性能比较。

范围仅为批准的 16 模块及其普通自有回环／离线验收；没有公网流量或网络安全专项测试。
未验收 macOS，也不代表并行工作区和既有 release 目录的整体发布完成。
全局 `check_xlir_contract.ps1` 仍停在基线已有的旧任务静止文本断言，未计为通过；
本组退休文件、精确声明、枚举、无裸句柄／旧回调等检查由专用产品验收独立验证。
