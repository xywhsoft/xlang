# UDP 驻留接入进度与依赖

这是既有 P3 范围的实现记录，不替换或缩小完整 UDP／网络目标。

## 已完成：原生结果拥有权前置

XRT 提交 `d0bf1863459628c5d4419f8df25c134071f20ab4` 新增六个入口：

| 对象 | 原生拥有关系与适配 |
| --- | --- |
| Packet | `xrtNetUdpPacketOwnership`、`xrtNetUdpPacketOwnershipAdapterV1` |
| ErrorPacket | `xrtNetUdpErrorPacketOwnership`、`xrtNetUdpErrorPacketOwnershipAdapterV1` |
| Batch | `xrtNetUdpBatchOwnership`、`xrtNetUdpBatchOwnershipAdapterV1` |

普通包和错误包的数据／元数据独立拥有，不借用生产者 UDP、Engine 或 Future。
Next 只是队列结构，不是包对后继的拥有边。Batch 为每个未取走槽持有实际包；
Take 后的空槽不再枚举。没有伪造引用、复制包包装或猜测用户回调身份。

Ref／Destroy／Take 纳入统一 mutation。BatchTake 使用原子交换，不分配、不复制
载荷；并发取同一槽只交付一次。Count 保留原索引范围，空槽返回 NULL。借用
BatchPacket 仍须排除同槽 Take 并保证批次存活，原子读取不等于取得拥有权。
Freeze 排除其他线程的修改；持有线程可做受控且平衡的嵌套游标持有。

## 验收证据

证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-values`，提交封条为
`native-verified-commit.json`。共 34 个阶段：Windows 16、Linux 18，包含单头
GCC／TCC O0／O2、Linux ASan／UBSan，以及各自最小模块的严格独立源码编译。

每个普通运行进程验证 48 次图关系、9 个离线分配预算、64 次并发 Take（8 次唯一
交付）、8192 次冻结图检查；真实四包 Future 批次含 NUL／零长度，检查空槽、
空批次、单包／批量接收取消、零分配接管及生产者销毁后的载荷存活。错误包只用
实际内部构造器验证，无 ICMP／错误队列探测。漏批次边、漏 Take mutation 的两个
独立对照均成功编译，并被真实图断言拒绝。输入／日志／产物哈希提交前重新校验。

本批仅修改独立 XRT 的 UDP 结果代码和相应生成段；未同步语言固定头，未修改
Cgen、公开标准库或发布 DLL／SO／VFS，不冒称整个 UDP 接入通过。

## 仍必须依次完成

1. 原生 UDP 对象的完整拥有图与静止／退役协议：真实 Engine、运行时持有、排队
   命令、接收队列、发送节点、completion 槽、缓冲池与 Future 等待者。未知用户
   Data、释放回调或裸 socket 逃逸不能直接认证。结果对象图不替代这一层。
2. 原生 UDP Future／取消节点及结果策略：跟踪实际 Promise、Watch、UDP、Batch／
   Packet 持有，保证竞争终态唯一消费；不把 opaque 区域、临时视图或镜像结构当图。
3. 同步必要快照，接入驻留 Execution UDP family 和 Source／Engine 父边；显式
   Close 与自动退役分离，保留 checked 错误及可重试的清理边界。
4. 迁移 `udp`、配置、统计、Packet／Batch／ErrorPacket 与完整发送／接收／writable
   公开面。使用精确类字段、状态枚举、效果、期限及取消合同；复用现有 NetAddress、
   数据报元数据／控制类型与 Engine 后端选择，不另造裸指针接口。
5. 补齐受管向量／批量、引用／接管、元数据、能力协商、多播和普通错误通路；
   同步和异步接口都复用 XRT。异步等待使用固定工作线程／就绪队列，不逐等待建立
   线程或协程栈，不因封装而遗漏已批准的底层能力。
6. 清除旧 UDP Cgen 函数体、隐式全局 Engine 和裸 point 转接；Cgen 最终只保留
   声明／调用／拥有清理。不可把当前旧入口有声明或普通案例可运行当成迁移完成。
7. 双端源码 JIT／module／独立 AOT／IR／裁剪／生命周期验收，最终再做发布联合
   验收。检查仅使用离线合同／固定数据及自有字面量回环正常通信；网络安全性质
   专项由用户在 XRT 层完成，不在此处扩大测试范围。

完整 UDP 仍开放，之后继续网络文件／任务组合、剩余并发、非对称密钥、X.509、
TLS 和发布验收。未决策 CRT 0／XRT 11，对象池最后不变。
