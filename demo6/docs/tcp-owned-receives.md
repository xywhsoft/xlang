# TCP 拥有型接收

接续 [NetBytes 与拥有型发送](tcp-owned-sends.md)。本批只完成 TCP 拥有型接收，
不代表完整 TCP／UDP、TLS 或五组长期任务已经完成。

## 公开合同

| 同步入口 | 异步入口 |
| --- | --- |
| `recvOwned(maxBytes=65536, timeoutMillis=-1)` | `recvOwnedAsync(maxBytes=65536)` |
| `recvOwnedCancel(cancel, maxBytes=65536, timeoutMillis=-1)` | `recvOwnedAsyncCancel(cancel, maxBytes=65536)` |
| `recvOwnedUntil(deadline, maxBytes=65536)` | `recvOwnedAsyncUntil(deadline, maxBytes=65536)` |
| `recvOwnedUntilCancel(deadline, cancel, maxBytes=65536)` | `recvOwnedAsyncUntilCancel(deadline, cancel, maxBytes=65536)` |

同步返回 `NetBytes`，异步返回 `future<NetBytes>`。maxBytes 必须为正，接收最多
这个字节数，不承诺填满；应用协议需要自行组装完整消息。数据不解释 UTF-8 或 NUL。
相对超时单位为毫秒；Until 使用与 `time.clock()` 一致的绝对微秒时间。

EOF 返回长度为 0 的独立拥有块，与现行 `recv()` 的空 bytes 规则一致。
取消、超时和分配／资源错误分别 checked 报告，不冒充 EOF。流关闭前已提交的等待
会完成并排空；不能把取消、异常或装箱失败解释成 TCP 输入完全没有被消费。

```xlang
NetBytes packet = await source.recvOwnedAsyncUntil(time.clock() + uint64(5000000))
assert target.sendRef(packet) == NetStatus.ok
```

上例不经临时基础 bytes，也没有接收结果到语言字符串的转换。
`toBytes()` 是显式复制；NetBytes 可在原流、监听器和 Engine 清理后独立存活。
`sendRef()` 仍可能背压，实际应用应处理 NetStatus，而不是总用 assert。

## 实现边界

- 同步直接接管 XRT 接收返回的 `xnetbytes`。异步结果解码保留原生 Future
  实际交付的同一个 NetBytes 引用，不复制载荷。
- 异步私有 producer 先创建真正的空 NetBytes 类实例，驻留解码器核对精确字段与
  policy 后安装结果；没有假指针、公开裸句柄构造或生成的释放闭包。
- 接收者、预建类、原字段句柄和字段名均由真实捕获帧拥有。结果只在捕获清理成功
  后发布，失败时释放装箱结果；原生 Future 保持实际终态的生命周期。
- Future 回调复用现行固定工作线程与就绪队列；取消／期限复用原生订阅和 deadline
  服务，不为每次等待分配系统线程或协程栈。期限／取消在提交原生读取前绑定。
- 八个驻留桥有精确 owner／class-field／服务／效果／checked 错误 ABI 合同。
  Cgen 只生成声明、调用、拥有清理，本批未修改 Cgen 或固定 XRT 头。
- 同步、普通 Future、带中断 Future 分成三个按需构件。原有普通 bytes 接收和
  Stream／Server 类结果映射没有反向依赖 NetBytes decoder。

这是消除“原生拥有结果 → 语言值”的额外载荷复制，不是套接字到用户数据的
内核端到端零复制承诺。

## 验收

证据根：`F:/xlang-validation/five-groups-20261022/p3-tcp-recv-owned`。
源码驱动 Windows 788／Linux 787 个实际编译单元，不重建发布 DLL／SO／VFS。

| 范围 | Windows | Linux |
| --- | ---: | ---: |
| 原生结果、拥有图、分配失败及拒绝对照 | 29 | 32 |
| 严格 C、JIT／module／独立 AOT、裁剪与 IR 合同 | 61 | 61 |
| 实时期限、直接取消及关闭排空的语言专项 | 9 | 9 |

合计 201 个正式检查阶段。最终报告及全部输入、日志、产物的再次哈希核验见
证据根的 `verified-commit.json`；问题发现轮次不计入完成数。

验证包含 GCC／TCC O0／O2、select／IOCP 或 epoll、Linux ASan／UBSan；原生结果
指针同一性、完整拥有图、EOF、独立载荷生命周期，以及 24 个提交前分配预算和
5 个实际结果线程的装箱预算。结果线程的两处真实失败必须成为失败 Future 并回收
资源。删除拥有域、亲缘边或谎报叶节点的对照必须被拒绝，不能仅以成功编译替代。

语言用例覆盖八个入口、NUL、引用转发、预取消／实时取消／真实期限触发、直接
Future 取消、关闭排空和严格范围错误；共享桥的普通 bytes／wait、连接／accept、
聚合 Server 及 recvInto 有真实 JIT／module／独立 AOT 回归。IR 合同拒绝错误拥有者、
字段、效果和服务声明，并在完整 verifier 及 C／头文件输出门禁前阻止。

不执行外网、DNS、扫描、畸形协议或网络安全专项。

## 剩余及未决策

原子批量引用、链式跨线程接管、完整 UDP、网络文件／任务组合，以及 P4—P7
受管理非对称密钥、X.509、TLS 和发布联合验收仍开放，不缩减原批准范围。
未决策仍为 CRT 0、XRT 11 组：core／error／ownership；内存；动态值与图；其余
平台信息；HTTP；WebSocket；运行时类型／转换；对象／字段图；调用／value 桥接；
typed 容器；内部支撑。内存池依旧最后处理。
