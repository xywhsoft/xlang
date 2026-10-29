# XLIR TCP 父对象拥有图（v235）

本轮把 Stream／Listener 从始终拒绝 Trace 的父节点，推进为具有真实引用图和回收准备协议的参与者。UDP／Dial 父对象、部分发送／分发节点和语言／动态库整体接入仍未完成，不能据此切换默认 Cgen。

## 实现

- Engine 与 TCP 共用真实 Worker 静止检查。必须同时持有拥有关系冻结，并确认 Worker 已停在运行时边界或线程已结束；CLOSED 状态本身不足以证明关闭回调结束。
- 枚举实际 Engine 外壳、Socket、回调绑定、Stream Error、读写缓冲、控制上下文和 Future／Accept 链表头。RuntimeHeld 是真实运行期自持有引用，明确计为一条自身边。EngineHeld 只是运行准入，不能再算一份 Engine 引用。
- 链表 Next 拥有后继；尾指针、Worker、内嵌命令／完成描述、唯一分配的 AcceptSlots 不伪造引用。原始回调／数据没有显式拥有描述时继续拒绝，包括关闭后的对象。
- 直接 IO 的缓冲预留区若新分配一个块，就计入该块实际持有的池引用；若复用现有尾块，则只是借用，不能重复计数。
- Prepare 在冻结域外沿现有 Abort／Close 路径停止原生 IO，等待关闭通知与运行准入退休。初始快照不稳定或调用线程属于该 Engine 时，BUSY 不做任何关闭动作；关闭已经开始却未在五秒内完成时明确 FAILED，不能把已经产生的副作用伪装成 BUSY。
- Clear 只提交清除状态。Finish 在变更域内摘除回调／Error／剩余 AcceptNext，再在域外释放，且可重复调用。真实 Engine 外壳引用保留到父对象物理销毁，以保护仍被发送／Future 外壳借用的 Worker 存储。
- 收集适配器 Hold 独立于公开 Ref 的错误报告路径；拒绝已清除对象时也不分配内存、不改写当前错误。

## 证据与边界

`xlir_launch_v235_parents_plan_final` 已通过 Windows／Linux GCC、GCC O2、TCC，以及 Linux ASan／UBSan／泄漏检查共 7 种配置。未改动的生产 XLIR planner 完成 72 个真实开放状态循环图：包含 Engine 创建者引用、TCP 父对象、回调上下文、Socket、Port 租约及缓冲池。覆盖原生关闭先于语言终结器、外部强引用保活、36 次终结器复活、重复 Finish，以及 108 次零分配拒绝；内存计数精确归零。

同一父对象测试还覆盖实际 Accept 两节点队列、Future 等待链、直接读预留区、CLOSED 回调中的异线程冻结拒绝、当前 Worker 的无副作用 BUSY、旧式上下文拒绝，以及 Engine 创建者退休后的父对象保留。

`xlir_launch_v235_parents_modular` 已完成 Windows／Linux 两种功能裁剪（TCP、TCP Future）的 116 次独立翻译单元编译、4 次链接和 4 次公开 API 运行。它证明了新增独立源码及跨翻译单元符号接线，不等于全部模块化原生测试验收。

最终扩大批次为 156 项测试、8 项裁剪编译全部通过；所有报告哈希由 `xlir_launch_v235_parents_checkpoint_final/results.json` 统一记录。初始失败、修正后的定向批次、前一版本扩大批次，以及 Hold 旧路径的负对照均保留，不能混作最终源码通过记录。

封存时还核实了相对 v234 的非本轮变化：原生 `process_windows.c` 增加 SearchPathW 命中目录的拒绝检查，以及原生 features／single 生成头变化。它们在本轮首个发现批次中已经存在，此后到最终批次哈希一致，单独列为先前快照差异；本轮没有写入、回退或据此认领这部分修改。

剩余仍包括完整 UDP／Dial 父图、发送和接受分发节点收集适配器、混合生命周期、语言／FFI／异步帧与动态库卸载、其余平台边界和最终发布。Windows TCC 32 字节结构体参数与布尔比较组合的独立缺陷没有在本轮修复。DEMO6 的生产 planner、Cgen、生产单头和发布产物保持原样。
