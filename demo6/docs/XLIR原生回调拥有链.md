# XLIR 原生回调拥有链

2026-09-20，v224 候选。接续传输操作引用和完成事件交接，明确区分
“传输对象还活着”与“它调用的代码及 Data 还活着”。二者不可互相替代。

## 显式绑定合同

原生 TCP、Listener、UDP 增加可选拥有型回调绑定。调用方提供一个实际拥有的
Context 引用，以及常驻 Drop 和精确 Count/Trace 策略；Context 必须保护全部
已安装的回调代码与借用 Data。成功才转移引用，失败保持调用方拥有权。
旧的借用构造仍可用，但借用 setter 不能覆盖拥有型绑定。

每个对象槽持有一个绑定引用，每次回调执行另持一个引用。Worker 上替换绑定
时，先在 mutation 内发布新回调、Data 和绑定，再在 scope 外释放旧槽引用；
正在执行的旧回调直到返回后才失去其保护。绑定活动时 Count 明确拒绝；静止
时报告物理 RC，Trace 必须访问实际 Context 边，不能伪造叶节点或外部根。

Listener 接受的 Stream 共享绑定引用，但不继承 Listener 的 Data。Listener
销毁后，仍存活的 Stream 保留默认回调代码的拥有者。Data getter 保持原有
原子借用指针快照，不改成会追踪已退休绑定的包装指针。

## 并发与固定尺寸

新增绑定指针通过压紧 Worker 私有状态标志容纳，不提高原有尺寸预算。
TCP 两条发送快路径先判断当前 Worker，再读取私有标志。Future 的独立锁域
字段与 UDP 的跨线程只读配置不能和可变 Worker 标志共用存储单元。
常规传输尺寸回归继续保留；这不补写此前未完成的独立工具链布局证明。

## 分批验证

新持久测试涵盖真实 TCP 主动连接、UDP 收发、Open/Read/Receive 中重入替换、
绑定/Socket/对象创建失败、setter 分配失败与错线程拒绝、两 Worker 同时
执行共享回调、Listener 先销毁、真实 Context 边与最终精确内存平衡。

首批发现测试在 backing allocator 注入失败不可靠：逻辑分配可能命中缓存。
改用既有线程局部逻辑分配故障接口，并断言故障确实触发。原批次失败保留，
定向修复证据不能代替最终候选的扩展回归。验收以同批 checkpoint 为准。

## 未完成边界

这些 API 只暴露回调绑定子图，还没有绑定 collection adapter，也没有完成
内部 Control/Command/Timer、Future continuation、缓冲载荷和 TLS 的拥有图。
完整 Engine/服务/传输图、混合生命周期、DLL 卸载及语言/FFI 端到端仍待闭合。
生产规划器、opaque-live-service 拒绝门、默认 Cgen、发布物和生产单头不变。
原生测试通过不等于本次统一 IR 长期任务完成。
