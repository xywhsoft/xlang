# Server 固定 ABI 与驻留拥有目录

2026-10-22：Listener／Server 原生闭包已同步到 xlang 固定快照，并接入按需选择的
驻留拥有目录。本批不是 `TcpServer` 语言公开面完成，也不是发布 DLL／SO／VFS 验收。

## 实际修改

- 原生来源截止 XRT `c09e357a7381d052f26d2ae28220c0ffa5ac9ba0`，包括已验收的
  Listener 事件拥有上下文、Server 真实计数绑定和 Accept Future。
- 四份固定实现／声明头只导入明确列出的 TCP／Server 区段。逐段核验未选择的
  文件、内存、TLS、xruntime 等区段不变；没有复制原生仓库的其他工作区修改。
- 保留 `uint64` 微秒和 `xdeadline` 绝对期限 ABI。同步接受内部改用固定期限类型，
  新增的入口保护和 closed-shell 检查保留；原生相对毫秒包装不进入产品。
  Server／Listener／Stream／Dial 配置声明及 Engine 期限接口逐项与原固定版本比较。
- XRT 3318／xruntime 3783 个原生公共函数（扩展自有 465）；新增 13 个底层入口。
  同步元数据、TCC 符号面和双平台精确原生链接目录。数量不等于语言公开 API 的完成度。
- `NetTcpBaseNative` 统一 Stream／Listener 的真实拥有图；旧 `NetTcpNative` 只负责
  TCP Future。新增 `NetServerNative` 和 `NetServerFutureNative`，分别处理实际
  Server／绑定／Listener 事件及接受生产者／取消 Watch／拥有结果。
- Listener 事件策略增加到 typed catalog，按独立提供的精确身份准入。
  不推测回调名称、不伪造引用、不将绑定或异步激活当成不透明叶子。
- 新增组件 ID 仅追加，既有 ID 不移动；依赖、导出符号、拥有域、两个构建响应模板
  和完整源码驱动的源清单同步更新。Cgen 没有新增实现函数体。

基础 Server 的实际选定链接不包含 Server Future、TCP Future 生产者或语言 Future
就绪队列；Engine 本身既有的受管理执行源仍保留，不把它误称为纯 C 无宿主包装。
只有显式选定异步域才登记接受生产者与结果合同。

## 最终验收

证据根：`F:/xlang-validation/five-groups-20261021/p3-server-import`。

| 最终报告 | 阶段 |
|---|---:|
| pinned-win-r3/results.json | 77 |
| pinned-linux-r3/results.json | 90 |
| tcp-regression-win-r1/results.json | 43 |
| tcp-regression-linux-r1/results.json | 49 |
| source-accept-win-r1/results.json | 74 |
| source-accept-linux-r1/results.json | 74 |
| 合计 | 407 |

GCC／TCC O0／O2、两份固定产品及真实驻留目录，select／IOCP 或 epoll，Linux
另有 ASan／UBSan。输入、日志和产品哈希均不变。

原生对照夹具保持独立，只调整 include 和固定时间 ABI；驻留夹具使用实际选定目录
而非手写准入器。每次覆盖 6 次整图退休、13 次精确策略／裸回调拒绝、15 个分配预算
（13 个失败前缀）、两个端点的 NUL 通信、9 次最终 Drop 竞态和 8 次关闭／取消竞态，
并验证 Engine 已物理释放后的 Server 壳体。驻留目录另验 64 个创建分配预算。
core-only 每次核验三次实际 8 节点图及物理退休，并以最终符号表检查裁剪。

六个离线目录对照分别去除 runtime、替换为复制的事件策略、去除生产者、取消 Watch、
Stream 结果或激活准入，均被图验证拒绝。这些是本地元数据测试，不是协议拒绝流量。
首轮脚本漏列同步模块依赖、删除激活后的测试变体触发严格未用参数告警；已修正，
首轮发现证据保留，不计入上述最终通过，没有放宽产品检查。
提交前清理了驻留测试夹具的一处尾空白；受影响的 Server 专项重新验收，
未受影响的旧 TCP 和语言回归保留原报告，不重写旧哈希或计为重复通过。

完整源码驱动已重建：Windows 766、Linux 765 个编译单元。七份既有 TCP／向量／
文件／receive-into 语言夹具的严格生成 C、JIT、module、独立 AOT 及导入／裁剪检查，
五份语义负例和 O0／O2 封闭 IR 对照均通过。IR 对照每次验证 303 个目标、1330 次
拒绝、11 类合同守卫；仍在 C／头文件输出前拒绝损坏合同。没有修改 Cgen 或放宽类型。
Windows 源码驱动的首次调用误用了旧版 Windows PowerShell，遇到缺少 GetRelativePath；
以 PowerShell 7 在全新目录完成构建，未更改产品或构建脚本的兼容范围。

## 仍需完成

后续更新：配置、受管理资源、统计和八种接受入口已在
[聚合 Server 语言接入](tcp-server-language.md)完成本批实现与专项验收。
以下保留固定底座批次当时的边界，不将原 407 阶段追溯解释为语言 Server 验收。

不可变多端点配置、聚合 `TcpServer` 驻留资源、同步／异步接受、状态与统计及其
语言效果／类型合同和实际产品验收仍未完成；本批只解除它们的固定 ABI／目录前置。
引用／接管发送、完整 UDP／多播、网络文件／任务组合、P0／P1 剩余项及 P4—P7
保持开放。所有验证仅离线检查或自建字面量回环正常功能，没有 DNS、公网、畸形
协议或网络安全性质专项。发布库、VFS 和无关扩展库未改动。

未决策仍为 CRT 0／XRT 11：核心错误与所有权、内存、动态值图、剩余平台信息、
HTTP、WebSocket、运行时类型转换、对象字段图、调用值桥接、typed 容器、内部支撑。
