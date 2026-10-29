# XLIR 原生 Watch 拥有链

2026-09-20，v218。readiness 的代次合同已加入 Select／epoll／kqueue 实现。
Windows Select 和 Linux Select／epoll 已有真实内核运行证据；当前机器没有
BSD/macOS 环境，kqueue 原生构建及运行仍未验证，不以共享原语测试替代。
完整 Engine／transport 和原 18 个生成服务图场景仍开放。

## 实际引用与代次

`xrtNetPortWatchOwnedV1` 为非空关注位注册一个新代次；成功消费一个实际
Context 引用，失败不消费新的引用。移除操作使用 Unwatch，旧 Watch(..., 0)
仍保留。Context 必须真正保护 socket、User、回调代码及其访问的数据。

每个代次只有一个 Context 拥有引用。端口和每个独立交付事件分别持有这个
lease 的真实引用，不给 Context 增加虚假的、仅用于满足图检查的计数：

- 部分方向就绪：保留端口引用，为新事件增加一个 lease 引用。
- 最后方向就绪：将端口引用转给事件。
- 同批同输出合并：不再增加事件引用；最后方向合并时仅移除端口引用。
- 替换／移除：退休旧代次的端口引用，不改变已经交付的旧事件。

事件元数据先在私有存储构造，再与拥有槽一起在 mutation 和端口锁内发布。
真实图仅扫描实际拥有链和宿主的事件槽，不扫描被 parked 标志掩盖的私有后端
队列。端口销毁后，尚未释放的事件依然保护旧 Context。

## 错误与回调边界

后端 Watch／Unwatch 显式返回“旧代次是否退休”，独立于操作的成功／失败。
epoll 删除失败或 kqueue 回滚失败时可能已经移除了用户身份；只看 false 会
丢掉释放职责。公共层因此依据实际 retired lease 解除拥有槽，再在锁和
mutation 外调用 Drop。新代次提交失败仍不消费新 Context。

旧 Drop 可以重入并再次替换刚发布的新代次，外层返回前不再访问已退休的新
lease。旧借用 Watch 仍使端口拒绝完整图准入。epoll／kqueue 的唤醒排空失败
路径也保留已经交付的事件数量，避免部分成功中的引用被遗失。

## 验证范围

`test_net_port_watch_ownership.c` 与单头对应程序验证真实分方向及合并就绪、
旧事件跨替换／Unwatch／Destroy 保活、精确图计数、回调重入、非法新代次、
OOM 回滚以及“退休已发生但返回错误”。外线程冻结探针确认释放回调没有继承
API 的 mutation。内存活动量、字节、分配／释放差额和非法释放计数精确平衡。

共享发布原语另有明确标注的单元测试：实际 Context／lease／Port 槽依次经历
首次部分发布、已有输出合并和终态合并。它没有使用 kqueue 内核，不计作
kqueue 平台证据。

原样 `xl6_ownership_plan.c` 的新增场景让 watch 保留 READ，同时已经交付 WRITE，
因而回收器面对的是共享 lease 的真实多重入边。Windows／Linux 三配置各
160 轮，含 80 次外部保活、80 次析构复活、160 次策略拒绝和 400 次释放回调
冻结探针。Windows 保留原 128 个 Post／OS 场景，增加 32 个 watch 场景；
Linux 保留原 96 个场景，增加 Select／epoll 各 32 个场景。

首批聚焦 Windows 三配置各 54/54，Linux 各 26/26。后续候选加入共享发布
原语测试时，一次 Windows TCC 模块化运行在双方向断言失败，其余通道通过。
原夹具只等待发送返回，没有确保接收端已可读；现先以 Available 和有限截止时间
确认接收数据已排队，再保留必须合并两个方向的原断言，没有弱化为允许只报一个。
失败批次及源码完整保留。最终扩大回归 Windows GCC／TCC 各 315/315，保留
上一轮全部 313 个程序；Linux 受影响程序 GCC／TCC／GCC O2 各 26/26。
另对最终 Windows 原始二进制进行 128 次聚焦重复运行，全部通过，未重复全量
回归。端口规划器 Windows／Linux 各三配置 160 轮均通过；已有 Engine 192、
Buffer 96、TaskNet 80 轮三配置回归也通过。证据封存以 v218 checkpoint 为准；
这些原生结果不计作完整 Engine 或生产语言图验收。

## 下一步

1. 补齐真实 BSD/macOS 的 kqueue 构建、部分回滚与同批合并运行证据。
2. 将完整 Port 拥有链及 Worker.Events 的固定事件槽接入 Engine 精确图；
   不能仅删除 `OwnershipPortExposed` 拒绝，也不能把整个 Wait 的 parked
   当作后端停止移动引用。
3. 迁移 TCP／UDP／TLS 的实际宿主计数和旧 continuation self-drop，再接回
   语言服务 ABI 与原 18 个模块图门。

生产语言集成仍为 v209。混合寿命、DLL 卸载、全语言／FFI、同源发布／隔离部署、
其他平台与旧路径退役仍待完成；此前 ResolverFuture／layout 的 TCC 隔离缺口
仍保留。默认 Cgen、生产嵌入头及发布文件未由本轮切换或覆盖。
