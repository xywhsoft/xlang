# 受管 TCP 传输：当前接入与验收边界

本批实现基础 `TcpStream`／`TcpListener`，不是五组长期任务或整个网络分类的完成声明。
底层算法、就绪通知和同步等待复用 XRT；适配驻留 runtime，Cgen 没有增加 TCP 算法、对象重建或按符号名猜测结果类型的分支。

## 公开能力

- `TcpStream(engine, remote[, options, worker])`／`tcp.connect`：显式 Engine 和不可变 NetAddress，创建连接中的传输；通过 `wait(TcpWait.open)` 或 `connectedAsync()` 观察完成。
- `tcp.connectAsync`：等待连接后返回同一个已创建的语言对象，不在 I/O 工作线程重建类实例。
- `TcpListener(engine, options)`／`tcp.listen`：单个监听器。旧 `TcpServer` 实际只是监听器，现改为准确名称；真正的聚合 Server 仍在后续计划。
- `accept(timeoutMillis)`／`acceptAsync()`：返回受管 TcpStream。异步入口先创建未启动的结果载体，成功建立语言实例后才提交原生 Accept。
- `send(string|bytes|Buffer, size=-1)`：按实际字节长度发送，保留 NUL；过大的 size 截断到输入大小，其他非法负值 checked 报错。返回 NetStatus，不假装发送成功或返回无意义的布尔值。
- `recv(maxBytes=65536, timeoutMillis=-1)`／`recvAsync(maxBytes=65536)`：返回 bytes，最多取指定数量的当前数据；不保证一次接收对应一次发送。EOF 返回空 bytes，格式转换不属于传输层职责。
- `wait(TcpWait, timeoutMillis=-1)`：返回 NetStatus；`waitAsync(TcpWait)`／`waitAvailableAsync(minimum=1)` 返回 future<bool>，分别表达条件满足与传输结束。
- `shutdownWrite()`、`pauseRead()`、`resumeRead()`、`close()`、`abort()`：半关闭、读控制、正常关闭与立即中止。
- `state`、`available`、`pendingSend`、`writeLimit`、`writable`、`localAddress()`、`remoteAddress()`、`snapshot()`：明确类型的状态、背压指标、独立地址及统计快照。

同步超时单位为毫秒，-1 表示无限等待，0 表示真实的立即期限。
即使连接已经打开，原生等待也可能仍需 Worker 处理就绪通知，因此 0 不承诺成功。
XRT 内部期限 ABI 仍为固定快照的 uint64 微秒／xdeadline，不混用上游新的 double 毫秒布局。
同步方法保留 XRT 的同 Worker 阻塞拒绝规则；异步语言工作线程不应执行长时间同步阻塞。

## 生命周期与取消

Stream／Listener 保存实际的 Engine 拥有边；Accept 的私有结果载体同时拥有实际 Listener。
Scope 中的 Engine 指针只是保护期内的类型化借用，不成为语言公开裸指针。
正常 Close 与最后引用退役分开：显式 Close 请求正常关闭，最后引用退役中止剩余工作并等待实际终态。

新增队列 Future V6 独占启动合同。输出取消请求传递到本次原生操作，既有 V5 共享源行为不变。
取消请求不是原生 Future 的终态；若取消与完成竞争，仍保留 Watch、原生 Future 与结果载体到真实终态。
不能为了快速清理而释放仍可能接收连接的 Accept Future。
回调投影、捕获清理和语言对象释放在固定就绪队列执行器中进行，不在 I/O Worker 中执行语言回调。

Accept／Connected 的描述符固定结果类和原生字段身份，保留同一个语言对象。
字段名在调用时复制，不借用可卸载模块的元数据。接受结果前再次验证实际字段句柄，拒绝错误 Listener 配对或被替换的结果字段。
recvAsync 的描述符同时固定 bytes 的物理 ABI 和 retained-xvalue 结果合同，不允许仅因装箱载体相同而替换成 future<value>。

已交给调用者的 bytes 不依赖流的寿命。当前异步接收从原生 NetBytes 视图直接复制一次到 bytes，不宣称零复制，也不绕行 Buffer 或 C 字符串。
统计快照独立于源对象寿命；可检查的方法遵守 checked 错误边界。安装声明头不依赖未打包的 resident `.inc` 字段表。

## 验收

可复现入口：

- `tools/check_stdlib_tcp_language_runtime.py`：实际组件／拥有策略计划；GCC、TCC，O0／O2；Linux ASan／UBSan；独占和共享 Future、分配失败、错误类配对、投影拒绝及正常本机收发。
- `tools/check_stdlib_tcp_language_source.py`：完整 SourceXrt 编译器的 JIT、模块、独立 AOT、生成 C 的 GCC 严格编译、IR 合同负例、枚举独立裁剪及 DLL／SO 导入检查。
- `test/cases/xlir_tcp_managed_contract.c`：每个描述符的错误、效果、执行边界、服务与所有权变异拒绝；八种违规分别经过完整 verifier 和 C／header 输出门禁。

原生验收使用 select 及 Windows IOCP／Linux epoll；语言用例使用 select。
只运行自有字面量 loopback 正常通信、离线 ABI／生命周期／分配失败检查。
不使用 DNS、公网端点、扫描、攻击模拟或畸形网络协议测试。
Linux 系统 TCC 不支持旧共享回归用例使用的 C11 `_Atomic`：仅该历史用例的测试 TU 用 GCC 编译，驻留闭包、原生实现和最终链接仍使用 TCC；新独占及 TCP 用例直接由 TCC 编译。报告明确记录此边界。
同步 xlang 程序不建立语言就绪队列；但 XRT 的同步 TCP 本身依赖原生 Future，不能把两者混为一谈。

本轮外部证据目录为 `F:/xlang-validation/five-groups-20261021/p3-tcp-public`，保留发现阶段失败记录，只有输入、输出和日志摘要一致且 Complete=true 的冻结结果可作为通过证据。
这不是新 shipping DLL／SO／VFS 的发布验收，未修改 release 二进制。

## 仍在长期任务中

继续完成聚合 TcpServer、Resolver／地址列表驱动的 Dial 与代理、期限／取消组合、向量／文件发送、帧适配、完整 UDP，以及后续受管非对称密钥、X.509、TLS 和联合验收。
旧 TCP 配置和裸句柄入口已退出默认注册；依赖它们的历史 TCP／TLS／WebSocket fixture 需要随相应传输适配迁移，不计入本批通过结果。
不能用旧 TLS／WebSocket 的裸 Stream attach 将受管 Resource 指针解释成原生 xnetstream。
组件边界及按需体积还需在高级传输补全和最终裁剪验收时继续压实，不把当前基础层通过等同于所有网络功能已完成。
未决策分类未改变：CRT 0 项，XRT 11 组；本批不擅自扩展未决策范围。
