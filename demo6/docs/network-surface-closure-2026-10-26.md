# 网络 40 模块归属与普通产品回归

本批补齐网络模块的逐项角色核对，以及此前缺少的普通产品组合证据。
不是新增一套网络算法，也不是五组长期任务／全仓 RC0 完成声明。

## 模块归属

完整、可检查的逐模块记录在 `tools/five_groups_module_roles.json`，保留原始
198 模块范围。网络 40 项现在全部有具体公开面、原生复用理由、源码、设计
和正常产品用例见证；角色核对不等于全部入口运行覆盖。

| XRT 模块 | xlang 归属 |
| --- | --- |
| net | 不可变拥有的 NetAddress；数字解析、完整端点和地址操作 |
| net_interface、net_interface_text | NetInterfaceList／Interface／Address，以及 net 本机查询 |
| net_dns、net_resolver、net_resolver_future | 同步数字／系统解析、NetResolver／请求，以及同请求的 Future／取消组合 |
| net_socket | NetSocket；控制、标量／向量／批量、逐包元数据及类型化状态 |
| net_buffer | NetBuffer／NetBytes；复制、引用和接管 |
| net_frame、net_frame_line、net_frame_length | NetFrameResult、NetLineFramer、NetLengthFramer |
| net_proxy、net_proxy_handshake、net_proxy_socks5、net_proxy_http_connect、net_proxy_dial | NetProxy／Handshake 和 TcpStream 代理拨号；SOCKS5／CONNECT 保留为统一协商器的内部协议实现 |
| net_port、net_port_select、net_port_epoll、net_port_uring、net_port_kqueue、net_port_iocp | Engine 下的原生驱动；NetBackend 选择和能力查询，不公开裸 Port 或重叠 I/O 指针 |
| net_engine | 显式 NetEngine／不可变 Options／Stats、工作者、post／timer 和 checked 退出 |
| net_file | 拥有型 NetFile；定位 bytes I/O、取消和真实物理 close Future |
| task_net、task_group_net | NetEngine.submit 系列，以及既有 TaskGroup 的相同执行源组合 |
| net_tcp、net_tcp_file | TcpStream／Listener 和独立文件范围发送 |
| net_tcp_server、net_tcp_server_future、net_tcp_server_sync | 真正聚合 TcpServer，以及同 Server 的同步／异步接受 |
| net_sync、net_tcp_future、net_tcp_sync、net_tcp_dial、net_tcp_dial_future、net_tcp_dial_sync | 原生等待底座、Stream 同步／异步投影，以及 tcp／TcpStream 拨号组合 |
| net_udp、net_udp_future、net_udp_sync | UdpSocket／Packet／Batch／Message 及同对象的同步／Future 操作 |

这遵循原方案：可以切换后端，不机械公开每一个 C 容量查询、裸输出指针或
内部协议函数；也没有将尚未完成的 TLS CRL／回调重新归类为内部而略过。
总账本为 149 已复核、49 角色待核对，后者是 TLS 47 和密码学范围确认 2 项，
不是“还差 49 个功能”或实现完成比例。

## 本批新增的真实组合

`stdlib_net_public_backends.xl` 对本产品实际编入的每个驱动分别创建两工作者
Engine，检查每个 worker 的实际 backend 和能力，再执行 IPv4、IPv6 的自有
TCP／UDP 回环通信。包括 NUL、UTF-8、精确 size、空数据报、物理关闭和
checked Engine 退出。显式创建失败就失败，不能自动替换成 automatic／Select。

- Windows：Select、IOCP。
- Linux：Select、Epoll、io_uring。

以上在 JIT、module、独立可搬移 AOT 和普通 CLI 中实际运行，不只有
backendCompiled 元数据。Kqueue 仍按原生平台条件裁剪，本批没有 BSD／
macOS 环境，不将其记作 Kqueue 运行验收。

`stdlib_net_public_file.xl` 使用自身唯一临时文件并在 finally 删除，避免旧
共享路径／环境变量前置。覆盖 IOCP／io_uring 定位写入、string／bytes／
Buffer 输入快照、NUL、取消令牌、重复 await 的独立 bytes，以及重复物理关闭。

## 批量发现、定点修复、证据边界

新增 `network_surface_public_fixtures.json`：43 个正例、4 个编译拒绝例。
复用原有正常本机、数字解析和离线成功协商，不重复编写传输／解析算法。

主批中 42 正例和 4 拒绝例在两端通过。新文件用例首次误将 await 写入普通
函数，被现有语义规则正确拒绝；修成 async 后仅针对该用例复测，不重跑
未变程序。首轮失败报告、旧输入和日志全部保留，主报告本身仍是失败状态。
独立 reader 逐项核对未变用例和定点复测，不能把主报告改成 Complete=true。

两端各 264 个成功阶段：主批 257，文件定点批 7；另各有 1 个保留的失败阶段，
不混入成功计数。每个正例都经过 typed／lowered IR、重复 C 一致性、header、
JIT、module、checked 退役、GCC O0／O2 严格 C、独立 AOT 搬移执行／导入及 CLI。
实际阶段计数与宿主内部标记不相加。代理拒绝例保留现有笼统成员诊断，正例
证明相同接口存在，但本批不将这些诊断解释为精确的类型错误文案。

使用已冻结、正常全构建的 `p7-crypto-migration/shipping-source-r3` 普通产品，
隔离目录仅有真实 CLI／两库；无源对象覆盖、provider 替身或 release 二进制
替换。它证明本批用例在该候选中运行，不证明本轮最新完整工作区已经重建；
前批新增的私有 TLS packet 也不因此取得普通产品验收。

独立 reader 复核构建输入／产品、日志／输出哈希、精确阶段和用例集合、当前
源码／冻结输入、真实后端输出。Cgen 全面扫描仍发现未决策 WebSocket 的旧
stream 生成器引用六种 xrtNet 函数；本批没有声称“整个 Cgen 零网络引用”，
也不擅自扩张到 WebSocket。全部本批生成程序都不包含该旧实现，网络旧
wrapper／隐式服务也未重新进入。生成调用位置只作见证，不声称全部入口覆盖。
角色库存两端另各拒绝 11 份坏数据；这是静态库存检查，不是运行验收。

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`，收据为
`verified-network-surface-windows.json`／`verified-network-surface-linux.json`。
Linux 主批、定点批分别保留原件和逐文件／完整成员哈希验证归档。

## 继续保留的范围

TLS CRL／附加策略、回调拥有图和恢复绑定继续；原始 AES／独立 Poly1305
公开范围仍待确认。历史间歇诊断、完整参数化门、相关 IDE／嵌套编译和最后
跨组联合验收不因本批成功关闭。代理拒绝的笼统诊断也不作为诊断质量完成；
WebSocket 旧后端明确留在该独立未决策类别，不在本批删除或迁移。

本批只做离线正常材料、本机接口读取、自有字面回环和普通文件操作。
不查询外部 DNS、不连接其它服务，不做多播组流量、错误包／ICMP 注入、
扫描、攻击模拟或网络／密码学安全专项；后者仍由用户在 XRT 层完成。
未决策 CRT 0／XRT 11 不变，内存池最后。
