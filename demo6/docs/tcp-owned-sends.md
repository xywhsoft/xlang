# NetBytes 与 TCP 拥有型发送

本批接续受管理 TCP 数据通路和 Server。不是全部 TCP／UDP／TLS 或长期任务完成声明。
原先的 `NetBytes` 是旧 Cgen 包装类；现在收敛为同一个驻留、受管理、不可变的网络
字节资源，不另造第二个同义类型，也不改变基础类型 `bytes` 的传值规则。

## 公开面

| 接口 | 合同 |
| --- | --- |
| `NetBytes()` | 独立拥有的空网络字节块 |
| `NetBytes(string|bytes|Buffer input, int size=-1)` | 按实际字节长度复制所选前缀，建立不可变快照；包含 NUL，不用 strlen |
| `length` | 字节数；pure／no-raise／nonblocking |
| `byteAt(int index) as int` | 0 起始索引，返回 0—255；负数或越界报 RangeError，不解释 Unicode |
| `toBytes(int size=-1, int offset=0)` | 复制为独立的基础 `bytes` 值 |
| `appendTo(Buffer output, int size=-1, int offset=0) as int` | 直接向目标追加所选片段，返回追加字节数；不先建立临时 bytes |
| `TcpStream.sendRef(NetBytes data, int size=-1, int offset=0) as NetStatus` | 复用原生不可变片段；原生发送实际持有自己的 NetBytes 引用，调用者随后可释放语言包装 |
| `TcpStream.sendMove(Buffer data) as NetStatus` | 接管整个连续 Buffer 的载荷；仅成功时源变空，所有别名可见且源可复用 |

`size=-1`／缺省取全部，超长截断，小于 -1 报 RangeError；offset 必须在
`[0,length]`。网络字节前缀可以截在 UTF-8 编码中间。NetBytes 类实例赋值是共享
不可变资源；它不是第二个基础 bytes 类型。构造／toBytes 有显式载荷复制，重复
sendRef 不再做用户态载荷复制；不把它宣传为内核端到端零复制。

sendMove 没有部分消费参数：它转移整个容器的有效内容，失败／背压不丢弃任何源
内容。空源成功时归还其预留容量并归零。普通 send(Buffer／NetBuffer) 的复制语义
不变。Buffer 是可变引用容器，并发访问仍由调用者同步，不新增语言借用或参数修饰。

后续加入的 `sendRefs(array<NetBytes>, size=-1)` 采用真正原子受理及原生拥有边，见
[批量引用发送](tcp-ref-batches.md)；不是对单项 sendRef 的逐项循环。

## 拥有权和分层

- NetBytes 私有 handle 真正拥有一个 `xnetbytes` 引用；类字段／句柄／原生数据是
  明确的图边，不将带子拥有者的句柄标成叶节点。
- sendRef 直接复用 `xrtNetStreamSendBytes`，由 XRT 保留实际 payload，数据消费或
  关闭后的真实终态才归还。没有生成的释放闭包、借用语言栈或假引用计数。
- sendMove 直接复用跨线程可调用的 `xrtNetStreamSendTake`。队列受理与源归零置于
  同一 ownership mutation 内，Freeze 不会观察到两个持有者都声称拥有同一分配的
  中间状态。失败保留指针、长度、容量和内容。
- 11 个公开驻留入口及一个内部 owned-adopt 边界均有精确 owner／效果／物理 ABI
  合同；状态码保持 NetStatus 名义枚举，错误仍 checked。Cgen 源文件未修改。
- 构件按 NetBytes、本地 Buffer 转换、拥有域、引用发送、连续 Buffer 接管分开。
  单独用 NetBytes 不需要 Engine、TCP、Future 或就绪队列；最终 AOT module.c 的
  私有目录确实包含 NetBytes 及原生网络缓冲拥有域，而不是只检查 Cgen 的声明。

旧 NetBytes 的裸 data／bufferNative／textNative 和隐式二进制字符串转换不再进入
公开类；使用 length、toBytes、appendTo，文本解码另走显式 encoding。旧 TLS 的同步
owned producer 暂经内部 `__xl_std_runtime_take_net_bytes` 接受其拥有型结果，避免为
私有字段保留公开 point 构造；它随 P6 producer 的完整驻留迁移收敛，不是 TLS 完成。

## 2026-10-22 验收

证据根 `F:/xlang-validation/five-groups-20261022/p3-tcp-ownership`：

| 范围 | Windows | Linux |
| --- | ---: | ---: |
| 受选原生图、快照、发送、分配及拒绝对照 | 43 | 48 |
| 严格生成 C、实际 JIT／module、独立 AOT、裁剪和 IR 合同 | 53 | 53 |
| 旧 TLS 依赖的离线 C 生成／IR 检查，无 I/O | 2 | 2 |

合计 201 阶段，最终报告 Complete；封存时再次核验输入、日志和产物哈希。源码
驱动为 Windows 785／Linux 784 个实际编译单元，未重建发布 DLL／SO／VFS。

GCC／TCC O0／O2、select／IOCP 或 epoll，Linux ASan／UBSan。纯资源专项验证
准确两节点拥有图、独立 NUL 快照、范围错误、16 个预算（2 次真实失败）及内存平衡。
发送专项每次 16 个预算，12 次成功／4 次真实失败；完整拥有图被接纳，语言包装
先释放仍能接收精确 NUL 片段，OOM／关闭后源保持不变，最终内存和错误计数平衡。
预算专项使用公开 NodeCacheBytes=0 配置，确保不是把命中暖缓存当作 OOM 覆盖；
未改变生产默认缓存。缺失 Stream／Server 家族、缺失父边、缺失字节拥有策略和
错误叶标记这五个对照均在实际运行中被拒绝，不靠编译失败冒充对照有效。

采用 `check_stdlib_tcp_server_source.py`／`check_stdlib_tcp_server_resource.py` 的
`--profile owned-sends`，保留原 server 默认批次；源夹具先批量发现问题再统一验收。
通用脚本补齐合同第二个输入夹具的冻结，并改为绑定实际追加的 AOT module.c；没有
重写旧报告。TLS 依赖只证明声明／IR 接入，无握手、TLS 运行或安全性验收主张。

所有通信都是自有字面量回环的小型正常载荷；没有 DNS、公网、扫描、畸形协议、
攻击模拟或网络安全性质专项。XRT／xruntime 固定头及原生仓库工作区未修改。

## 仍开放

拥有型网络接收与不可变块复用、原子批量引用、链式 NetBuffer 跨线程接管、完整
UDP／多播、网络文件／任务组合、剩余 P0／P1，以及 P4 非对称密钥、P5 X.509、
P6 TLS、P7 发布联合验收继续。没有把普通复制发送冒充引用发送，或把本批当完整
传输完成。未决策仍 CRT 0／XRT 11，对象池最后不变。
