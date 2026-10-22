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

## 已完成：基础 UDP 对象图及关闭尾部

XRT 提交 `a6980d1b2230de5a50b044272d2a6cb2c126a534` 增加 UDP 的 Ownership、
Quiescence、Adapter 与 RuntimeOwnership 四个入口。Engine 的真实运行时槽与 UDP
相互持有，UDP 继续枚举池租约、普通／错误包队列、复制／Take 发送、completion
记录和不可变诊断；不把 Active、唯一分配记录或借用 completion 指针算成额外 RC。
来源标签由构造入口决定，外部释放回调不用于推断拥有含义。

短元数据门覆盖真实调用／Worker 帧尾部；operational LiveObjects 与关闭后的物理
存储保活分离，保持 CLOSED 后可以销毁 Engine 的既有合同。关闭尾部可在 Worker
已退役后解绑，无需向失效 Worker 补发命令。Claim 后隔离新工作，Prepare／Ready／
Clear 分阶段推进，无分配的生命周期投递只借用原本存在的 runtime 引用。

本次 86 个阶段（Windows 41、Linux 45）包括基础图完整退役、Creator／计划 pin
精确计数、仍有原生调用时销毁 Engine、回调重入、普通与错误包漏边拒绝、来源
标签、16 个构造预算、结果对象回归、两端最小模块及六个 TU 的严格独立编译。
错误队列只做真实未发布构造器／队列的纯内存检查；IOCP 不支持 DGRAM_ERROR 时
核对 checked Unsupported，不制造网络错误。证据根是
`F:/xlang-validation/five-groups-20261022/p3-udp-owner`，封条 `native-verified-commit.json`。

基础 V1 明确拒绝尚无已注册图策略的 Future 等待者、未知事件／Data 和裸 socket
逃逸；这不是“已经完整接入所有 UDP 异步能力”的声明。语言固定头和 Cgen 尚未
同步，新标准库还不能使用本批接口；没有重建发布库或 VFS。

## 已完成：原生 UDP Future 拥有图

XRT 提交 `34d4721539a9b110017f2fb288c5b092cab2c2d4` 补齐等待者真实 RC、Future
producer、owned Cancel Watch、Packet／ErrorPacket／Batch payload，以及静止检查和
分阶段退役的七个策略入口。UDP 的三类真实等待链现在枚举原来源槽；等待者枚举
实际 UDP、缓存 Engine 租约、Promise、Watch 和预分配空 Batch，不制造 Active 或
结构链接的拥有边。固定策略身份需要独立准入，副本不能代替注册合同。

唯一摘链胜者完成交付；Unwatch 同步之后才移除来源持有，结果拒收也正确释放。
正常 Worker 接收仍复用原小节点缓存；关闭来源或外线程完成／取消在发布 Future
终态之前先分离 operational 缓存租约，保留“完成后可销毁 Engine”的既有合同。
迟到的关闭命令、readiness 和发送挂接的既有引用贯穿帧尾，不在活动帧内释放最后
引用。节点保持 128 字节以内，未引入逐等待线程或协程栈。

最终 161 个阶段（Windows 75、Linux 86）全部通过，覆盖 Select／IOCP／epoll、
GCC／TCC O0／O2、Linux ASan／UBSan、精确整图及外部根、24 种拥有准备／完成／
释放组合、真实竞争 pin、36 个构造预算、48 轮取消／交付／关闭、模式冲突、拒收、
三种结果的离线拥有合同、晚建 Future 和唯一引用的迟到命令。基础图、包／批次和
五个 TU 的最小模块独立编译一并回归。四个新漏边／提前释放对照均成功编译且被
拒绝；提交前复核输入、日志和产物哈希。

错误等待仅使用真实未发布构造器及纯内存队列；未发布 START 状态仍被整图认证
拒绝，不伪造已启动对象，不进行 OS 错误队列操作。没有运行含 ICMP 测试的旧通用
UDP Future／Sync 夹具，没有外网或网络安全性质专项。证据根为
`F:/xlang-validation/five-groups-20261022/p3-udp-future`，封条 `native-verified-commit.json`。

本批没有同步语言固定头，没有迁移 xlang 驻留 UDP 或旧 Cgen，也没有提升发布
DLL／SO／VFS；不能把原生前置通过算作整个 UDP 或五组长期任务已经完成。

## 仍必须依次完成

1. 以已完成的基础／Future 图补齐受管理引用发送的 typed 载荷边。
   现有 copied／Take 路径不替代引用发送，未知用户 Data、释放回调或裸 socket
   逃逸不能直接认证；有声明或基础图通过也不能替代异步图的实际证明。
2. 将已完成的原生 UDP Future／取消及结果策略接入语言运行时的精确准入表和
   注册合同；不得把 opaque 区域、临时视图或镜像结构当图，也不复造一套原生节点。
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
