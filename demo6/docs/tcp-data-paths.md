# TCP 聚集发送、文件发送与分帧接收

本批补齐 P3 的 TCP 数据通路，复用 XRT 算法和现有受管理 Stream／Future。它不是整个 TCP／UDP／Server／TLS 组或长期任务完成声明。

## 公开接口

| 方法 | 合同 |
| --- | --- |
| `sendVector(array<string|bytes|Buffer> data, int size=-1)` | 三个确定重载；最多 64 段，按顺序原子受理；返回 `NetStatus` |
| `send(NetBuffer data, int size=-1)` | 不消费源缓冲；从原生片段直接聚集复制，不先拼接临时 bytes |
| `sendFile(File data, uint64 offset=0, int size=-1)` | 按文件区间发送；成功前 XRT 复制原生句柄，调用者随后可关闭 File；不改变文件游标 |
| `recvInto(NetBuffer|NetLineFramer|NetLengthFramer output, int maxBytes=65536, int timeoutMillis=-1)` | 三个确定重载；接收一个当前数据块并追加到目标，返回实际字节数；0 表示 EOF |
| `recvIntoAsync(NetBuffer|NetLineFramer|NetLengthFramer output, int maxBytes=65536)` | 三个确定重载；返回精确的 `future<int>`，不是动态值或 bytes |

共新增 11 个驻留公开入口。`sendVector` 的数组元素类型在前端／XLIR 共同声明表登记，Buffer 数组使用精确的原生字段、句柄策略和数组布局合同；没有为任意 typed 容器开放 C 指针通道。

`size` 使用既有统一字节规则：缺省／-1 使用全部；超长取实际长度；小于 -1 报 RangeError。聚集发送的 size 作用于拼接后的总前缀，可以落在片段或 UTF-8 编码中间，因为这里传输的是字节。

文件的 size=-1 表示 offset 后全部剩余内容；显式 size 超长时截断，offset 超出文件长度时报错。整个选定区间必须适合平台 size_t 和 Stream 写预算；背压返回 `NetStatus.again`，不是“已发送一部分”。无原生句柄的 File 不伪装为内核文件发送。文件元数据／句柄操作没有错误标记为 nonblocking。

## 接收与分帧

recvInto 直接把原生 NetBytes 视图复制到目标链，不经过临时语言 bytes／Buffer。maxBytes 必须为正；开始接收前检查目标状态与剩余上限，并限制本次最大读取量。目标已满时先报错，不为这次请求消费网络数据。

分帧器仍是原有增量解析器：recvInto 只追加输入，不假装一次 TCP 接收就是一帧。调用 `next()`／`nextInto()` 得到完整帧；多个帧和跨读取边界由同一 XRT parser 处理。NUL 是普通数据。

异步方法捕获实际 Stream 资源和目标原生句柄，允许目标语言包装离开作用域；不借用可卸载的字段名称或生成回调。原生完成只唤醒固定就绪队列；目标追加、结果装箱和拥有清理在队列执行器中完成。取消传给实际接收 source，保留捕获到原生真实终态，不为每次等待新建线程或协程栈。

目标是可变容器，调用者应序列化并发操作；这里不引入语言借用语法。接收不是事务：一旦原生已经交付数据，后续分配、追加或清理错误不能把字节放回 TCP。追加失败会把目标输入标为失败，分帧器须 clear/reset 或弃用；其他接收错误也不能视为“流未被消费”。

## 性能和裁剪边界

- 聚集数组和 NetBuffer 发送是复制入口：原生只做一次载荷复制，源可以立即修改／释放；不宣称零复制。
- 文件发送使用 XRT 的平台内核路径。Windows select 没有这项能力，提交前明确返回 Unsupported 且连接仍可用；Windows 使用 IOCP，Linux 使用已有内核路径。不偷偷改后端或把文件隐式读入内存。
- Worker-only 的原生链接管和带外部释放回调的引用发送，没有被包装成可从任意语言线程调用的“安全零复制”。完整引用／接管发送仍是 P3 后续项，不能用裸指针回调隐藏拥有边。
- 输入捕获在 TCP 和旧 NetSocket 向量接口之间共用；原始 Socket 的发送／状态职责没有拉入 TCP-only 程序。NetBuffer-only 异步接收不拉入 Line／Length parser 或文件发送。
- 所有新实现都在驻留 runtime；Cgen 无改动，继续只生成声明、调用和拥有清理。现有 File 封装本身的旧生成适配不在本批重写。

## 验收与证据

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-data/`。

| 范围 | Windows | Linux |
| --- | ---: | ---: |
| 新数据通路、旧向量语言用例、TCP 异步回归：生成 C／JIT／模块／独立 AOT／IR | 74 | 74 |
| 新驻留 ABI、实际目标生命周期、72 个接收构造故障前缀、原有 TCP 回调回归 | 19 | 22 |
| 旧向量／批处理八种独立闭包及实际输入捕获回归 | 100 | 116 |
| 固定核心快照的文件能力专项 | 8 | 10 |
| 文件桥接的追加 IR 合同验证 | 2 | 2 |
| 独立 XRT 修复的干净源码／生成头专项 | 8 | 10 |

合计 445 个正式验收阶段，问题发现／中止轮次不计入。两个平台、两个优化级别的主 IR 验证均检查 303 个 TCP 目标、拒绝 1330 次描述符变异，11 类违反由完整 verifier 在 C／头文件输出前阻止；文件补查另覆盖 298 目标／1305 次拒绝／10 类门禁。使用 GCC／TCC O0／O2，Linux 额外 ASan／UBSan；源码驱动分别含 763／762 个编译单元。

入口为 `tools/check_stdlib_tcp_data_source.py`、`tools/check_stdlib_tcp_data_runtime.py`、原有 `tools/check_stdlib_net_vector_runtime.py`，以及 XRT 的 `tools/check_net_tcp_file_capability.py`。追加文件合同使用同一已验证 compiler-contract 二进制加载文件用例和异步接收用例，真实命令和哈希保留在 `file-contract-*/results.json`。

XRT 修复提交为 `452b18f`。只把文件发送函数的能力前检同步到固定 xrt／xruntime 快照；该函数在三处规范化字节完全一致。微秒／绝对期限 ABI、3305／3770 公共 API 和 465 个扩展自有 API 均不变，没有导入原生仓库其他文件／内存工作区修改。

全部验收只使用离线 ABI／拥有权／分配失败和自建字面量回环正常通信；不执行外网、DNS、扫描、畸形协议或网络安全测试。当前源码验收不等于发布 DLL／SO／VFS 已重建。

## 后续范围及未决策

聚合 Server 和廉价拥有型轮询已分别验收，见 [Server](tcp-server-language.md)。
不可变 NetBytes 引用发送、连续 Buffer 接管见 [拥有型发送](tcp-owned-sends.md)。
原子批量引用、链式跨线程接管、拥有型接收复用、完整 UDP、网络文件／任务组合、
受管理非对称密钥、X.509、TLS 和发布联合验收仍须完成；没有缩减批准范围。

未决策仍为 CRT 0、XRT 11 组：core／error／ownership；内存；动态值与图；其余平台信息；HTTP；WebSocket；运行时类型／转换；对象／字段图；调用／value 桥接；typed 容器；内部支撑。
