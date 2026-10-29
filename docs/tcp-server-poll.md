# 聚合 Server 的拥有型非阻塞轮询

接续 `dea1487c1` 的聚合 Server 公开面，增加：

```xlang
TcpStream? peer = server.tryAccept()
if peer != null
    peer.send(b"hello")
    peer.abort()
end
```

`tryAccept()` 标注 nonblocking，但不标 pure 或 no-raise。没有连接时成功返回
`null`；有连接时返回独立拥有的 `TcpStream`。关闭、错误的接收模式、已存在原生
异步消费者、分配／拥有权失败走 checked 错误，不能伪装成空队列。多个轮询消费者
沿用 XRT 接受队列的互斥；没有跨调用的 prepare／consume 隐藏槽位。轮询与等待式
accept 不应在同一个 Server 上混用。没有新增容量、超时或就绪队列默认值。

## 实现与成本

驻留适配器只调用 `xrtNetServerAccept`，不调用同步等待、创建 Accept Future、
协程、等待任务或工作线程。空队列时不创建资源、原生 handle 盒或语言类；实际
接收后才将 native Stream 绑定到同一个 ExecutionSource 和实际 Engine 父拥有者。
接管失败会中止已出队的 Stream 并释放原生拥有者，保留最初诊断。

新增独立 `TCP_SERVER_POLL` 构件；不借用 `TCP_SERVER_SYNC` 或 Server Future 构件。
共享 Stream 资源仍具有原来 pending Future 的生命周期能力，因而不声称整个
产物绝不包含任何 TCP Future 底座；这里保证的是不创建轮询 Future，也不引入
Server Future 根、同步 Server 等待适配器或语言 Future callback queue。

## 通用 XLIR 合同

扩展现有 owned native class-field 结果的可空语义，而非增加 TCP 专用生成器：

1. 语义层允许可空 class／内部 class-handle 结果；普通非空结果的规则不变。
2. 注册表同时声明精确 owner、nullable、nonblocking、ExecutionSource 服务与
   独立 foreign error scope；返回 NULL 不作为错误判据。
3. Lowering 先验证调用结果，再按原始指针分支。失败边由原 destructor 回滚；
   非空成功边完成 handle-TAKE／类构造；空成功边只有 null、store、jump。
4. 通用验证器检查三条互斥路径、唯一 TAKE、精确字段／析构、分支入口独占和
   空路径无分配。将空边导向非空边、交换边或破坏可空／错误／拥有合同，必须在
   C 与头文件输出前被拒绝。

Cgen 源文件未修改：它只消费已闭合的普通 CALL／ERROR_TEST／NULL_TEST／BRANCH／
nullable 操作以及既有清理。当前 XRT／xruntime 快照、发布 DLL／SO、VFS 均未修改。

## 验收范围

专项脚本为 `tools/check_stdlib_tcp_server_resource.py` 和
`tools/check_stdlib_tcp_server_source.py`，原 Server 资源、同步／异步、配置、停止和
旧 Listener／receive-into 回归仍保留。新增原生轮询专项每次检查 16000 次空轮询
（分配预算为零且不触发）、16 个接管预算、checked close 与最终精确内存平衡。
语言夹具覆盖空值、循环接收、拥有清理和内嵌 NUL；IR 专项核验空边及拒绝对照。

测试只用离线编译／拥有权／分配检查和自有字面量回环正常通信；不做 DNS、公网、
畸形协议、扫描或安全性质专项。源码驱动验收不代表 P7 发布库／VFS 联合验收。

### 2026-10-22 最终记录

证据目录 `F:/xlang-validation/five-groups-20261022/p3-server-poll`：Windows 原生
45、语言 87 阶段；Linux 原生 51、语言 87 阶段，共 270。四份最终报告 Complete，
输入／日志／产物未变，封存时逐项重新计算哈希。完整源码驱动为 Windows 780／
Linux 779 个编译单元，不把发现轮报告或旧报告计入这些阶段。

GCC／TCC O0／O2，select／Windows IOCP／Linux epoll，Linux ASan／UBSan；
8 份语言回归的严格 C、实际 JIT／module、独立 AOT、导入与裁剪，以及 3 份语义
负例通过。普通合同每次覆盖 43 个 Server 桥接、321 个 TCP 目标、1406 次描述符
拒绝与 10 类输出守卫。可空拥有专项每次核验 4 条无分配空 CFG 边、746 次描述符
拒绝及 18 类零输出守卫，包含交换空／非空边与将空边导入接管边的拒绝。

原合同的第二个输入夹具未列入旧检查脚本的通用输入字典；本批在两端合同阶段
开始前额外冻结其哈希及阶段检查点，封存时再次核验，见
`contract-fixture-witness.json`。没有改写已生成报告或将遗漏的输入默认为已绑定。
发现轮补齐语义层内部 class-handle 的可空准入、拥有结果三路互斥验证，并修正
Linux 测试辅助名称冲突；未放宽既有失败回滚、独占接管或输出前验证。

## 剩余范围

本批只关闭廉价拥有型 `tryAccept` 缺口，不关闭完整传输、剩余并发或长期任务。
引用／接管发送、UDP／多播、网络文件／任务组合、P4 非对称密钥、P5 X.509、
P6 TLS、P7 发布产品联合验收继续开放。未决策 CRT 0／XRT 11 与对象池最后不变。
