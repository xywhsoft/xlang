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

## 已完成：原生 typed 引用、聚集与混合批量发送

XRT `b893607f81cceadf52010610aa95781fbc4bf59f` 增加 `SendMsgBytes`、`SendMsgPacket`、
`SendMsgBytesVec`、`SendOwnedBatch` 四个入口。前两项支持严格的不可变字节片段；
向量经真实 Socket／Port 聚集为单个数据报，不拼接复制载荷；混合批次用明确
Kind／联合体选择 Bytes／Packet／Vector，所有者及元数据在调用返回前落实。
空片段不增加载荷持有，但完整空输入仍发送一个空数据报，不是空操作。

每个非空片段拥有真实引用，重复 owner 保留重复边。前缀构造只投递一次；后项
失败可以返回 ERROR／AGAIN 加非零 accepted，投递失败则 accepted=0 且内部持有
全部回滚，调用方原引用始终不转移。来源使用明确标签并验证真实槽，不由用户
释放回调反推所有权。Vector 的 Count／Capacity、Span 和 owner 数组仅占节点
尾部，控制记录之后按真实对齐补齐；普通节点 x64 sizeof=104、Tail=96，与父版本相同。

最终 212 个阶段（Windows 99、Linux 113）通过：Select／IOCP／epoll、GCC／TCC
O0／O2、Linux ASan／UBSan、静止整图与外部 payload 根、重复拥有边、8 类图、
9 类正常收发、17 轮前缀／15 个分配预算、24 轮 Close 竞态；均为每个测试产品的
实际计数，不拿阶段数冒充语言端用例数。三种漏载荷边及漏投递失败回滚的独立
对照都成功编译且被拒绝；基础／Future、TCP 字节向量、六个最小 TU 和两种独立
声明根一并回归。共享 xnetbytesref 移到 net.h，TCP 类型与布局不变，不给 UDP
引入 TCP 实现依赖。输入、日志及产物哈希在提交前再次核对。

旧 Future 夹具的未发布构造器回滚先与真实内嵌等待命令汇合，不放宽生产静止
断言。没有错误队列、ICMP、外网或网络安全专项；证据根为
`F:/xlang-validation/five-groups-20261022/p3-udp-refs`。没有同步语言固定头、迁移
驻留 UDP 或旧 Cgen，没有重建发布库／VFS；完整 UDP 与五组目标仍未完成。

## 已完成：固定快照与驻留 Execution UDP 基础

只同步 `net.h`／`tcp.h`／`udp.h` 与 Engine、UDP 的必要内部／源码段，保持固定
微秒时间 ABI、标量域合同及其他未选择差异。两套实现头、两套声明头、快照元数据
和 TCC 符号面一致增加 21 个入口：XRT 3342、xruntime 3807（自有 465）。链接归属
目录从冻结上游重新预处理，3807 个符号均有实际定义和模块归属，不按名称猜测。

新增永久驻留 `ExecutionUdp` family；具体记录只持有实际 UDP 与 Engine 父引用，
不在通用 Resource 中增加不透明载荷。Source 负责 last-owner 自动退役和 checked
重试，显式 Close 不消耗语言 owner。每次原生调用保留物理 receiver scope；
发布失败回滚不把 resident 指针交给原生 callback。图 Finish 只消耗已准备／清除
子节点的真实 creator 和 parent，不再次调用已清除对象的 Abort／Close。

BaseNative 精确登记 Socket／Packet／ErrorPacket 与 runtime／静止策略；Future
域独立登记等待者、producer、owned Cancel Watch、三种 payload 和 Batch。复制、
Take、不可变字节／Packet 引用、字节向量和两种批量发送均直接复用 XRT。批量在
作用域准入失败时也保证 accepted=0；Take 失败不转移。原始 Future 的调用者必须
独立持有 receiver，后续语言 continuation 仍须捕获它并进行 typed 结果投影，
不得把原生 Packet／Batch 指针当成语言类指针。

基础 closure 不选择 UDP Future，异步 closure 才加入相应策略；两者均不引入 TCP、
Resolver、TaskPool 或语言 Future 回调队列。构建响应模板、公开源码驱动和精确
symbol／domain 计划已同步，原有模板无关改动未纳入本批。

最终 221 个阶段（Windows 106、Linux 115）通过，包括 GCC／TCC O0／O2，
Select／IOCP／epoll，Linux ASan／UBSan、真实图的 8 种准备／完成／释放顺序、
普通退役、待收单包／批次／Close Future、取消与完成结果、关闭后拒绝发送、
调用中释放最后 owner、Packet 越过 Engine 的生命周期、24／48 个构造预算，
以及 TCP 基础整图与双声明根标量域回归。两种漏 family／漏 parent 边对照成功
编译并被拒绝，TCP 的三种原负对照也继续拒绝。这里的阶段包含编译／计划检查，
不等于语言用例数量；最终报告保持输入、日志及已记录产物哈希。

证据根 `F:/xlang-validation/five-groups-20261022/p3-udp-resident`，封条
`verified-commit.json`。没有 OS 错误队列／ICMP、外网或网络安全专项，没有提升
发布库／VFS。**本批只是驻留资源服务，不是公开 UDP 标准库迁移完成。**

## 仍必须依次完成

1. 迁移 `udp`、配置、统计、Packet／Batch／ErrorPacket 与完整发送／接收／writable
   公开面。使用精确类字段、状态枚举、效果、期限及取消合同；复用现有 NetAddress、
   数据报元数据／控制类型与 Engine 后端选择，不另造裸指针接口。
2. 补齐受管向量／批量、引用／接管、元数据、能力协商、多播和普通错误通路；
   同步和异步接口都复用 XRT。异步等待使用固定工作线程／就绪队列，不逐等待建立
   线程或协程栈，不因封装而遗漏已批准的底层能力。
3. 清除旧 UDP Cgen 函数体、隐式全局 Engine 和裸 point 转接；Cgen 最终只保留
   声明／调用／拥有清理。不可把当前旧入口有声明或普通案例可运行当成迁移完成。
4. 双端源码 JIT／module／独立 AOT／IR／裁剪／生命周期验收，最终再做发布联合
   验收。检查仅使用离线合同／固定数据及自有字面量回环正常通信；网络安全性质
   专项由用户在 XRT 层完成，不在此处扩大测试范围。

完整 UDP 仍开放，之后继续网络文件／任务组合、剩余并发、非对称密钥、X.509、
TLS 和发布验收。未决策 CRT 0／XRT 11，对象池最后不变。
