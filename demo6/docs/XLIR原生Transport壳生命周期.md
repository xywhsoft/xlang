# XLIR 原生 Transport 壳生命周期

2026-09-20，v220 候选。此轮是完整对象拥有图的实际寿命前置修复，
不是 Stream／Listener／UDP 或完整网络服务图已经可追踪的声明。

## 已定位的缺口

TCP Stream、TCP Listener 和 UDP 在 CLOSED 前释放了 Engine 的活动占用及
同一份强引用，但仍把 Engine／Worker 指针保存在存活对象中。
Engine 创建者完成销毁后，这些指针可能悬空；仅补写 Trace 边不能修复它。

同一新增测试针对封存的 v219 单头运行，在真正创建并关闭三个宿主、
完成 Engine 创建者退休后，期望 4 个实际引用，实际只有 1 个。
其中 1 个是测试独立持有的观察引用，保证反证先在计数断言处失败，
不以越界访问或偶然没有崩溃作为判据。

## 本轮实现

保持每个宿主创建时取得的同一份实际 Engine 强引用，不额外增加引用。
`EngineHeld` 只表示 `LiveObjects` 中的运行占用：关闭时先退休运行占用，
然后发布 CLOSED；Engine／Worker 存储引用直到宿主最后一次 Destroy 才释放。
公开 Pin／Unpin 的成对语义不变；未构造成功的失败路径仍同时释放两项职责。

因此关闭对象不阻止 Engine Stop、重新 Start 或创建者退休，
但它仍能保护自身借用的 Worker 存储。保留存储不代表允许提交新 IO，
也不延长线程、Port、缓存或操作系统请求的运行寿命。
没有新增 Stream、Listener、UDP 字段；原有固定尺寸预算不提高。

## 验证边界

新增原生测试覆盖三个真实宿主的六种销毁次序、Stop／重启、
创建者先退休、重复宿主引用、关闭后 Future 的精确终态、
Worker 查询和缓存退休后的分配／归还，以及每例精确内存平衡。
观察引用在关闭后 API 测试之前释放，此后的 Engine 存活必须由实际宿主保证。
实际 RC 由 Engine 拥有视图在全图冻结下读取，不由 LiveObjects 推算。

Select、IOCP、epoll、io_uring 的结果分别以真实执行记录为准。
候选分批验证和最终扩大回归的权威结果为
`build/tmp/xlir_launch_v220_checkpoint/results.json`；在检查点封存前，
不能仅凭此文档认定全部验证已经通过。失败和未运行记录必须保留。
上一轮 v219 检查点和生产头／发布二进制不覆盖。

## 接下来仍必须完成

Socket 包装对象当前在关闭时立即释放；IO 缓冲在完成回调中可能被消费。
直接把旧端口 API 换成 OwnedV1、令 Context 指向 Stream，不能自动证明
这些借用地址直到事件释放的寿命。需要实现真实拥有权转移，并验证
完成、取消、关闭和回调重入的所有释放职责。

Transport 的 callback context、内部命令／Timer、Future continuation、
发送缓冲、TLS／文件宿主及完整服务图仍需接入。
Engine 对 legacy raw-port 的拒绝保持不变；Cgen 中 live network-service
的明确拒绝仍保持。原 18 个生成服务图、语言／FFI、混合寿命、DLL 卸载、
同源发布、平台验收和旧路径退役仍未完成，长期任务保持 active。
