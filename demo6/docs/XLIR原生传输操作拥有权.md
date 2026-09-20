# XLIR 原生传输操作拥有权

2026-09-20，v223 候选。此轮把 v222 的完成事件交接合同接到真实 TCP、
Listener 和 UDP 操作上。生产拥有图规划器、live-service 拒绝门和默认
Cgen 不变；操作引用正确不等于完整传输对象图已经可回收。

## 实际引用与载荷处理

每次成功受理的 OwnedV1 操作拥有一个真实传输对象引用，lease 另持一个
真实 Socket 引用。受理失败只回退本次取得的传输引用；接收预留缓冲仍按
原合同取消。Count 读取实际 RC，不把活动对象、操作或回调虚构成外部根。

完成处理先取得独立处理函数引用，再 Take 当前 Engine 固定事件槽并释放
操作。此后才消费或复用缓冲、发布数据回调；回调重入关闭不能使正在执行
的处理函数失去宿主。Accepted 是独立拥有权，必须接管或关闭，不能随
EventRelease 遗失。未增加 Stream、Listener、UDP 或操作槽的固定字段。

迁移覆盖 readiness Watch、Connect、Recv/ReadProbe、SendVec/SendFile、
Accept，以及 UDP RecvFrom/RecvMsg/RecvError 和 Send/SendTo/SendMsg。
TLS 的底层 TCP 受这些路径保护，但 TLS 自己的回调与数据图尚未完成。

查询后端能力改用 `xrtNetWorkerPortCapabilitiesV1`。它不暴露可变 Port，
不会仅因查询就令整个 Engine 不可追踪，并通过 Engine 锁与资源退休排序。
调用方仍须保护 Worker 生命周期；已退休资源返回零而不是悬空 Port。

## 本批验证范围

新测试使用真实 TCP/UDP 收发、主动连接、Accept、直接读取和 probe 读取，
检查实际 lease Context、Socket 和 RC，检查载荷回调前原事件拥有字段已经
清空，以及重入关闭、取消完成、创建者退休和最终精确内存平衡。

操作策略的 Trace 必须明确拒绝当前尚不完整的图，不能把它登记成无子节点
对象。测试中对此有反向断言。Windows Select/IOCP、Linux Select/epoll/
io_uring 分别运行模块化与单头版本，并覆盖 GCC O0/O2 和 TCC。

首轮发现并修复两个验证问题：接受的 Stream 没有设置自己的回调数据；
并行构建日志的 `[build]` 标签粘到前一条输出路径。失败与未运行记录保留，
不把日志映射失败记作运行通过。修复后只先复测相关项，再统一扩展验证。
现有 OOM、缓存、线程和尺寸预算测试不放宽；未变的语言证据按输入哈希复用。

扩展回归又捕获到跨换行的同类日志交错。构建工具现对整条命令日志加锁，
直接命令和响应文件命令共用同一输出边界；Windows/Linux 的并发测试保留
旧实现失败、新实现通过的对照。兼容解析只用于恢复已有历史日志，不能
替代实际执行。已编译但因映射失败未运行的 O2 程序单独执行，不重复构建，
也不把此前未运行的记录改成通过。此日志修复不改变编译参数或运行时源码。

最终验收以 `build/tmp/xlir_launch_v223_checkpoint/results.json` 为准，
本说明和定向通过不能替代扩展回归或完整语言验收。

## 仍须完成

Callback Data、内部 Command、Timer、Future continuation、缓冲载荷和
TLS 的真实拥有边还需要明确策略与完整投影。随后才能验证原生成模块的
完整服务图、混合生命周期和 DLL 卸载，以及语言/FFI 端到端行为。
这些条件满足之前，不解除 Cgen 的 opaque-live-service 拒绝门，不把原生
操作测试当成语言集成完成，也不提前切换默认后端或同步生产单头/发布物。
