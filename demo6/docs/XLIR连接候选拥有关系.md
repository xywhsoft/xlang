# XLIR 连接候选拥有关系

## v233 实现

Dial 候选现在具有真实物理引用计数，并将连接资源结束与对象外壳释放分开。控制槽、正在执行的控制回调及独立保留者分别持有实际引用。连接资源归零仍可及时触发 Done，额外引用只延长对象存活，不延迟连接完成。

Stream.Control 由明确的常驻策略和上下文组成。策略绑定 Open、Published、Close、Enter、Leave、Release 和规范 Ops，不根据释放函数地址猜测拥有关系。控制槽先在变更域内摘除，再于域外执行释放；每次回调单独取得保活引用，避免重入摘除后继续使用已释放上下文。

候选上的 Dial 引用一直保留到物理最终释放，因此 Done 内释放调用方 Dial 引用、或者随后销毁 Engine 的调用方引用，都不会使仍存活的候选借用悬空 Worker。候选 Stream 引用在成功发布时转交给 Dial 完成路径，失败关闭时正常释放。Dial.Attempts／Next 只作索引，不冒充额外拥有边。

Count 在控制回调或退休过程活跃时拒绝快照；稳定状态的 Trace 枚举真实 Dial 和尚未转移的 Stream 引用。完整 Dial／Stream 父对象 Trace 仍拒绝不完整图，本轮没有放宽 Cgen 或生产 planner 的安全门。

已测 Windows／Linux x64 布局中，候选保持 40 字节；控制槽由 40 字节缩为 16 字节。新增状态没有增加候选分配大小，也没有修改公开 API 结构。

## 验证证据

- 发现批次：52 个 Windows／Linux GCC 测试组合及 8 个裁剪编译配置通过。
- 扩大定向批次：128 个测试组合及 8 个裁剪编译配置通过，包含 Windows／Linux GCC、GCC O2、TCC 和 Linux ASan／UBSan／泄漏检查。
- 新拥有关系测试在 7 种配置中执行 54 个真实 Dial 成功连接场景，覆盖 36 次独立保留、36 次嵌套摘除、36 次嵌套帧保活、360 次异线程冻结检查及 108 次不完整描述符拒绝。
- 新测试直接调用公开 Dial 建立连接，在真实 Published 回调中检查控制槽与候选身份。重入情形包含有独立引用和仅有回调帧引用两类；检查 Done 内释放创建者、Engine 创建者退休后保留候选及精确内存平衡。
- 失败、取消、超时、OOM、Future、同步包装及既有 TCP 行为由同批独立回归覆盖。新增私有测试不冒称覆盖所有控制回调的内部状态。
- Windows／Linux GCC O2 对 tcp.c、tcp_dial.c 的 4 次独立翻译单元编译通过；这不等于完整分离链接／运行验收。

扩大批次报告：`E:/xlang-goal-artifacts/xlir_launch_v233_dial_verified/results.json`。
SHA256：`2C15C41C0144013DAC0889E9B1E407D7CE692A4724EB00E33ACA8F03794F3E2D`。
源码从发现批次到扩大批次未发生变化，输入哈希与保护文件核对通过。

## 剩余边界

候选描述符与稳定 Trace 已建立，不代表完整收集适配器或 Prepare 完成。SendRefs 条目仍有未声明外部释放上下文；完整 Stream／Listener／UDP／Dial 父对象图、退出准备、混合生命周期、语言／FFI、动态库卸载与最终发布集成仍未完成。

Windows TCC 大结构体参数与布尔比较组合的独立编译缺陷仍然开放，本批通过不能作为该缺陷已修复的证据。生产单头、XLIR planner、Cgen 和发布文件均未修改，也未切换默认后端。
