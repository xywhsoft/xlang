# XLIR 原生 Socket 拥有链

2026-09-20，v221 候选。延续 v220 的真实 Transport 壳寿命修复，
本轮把 Socket 和 owned IO／Watch 之间的实际引用纳入统一拥有图。
完整 Transport／服务图和生成模块仍未验收；不能用本轮局部结果解除
`NETWORK_SERVICE_RESIDENT_RETRYABLE_V1` 的 Cgen 拒绝条件。

## 实际寿命，不是补写一条 Trace

Socket 原有的 Close 同时关闭句柄并释放包装对象，不能在完成回调关闭
创建者之后继续保护事件里的 Socket 地址。新的 Ref／Destroy 管理实际
强引用；Close 仍关闭原生句柄并消费一份引用，其他拥有者只保留关闭后
的包装存储。最后一次 Destroy 也会关闭尚未关闭的句柄。

owned OS 请求和每个 Watch generation 各持有一份真正的 Socket 引用。
投递只移动原 lease；两个 readiness 事件共享同一个 lease 时不重复取得
Socket 引用。lease 的 Trace 同时遍历 Context 和 Socket，Drop／Finish
先完成 Context 的释放职责，再归还 Socket 引用；用户释放回调重入时
仍能安全使用该包装对象。提交失败只归还私有 Socket 引用，不消费 Context。

这不授权提前关闭仍有原生请求的句柄，也不把 Cancel 返回等同于完成。
发送／接收地址、User 和回调代码仍必须由真实 Context 保活。

## Accept 是另一份独立拥有权

Accepted 不是监听 Socket 的 lease 子节点。Engine 的固定事件槽在分发前
拥有接受结果，分发后这份拥有权交给完成回调；没有回调则关闭结果。
分发结束时清空已转移的槽，避免下一次 parked-worker 检查读取悬空地址。
独立 Port 调用者仍自行接管或关闭 Accepted；EventRelease 不消费它。

原生 Port 和 Engine 的 XLIR planner 测试现在显式投影 Socket 子节点，
并使用 Socket 的真实 adapter。一个位于图外的 Accept 事件可以同时
保活 Context 和共享监听 Socket；测试按两处实际外部引用计数，不修减 RC。
生产 `xl6_ownership_plan.c` 无需为 Socket 添加特殊回溯或猜测规则。

## 验证与剩余边界

新增测试覆盖关闭后保留引用、最后释放、Claim／Restore／Clear、真正的
OS 完成／取消／Port 退休、共享 generation、提交失败回滚，以及 Context
Drop 中跨线程冻结和引用重入，并逐例核对精确内存平衡。
既有 Socket、TCP／UDP／TLS、Port 和 Engine 回归保留。

Engine 的 IOCP 专项另覆盖真实 Accept 完成事件：固定槽拥有一个实际
Accepted 引用；回调立即 Close 或没有回调由 Engine 自动关闭后，槽必须
清空，逐例内存仍精确平衡。该专项不等同于所有平台的 Accept 验收。

Windows GCC O2 批量检查发现既有 UDP OOM 测试存在同步缺口。原二进制
32 次运行有 21 次失败；仅补诊断的版本同样有 21 次失败，现场均为先前
成功发送的 1 字节、1 包尚未完成，其他 OOM 不变量正确。测试现在在
故障注入前等待 Drain，再投递无分配的 Worker 栅栏，确保完成回调与
节点回收均已结束；失败发送仍须立即回滚预算，没有增加等待或放宽断言。
同一批 GCC O2 运行时对象链接修复测试后，固定 32 次运行全部通过。
原始失败和诊断记录均保留，本次不修改 UDP 生产实现。

候选平台结果以 `build/tmp/xlir_launch_v221_*` 的原始记录为准；
扩大回归、同源性核对及失败记录封存完成前，本文件不是验收报告。
Socket 与 lease 的布局变化要按真实编译结果记录，不能冒充零成本；
Stream／Listener／UDP 不增加字段，原有尺寸断言不提高。

TCP／UDP／TLS 仍有借用式提交；payload、内部命令／Timer、Future
continuation、完整服务拥有图、混合寿命和 DLL 卸载仍需要开发与验证。
POSIX runtime 线程存储退休、原来被隔离的精确 TCC 探针、其他平台
和最终语言／FFI／发布验收仍是独立未完成项。长期目标保持不变。
