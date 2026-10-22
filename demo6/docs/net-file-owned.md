# NetFile 拥有型接入

状态：P0／P3 正在开发。原生借用边界、EOF、拥有型 `net_file_future`、定点
快照和执行资源／拥有目录已落地；语言 NetFile Future 尚未开放，不能将原生
或驻留 C 验收计作完整语言接入。

## 定点同步与受管理执行资源（2026-10-23）

定点同步 17 个拥有型原生入口及其 feature／精确 policy。XRT 公共数为 3369，
xruntime 为 3834（自有 465）；双份实现头、双份声明头、符号登记及派生元数据
同步更新。`backports/net_file_owned.json` 记录原生输入、前后哈希和可逆范围，
不升级其他模块或时间 ABI：独立 XRT 保留毫秒／For，固定快照保留微秒／Until。

新增独立 NetFile execution family、原生 File／操作拥有域和 SDK provider。文件
保留实际父 Engine，最后语言拥有者消失后由 execution source 请求真实关闭、
等待原生终态，再释放父资源；物理关闭失败作为 checked 失败保留，Retry 不重做
已经发生的 OS 关闭。真正不可达图中的原生 File Prepare 复用已有 runtime 槽
排队，不额外增根，不提前释放在途操作，也不伪造取消／完成。

泛化构造失败收尾：Rollback 返回 false 不再 abort 或丢失资源。只有显式授权
deferRollback 的工厂可把无诊断的 false 解释为异步待清理；其余 false 即使没有
分配出错误对象也保留 checked 失败，等待宿主检查／Retry。构造引用转交真实
source 登记槽，原始构造错误仍返回调用者。另修复关停线程在仍有构造 entry 时
过早退出的竞态，避免拒绝发布后的异步清理失去执行线程。

源码构建清单发现 Linux 脚本漏列 90 个既有网络 provider，本批按规范组件表
补齐并新增 NetFile 三个源。四个 shipping 源清单静态门均为 845／845、无缺项
或重复；这只是源清单验收，不等于已重建／验收发布 DLL／SO／VFS。

正式证据根 `F:/xlang-validation/five-groups-20261023/p3-net-file-language`：

| 验证 | Windows | Linux |
|---|---:|---:|
| 原生模块化／生成单头／固定 XRT／固定 xruntime | 24 | 32 |
| 当前全源码 XLIR／execution 图 | 6 | 6 |
| 选中 provider 闭包、真实文件与产物裁剪 | 13 | 16 |
| 快照与陈旧资产拒绝门禁 | 10 | 5 |
| 旧 execution resource 回归及实现反证 | 84 | 86 |
| 合计 | 137 | 145 |

共 282 阶段通过。当前源码驱动器分别重建 889／888 TU，另记、不混计阶段。
原生及裁剪产品通过 GCC O0／O2、TCC、Linux ASan／UBSan；实际 IOCP／io_uring
执行自建文件读写，不用 Unsupported 代替。每个全源码图配置检查 24 轮实际
XLIR cycle、12 次恢复可达、8 次最后拥有者自动关闭、24 次拒绝发布、16 次
checked 清理失败，以及精确分配／释放平衡。旧执行资源的 13 类错误实现均在
成功编译后被断言拒绝。没有应用网络流量或网络安全性质测试。

正式轮：Windows 原生 `native-win-r1`、图 `execution-win-r5`、裁剪
`pruned-win-r2`、快照 `snapshot-win-r1`、回归 `execution-regression-win-r2`；
Linux 对应 r1／r4／r2／r1／r2。最终源驱动器均 r4。输入／日志／产物哈希经
统一复核，Linux 原件及 F 盘逐字节镜像同时保留，提交身份见最终
`verified-commit.json`；准备轮和过期输入轮不计通过。

本批 SDK 的低层 C 入口不是默认标准库公开面。语言 NetFile 声明、受管理句柄
policy、只转换一次的 typed bytes／count／close Future，以及 JIT／module／
独立 AOT 仍待下一批实现。没有修改 Cgen 或用户的 xl6_ir.c 改动，没有更新发布
库／VFS。完整服务图、P4 格式、X.509、TLS、shipping 继续开放，长期目标 active。
未决策 CRT 0／XRT 11，对象池最后。

## 原生拥有型实现（2026-10-23）

独立 XRT 新增 `net_file_future`，17 个创建／引用／定位读写／关闭及精确拥有
family 入口。原有 C 借用 API 保持合同，复用抽取后的内部提交 primitive；
拥有型路径以真实 Engine runtime 注册槽持有文件、文件在途槽持有操作，
Future producer 和 CancelWatch 各持有自己的真实操作引用，不猜测 RC。

构造先核对实际 IOCP／io_uring FILE_IO 能力，再预备 Close Promise、目录和
控制记录，最后打开文件；不支持的后端不会创建／截断。定位 read 返回拥有
Buffer，write 返回实际计数；复制输入和保留不可变输入两个出口明确分开。
读取分配可直接 BufferTake 转交给 bytes，后续语言桥必须只转换一次并缓存
语言值，不能在多次 await 时反复取走同一原生 Buffer。零长、EOF、NUL、
预取消、受理后取消、唯一终态、重复 Close 及最后观察引用都有明确原生合同。

Close Future 在文件物理关闭和 Engine 运行占用释放后才发布终态，可立即 Stop；
Destroy 仅释放引用、不阻塞。原生操作注册和预分配控制命令保持清理所需引用；
语言 execution resource 仍需接入主动 Close，不能将原生目录保活当成语言自动
清理。构造中的 OS 文件打开是同步准备，注册时不得误标 nonblocking。

本批正式证据根 `F:/xlang-validation/five-groups-20261023/p3-net-file-owned`：
Windows `native-win-r7` 12 阶段，Linux `/var/tmp/xlang-net-file-owned-linux-r4`
16 阶段；两端已登记原生借用／拥有型无内存调试版本各 8 阶段，共 44 阶段。
模块化和从当前源码生成的单头分别验证 GCC O0／O2、TCC；Linux 另用
ASan／UBSan。每个调试产品执行 128 个准备分配预算、首次后端分配失败、
8 轮双调用线程提交／关闭、已丢弃观察者的结算、重复 Close、实际 RC／注册
槽及 Drop 中独立 freeze。正式结果含输入／产物／日志哈希，准备轮保留但不
冒充最终通过。不修改仓库已有脏的 single 发布资产。

上述原生实现批次未修改 xlang 嵌入头、发布声明／DLL／SO／VFS、Cgen、语言注册或时间
ABI。下一步必须定点同步新 feature／原生 family，接入句柄目录与 execution
resource，再实现 typed bytes／count／close Future 及 JIT／module／AOT 验收。
上层完整服务图、P4 密钥格式、X.509、TLS 和 shipping 继续开放。

## 已确认并修正的前置问题

原生 `xrtNetFileRead`／`Write` 借用 xfile、缓冲和 xnetcompletion，生命周期由
C 调用者维护。此前它们借用内部端口，但未记录外部完成上下文的暴露，Engine
可能在不知道这些职责时被认证成闭合拥有图。现在成功受理后在回调返回前发布
不透明标记；Worker 活动状态保护发布间隙，mutation 仅覆盖标记而不跨 OS 提交。
失败不污染状态，端口退休后恢复认证，不发明引用
数、影子拥有者或猜测 Completion 内容。

另以独立 Win32 对照确认普通文件 EOF 仍有真实 OVERLAPPED 完成包。IOCP 现在
将这个包映射成成功零字节读取，而非传输错误，操作记录仍保留至完成包取走。
文件 EOF 不等于 TCP 关闭。Linux io_uring 保持同样的 OK／零字节读取语义。

修正只定点同步原生 file／IOCP 实现和声明注释，四份嵌入／声明头其他内容可
逆向逐字节恢复。XRT 3352／xruntime 3817 个公共函数和 465 个 xruntime 自有
函数不变；独立毫秒／For 和固定微秒／Until ABI 不变，没有更新发布库或 VFS。

## 继续实现的合同

1. 原生拥有文件对象绑定指定 Engine／Worker，文件、操作、Future producer、
   取消 Watch 和排队命令都必须是真实拥有节点／槽。OS 完成队列只借用它们，
   原生记录保持到唯一完成事件。受管理路径不能调用会封口的旧借用入口再
   清除不透明标记；应抽取共用验证／提交 primitive，并有独立的封闭拥有 family。
2. 构造前查询所选后端的真实 FILE_IO 能力，并保护 Engine 运行资源；不支持
   时在可能创建／截断文件之前拒绝。不转投 TaskPool，不静默切换后端。IOCP
   和 io_uring 执行真实完成；select／epoll 等保留诚实的 Unsupported 边界。
3. 公开面以 `NetFile` 的定位 readAt／writeAt、显式取消和 close 为目标；默认
   读取 `future<bytes>`，写入返回实际字节数。字符串／bytes 输入按实际字节
   长度和有界 size 选择，内嵌 NUL 不截断。Buffer 若作为输入必须先取得稳定
   拥有快照，不能借用可变数据指针到异步完成。没有文本编码猜测或共享游标。
4. 输入保留语义与传输复制分开：尽可能复用已经批准的不可变 bytes／string
   值和精确数据引用合同，不为每个 I/O 再建语言回调或线程。原生读取缓冲可
   通过 BufferTake／ValueBytesTake 移交数据，避免完整载荷二次拷贝。
5. 调用者、已排队命令、实际在途 I/O、Future producer 和 CancelWatch 的引用
   逐一线性化。所有可失败准备先于受理，失败不消费借用输入；受理后不能丢
   完成、提早关闭文件／回收缓冲，或用一个已就绪 Future 替代真实工作。
6. 取消只请求原生取消，不强行释放已提交资源。若系统完成已赢得竞态，可以
   返回成功；预先取消不执行 I/O。失败不交付半成品读取结果；部分写入已经
   发生的文件变化不能承诺回滚。关闭拒绝新提交、结算所有已受理操作，重复
   close 观察同一个预备终态，不把清理失败静默当作成功。
7. 文件显式关闭后不得再依赖已经退休的 Worker。文件未关闭时占用真实运行
   生命周期；模块／宿主 checked 退休先请求文件关闭，再停止 Engine。失去
   最后语言引用不在任意回调栈中阻塞 join，也不靠 abort 处理正常关闭错误。
8. xlang 对象遵守现有 execution resource／原生句柄目录、typed Future profile
   及错误边界。适配驻留 runtime，Cgen 只处理既有精确声明、调用与清理。
   公开默认库不暴露原生文件指针、Completion、Worker 或短期缓冲借用。

## 验收范围

前置入口为 `check_stdlib_net_file_boundary.py`、`check_stdlib_snapshot_guard.py`，
并回归真实 TaskNet 拥有图／取消／OOM；原生已登记的 net_file 用例另以当前
模块化源码执行。只操作自身新建普通文件和 Engine 任务，不产生应用网络流量，
不做扫描、畸形协议或安全性质专项。

证据根：`F:/xlang-validation/five-groups-20261023/p3-net-file`；最终正式轮次、
计数、提交身份、输入／日志／产物哈希及 Linux 原始证据镜像记录于
`verified-commit.json`。准备轮和被拒绝的实现反证不冒充成功功能用例。

2026-10-23 本批正式 Windows 98／Linux 109，共 207 阶段通过：文件边界各 36，
快照门禁 10／5，TaskNet 拥有图／取消／OOM 回归 48／64，原生已登记文件用例
各 4。边界正式目录均为 `native-*-r4`；其他三项均为 `*-r2`。Windows 的 36
阶段含 GCC／TCC Win32 对照和三个成功编译后的错误实现反证；Linux 四种产品
都运行 ASan／UBSan，并实际使用 io_uring，没有把 Unsupported 当作真实 I/O。
反证只修改正确性标记或 EOF 映射，不构造攻击输入。额外的间隙探针直接在真实
OS 受理后、生产标记之前取独立 freeze，两端 GCC O0 均检查四次独立 freeze
能够受理而 Engine 仍为 BUSY，证明不需要把 OS 提交放进全局 mutation 区间。
每种调试配置实际覆盖
四类成功读写／EOF、五类拒绝／OOM、四次不透明认证拒绝及端口退休后恢复。

TaskNet 回归使用当前两边的真实拥有图与原样 XLIR 规划器，时间合同静态校验
保留；每种配置 80 轮实际图、64 个分配预算及既有预取消／关闭路径。原生已
登记用例还执行写后链式读、跨 Worker 拒绝和唯一取消终态。阶段包括构建和
拒绝反证，不等于 207 个独立语言程序。本批未重建语言源码驱动器或发布产品，
不将这些原生证据冒充尚未实现的 NetFile 语言接口验收。

后续必须用实际语言对象及拥有目录完成 JIT／module／独立 AOT、实际原生
后端、零长／EOF、取消、关闭／最后引用、未 await 的模块退休和裁剪验收。
本轮尚未获得这些语言级证据；完整服务图、P4 密钥格式、X.509、TLS、shipping
也继续开放。目标保持 active；未决策 CRT 0／XRT 11，对象池最后。
