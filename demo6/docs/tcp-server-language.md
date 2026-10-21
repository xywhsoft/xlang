# 聚合 TcpServer：受管理资源与声明式公开面

2026-10-22。本批在已提交的固定 Server 拥有目录之上接入受管理资源、不可变配置、
统计、同步／异步接受。42 个驻留桥接和 3 个永久拥有策略 getter；Cgen 未新增实现。
没有修改 XRT 算法、固定时间 ABI、发布 DLL／SO 或 VFS，不代表整个网络长期任务完成。

## 公开接口

```xlang
auto engine = NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto listen = TcpListenOptions(NetAddress.loopback(4, 0))
auto options = TcpServerOptions(listen).withAddedEndpoint(listen).withAcceptQueueLimit(64)
auto server = tcp.server(engine, options)
auto accepting = server.acceptAsync()
TcpStream client = await tcp.connectAsync(engine, server.localAddress(1))
TcpStream peer = await accepting
client.abort()
peer.abort()
server.close()
engine.close()
assert engine.wait(5000)
```

也可直接构造 `TcpServer(engine, options)`。Engine 必须已经启动；Server 不创建另一套
调度器，不保留配置、地址或语言对象的借用指针。

| 类型 | 能力 |
|---|---|
| `TcpServerOptions` | 默认配置／首端点构造；`endpointCount/mode/sharedPort/acceptQueueLimit`；`endpoint(index)` 独立投影；`withEndpoint/withAddedEndpoint/withoutEndpoint/withMode/withSharedPort/withAcceptQueueLimit` |
| `TcpServerMode` | `shared`、`reusePort`；是否支持仍由 XRT／当前平台决定，不暗中回退 |
| `TcpServer` | `close/state/endpointCount/listenerCount/localAddress/snapshot`；八种接受方法 |
| `TcpServerStats` | `state/accepted/rejected/errors/endpoints/listeners/closedListeners/queuedAccepts/peakQueuedAccepts/acceptWaiters` |

所有配置更新返回新对象；一个连续分配拥有整份端点配置，`Additional` 只指向自身。
投影和删除首／中／尾端点均生成独立值；不能删除最后一个端点。索引、队列大小、
模式及共享端口一致性 checked 校验。默认值来自 XRT，不复制一套默认常量。
`acceptQueueLimit` 是聚合已接受连接队列的上限；端点的 Listener 分发队列／并发量
继续通过 `TcpListenOptions.withAcceptQueue` 配置，不把它们混成同一个配额。

同步接受：

- `accept(timeoutMillis = -1)`。
- `acceptCancel(cancel, timeoutMillis = -1)`。
- `acceptUntil(deadline)`。
- `acceptUntilCancel(deadline, cancel)`。

返回拥有的 `TcpStream`；取消、超时和关闭使用 checked 错误。毫秒只用于语言超时参数，
显式期限保持 `time.clock()` 的微秒单调绝对时钟。阻塞调用复用 XRT 的 Worker 检查，
不在所属 Engine Worker 上偷偷阻塞。`accept(0)` 仍是 checked 同步接受，不冒充无分配 poll。

异步接受：`acceptAsync`、`acceptAsyncCancel`、`acceptAsyncUntil`、`acceptAsyncUntilCancel`，
均返回 `future<TcpStream>`。沿用固定工作线程＋就绪队列，没有每次等待新建线程／协程栈。
取消／期限在原生提交前装配；已经开始的源必须到达真实终态，不能仅凭取消请求丢掉拥有者。
关闭已有等待以 `ClosedError` 结束；关闭后不再准入新的接受。状态、地址和累计统计仍可读取。

## 拥有与代码生成合同

- `XL6_EXECUTION_TCP_SERVER_V1` 追加为 7，不改旧 kind。Server 使用自己的 typed 扩展，
  不给每个已有资源都添加一个 Server 指针。
- Resident Server 拥有实际 Engine 记录和原生 Server；原生绑定、Listener、队列及 Future
  继续由已验收的 XRT 图追踪。没有不透明叶子、虚构引用或回调名称推断。
- 原生 Server 的回调／Data 为空。构造失败和发布竞态走事务回滚；原生 runtime 继续
  拥有实际关闭尾部及 Engine 租约，不把 CLOSED 当作物理退休证明。
- 接受结果复用现有 Stream 载体，只追加明确的 pending kind。Prepare 不提交工作，
  独占提交必须验证准确的 Server；提升后释放实际接受父拥有者。
- 沿用驻留的类结果捕获／Field 合同，原生结果未交付就失去普通拥有者时必须 abort，
  不依赖将来关闭 Engine 来掩盖遗留连接。
- 42 个桥接都有精确的参数／结果拥有者、枚举、效果、错误边界和服务合同；不可变属性
  才标 `pure + no-raise + nonblocking`，构造、变更、I/O 不误标。私有异步启动还声明
  Stream 类字段和 Future 结果类型，不通过 C 文本猜测。
- 基础 Server 与接受源／语言 Future 分开裁剪；仅配置不创建执行源。真实基础 Server
  AOT 不选接受 Future／TCP Future 生产者或语言就绪队列。13 个新 provider 的依赖、
  导出、拥有域、两个响应模板及完整源码清单一并登记，组件 ID 只追加。
- `tcp.server` 追加在原函数表末尾。原 998 行及索引全部保持，4096 桶中只增加一个
  原本为空的槽，避免改变其他命名空间的 ID／查找行为。

生成代码调用 `xl6TcpServerCreateV1((XL6_ExecutionServicesV1){...}, engine, options)` 等
已声明入口，再走既有通用拥有／错误清理；Server 实现不由 Cgen 生成。

## 最终验收

Windows 证据根：`F:/xlang-validation/five-groups-20261022/p3-server-language`。
Linux 在原生文件系统构建／验收，路径为 `/var/tmp/xlang-server-language-*`；报告与
所有输入／日志／产物哈希在提交收据中逐项封存，不将发现轮计入最终通过。

| 最终报告 | 阶段 |
|---|---:|
| resource-win-r2 | 33 |
| resource-linux-r2 | 36 |
| options-win-r2 | 15 |
| options-linux-r2 | 17 |
| source-accept-win-r1 | 72 |
| accept-linux-r1 | 72 |
| extra-win-r1 | 7 |
| extra-linux-r1 | 14 |
| 合计 | 266 |

GCC／TCC O0／O2，Linux ASan／UBSan，select／IOCP 或 epoll。受管理图每次覆盖
24 次整图清理、3 次普通最后拥有者清理、未交付结果、延迟终态、80 个构造分配预算
及精确内存平衡。缺少 Stream／Server 家族、删去 Server 父边或放弃 pending 源的
离线对照均被拒绝；延迟终态用真实检查 pin、Freeze 和静止状态观察，不以 sleep 造证明。
配置另覆盖自有重定位、首中尾删除、独立投影、6 个范围拒绝及 16 个分配预算（8 失败）。

完整源码驱动 Windows 779／Linux 778 个编译单元。五份新语言夹具、旧 Listener
异步及 receive-into 回归通过严格 C、JIT、module、独立 AOT 和导入／裁剪检查；三份
语义负例在输出前拒绝。所有 42 个新桥接均在实际 IR 中；O0／O2 每次验证 320 个
目标、1402 次合同拒绝和 10 类 C／头文件输出守卫。另验公开 pending-close 的终态、
Linux `reusePort` 的两个实际 Worker Listener 及累计统计。

发现轮包括漏更新静态函数索引、独立编译了被包含的 IR 单元、配置夹具需分开原生
实现／声明 TU、以及旧延迟清理对照的观测竞态；均已修正对应登记或夹具，没有放宽
生产拥有／类型规则。Windows 修复索引后的源码重建复用 777 个已验证对象；只有
实际改变的两个编译单元重新编译，链接和运行验收仍重新执行。

本批仅离线 ABI／图／OOM 与自建字面量回环正常通信，没有系统 DNS、公网、畸形协议、
扫描或网络安全性质专项；后者继续由用户在 XRT 层负责。发布库／VFS 联合门仍属 P7。

## 后续仍开放

廉价的拥有型 `tryAccept`／空队列返回合同不以 `accept(0)` 冒充；低层裸 poll、借用
Listener、Data／回调指针不直接提升为默认类 API。后续继续完整传输的引用／接管发送、
UDP／多播、网络文件／任务组合和剩余并发，以及 P4 非对称密钥、P5 X.509、P6 TLS、
P7 发布产品联合验收。当前源码与专项通过不能代替这些目标完成。
未决策仍 CRT 0／XRT 11，范围与上一批不变。
