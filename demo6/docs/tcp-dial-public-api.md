# 受管 Dial：同步、Future 与部署合同

本批公开基本 Dial；连接竞争、候选排序、回退和超时继续复用 XRT。
适配在驻留 runtime，Cgen 仅按已验证的声明、调用、结果布局与拥有清理合同生成代码。
没有添加按函数名猜语义的 Cgen 分支，也没有第二套连接算法。

## API

以下接口同时可用作 `TcpStream.dial`／`TcpStream.dialAsync` 静态方法：

```text
tcp.dial(engine, resolver, host, port [, options]) -> TcpStream
tcp.dial(engine, addresses [, options]) -> TcpStream
tcp.dialAsync(engine, resolver, host, port [, options]) -> future<TcpStream>
tcp.dialAsync(engine, addresses [, options]) -> future<TcpStream>
```

`engine` 必须是已启动的 NetEngine；主机路径显式传 NetResolver，不创建默认实例。
`addresses` 是不可变 NetAddressList，保留各端点端口，不经 DNS。
`options` 是 TcpDialOptions；缺省直接使用 XRT 默认配置，不临时构造语言 Options 对象。
主机端口严格要求 1—65535；空主机、内嵌 NUL 和非法 UTF-8 在提交工作前 checked 拒绝，不截断成另一个主机名。
端点列表遵守原生 Family 过滤及非零端口规则。

同步返回和异步成功都交付已打开的受管 TcpStream。
原有 TcpStream 构造／tcp.connect 的“立即取得连接中对象”语义不变。
同步 Dial 明确可能阻塞，在同一 Engine 的 I/O Worker 上于提交前拒绝，避免等待自身。
同步组件不选择语言就绪队列；原生 Future 依赖不等于语言执行器依赖。

```xlang
auto endpoints = NetAddressList([listener.localAddress()])
auto options = TcpDialOptions().withTimeoutMillis(5000)
auto pending = tcp.dialAsync(engine, endpoints, options)
TcpStream stream = await pending
```

## 拥有权与完成

异步先创建私有、尚未启动的结果载体和真实语言类实例，再提交 Dial。
载体保留实际 Engine／可选 Resolver；原生 Future 保留实际请求、候选和回调拥有图。
捕获布局保存真实 receiver、结果对象、原生字段句柄及字段名副本；不借用模块元数据。

Accept、Connected、Dial 共同使用一套私有类结果投影和清理实现。
操作种类仍由各具体 provider 和载体检查；promote 是永久驻留代码，不是可卸载模块回调。
成功前再验证类字段身份，提升同一个对象，不在 I/O Worker 重新构造语言类或执行 getter。
投影、捕获清理和结果提交在固定工作线程／就绪队列执行。

`future.cancel()` 是请求，不是原生终态；取消与成功竞争时保留实际源直到终态。
原生已成功但尚未向语言交付的连接，在取消／投影失败后中止并释放，不等待最终 Engine 销毁来掩盖孤儿连接。
已经交付给调用者的连接仍归调用者管理；已完成结果的取消不能代替关闭该连接。
所有能分配或失败的入口保留 checked 错误合同，没有误标 pure／no-raise。

## 通用 IR 修正

引入重载所需的类型声明后，暴露了通用部署需求扫描的旧问题：未使用的 async 方法／Future continuation 也会强迫启动执行器。
`xl6IrModuleContinuationRequirement` 现在只从已封闭可达性中的保留函数、原生服务及 continuation 指令推导部署需求；构造阶段保持保守。
验证器仍检查不可达异步状态机的全部执行合同，但仅保留的状态机要求实际模块执行器。
不能通过伪造 reachability 来绕过：闭包事实重新验证后，错误执行域、机器合同和伪造可达性均在 C／header 输出前拒绝。
这不是针对 NetResolver 或 Dial 名称的裁剪补丁。

## 验收及未完成范围

- `check_stdlib_tcp_language_runtime.py --dial`：实际驻留闭包；GCC／TCC、O0／O2、Linux ASan／UBSan；原有 Accept／Connected、共享及独占 Future 回归；同对象返回、字段替换、元数据副本、原生成功后取消、分配失败及 I/O Worker 同步阻塞拒绝。
- `check_stdlib_tcp_language_source.py --dial`：完整源码编译器的 JIT、模块、独立 AOT、严格生成 C 编译、十个基本 Dial C 入口、私有入口和错误重载拒绝、同步／枚举裁剪及实际 live／unused continuation 对照。
- `check_xlir_continuation_deployment.py`：完整 verifier 与零输出门禁，分别拒绝错误部署 ABI、错误机器执行 ABI 和伪造可达性。

证据目录为 `F:/xlang-validation/five-groups-20261021/p3-dial-public`。
只运行离线检查与自有字面量 loopback；主机入口本批验证提交前拒绝，实际主机原生路径继续由固定地址的原生 Resolver 夹具覆盖，不做系统 DNS／公网／畸形协议或安全专项测试。
快速重链接仅用于发现问题；验收依赖完整冻结源码构建，拒绝使用中途修改的构建报告。
这不是新 shipping DLL／SO／VFS 的发布验收，未修改 release 二进制。

代理、绝对期限／显式 token 的组合、聚合 Server、向量／文件／帧、UDP、受管非对称密钥、X.509、TLS 和最终联合验收仍在长期任务中。
基础 I/O、拥有权和效果合同的通过，不代表剩余模块已经完成。
未决策不变：CRT 0，XRT 11 组。
