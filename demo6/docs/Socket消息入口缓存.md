# Socket 消息入口的并发发布前置

推进数据报接入时发现：固定 XRT 的 Windows `WSARecvMsg`／`WSASendMsg` 入口缓存
使用普通指针读写；并发首次查询或消息 I/O 存在数据竞争。本批先修复原生底座，
不靠在语言 I/O 外面长期加互斥锁来回避。

上游提交 `296927fcf7cd7d68b3d525b69cda5f05d2625173`：缓存采用一致的 Interlocked
指针读写，以 compare/exchange 发布首个成功入口，竞争者返回同一个已发布值。
不持锁调用 Provider，失败不缓存，仍可重试；不增加分配、模块依赖或公开 API。
关闭和可变配置的原有串行化要求不变。

xlang 仅移植两个私有缓存字段和一个加载函数到两份固定实现单头，并更新摘要。
未整体升级 XRT，没有改变时间／deadline ABI，也未修改公开声明头。
精确来源与前后摘要见 `lib/xrt/backports/net_socket_message_cache.json`。

证据根：`F:/xlang-validation/five-groups-20261020/p2-datagram/`。
原生检查为 cache-win-r4／cache-linux-r4，两端各 32 阶段、16 个运行产物，覆盖
模块化、上游单头及两份固定消费单头，GCC／TCC、O0／O2。可重用上游检查器为
`tools/check_net_socket_message_cache.py`，独立消费端检查器为
`demo6/tools/check_stdlib_net_socket_message_cache.py`。

每个产物为 IPv4／IPv6 各创建未绑定的 UDP 对象，让 8 个线程首次查询消息入口与
能力，校验一致性后回收。不 bind／connect／send／recv、不解析名称，没有网络流量；
不是网络安全或压力专项。该证据只覆盖缓存发布及本机 Provider 兼容性，不证明整个
Socket／传输层线程安全，也不是新 shipping DLL／SO／VFS 验收。

数据报元数据、发送控制、多播等语言接入仍在工作区开发，**不纳入本前置提交**。
枚举标志组合规则已单独询问；拥有型原生 getter 的 IR 合同需继续查明，不能仅为
通过验收就改掉 API 或绕过 verifier。向量／批量、错误队列、Resolver 与后续各组
保留完整范围。旧偶发 module 失败依然未查明，不归因于本次原生缓存问题。
