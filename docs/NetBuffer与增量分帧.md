# NetBuffer 与独立增量分帧

本批实现 XRT `net_buffer`、`net_frame`、`net_frame_line`、`net_frame_length`
的受管理语言入口，不包含 Socket／TCP 传输组合。旧的传输绑定 `LineFramer`／
`LengthFramer` 尚待 P3 一并替换；不能把本批视作完整网络接入完成。

## NetBuffer

`NetBuffer(maxBytes=67108864)` 是拥有分块的字节链，不替代现有连续 `Buffer`。
语言对象负责生命周期，底层复用 XRT，不开放裸视图、外部释放回调、借用块、
文件区段或池指针。容量上限必须为正，超过上限 checked 报错。

| 功能 | 接口 |
| --- | --- |
| 查询 | `length`、`spanCount`、`maxBytes` |
| 输入 | `append(input, size=-1)`、`prepend(input, size=-1)`；支持 string／bytes／Buffer |
| 移动 | `appendMove(NetBuffer)`；转移真实拥有块，源清空，不持有源对象 |
| 读取副本 | `peek(offset=0, size=-1)`、`read(size=-1)` 返回 bytes；read 成功后消费 |
| 复用输出 | `peekInto(Buffer, offset=0, size=-1)`、`readInto(Buffer, size=-1)` 返回追加字节数 |
| 管理 | `consume(size=-1)`、`clear()`、`compact(size=-1)` |
| 搜索 | `find(byte, offset=0)`；字节范围 0—255，未找到返回 -1 |

size 按字节计数，-1 为完整输入，非负数超出剩余长度时截断，其他负数拒绝。
offset 必须在 0 到 length 之间；字符串使用双字段 ABI，内嵌 NUL 不截断。
输出 Buffer 是追加而非替换；预留或复制失败不消费输入。

appendMove 为 O(1) 块链转移，compact 则可能分配、复制以合并前缀，不能称为
零拷贝操作。当前使用无池的拥有块：频繁单字节 append 不承诺摊销扩容性能；
逐字节构造可先使用连续 Buffer，再按块输入。原生池与传输缓冲复用留待后续，
没有擅自加入公开内存池设计或隐含跨对象池生命周期。

## 两种独立解码器

`NetLineFramer(delimiter="\n", maxPayload=8192, includeDelimiter=false,
maxBufferedBytes=67108864, size=-1)` 支持 string／bytes 分隔符。分隔符经 size
选择后不得为空，由解码器独立复制；maxPayload 可为零，不含分隔符。

`NetLengthFramer(lengthOffset=0, lengthSize=4, adjustment=0, strip=4,
maxFrame=1048576, littleEndian=false, maxBufferedBytes=67108864)` 支持 1—8 字节
长度域及大小端；计算沿用 XRT 的 `fieldEnd + declared + adjustment`，再剥离
strip 字节。maxFrame 限制整帧，不是只限制返回负载；declared 保留完整 uint64。
头部位置、主机尺寸、负调整量及完整帧尺寸由既有 XRT 检查。

两者均提供：

- `append(string|bytes|Buffer, size=-1)`、`appendMove(NetBuffer)`。
- `buffered`、`maxBufferedBytes`、`failed`。
- `next(consume=true)` 返回独立 `NetFrameResult`；`next(false)` 不消费。
- `nextInto(Buffer, consume=true)` 直接追加负载；MORE 返回 false，空负载的
  READY 仍返回 true。输出容量足够时，原生输出路径无分配，不创建结果对象。
- `reset()` 保留输入并重置搜索／失败状态；`clear()` 同时清空输入。

解码器独占输入链，只允许追加或移动输入，不暴露可破坏增量游标的任意前缀修改。
解析错误抛出 checked 异常并进入 failed；必须 reset／clear 后才允许继续输入／
解析。reset 不会移除引起错误的数据，重试相同输入仍可能失败。

## 结果与消费原子性

NetFrameStatus 只有 `more`、`ready`。结果不能自行构造，`ready`／`more` 为便利
属性；MORE 不能读取负载或帧元数据，否则 checked 报状态错误。

READY 提供 `payloadOffset`、`payloadSize`、`frameSize`、`declared`，以及
`bytes()`、严格 UTF-8 的 `text()`、追加输出的 `copyInto(Buffer)`。二进制用
bytes；无效 UTF-8 不能塞入 string，合法内嵌 NUL 则保留。结果拥有负载快照，
源缓冲被清空、继续消费或解码器销毁后，既有结果依然有效。

next 是普通语言组合：先取得不消费的原生快照，完成语言句柄／对象装箱，
再提交消费。提交携带不可复用的解码器编号和修订号；过期、跨解码器及已消费
快照拒绝。reset／clear 同样推进修订号，编号或修订号饱和 checked 拒绝，
不环绕。这样原生复制成功但语言装箱分配失败时，输入不会丢失；无需在 Cgen
加入分帧算法、特例补丁或返回后回调。

## 同步、拥有与裁剪

每个可变输入以一次原子 CAS 尝试进入，拒绝重叠／重入操作并 checked 报状态
错误，不在大型复制或分配期间自旋等待。需要共享并发访问时由调用者显式同步；
next 的整个语言组合也应包含在调用者同步范围内。现有连续 Buffer 参数同样
不能被其他线程并发修改。本批没有承诺无锁队列或多消费者解码语义。

四类对象均登记精确拥有域，没有指向调用者 Buffer／池／原生回调代码的隐藏边。
不可空 native resource 字段必须显式初始化，例如 `NetBuffer input = NetBuffer()`；
延迟初始化使用可空类型，未放宽既有 IR 原生资源默认构造规则。

58 个声明合同闭合错误、效果、阻塞、拥有方及精确标量／枚举表示。mutable 查询
可能因重叠访问报错，不误标 no-raise。17 个驻留组件按需选择，字符串／bytes、
连续 Buffer、两种解码器、文本结果分别裁剪；单用 NetBuffer 不拉入分帧，单用
一种解码器不拉入另一种，也不引入 Socket／Engine／Resolver／TLS。Cgen 无改动。

## 验收范围

证据位于 `F:/xlang-validation/five-groups-20261021/p2-frame`，检查脚本为
`tools/check_stdlib_net_frame_runtime.py`、`tools/check_stdlib_net_frame_source.py`。
覆盖实际组件闭包、拥有图、普通尺寸边界、分块／NUL／UTF-8、输入快照、失败
恢复、私有接口拒绝、声明合同拒绝、JIT／模块／严格 C／独立 AOT 和导入裁剪。

另有完整语言对象分配失败前缀，以及只在 next 导出调用期间注入失败的消费
原子性验证。对照测试故意恢复“先消费后装箱”的错误顺序，必须观察到指定的
输入丢失断言失败，以证明检查确实能检出该回归，而不是只检查退出码。

所有测试离线，不开 Socket，不进行 DNS、外部流量、探测、模糊测试或网络安全
性质测试。不替换 release／VFS；完整传输组合和 shipping 联合验收仍未完成。
