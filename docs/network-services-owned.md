# 网络服务联合拥有图验收

## 范围

此项组合当前受管理的 NetEngine、NetResolver、TcpListener、TcpStream、
UdpSocket、NetFile、TaskGroup 及各自 Future；不使用旧生成代码里的全局
`xl6_net_engine`／`xl6_net_resolver`，不将旧宿主测试套用于新接口。
普通网络 Engine 使用 Windows Select／IOCP、Linux Select／Epoll／IoUring；
文件 Engine 明确选择 Windows IOCP 或 Linux IoUring，不伪装线程池为异步文件 I/O。

入口：`tools/check_stdlib_network_services.py --source-build <build.json>
--out <新的证据目录>`。Linux 另传 `--native-root`。它复用已验证哈希的当前
源码驱动及所有编译对象，不链接旧编译器实现来代替当前源码。底层 TCC 核心
仍来自指定运行库，因此这是源码产品验收，不是新 shipping DLL／SO 验收。

## 实际数据与退役

- 数字字面量 `127.0.0.1` 的 resolver 结果，禁止外部 DNS。
- 自建 loopback TCP 的 accept/connect/send/receive；自建 UDP 向自身地址发包。
- 自建普通文件的定位写入／读取，数据含内嵌 NUL。
- 同一语言对象保留两个 Engine、resolver、listener、TCP 两端、UDP、文件、
  TaskGroup，以及待完成的 accept/receive 和延迟任务 Future；另保留文件读取、
  解析 Future（这些快速操作允许已经完成，不冒称强制保持未完成）。
- 宿主回调保留真实 xvalue，而非伪造引用计数。对象内通过 DynamicFields
  保存指向自己的真实引用边，使末次释放必须经过完整拥有图检查。

宿主分别测试普通外部根、真实 Engine 进入区、析构抛错三种情况。
销毁模块及编译上下文以后，外部语言对象仍须保有资源及代码租约；放弃该根后，
持有中的原生进入区仍须阻止物理退役。析构仅执行一次，析构内部的同驱动
checked drain 重入报告 busy；析构错误由 checked 结果报告，而不是吞掉错误。
没有提前 close/stop/cancel/join，也不采用 native-owner/orphan 救援补足拥有图。

## 图式载体边界

普通 XRT Value 容器的现有存储合同拒绝形成容器引用环；类字段当前仍使用
该存储合同。发现阶段的 `root.cycle = root` 因此得到 ValueError，并非网络
拥有型接口漏实现。此次不修改这一基础语义，也不在 Cgen 加特殊分支绕过它。
联合用例改用已有、可被独立目录认证和追踪的 DynamicFields 引用环；键包含 NUL，
并检查实际存入的对象身份。普通类字段的直接自引用能力不因此宣称已支持，
应随尚未决策的 X07／X26 值图、对象图能力另行明确。

## 裁剪、证据及安全边界

独立程序的空观察回调由测试源码包提供，明确声明 `xrtModules: []`，因为它
不调用任何 XRT API。不会使用未声明混编 C sidecar 后，又错误要求编译器忽略
其“允许完整原生能力”的现有合同。正式 AOT 必须没有 `_MODULE_ALL`。
宿主路径则注册进程常驻回调，不让即将卸载的模块代码持有外部根。

导入 `.xl` 的发现轮还定位到包解析错误泄漏：候选路径会先尝试 JSON 清单，
失败后允许作为普通源码导入，却遗留了原生错误。修复在 speculative 包解析
边界隔离临时诊断，成功和 miss 均恢复调用者原来的确切错误拥有者；显式
LoadManifest 的无效清单仍返回失败并提供错误。没有在运行前全局清错、
改变 JSON 语法或放宽 verifier。两个直接包宿主检查与实际导入程序一起回归。

所有后端执行实际 JIT、模块、独立 AOT 及三种宿主模式；生成代码另经严格
GCC O0/O2。报告保存输入、日志、产物哈希和命令；失败发现轮保留，不计正式
通过，Linux 原件与归档镜像均保留。复核最终输入哈希以后才提交。

只允许本机自建普通文件、数字字面量解析与 literal loopback 正常通信。
没有公网／外部 DNS、组播 membership/traffic、ICMP／错误队列、扫描、恶意报文
或网络安全专项。本项不会将多播的编译／合同／链接覆盖冒充组通信实测，
也不代表五组任务或全仓 RC0 已完成。时间 ABI 不变：独立 XRT 毫秒／For，
固定快照微秒／Until；本批不改 XRT，也不升级完整快照。
