# XLIR 原生 Engine 端口拥有图

2026-09-20，v219。Engine 已将唯一拥有的 Port 及 Worker 固定事件槽投影到
实际拥有图，接入原样 XLIR 规划器。这里补齐的是 Engine 的端口边界；
TCP／UDP／TLS 的旧宿主计数、continuation self-drop、完整服务图及原 18 个
生成模块场景仍待迁移，不能据此解除生产 Cgen 的完整网络服务图拒绝。

## 引用与准入

`xrtNetWorkerPortOwnedV1` 只允许在所属 worker 的回调线程借用端口，不允许
转移宿主、销毁、单独 Wait 或跨线程保留。旧 `xrtNetWorkerPort` 仍保留其
不透明标记；新入口若使用任何旧借用型提交，Port 自身也会拒绝图准入。
没有删除旧拒绝来使测试通过。

`xrtNetEngineOwnershipAdapterV2` 分别接纳准确的 task、timer、port 策略身份。
V1 不接纳任何活动 port lease。错误策略身份在执行其 Context Count／Trace
之前拒绝，拒绝不改输出 preparation 或调用方诊断。

Engine 冻结视图读取 Port 的真实拥有链，以及 `EventBatch` 个固定事件槽，
不读取尚未由 Wait 返回的事件数量，也不扫描后端的可变队列。部分就绪时，
端口与已交付事件各持有同一个 lease 的一份真实引用，lease 对 Context 仍
只有一份引用；唯一 Port 存储和 worker 都不是虚构的 RC 节点。

`OwnershipParked` 仅说明没有运行用户回调，不代表整个 Wait 中拥有边不移动。
所有事件发布／引用转移在 mutation 内进行，图扫描与之互斥；回调分发前退出
parked 状态，回调和 Drop 期间不能把正在执行的借用误判为可回收对象。

## 退休准备

只停止线程还不够：待处理 watch／OS lease 仍挂在端口，其 Prepare 会持续
返回 BUSY。现在 Engine Prepare 确认真实 worker／线程清理退出后，先认领并
物理销毁 Port，再允许图 Clear。端口消费槽只在实际退休后移除；线程认领
失败则保留槽，供稍后重试。

退休回调不持有 Engine 生命周期锁或 ownership mutation。整个阶段通过
Joining 拒绝 Engine 图观察；其他 worker 资源和计数外壳保留到后续 Finish，
支持重新验证和析构复活。普通 Stop 的重启语义以及创建者、Pin、独立 Hold
之间的区别保持不变。

## 本轮证据

- 原生聚焦：Windows 三配置各 56/56，Linux 三配置各 41/41。
- 新原生程序让真实后端在已经发布事件、尚未返回 Wait 时暂停，确认固定槽
  和端口待处理边均可见。Windows 每程序 28 场景／20 个图，Linux 32／24；
  覆盖待处理 READ、共享 READ／WRITE、已交付 Post、错误策略、旧借用拒绝、
  BUSY 准备及外线程冻结探针，Windows 另含真实 IOCP 待处理接收。
- 原样 `xl6_ownership_plan.c`：Windows 三配置各 48 个真实循环，含 16 个
  IOCP 待处理请求；Linux 三配置各 64 个 Select／epoll 循环。逐案检验实际
  释放、外部引用保活、错误策略拒绝、析构复活和精确内存平衡。
- 既有 Engine 192、Buffer 96、TaskNet 80、Windows／Linux Port 各 160 轮
  的三配置回归全部通过。新增程序没有替换此前任何一个回归程序。
- 扩大原生回归与最终封存以 `xlir_launch_v219_checkpoint/results.json`
  为准；该报告不会将整个 XLIR 长期任务标为完成。

失败证据保留：最初新夹具的严格 C 缩进警告；第一次端到端夹具将线程离开
Wait 时的可重试快照拒绝当成语义失败。后者依据现有规划器的 ERROR／FAILED
区别修正为同一计划、同一组 pin 的有时限重试，FAILED 仍立即失败，错误策略
反例仍必须拒绝；没有修改规划器、弱化引用计数或把拒绝次数计作通过。

## 未完成范围

下一步是将 TCP／UDP／TLS 和文件宿主的实际引用及释放职责迁移到这套合同，
然后接回语言服务 ABI 和 18 个生成图门。Linux io_uring 的独立 Port 合同已有
此前证据，本轮新增 Engine 专项尚不覆盖 io_uring；kqueue 原生平台仍缺验证。
混合寿命、DLL 卸载、完整语言／FFI、同源发布与旧路径退役仍未完成。
此前两个被隔离的精确 TCC 专项缺口继续保留。

生产语言证据仍为 v209；本轮没有更新四份生产嵌入／声明头、三个发布文件，
也没有切换默认 Cgen。标准库功能仍按最低可用基线处理。
