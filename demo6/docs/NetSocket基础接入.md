# NetSocket 基础接入

本批提供受管理的原始 Socket 生命周期与基础非阻塞 I/O，不是完整网络模块封板。
高级数据报、向量／批量 I/O、Resolver 和高层传输继续在原长期任务中开发。

## 公开合同

```xlang
auto listener = NetSocket(NetSocketKind.stream)
listener.bind(NetAddress.loopback()) // 默认端口 0，由系统分配。
listener.listen()
auto endpoint = listener.localAddress()
auto peer = NetSocket()             // 已分配、尚未持有 OS socket 的接收槽。
auto status = listener.acceptInto(peer)
// status == NetStatus.again 时 peer 保持原来的空状态。
```

`NetSocket(kind, family = 4, nonblocking = true)` 打开 stream／datagram。
family 与既有 NetAddress 一致，为 4／6。系统句柄不继承给子进程。
NetSocket 是对象，赋值产生共享对象别名，不复制 OS 句柄；close 对所有别名生效。
`NetSocket()` 是明确的空接收槽；已关闭的对象也能作为 acceptInto 的目标复用。
acceptInto 要求源与目标不同、目标为空且没有活动调用；成功才转移实际新 socket，
不在接受连接后再申请语言包装体。对端地址通过 peer.remoteAddress() 取得。

已提供 close、isClosed、family、kind、available、setOption／getOption、bind、
listen、connect／finishConnect、acceptInto、shutdown、localAddress／remoteAddress。
选项使用全部 17 项稳定的 NetSocketOption 枚举，不公开平台 SO_* 数字；具体平台不
支持的选项 checked 报错，不静默成功。方向使用 NetShutdown.read／write／both。

发送接受 string／bytes／Buffer，提供 size = -1；超出实际字节长度会截断到实际长度，
小于 -1 报 RangeError。内嵌 NUL 与 UTF-8 直接按原始字节发送，不经过 strlen。

```xlang
auto status, sent = socket.send("a\0b", 99) // 最多发送 3 字节，允许成功短写。
auto result, count = socket.sendTo(b"payload", address)
auto status, received = socket.receiveInto(output, 65536)
```

发送与 receiveInto 返回值类型为 `(NetStatus, uint64)`，不是堆结果对象。
receiveInto 向 Buffer 末尾追加，maxBytes 是非负的单次最大接收量，默认 65536；
只在读取前扩容，分配失败不消费输入。调用者须独占该 Buffer 的可变内容；并行
Socket I/O 可以使用不同 Buffer，不自动赋予普通 Buffer 跨线程同步能力。

NetStatus 明确区分 ok、again、closed、truncated、timeout、cancelled；failed 对应
原生错误，但真实操作错误走 checked 抛出，不作为普通成功结果交付。流 EOF 为
closed；空数据报是 ok 且计数 0，不能误当成 EOF。数据报截断保留已经复制的前缀并
返回 truncated，剩余报文已经由原生操作消费。again 不修改 Buffer 的有效长度。

默认非阻塞。显式关闭 nonblocking 可能阻塞调用线程，应放到显式线程／阻塞执行环境；
本原始接口没有隐式工作线程、Future 或任意阻塞回调的免饥饿承诺。高层 TCP／UDP／
期限／取消由后续传输层组合，不在这里伪装成同步原始 socket 的能力。

## 生命周期、并发与真实拥有图

Scope 在短元数据锁内取得实际引用后，再释放锁执行 OS 操作。接收／发送可并行，
Close 必须无活动调用；连接推进和可变原生元数据修改使用独占 Control，拒绝与 I/O
或其它 Control 重叠，不持有元数据锁等待内核。shutdown 使用受保护的共享借用。

XRT Close **即使报错也消费对象**。语言先封闭 live slot，再调用 Close，失败后
isClosed 仍为 true；再次 close 幂等，不重试已消费的 OS 句柄。析构错误／无诊断错误
也通过 checked 清理状态报告，不依赖错误指针是否非空表示失败。

语言 handle → 实际 Socket owner 是明确拥有边，原生 wrapper 不持有语言值。
活动借用和关闭尾部为 BUSY，精确 Ops 身份先于 Count／Trace 回调核验。Clear 仅移走
原生槽，Finish 在冻结区外关闭，任意父子 Finish 次序都只消费一次，重复 Finish 安全。
没有公开 nativeHandle、point adopt、端口或通用回调注入。

控制与 I/O 分开裁剪。仅构造／控制不根入 Buffer；仅使用普通标量组合也不根入 Socket。
Socket 不引入 Engine、Resolver、DNS、TCP／UDP 高层服务、TLS、Executor 或默认异步源。
驻留 runtime 承担实现；标准库仅声明，Cgen 没有新增 Socket 函数体／符号特判。

## 编译器公共 ABI 增量

新增闭合的 I64/U64 物理组合 ABI。语义层和 IR 分别核对标量宽度、所有权、可空性；
数值枚举仅共享物理布局，不等同于普通 int。驻留描述额外逐元素验证完整 NetStatus
声明与成员值，不能把 `(int,uint64)` 或其它同宽枚举冒充网络结果。
原有 Atomic 五种 bool 组合及三标量组合保留独立回归。

## 证据与尚未解决事项

证据根 `F:/xlang-validation/five-groups-20261020/p2-socket`。专项检查器为
check_stdlib_net_socket_runtime.py 与 check_stdlib_net_socket_language.py。
Windows／Linux GCC／TCC O0/O2，真实拥有图、受控关闭失败、分配失败、JIT／模块／
独立 AOT、严格 C 及无效合同拒绝分别验证；Linux 另做 ASan／UBSan／泄漏检查。
每种优化验证 25 个直接目标、7 个状态组合，拒绝 174 项损坏合同后不输出 C／header。

正式限定验收为 runtime-win-r5（39 阶段）、runtime-linux-r3（41 阶段）、
language-win-r3 与 language-linux-r1（各 65 阶段）。完整源码驱动分别为
source-win-r2（652 单元）及 source-linux-r1（651 单元）。六个正向 AOT 均检查
无 xlang 共享库依赖；四配置 shipping 源清单为 608／608，但未构建／验收完整
shipping DLL／SO／VFS。提交前重新核对输入、日志及产物摘要，旧失败轮不计为通过。

只使用自己的 IPv4 loopback 和动态端口；不查询外部 DNS，不连接其它服务，不做安全
专项或扫描。受控关闭失败在真正且唯一的原生 close 之后注入失败结果，不重复关闭
实际 OS 句柄，不把替身结果冒充真实系统异常复现。

Windows 的一次 UDP module 定位得到笼统的“xlang compile failed”，没有具体诊断。
随后 5 次检查及单独 40 次进程均未复现。原失败保留在 language-win-r2，后续独立记录
为 module-repro-r1；**原因尚未定位，不能算已修复**，保留到 P0／P7 的诊断和整体验收。
正式完整通过轮次与摘要以本批提交回执为准；不得把此项或整个 shipping 验收标为完成。

待继续：完整接收来源／数据报元数据与逐包控制、错误队列、PMTU／多播、散聚与批量
I/O、Engine 传输租约组合、Resolver／Future；不以本批基础 API 缩减这些已批准能力。
IPv6 端到端、两平台完整 shipping DLL／SO／VFS 与跨组验收也仍保留。
