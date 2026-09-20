# UDP 父对象、发送上下文与统一回收器

本批完成的是原生 UDP 对象图及其与现有 XLIR 回收器的实际接合，不是完整语言、动态库卸载或发布验收。生产 XLIR planner、Cgen 和发布文件均不因本批改变。

## 实现

- UDP 提供规范的物理 Count/Trace、收集适配器和原生 Prepare。父图包括一个 Engine 外壳引用、RuntimeHeld 自持有、Socket、回调绑定、Error、接收/错误包头、发送头和三个 Future 链头。
- Next 是拥有链接；Previous、尾指针、SendReady、Worker、完成槽中的 Send 投影不增加引用。Port 的 IO 职责持有 UDP，而非再持有一次发送节点。
- 接收和错误缓冲显式持有内存池，关闭时 Unit；预热分配失败也归还池引用。完成式 IO 的稳定预留块计入其真实池引用。
- TCP/UDP 共享同一个 Worker 稳定性与 Prepare 协议。开始前不能稳定或正在 Engine Worker 上执行时返回无副作用的 BUSY；请求 Abort 后超时是 FAILED。Close 回调可能复活对象，回收器必须重建图。Clear 不执行 IO，Finish 幂等并在解锁后释放子对象。
- `xrtNetUdpSendOwnedV1` 显式转移一个带类型的上下文引用，空数据报也执行一次释放；拒绝受理不转移。发送节点区分活动、释放中、已退役和已清除，额外物理 pin 不推迟数据释放。发送任务的 pin Drop 与调度器的操作性 Drop 分离。
- 发送节点/任务的收集适配器只接纳精确允许的策略；未退役时 Prepare 返回 BUSY，由父对象的原生准备收敛。原始、未描述的释放回调不会被假装成叶子。

## 验证

1. 本批扩展检测执行 214 个测试配置及 14 个裁剪编译配置。初次扩展批有 7 个失败，全部是旧 Future 用例断言“父图必定不可检查”。修正为稳定状态成功、活动状态拒绝，并加入确定的稳定图检查后，7 种配置全部通过。
2. 当前合成证据为 207 个不变测试结果加 7 个修正后重测结果。复用前检查完整源码哈希，唯一差异必须是该测试文件，并审计其不在被复用测试及 planner 的项目包含依赖中。保留原始失败，不能将原始批次称为全绿。
3. 7 种 GCC、GCC O2、TCC、ASan/UBSan 配置完成 72 次 OPEN UDP 循环引用回收、36 次语义复活、72 个已挂接发送节点的原生退役。复活后中止/重建不重复释放或终结，内存分配释放精确平衡。
4. Windows/Linux 的 UDP 和 UDP Future 4 组独立翻译单元构建，合计 124 次 TU 编译，完成链接并运行公共 API 测试。它们不依赖单头实现的私有字段可见性。

原生用例还覆盖接收/错误包链、三个 Future 头、空数据报、真实任务和节点保留、两处分配失败、回调活动拒绝、Close 中的引用复活、Engine 创建者提前退役及清除后的无分配 pin 拒绝。错误包用真实发布函数注入结构化错误，不宣称生成了 OS ICMP；planner 用同一分配、挂链与关闭实现保留待发送队列，专门验证 Prepare 的实际退役，不把这当作自然网络拥塞。

## 未完成范围

Dial 父对象、TCP SendRefs/SendBuffer/Dial attempt/accept-dispatch 的剩余收集接口，混合服务/TLS 生命周期，语言/FFI/async frame ABI、动态库卸载、POSIX/kqueue 与全部布局质量门，以及新全量回归、生产单头同步、发布、默认 Cgen 切换和旧路径退出仍未完成。已知 Windows TCC 大结构参数比较误编译仍未解决。

最终证据索引：`E:/xlang-goal-artifacts/xlir_launch_v236_udp_checkpoint/results.json`。其中整体 Complete/BatchComplete 必须保持 false；FocusedComplete 仅表示此处明确列出的批次已经闭环。
