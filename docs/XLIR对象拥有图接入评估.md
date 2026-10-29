# XLIR 对象拥有图接入评估

2026-09-20，v219：Engine 新增真实 Port／固定事件槽投影和准确 port 策略
准入，退休准备在真实 worker 退出后销毁端口。原样 XLIR 规划器 Windows
三配置各 48 轮、Linux 各 64 轮通过；旧裸端口拒绝保留。完整 transport／
服务／18 个模块图门仍未解除，详见 [Engine 端口拥有图](XLIR原生Engine端口拥有图.md)。

2026-09-20，v218：readiness watch 使用实际共享 lease 处理部分交付、同批
合并、替换和退休错误。Select／epoll 已验证，kqueue 实现已更新但原生平台
验证未完成；原样 XLIR 规划器 Windows／Linux 三配置各 160 轮，保留原 Post
与 OS 场景。完整 Engine／transport／18 个模块图门未解除，详见
[原生 Watch 拥有链](XLIR原生Watch拥有链.md)。

2026-09-19，v217：IOCP／io_uring 已接入显式拥有型 OS 请求，取消受理不提前
释放引用。原样 XLIR 规划器 Windows 三配置各 128 轮、Linux 各 96 轮，均含
64 个真实 OS 场景；Linux API 受影响批次三配置各 11/11。watch／transport／
完整 Engine 和原 18 个模块图门继续开放。唯一一次 Windows 扩大回归 GCC／TCC
各 313/313，保留原 307 程序，0 失败／未运行，输入未变。详见
[原生 OS 请求拥有链](XLIR原生OS请求拥有链.md)。

2026-09-19，v216 原生补充：端口 owned Post 已建立真实 lease 拥有槽，出队原子
转移给交付事件，Engine 在回调后释放。源级并发审查同时修复不透明标志并发写和
Prepare 冻结区外读取归属的问题，保留真实旧头反例。修复后聚焦三配置各 40/40，
原样 XLIR 规划器各 64 个循环，含外部引用保活与析构复活。扩大原生回归 GCC／TCC
各 307/307，保留原 294 程序，0 失败／未运行，输入未变化。
OS 请求／watch／transport 与原 18 个生成服务图场景仍开放，语言集成仍为 v209。
见 [原生端口拥有链](XLIR原生端口拥有链.md)。

2026-09-19，v215 原生补充：嵌入 Post 和私有 continuation 可明确转移一个
context 引用，执行器分离旧代 Drop 与回调重投；producer/ready 两条实际链
按精确策略和 pending 数投影。完整发现后修复 TCP 固定尺寸回归，原上限未改；
缓冲恢复 56 字节并保留有溢出拒绝的嵌套计数。修复后聚焦三配置各 37/37，Engine 规划器保留原
96 场景并扩至各 192 轮通过。port/completion/transport 与生成模块图仍开放；
ResolverFuture 的 TCC 专项本轮复测仍被隔离，未改安全设置。见
[原生嵌入命令拥有链](XLIR原生嵌入命令拥有链.md)。

下一批端口/完成记录的源级阻断点已经核对：

- `xnetport` 当前并非 RC 对象，用户 Post、watch 和 Submit 的 User/Socket/缓冲
  仍是借用；不能只给 User 附上 Ops 就声称 OS 仍访问的地址受到真实拥有引用保护。
- Worker 在整个 `xrtNetPortWait` 期间标为 parked，但 Wait 会摘取 Post、调用后端
  提取完成记录并填写 Worker.Events。后续完整图必须明确阻塞等待与所有权移动的
  冻结边界，不能直接沿用 parked 判断扫描变化中的后端链和事件数组。
- 后端队列到交付事件，再到回调/释放，必须有一次明确的引用转移。取消成功不等于
  IOCP/其他后端已不再访问请求缓冲；返回事件尚未派发时也不能出现拥有边空档。
- 当前 PortDestroy 即使 Driver.Unit 返回 false 仍会释放端口核心，不能直接当作
  通用图 Finish 的可重试退休接口。需要区分物理排空状态与错误报告、确立消费点。
- TCP/UDP/TLS 的旧内部 continuation 手动 self-drop；迁移须与实际宿主 Count/Trace、
  OS 请求/缓冲、回调代码寿命和取消尾部一起完成，不能再叠加一次 policy.Drop。

这些项是运行时语义与完整模块拥有图的依赖，不是扩展标准库功能。当前保留
`OwnershipPortExposed` 拒绝边界；18 个模块图场景仍不因此转绿。

2026-09-19，v214 原生补充：池和字节结果已采用真实引用，唯一嵌入缓冲直接
投影实际 allocator/block/context 拥有槽；Engine Prepare 保留 live bytes，
Finish 退休池 creator。聚焦三配置各 68/68，原样 XLIR 规划器各 96 轮验证
回调复活、独立池保活及两种物理释放顺序。完整 port/transport／生成模块图
仍开放，语言生产集成仍为 v209。见 [原生缓冲拥有链](XLIR原生缓冲拥有链.md)。

2026-09-19，v213 原生补充：网络任务已统一 Post/Timer 的实际 Frame，补齐
Engine 外壳、Job、Data 和 cancel Watch 拥有槽，将可失败装配移到受理之前。
聚焦三通道各 32/32，原样 XLIR 规划器三通道各 80 轮，包括 owned payload、
取消／关闭、32 次错误策略拒绝和 40 次复活后重建回收。完整服务／模块图和
语言集成不因此转绿。见 [原生网络任务拥有链](XLIR原生网络任务拥有链.md)。

2026-09-16，v212 原生补充：Engine 已分离真实强引用与 LiveObjects，提供
拥有型命令／定时器和分阶段退休合同。聚焦 GCC O0/O2、TCC 各 21/21；
未修改的 XLIR 规划器在三通道各完成 96 个真实拥有循环，包括 timer 入堆、
错误策略拒绝和复活撤销／重试。裸 port、存活 pool block、嵌入式命令及
transport 图仍未认证，不解除原 18 个模块图门。语言生产集成仍为 v209。
本次 Engine TCC 通过不替代 v211 ResolverFuture 被隔离的专项。详见
[XLIR 原生 Engine 拥有链](XLIR原生Engine拥有链.md)。

2026-09-16，v211 原生补充：实际 Resolver Future 的 producer、Promise、request、
cancel watch 和 AddrList 已进入统一拥有合同。接入真实 XLIR 规划器发现并修复了
退出后无法重新采集的 Join 循环等待；聚焦三通道各 15/15，规划器 GCC O0/O2 各
96 个真实循环通过，TCC 规划器产物被 Defender 隔离仍未验收。语言生产集成仍为
v209，完整 Engine/服务模块图及发布边界未改变。详见
[XLIR 原生 Resolver Future 拥有链](XLIR原生ResolverFuture拥有链.md)。

2026-09-16，v210 原生补充：Resolver、ResolveOp 与地址列表已提供精确物理图、
认证 context、mutation/冻结边界及分阶段退休；GCC/TCC 原生统一批次各 139/139，
旧 129 程序保留。DEMO6 未同步本候选，原生 Future producer/bridge/cancel 和
Engine 完整图仍待接入，18 个服务模块图门不因此转绿。详见
[XLIR 原生 Resolver 拥有图合同](XLIR原生Resolver拥有图合同.md)。

当前状态：2026-09-16，v209。原生服务可重试退休已通过显式 IR、常驻服务槽、
V4 宿主和通用对象图 typed Finish 接入。聚焦检查已通过；唯一一次统一 44 门
43 通过、1 个检查器计数失败，0 未运行，818.297 秒。检查器修正后该门专项
复验通过，未修改生产源码或重复其余 43 门；4233 个批次输入前后相同。
原 18 个反例的服务拥有指针与重试清理全部修复，无宿主救援；完整服务 trace
仍拒绝，原门继续红色，不将退休成功当作拥有图完整。详细证据和边界见
[XLIR 原生服务退休合同](XLIR原生服务退休合同.md)。

上一封存完整语言批次为 v207：
原生 Future 映射已使用可验证 XLIR 合同及
常驻 callback/payload，网络 class 的析构与身份回调随对象保留代码能力。
新门已验证 600 次真实适配器 DLL 卸载，包括 50 次 Future 释放后的独立结果
对象；6 个网络程序的原公开生命周期检查保持通过。唯一一轮统一复测 41/41，
0 失败、0 未运行，671.113 秒；4219 输入前后指纹一致，工程检查 13/13，
原 40 门、35 契约驱动和 17 份逐字节相同的独立程序生成 C 全部保留。
native/network 完整服务图、混合对象生命周期、全语言/FFI、同源发布/隔离
部署、平台及旧路径退役继续开放。

### v208：服务销毁的真实 BUSY/ERROR 与拥有权提交点

六个实际保存 AOT 程序的 18 个公开生命周期探针全部构建成功但仍未通过：完整服务
trace 均拒绝，12 个真实 Pin/Resolver 自身回调退出场景还丢失了清理失败后的服务
拥有指针。宿主在记录后救回资源，救援不计为通过。原失败记录完整保留。

独立 XRT 源码增加共享 READY/BUSY/ERROR 退休合同，只有 READY 消费创建者拥有权。
同步与非等待接口共用停机与真实 join；线程执行体和 TLS 尾部退出前不得释放服务。
真实不收敛停机仍报告 ERROR，物理重试不会抹掉已有错误。GCC/O2/TCC 的模块化与
单头聚焦通道均通过；唯一一次扩展原生批次 GCC 129/129、TCC 129/129，原有 110
个程序保留，2020 记录输入前后不变。具体数据和接入边界见
[XLIR 原生服务退休合同](XLIR原生服务退休合同.md)。

尚未修改 Cgen 生产源码或 DEMO6 嵌入/声明头及发布物，不将原生专项通过视为 18 个
生成模块场景转绿。下一步须接入显式 IR 服务合同，分开语言 finalizer 与物理退休
状态，并令宿主区别 BUSY 和语义失败；原生 producer/取消/句柄完整拥有图仍待建模。

### v207：原生 Future 映射、独占取消与结果对象代码寿命

旧 v206 实际生成的 TCP 和 resolver 适配器在受控真实 Promise 上均未持有
pending 回调所需的代码引用；地址列表结果也未独立保护生成的身份/析构函数。
原失败探针保留。修复不靠延长所有模块的全局寿命：Lowered XLIR 从精确
runtimeBridge 身份派生 nativeFutureMapAbi，由验证器和产物依赖计划共同
约束 Cgen。25 个缺失/错误合同在 IR、C/header 发射前拒绝；不扫描 C 名称。

FutureCallbackRuntimeV3 为独占生产链增加 certified CancelWatch，注册实际
持有 frame，frame 实际持有注册/取消令牌。取消源可能同步重入完成和清理，
因此在锁外调用、保留独立源引用，最后的 frame/代码能力释放留在常驻路径。
GCC O0/O2 和 TCC 每路均验证 100 个精确 pending 图、200 次源取消、100 个
非独占对照、6 个分配失败回滚、1000 次取消/完成竞争及精确内存平衡。

TCP/resolver/TLS 共用一个明确的映射合同，TLS 的 XLIR 路径不再转换成旧
Future result shim。标量和 xvalue 载荷使用常驻 policy；网络 owning class
通过 ClassRuntimeV2 创建、绑定身份、提交。真实生成适配器在 O0/O2 下共
600 次 Close 后完成与真实 DLL 卸载，含成功/失败/取消/关闭四终态、真实
地址列表、真实 TLS 拥有型上下文，以及 50 次独立保留结果对象。地址释放
确认 MEM_FREE，内存精确平衡。拥有型 TLS 上下文仍显式拒绝未知原生图准入，
没有伪装成 trivial capture；这些探针不证明活动 engine/resolver 整模块卸载。

发布环境与 v206 有差异：v207 首次构建即记录当前 xl.dll，另一个 xlc.dll
也不同。原 baseline 和失败工程报告不覆盖，补充环境记录保存当前依赖；两个
旧 DLL 均找到哈希匹配副本并保留。未回写发布目录、未修改 XRT 或四份嵌入/
声明头，不宣称发布物与 v206 相同或同源发布已通过。详见
`docs/XLIR原生Future映射合同.md`。

最终批次输入指纹为
`9E7CE87321D1A16600AF5A66BB7A521DDF9A4BF894F6FE847559849BE2E7A458`。
6 程序的 30 项 JIT/AOT/EXE、30 次同进程公开生命周期及 32 次真实停止
worker 提交拒绝保持通过，17 个独立产物的 68 阶段和 48 个适配器也保留。
当前 Cgen .c/.h 为 5075261 LF 字节，含 .inc 为 5095448，均比 v206 减少
963；开发 CLI 为 9639895 字节，增加 335，不能将小幅源码缩减宣传为显著
二进制缩减。公共源码构建 69 TU、68 命中、17.604 秒；35 契约驱动共享
23 TU、19 命中、20.734 秒。原生 2014 输入和旧 110/110 只重核沿用。
最终证据入口为 `build/tmp/xlir_launch_v207_evidence/results.json`。
体积按原 UTF-8 字节计入既有 BOM、仅统一 CRLF 为 LF。初次文档统计使用
ReadAllText 自动剥离一个未修改文件的 3 字节 BOM，已按封存报告的原始字节
口径更正；文档补正见 `build/tmp/xlir_launch_v207_documentation_addendum/results.json`，
未修改源码或覆盖原封存，也没有重新执行统一测试。

### v206：嵌入事件生命周期与错误调用的语义边界

集中发现先保留原 5 个网络程序。隔离工作目录中的 JIT 缺少 tccdefs.h，
原因是开发源码构建关闭了内嵌 VFS，而探针只设置部分磁盘工具链路径；补齐
明确的 include、wininclude、winapi 和 libtcc1 配置后，5 程序的 JIT、AOT
及各 3 次 EXE 执行全部通过。这是测试提供者配置修正，不冒称修改了编译器
部署解析或通过了共享发布/完全隔离部署验收。

随后将实际保存的 AOT module.c 编译进常驻代码宿主，反复调用其真正的公开
main，而非只调用片段私有 fini。5 程序各循环 5 次：4 个正常，TLS 每次记录
4 次非法释放，共 20 次；虽然功能断言和最终活跃字节均为零，仍判失败。
保留的原生调用栈将问题定位到 xrtEventDestroy 释放栈内 command.Done。
事件是由 Init 初始化的调用方内嵌存储，只能 Unit；Destroy 属于 Create 的
配对接口。共享 TCP/TLS 命令及 WebSocket attach/command 的同类路径统一
修复，后两者另补齐 EventInit 成功、PostInit 失败时的资源清理。XRT 本身
的事件接口没有错误，本批未修改独立 XRT 或嵌入/声明头。

新增 TCP peek/consume 正例，最终 6 程序每个完成 JIT、AOT、3 次独立 EXE，
以及严格 GCC 宿主内 5 次公开生命周期。逐轮检查模块、executor、engine、
resolver 关闭，非法/重复释放、越界和释放后使用计数不变；最终在线程存储
正式退休后，活跃数量/字节为原值且分配释放精确平衡。首轮数据也记录，不靠
暖机隐藏增长。TCP/TLS 另各向真实已停止 worker 提交 16 次，要求 CLOSED
错误、engine 正常销毁、Windows 句柄逐次平衡。WebSocket 同类修复仍只由
共享源码审计及原 48 适配器严格编译覆盖，不宣传协议运行或其完整拥有图验收。

新 TCP 探针起初误用了 TLS 的双参数 send 写法；原失败文件和日志保留，
正例改为 TCP 的规范单参数。该错误调用同时暴露了真正的前端缺口：未选中
可调用目标的 nominal class 调用仍可能越过语义层。11 个集中反例中，旧实现
8 个拖到 IR 才拒绝，2 个甚至通过 Typed IR，另 1 个已有语义错误。最终调用
边界现在在方法、函数字段及 __call__ 路径均未绑定目标时报告带位置的语义
错误。全部 11 反例均在 sem 阶段、零 IR 节点时拒绝；合法重载、默认/命名/
可变参数、继承、泛型、函数字段、动态和 async 调用保留严格 C 与实际运行。
没有放宽 IR 验证器或让 Cgen 从缺失类型重建调用。

统一批次仅运行一轮，40/40；4213 输入前后均为
`59CB9911F0FDFE077CC40D67B65593A0D52DE6756D68545F5D0EA73D6E2D75E3`。
原 38 门、35 契约驱动和原用例覆盖保留；17 个独立产物的 68 阶段及
48 适配器继续通过，原 17 份生成 C 与 v205 相同。最终源码构建 69 TU、
68 命中、16.92 秒；契约驱动共享 23 TU、22 命中、16.15 秒。Cgen .c/.h
为 5076224 LF 字节，含 .inc 为 5096411，均增加 316；开发 CLI 增加 1536
字节至 9639560，发布物不变。工程检查 13/13（编码扫描 6600 个文件）。
原生 2014 输入及旧 110/110 证据仅重核沿用，未重新执行。

常驻宿主的资源平衡不等于动态代码可卸载：网络模块 trace 仍明确返回 opaque，
原生服务拥有状态、mapper/payload 析构回调的代码保活及混合图准入尚须建模。
最终证据入口为 `build/tmp/xlir_launch_v206_evidence/results.json`。

### v205：原生实现需求、C 返回 ABI 和装箱拥有事务

244 个 C 实现需求字段与 54 个显式原生组共用一份元数据。C 输出和产物
计划都先验证 Lowered XLIR，再运行同一实现选择；产物包装的固定公开 ABI
另有明确基础依赖。JSON/XLON 模式、通用 value 格式化、共享网络生命周期
及 TLS 证书转换的依赖属于实现组合同，不扫描生成文本，也不在链接失败后
逐符号补选源码。组对应实际发出的支持组件，尚不承诺全局最小原生体积。

严格 C 检查集中暴露了两类 ABI 缺口。12 个路径原生函数的 char* 返回现在
由 IR 的 resultCAbi 表达，并核对参数数量、借用、返回布局和转换；Cgen
只消费该事实，32 项错误签名反例拒绝。共享 HTTP/WebSocket 支持体通过
显式模板 ABI 使用 XLIR 的真实 void 返回或 legacy 的既有整数哨兵，不做
生成文本替换。此处只验收规范声明，不宣传完整任意 FFI 体系已经闭合。

持久化字符串装箱暴露的拥有图错误确实来自异常路径：源字符串被锚定，
但装箱分配失败时旧 Lowering 漏清理。修复将拥有源以及后续参数求值期间的
装箱结果纳入统一求值事务，复用既有错误 CFG 和所有权清理规则，验证器未
放宽，Cgen 未追加清理特例。两个格式的 192 项运行覆盖 64 个真实分配失败、
64 个后续参数抛错、64 个成功；每项检查活跃对象/字节、分配释放平衡和非法
释放。网络适配器只做严格编译和真实符号闭包，不冒充网络生命周期运行验收。

17 个独立程序通过生成、JIT、AOT 和 EXE 四阶段共 68 项；实际保存的 AOT
原生实现与计划生成内容逐字节相同。原 15 份生成 C 与 v204 全部不变。
新增门保留集中发现的全部用例：46 个适配器有 31 项失败，按共同根因修复后
扩为 47 个、只剩装箱生命周期 1 项失败，再补齐事务修复和运行探针至 48 个
全部通过。原独立产物发现的 4 项 AOT 失败及 4 项未产生 EXE 的执行失败、
工具自身的失败日志，以及已生成报告阶段的精确输入版本均保留。

工程检查 13/13。当前源码构建 69 TU、68 个命中，17.166 秒；35 个契约
驱动共享 23 TU、19 个命中，255.32 秒。Cgen 的 .c/.h 为 5075908 LF 字节，
但加上移入 .inc 的显式描述后合计 5096095，比 v204 增加 15956 字节，不能
将字段搬移宣传成代码缩减。开发 CLI 增加 37645 字节，发布 DLL/EXE 未更新。
独立 XRT、四个嵌入/声明头未改；2014 原生输入及旧 110/110 证据仅重核沿用。
设计见 `docs/XLIR独立产物依赖合同.md`；最终统一证据为
`build/tmp/xlir_launch_v205_evidence/results.json`。

### v204：精确原生链接身份与可选择模块证明

目录从产品单头和两个实际模块清单生成，不手写 3484 个函数的模块判断。
402 个根与无选择核心分别由真实预处理核验，并依据启用功能闭包求最小
提供者。这样能处理共享源码、无独立源码的附加模块、括号保护的分配器
定义和宏生成的类型描述符。当前 3431 个符号为单根，2 个有两个最小提供者，
51 个属于无条件核心；附加可选行为仍由操作/桥接或包合同声明。

生产计划只查询可达 EXTERN_C 目标的精确链接名，不将字符串、未调用声明
或近似名字视为依赖。公共对象导出 3484 项逐一核对；查询覆盖全部名字及
3484 个近似反例，另有七项提供者边界检查。新增语言程序在原 v203 CLI 上
Cgen/JIT 成功但 AOT 明确缺 `xrtEncodingUnitSize`；本批通过 CHARSET 的正确
归属修复该缺口，实际 EXE 与严格生成 C 编译通过。

集中发现中的源文件归属不足、括号/无注解定义、附加模块，以及验收脚本的
单元素数组取值问题均已保留。Cgen 生产文件未改；完整原生需求迁移尚未
完成，RuntimeOp/桥接依赖与旧扫描退出仍是下一项工作，不宣称全面无回溯。
原 14 个程序的生成 C 全部逐字节不变。源码构建 69 TU，67 个缓存命中，
15.969 秒；35 个契约驱动共享 23 TU，全部命中，14.263 秒。工程检查 13/13。
Cgen 保持 5080139 LF 字节；新增目录属于编译器元数据，开发 CLI 增加
143521 字节，不冒称发布 DLL 减小。独立 XRT、四个嵌入/声明头及三个发布
二进制未改，旧原生 110/110 结果仅在重核输入后沿用，没有重复执行的宣称。
设计见 `docs/XLIR独立产物依赖合同.md`；最终证据为
`build/tmp/xlir_launch_v204_evidence/results.json`。

### v203：验证后产物依赖清单与源码 CLI AOT

新产物计划只接收 Lowered XLIR 和明确的产物入口 ABI；先验证，再收集模块
能力、可达指令、物理类型和已解析 C 链接身份，最后完成常驻及原生依赖闭包。
Typed、损坏 IR、未知入口或工作区资源失败均不能留下部分清单。checked
module/executable 的入口错误作用域与用户函数的可抛出效果分别表达。

23 个常驻组件的描述表明确源码、组件依赖和原生根模块；172 个精确导出
身份与真实对象逐项核对，组件的未解析私有符号必须处于声明的闭包内。
UTF-8 count/at 的 RuntimeOp 明确增加 Unicode text 根，避免将标量索引
文本操作等同于普通字节串。源码装配不再扫描生成 C 或追补链接报错。

原五个 CLI 样例加九个语言程序共 14 个，Cgen/JIT/AOT/EXE 56 项全部通过，
包括循环 cell 捕获、nullable callable、闭包拥有、类、generator 和 channel。
新增契约测试检查 529 个双根闭包、14 个程序的 Typed/损坏 Lowered 拒绝、
确定性和伪前缀反例；9 个分配预算中 8 次失败、1 次成功，内存精确平衡。

集中发现期保留了入口错误作用域缺项和 UTF-8 原生根缺项。按组修复后，
轻量 14 程序复验通过，再进行唯一一次完整 36 项复测，耗时 495.194 秒。
4193 输入前后指纹均为 `F90279AE54E01854E865CE126FD8BD90AF01086D540D84EBEC5028298FB70D14`。
源码构建 69 TU（68 命中）15.121 秒，35 契约驱动共享 23 TU（全部命中）
14.348 秒。五个原样例生成 C 与 v202 完全相同，Cgen 仍为 5080139 LF 字节。

普通语言/XRT 产品裁剪中的历史 AST/生成 C 文本分析尚未全部迁移；legacy
选源函数也仅隔离而未删除。发布物、独立 XRT 和嵌入头本批未修改，当前
开发目录源码 CLI 结果不表示共享发布 xlc.dll、隔离部署或 Linux 已验收。
设计及限制见 `docs/XLIR独立产物依赖合同.md`；本批证据入口为
`build/tmp/xlir_launch_v203_evidence/verified_results.json`。

### v202：共享多输入等待证明与真实生成模块退休

单输入 continuation/callback 描述与原生组合器的完整有序输入，共同进入一个
常驻求解器。先求最大阻塞集合，再判断当前 producer 是否确实处于该集合的
等待环中。ALL 有一个必需输入阻塞便受阻；ANY/RACE 必须所有选择都受阻。
未知或已完成输入按可能推进处理，重复槽不去重，通向环但不在环中的 feeder
不能获得中断许可。捕获/拥有边不是等待边，不能拿一个输入代替组合器规则。

描述仍要求完整物理图冻结、独立准入、同一不可达 claim，以及真实队列准备。
求解器只借用图，不取消 Future、不移除 Watch、不执行用户回调。小图使用本地
工作区，大图可增长；资源不足返回独立结果，保留原诊断和图状态。实际
continuation producer 此时只返回 BUSY，不锁存语言失败；仅肯定环成员证明
才进入原有的 frame 保活与协作取消流程。原生组合器本身仍不由描述自动获得
取消/清理权限，所有源回调、catch/finally 和后续 cleanup await 继续实际运行。

新增独立算法对照直接编译生产求解器，以“最小可完成集合 + 稠密传递闭包”
作为另一实现的判据。GCC O0/O2、TCC 每路检查 66064 个一至三节点完整图、
197648 个根判定，包含每种 ALL/ANY 组合、未知输入和重复边。另有 96 节点、
768 边增长与 16 个分配预算（8 个真实失败），检查全部回收和精确内存平衡。
此算法测试不伪造 provider、claim 或取消许可。

生成模块专项覆盖字面量/数组 ALL、RACE、ANY 六种形式。四条真实 TCC/DLL、
宿主 O0/O2 路径每路完成 170 次实际卸载：72 个环、24 个 feeder、24 个无环
对照、32 个原快照预算事务，以及 18 个模块 Close 后成功/失败/取消完成事务。
每路保留 128 个外部根检查、48 个 cleanup-await 再生根、704 次回调 finally；
检查原 async catch/finally、完整 ALL 结果、ANY 索引、全局析构和实际 MEM_FREE。
成功 ALL 装箱也在模块关闭后执行，不能仅靠取消路径证明 mapper 代码寿命。

其中 80 个真实模块图通过宿主完整解析器逐节点准入；172 次未准备队列拒绝、
172 次错误 claim 拒绝，随后证明 160 个环成员并拒绝 12 个 feeder。另核对
172 个原生等待视图的完整顺序、重复/已完成槽、ALL/ANY 规则和仅 Race 取消
其余输入。108 个证明分配预算中 12 次资源失败，实际 producer 的 12 次 OOM
准备均只 BUSY；诊断身份、取消状态及用户清理次数不变。原生工作线程停驻
转换导致的拒绝只限时重取完整快照，未知节点或语义失败不转为接纳。

完整批次实际执行三轮：35/35（485.085 秒），补强输入事实及关闭后完成后
35/35（488.631 秒），更新仍匹配旧单输入函数名的工程静态守卫后最终
35/35（489.185 秒）。每轮输入前后相同；最终 4188 个输入指纹为
`DFF28381BA7F26F80A3C3994B85BA468F3978283AB47A73DDE6D179CE91856E7`。
工程检查从 12/13 收敛至 13/13。发现期样例语法、const/缩进、队列准备前提
及原生池临时准入失败的日志和源版本均保留，未删除旧人口或放宽语言断言。

同源编译器 69 TU 构建 16.117 秒（66 个对象缓存命中），35 个契约驱动共享
23 TU，构建 14.576 秒（23 命中）。Cgen 未改，仍为 5080139 LF 字节；修复在
共享常驻求解器。嵌入头及三个发布二进制未改，原生 110/110 证据仅在重核
2014 输入后沿用。O2 路径优化本批常驻求解器/producer 与宿主，原生依赖对象
来自固定 O0 构建；不冒称原生全量 O2 或 Linux 执行。
证据入口：`build/tmp/xlir_launch_v202_evidence/results.json`。

下一批已开始集中发现：真实源码 CLI 壳链接当前编译器/运行时对象，在明确
TCC provider、私有声明头及源码部署后，五个核心样例的 Cgen/JIT 共 10 项通过，
AOT 五项仍分别缺 Slot、Closure 或 CodeLease 常驻符号，五个可执行产物未生成，
不计为执行通过。`xlAddPrivateLanguageRuntimeSources` 尚沿用 C 文本前缀与少量
旧标志选源，未覆盖新常驻实现的完整传递依赖。下一步应从已验证 IR 能力和
共享运行时依赖合同形成独立产物清单，不逐个追补报错符号。CLI 探针使用开发
工作目录及明确 provider，不代表已验证发布 DLL 或隔离部署。
证据：`build/tmp/xlir_launch_v202_cli_staged/results.json`。

### v201：验证后的聚合 ABI、常驻实现与真实 mapper code owner

`futureAggregateAbi` 从六种实际 lowered RuntimeOp 独立计算。Typed IR 不得
提前声明能力，lowered IR 不得缺失或伪造能力；无聚合模块也不得添加该能力。
Cgen 只消费已验证的模块/结果 ABI，调用 `xl6FutureAggregateRuntimeV1Get`
函数表。数组拆包、边界检查与结果映射移到现有 `xl_future_bridge.c`，继续
复用一个原生 ALL/ANY/RACE 引擎，不再在生成 C 内重复展开这些实现。

每个映射上下文是实际计数节点；有生成 boxer 时，经代码能力接纳后持有
一个真实 lease，lease 再拥有宿主模块。Box 执行期间进入实际 CodeScope，
释放发生在回调返回后的常驻尾部。同线程已接受的能力允许在 Close 后继承，
其他线程不能借用它。void All 不伪造代码引用。旧裸 boxer 接口仍兼容，但
不因此取得可回收认证；NULL owner 只表示调用方明确保证代码常驻。

宿主独立接纳原生 producer、投影 Watch、拥有型取消观察器、映射上下文和
代码 lease；识别组合器不等于自动认可任意 Data/code。原生组仍对 pending
返回 BUSY；本批没有把一个输入冒充 ALL/ANY，也没有据此授权取消或释放。

原聚合专项每路保留 3500 个生成事务、192 个 OOM 预算与 36 个 IR 变异。
新增 9 个模块 ABI 变异都由 verifier、C/header 输出门拒绝，并核对 text/JSON。
GCC O0/O2、TCC 三路均通过；五个原语言 fixture 的确定性 C/header/IR、公开
run/module 执行均通过。新增真实上下文令准备失败预算从 34 增至 40，接受后
映射失败仍为 8；只更新物理图/分配次数预期，不删除源错误、取消、关闭、
别名、结果顺序或精确内存断言。pending 物理图由 9/18/1 变为 10/21/0。

代码拥有专项每路完成 100 次真实 DLL 父/子映射生命周期、100 次跨线程拒绝、
500 次 scoped boxer、200 条精确 context→lease→owner 关系、300 个独立准入
物理图、100 次旧策略拒绝。另有 128 个 owned mapper OOM 预算，实际准备
失败 26、接受 102；全部精确核对引用、内存数量/字节与 DLL 实际解除映射。
O2 优化该路常驻实现及 boxer，原生对象来自同源 O0 构建，不冒称原生全量 O2。
这仍不等于生成语言模块的多输入环退休，后者保留为下一项联合验收。

完整发现 32/33，唯一失败是对象逃逸宿主缺少新桥接源的链接依赖；全部 28
原模式均保留。集中补依赖和更新生成能力矩阵后，统一复验 33/33，464.465 秒，
4182 输入前后指纹一致。13 项工程检查通过；72 私有头映射与 72 源清单位置
均通过静态检查，不代表 Linux 执行。69 TU 同源编译器及 35 契约驱动构建成功。
Cgen LF 字节从 5085928 降为 5080139，减少 5789；未重建三个发布二进制。
证据入口：`build/tmp/xlir_launch_v201_evidence/results.json`。

v200 状态快照：原生多输入组合器已接通真实 producer、投影
Watch 和拥有型取消监听；映射上下文使用明确的物理拥有策略。最终 GCC/TCC
各 110/110，新增六程序独立 O2 6/6，原 104 程序保留；七项工程检查通过。
DEMO6 尚未嵌入这批原生变更，语言多输入等待求解、生成映射代码拥有及真实
联合卸载尚未验收，不宣称长期目标完成。语言侧仍沿用输入不变的 v198 31/31。

### v200：一个组合器引擎，真实拥有与明确的多输入事实

Any/All/Race 和旧、新两类映射共用同一个原生组。组及所有内嵌输入槽仍一次
分配；每个源槽保留实际 Future 引用，重复输入不去重。新投影 Watch 将回调
槽地址映射到 Release 真正归还的 group 引用，保持 64 字节公开存储，不能
把多个槽地址伪造成共享计数的多个节点。输出真正持有 producer，取消监听
真正持有 Data，创建者在返回前归还自己的引用，不再留下 operation base 根。

Future V4 以封闭策略表分别准入载荷、生产者、直接/投影 Watch 和拥有型取消
观察器；先匹配策略身份再读字段，准入不调用投影。每个子节点仍独立认证。
旧 V1/V2/V3 拒绝边界保留。新的 mapped policy 持有一个物理 Data 引用，
策略和代码须常驻；裸 Trace 或旧 mapped callback 不自动获得回收许可。

发布、活动回调和释放尾部由真实引用及明确拒绝状态协调。认证映射/Drop 在
API 自身 mutation 外执行，旧映射保守隔离，调用者已有外层 scope 不被暂停。
全部可失败准备在接受 Data 与激活输入前结束，失败不消费、不通知也不取消。
真正完成后的原生结果保有组和源，但取消观察器实际注销，不拖到结果销毁。

等待视图给出同一个实际输出、全部有序输入及 ALL/ANY 规则，只有 Race 标明
取消其余源。ALL 已完成槽仍在，不能任选一个输入冒充完整规则。视图需完整
物理图冻结、同一不可达 claim；它只是事实，不提供死锁证明或取消权限。
组的 Prepare 对未完成操作只 BUSY，正常完成/取消及回调尾部真正结束后才
Ready。整个图重验、Clear 后，Finish 在 freeze 外归还实际槽并支持不同顺序。

三个新增源码/单头测试家族保留全部原生 104 程序：每个 ownership 进程有
576 生命周期、128 真实终态环事务、96 策略 OOM 预算（24 实际失败），并检验
pending BUSY、同 claim、外部根、Restore、整图重验、两个 Finish 顺序与幂等；
每个 projected Watch 进程有 256 次接管和 64 次未知策略拒绝；每个 phases
进程有 96 阻塞映射、96 真实 Drop 尾部、384 双/三线程完成与取消竞争。
既有 map/waiter 测试只更新真实图结构预期，不删除行为、OOM 或析构断言。

扩展发现 GCC/TCC 25/25。首轮完整均执行 110/110、通过 108/110，失败源于
新增竞争测试错误要求晚到取消必成功；修正只允许已真实 RESOLVED 时无错拒绝，
保留全部竞争与析构断言，第二轮两编译器均 110/110。独立新模块 O2 又发现漏
声明 atomic 依赖；修正清单后 O2 六程序 6/6、38.749 秒，最终完整 GCC 110/110、
108.591 秒，TCC 110/110、27.382 秒，2014 输入前后不变。实际三轮完整验证，
所有失败、源版本和实际运行二进制保留，不把完整执行等同全部通过。

工程七项包含两个公开 ABI 用例、原 54 个构建/边界/文档工具单测、Future API
63 函数/10 常量/23 类型零遗漏、DEMO6 源/测试/嵌入头/发布物不变及文本/脚本/
输入检查。主仓 IR/Cgen 源码未改，Cgen 仍为 5085928 LF 字节。语言专项没有
重复运行，最终按原 InputSnapshot 核对 v198 指纹后复用，不冒充新语言验证。
证据入口：`build/tmp/xlir_launch_v200_evidence/results.json`。

语言上层仍须完成三个相接的部分：宿主的独立准入、新映射上下文的真实代码
拥有、共享 all/any 等待求解，再做生成模块卸载。当前
`xl_future_bridge.c` 的 aggregate context 仍保存 borrowed Boxer 地址，
`xl6_future_wait.c` 仍为单输入契约；不能仅替换 API 名称或增加一条可疑回收
边就声称接入完成。必要 native/network、其他联合卸载、同源码 CLI JIT/AOT、
完整语言/平台/发布验收和旧路径退役继续开放。

v199 状态快照：多输入 producer 接入所需的原生拥有型取消
监听和阶段化调用已完成独立 XRT 验证：GCC/TCC 各 104/104，GCC O2 新增
四程序 4/4；原有 100 程序保留。本批尚未嵌入 DEMO6，也未修改 IR/Cgen。
语言侧沿用输入不变的 v198 31/31 结果，不把原生新验证冒充语言联合验收。
多输入完成规则、组合器实际拥有图、必要 native/network、其他联合卸载及
完整语言/平台/发布验收仍开放，不宣称长期目标完成。

### v199：原生取消观察器的真实拥有与阶段化生命周期

检查 all/race/whenAny 的原生组合器发现，它们仍使用借用型 CancelWatch，
且 pending group 的 operation base reference 尚是外部根。直接在语言环检测
中增加一个多输入枚举，不能解决真实拥有图和代码生命周期的缺口。本批先
补齐共享取消观察器底层，不新增语言专用组合器或在 Cgen 猜测等待关系。

原生 `xcancelwatchownershipv1` 明确一个实际 Data 引用：注册成功才接管，
Notify 借用；在实际 Unwatch、通知/派发和所有 plan pin 结束后 Drop 一次。
旧借用型观察器仍保守隔离、不被自动认证。Request 为令牌和每个实际派发
保活；通知、注销和最后释放在明确的转换边界协调，不把临时根从图中扣除。
拥有型 Notify/Drop 在 API 自身 mutation 外执行，Unwatch 等待另一线程时
不持有该 API 的 mutation；调用者已有的外层 scope 不被暂停。FutureCancel
也先持有真实 Cancel 临时引用，再进入同一取消机制，不伪造 Future 终态。

两个显式查询分别准入 Watch 和有观察器的 Cancel。它们匹配策略身份后才
读取策略，同时返回 lifecycle 和 preparation，拒绝时输出不变。令牌链表
仍只借用 Watch，真实拥有边是 Watch 到 Cancel/Data、Cancel 到 parent；
不能把链表伪造成反向拥有边。Prepare 只等待生产者真实 Unwatch，不代替
生产者取消、跳过回调或自行删除观察器。运行/派发/装配/注销过渡状态拒绝，
之后才允许在相同 claim 下 Clear，并在 freeze 外 Finish 归还真实槽位。

新增四个原生测试文件（两个源码程序及各自的单头程序），不改原测试。
每个 ownership 程序检查 320 个正常/迟注册/自身注销/外层 scope 生命周期、
192 个真实 Data/Watch 环事务（包含两个 Finish 顺序、幂等和 Restore），
320 个 Future cancel/close/生产端离开/父取消入口、32 个注册 OOM 预算及
未知/畸形策略拒绝。每个 phases 程序检查 64 次拥有型 callback、64 次旧式
callback、64 次 Drop 尾部，以及 256 次父/子取消和并发注销竞争。跨线程
注销测试以 child token 的真实链表移除证明已经进入等待阶段，不能用线程
已启动冒充 wait；每个场景都检查真实引用和准确内存数量/字节平衡。

发现批次：GCC 在三处 misleading-indentation 严格告警停下，0/14 执行；
TCC 14/14。修正并扩展 Future 入口后，首轮完整 GCC 执行 65/104，随后两个
新增单头程序因重复定义模块宏被 -Werror 拒绝；TCC 104/104。修正仅涉及
新增测试宏保护，并补充畸形/混合观察器准入负例。最终完整 GCC 104/104、
110.618 秒，TCC 104/104、27.234 秒；2008 输入前后不变。GCC O2 另外完整
构建、执行四个新增程序 4/4、32.344 秒，不宣称全部 104 程序都经过 O2。
实际两轮完整原生验证，所有失败日志、对应源版本和执行过的二进制保留。

七项工程检查全部通过：两项核心公开 ABI 用例、54 个构建/边界/文档工具
单测、取消 API 文档 15 函数/3 类型零遗漏、主仓源码与发布产物不变、变更
文本、批处理语法及 2564 个工程检查输入不变。初次工具验证误在不包含
xhttp/完整工作流的固定 core/runtime 副本运行，留下 1 failure/3 errors；
在完整原生仓重跑同一组 54 单测通过，未删除检查。文档缺少已有 Ownership
视图入口的说明也已补齐。扩展库本身不因此获得新的 ABI 或运行验收结论。

本批原生改变三个生产文件和测试清单，新增四个测试；DEMO6 的编译器、IR、
Cgen、语言测试、嵌入四头及发布三文件都不变。后续仍须把新合同接入实际
组合器、显式 all/any 完成规则与等待证明、语言模块的代码拥有和卸载，再
同步经过验证的 XRT。不能把本批 native 单元证明等同于多输入语言场景闭合。
证据入口：`build/tmp/xlir_launch_v199_evidence/results.json`。

v198 状态快照：async continuation 与 Future 回调链的混合等待
环已通过共享运行时依赖契约接通；实际生成模块同时拥有 executor 和 native
task pool，四通道验证 528 次真实卸载和 2192 次 callback finally。最终完整
专项 31/31，0 失败、0 未运行，4177 输入前后不变。本批 IR/Cgen 及嵌入 XRT
不变，发布 CLI/DLL/VFS 未重建。多输入 producer、必要 native/network、其他
联合卸载及完整语言/平台/发布验收仍开放，不宣称长期目标完成。

### v198：共享单输入等待契约与 async/callback 混合环

旧等待检测只识别 ContinuationProducer 的私有类型，在 Future 回调生产者处
停止。现在单独的 xl6_future_wait.c/.h 定义真实 output/source 槽的只读视图；
continuation 与 callback 各自验证自己的类型、closed layout、稳定状态和同一
不可达 claim。continuation 还验证实际 queue 准备及当前 frame 的独立 claim。
共享检测只沿一个确实必需的 pending input 前进，并核对被找到的生产者 output
就是该 input 的同一物理 Future，绝不遍历任意 capture 来猜测等待关系。

这是不分配的只读环成员证明。外部根、运行/就绪/发布/清理中节点、未知 producer
和非循环 feeder 不提供中断权限。只有已有 async once-only 协作取消协议可
中断一个真正环成员，实际 frame 在离开 freeze 前另行保活；Future callback
不会被移除 Watch 或提前清空，而由原 source 终态逐级驱动 then/catch/finally。
V1 明确只描述单输入，all/race/whenAny 的不同完成规则不能偷偷选一个 child
冒充这个契约。IR 的已验证 frame/storage/ownership ABI 及 Cgen 均没有新增补丁。

新语言用例先执行并 await 真实 task.spawn，再经 then、catch、两层 finally
等待环内或终态 Future。宿主不替换生成模块的 executor/pool，不伪造引用。
每个 TCC/DLL 与 O0/O2 通道包含 132 次生命周期：60 个一至三 async 的混合环、
20 个 feeder、20 个无环对照和 32 个 snapshot OOM 预算；112 次外部 Future/
代码根保留，40 次 async finally 再次 await 并重新保留模块，548 次实际 callback
finally。每次同时检查原 async catch/finally、模块全局对象、MEM_FREE 和准确
分配数量/字节平衡。四通道合计 528 卸载/2192 callback finally，最终实际 OOM
分别为 16、14、16、14。两个生产者实现和共享检测均以相应 O0/O2 重新编译，
其余未变依赖采用固定源码构建对象，不冒称所有 XRT 依赖都全量 O2。

修正前四通道 build=0/run=3，均在等待真正卸载时到期；修正后的发现批次四
通道全部通过。同一语言用例的 C/header/JSON 在修正前后逐字一致，修复发生于
常驻运行时，不在生成 C 上打补丁。最初用例误写 throw error（当前语法要求
string）被拒绝、以及初版脚本处理空 stdout 时异常中止的记录也保留，不作为
产品回归或完整失败批次统计。

发现批次 5/5、176.671 秒。首轮完整 27/31、418.138 秒：两个旧脚本的 Get-Item
源列表分隔错误、一个 runtime 新对象重复链接，以及混合宿主错误假定非阻塞
冻结准入必然一次成功。修正三个脚本接线，并只给新增混合宿主加入可观测预检
包装：拒绝发生时必须尚未调用生成 callback、没有诊断或其他退役事务，才可
重试入口；一旦实际调用必须 READY，不能重试 BUSY/FAILED。最终 528 次生成
预检均 READY，只观察到一次调用前准入延期。原宿主默认的即时断言保持不变。

最终完整 repaired 31/31、0 未运行、423.922 秒、峰值三；实际两轮完整验证，
全部原 30 门及其旧测试人口保留。4177 输入前后指纹均为
`24F4E46C2458260CAB969046099792C5757514B8CA2CDB31CBCC27A9311F1A7A`。
三个既有生产文件变化，新增一个 C 实现及内部声明头；一个既有测试宿主仅增加
可选接线，新增三个测试文件和一个门。Windows/Linux/嵌入编译器及专项源列表
同步接入共享运行时。public 构建 69 TU/17.417 秒/65 hit，34 合同共享 23 TU/
14.529 秒/23 hit。Cgen 仍为 5085928 LF 字节，IR/Cgen 源文件哈希均不变。

13 项工程检查包含静态契约、68 私有头映射、72 源列表位置、6568 文本编码、
嵌入头/符号/能力矩阵、变更文本和脚本检查。固定原生百程序只在重核 2005 原
输入及 2804 隔离副本后复用，本批不修改 XRT 或重建发布文件。最终文档后仍
以原 InputSnapshot 重核，不为文档或证据整理重复完整运行。
后续须补多输入 producer 的实际拥有和完成规则、必要 native/network 生命周期、
其他 Job/generator/async/native 联合卸载、同源码 CLI JIT/AOT、完整语言/平台/
发布验收及旧路径退役。
证据入口：`build/tmp/xlir_launch_v198_evidence/results.json`。

### v197：语言任务池的真实引用与显式模块拥有 ABI

XL6_TaskPool 不再是 frame 借用的指针。Create owner、每个 accepted frame、
完整 native API entry 和执行/通知尾部的 TaskActivity 均持有实际引用；最后
一个引用归还才释放 mutex、condition 和外壳。frame 清理归还自身 pool 引用后，
activity 仍保护代码租约释放尾部。Submit 的 promise 创建、失败回滚及惰性 native
创建均有 entry 保活；惰性创建使用单 creator 和已受理 reservation，实际创建与
失败线程 join 不在 ownership scope 或池锁内。关闭前已受理的工作继续完成，
关闭后的真正新任务仍使用 fresh CodeLeaseAcquire 准入，不改为继承租约。

wrapper 与 native pool 是两个真实拥有节点，wrapper 恰好报告一条 native owner
边。活动 entry/activity、创建或退役中状态拒绝 Count；Claim/Restore 使用原
token。不可达图的 Prepare 关闭 wrapper 新准入而不取消 accepted work，不偷做
native join；原生子节点必须独立完成 Prepare/join，整图 Clear 后才可 Finish。
两种 Finish 顺序、重复 Finish 与普通 checked teardown 都由真实 owner/plan
pin 保证物理释放顺序。V1 自任务 quiesce 仍关闭新准入并拒绝自 join。

ModuleAbi.taskPoolOwnershipAbi 显式选择 RESIDENT_NATIVE_JOBS_JOIN_V1。
lowering 从实际 lowered 操作计算，verifier 独立重算并拒绝 typed 提前持有、
未知值及 execution/ownership/retirement 协同伪造；text/JSON 都输出此契约。
有 owned executor 或 owned pool 即选择 V3，普通 V2 的排空含义不变。Cgen
只根据已验证契约发出真实拥有边，不再保留 v195 的 mixed-executor BUSY 补丁
或“已关闭且空闲任务池才能作为叶子”的 guard。宿主独立准入 wrapper/native
adapter；Job Data、Watch 和结果白名单并未放宽。普通 teardown 仍先 quiesce
服务，再执行原有全局 finalizer。

新增四种 IR 分区、24 个 C/header 前置拒绝及真实 wrapper/native 专项。
GCC O0/O2 和源码 TCC 三通道各 280 次生命周期，其中 240 个一至三 Job 排队
图、520 次真实 wrapper/native Prepare，检查精确图、已受理执行、join、两种
Finish 顺序、幂等以及准确分配平衡；此专项本身不是混合 producer 环证明。

原任务存储人口全部保留。原 200 个活动 blocker 事务现在明确验证整池图拒绝，
另加 200 个真正 worker 休眠的排队图（raw 7 节点/9 边，language 9/15）。原
200 个 Job/frame/class 事务以真实持有 bundle 保留对象到 quiescence 之后，
再检查完整退休图；400 次真实外部根延期、400 queued BUSY、200 outcome、
400 class finalizer、150 活动拒绝和 400 跨线程 finalizer/freeze 探针不减。
不能用省略 pool 边或假装活动池可检查来保持旧图节点数。

首轮完整验证暴露三个门失败：Future 宿主仍用旧 resolver、模块预检仍只认 V2，
以及 25 个 outcome 白盒探针手工 frame 没有真实 pool 导致尾部崩溃。分别接入
实际 V2 host resolver、保留并扩展两个协议人口、让白盒 frame 持有真实空闲池。
原 18 模式乘 O0/O2 的 36 条 V2 通道仍在，以显式 test-only 适配器执行真实排空；
再完整运行 36 条 canonical V3 通道，共 1800 次真实 TCC 模块生命周期。
适配器不是生产 Cgen fallback。专项还保留 PowerShell 单元素参数数组被展开的
失败，修正参数传递后 12 条聚焦通道通过，再由最终完整 72 条验证覆盖。

发现批次 3/6；首轮完整 27/30、393.220 秒；最终完整 30/30、440.800 秒、
峰值三、0 未运行。因此实际是两轮完整验证，全部失败日志及对应源版本保留。
最终输入前后均为 4171 项，指纹
`513EA602AD978B9A976E386838E69D98A223814CFD1D33B66D305FD5B7FA914A`。
原 29 门测试人口逐项比较，新专项不替代原门；任务存储原 33 条通道全部通过。

固定 XRT 的 core/runtime 实现头与声明头经原同步器导入，符号表和能力矩阵
同步生成。原生 v196 的 GCC/TCC 各 100/100 仅在重核 2005 原输入及 2804
隔离副本输入后复用，不宣称本批重跑原生百程序。发布三文件哈希仍不变。
初次工程检查保留能力矩阵过期失败；生成矩阵后 12 项工程检查通过，源码编码
扫描 6562 文本文件。文档完成后按原 InputSnapshot 重验，不为文档重复全跑。

十个既有生产文件变化（含生成的符号表），无新增生产文件；九个既有测试文件
修改、三个新增测试文件。Cgen 从 5086936 减至 5085928 LF 字节（-1008）。
最终 public 构建 68 TU/16.076 秒/67 cache hit，34 个合同共享 23 TU/
14.707 秒/23 hit。剩余混合 producer、必要 native/network、组合卸载、同源码
CLI JIT/AOT、完整语言/平台/发布验收及旧路径退役均继续开放。
证据入口：`build/tmp/xlir_launch_v197_evidence/results.json`。

### v196：原生任务池、Job 队列与延迟资源的真实持有关系

任务池从“只能在全部排空后折叠”的不透明资源变为可独立验证的拥有节点。
Create 的 owner、collector Hold、原生 API entry 和异步文件资源各有实际引用。
Count 只接纳所有 worker 已休眠或退出、没有运行任务/原生入口/不透明 finalizer
的状态；队列逐个报告真正 accepted Job 的 executor 引用，不重复 Tail/Next。
Job 的 Data policy 仍由 resolver 独立白名单验证，不因池被接纳而认证未知数据。
worker/control 是 join 前唯一包含的资源，不伪造 RC 节点，也不猜减休眠引用。

原生调用保活覆盖 Submit/Cancel 的私有取消链、容量/取消等待、回调和数据释放
尾部。worker 唤醒时先归还队列锁，再请求 ownership mutation，避免 freeze 与
条件变量锁反序；任何等待、用户 Proc/Drop/finalizer 和线程 join 都不包在
ownership scope 内。正常 Destroy 遇到重入或其他仍活跃的 native entry 时拒绝，
不提前关闭或释放外层调用仍要访问的池。Job 的物理释放也计入排空尾部。

已授权不可达图的 Prepare 只关闭新任务准入，保留已受理工作的原有执行与清理；
真正排空后让 worker 退出，再在锁/scope 外以零超时 join 检查终态。它不取消、
偷取或跳过 Job。Clear 要求已 join，Finish 只释放 worker 资源；原 owner 与
collector pin 分别归还，锁和条件变量随最后一个实际引用释放。

继续核查发现，xasyncfile 原先只长期借用 Pool，空闲池仍可能有未来的 finalizer。
现在文件创建取得一份真实资源引用，创建失败或物理关闭清理完毕准确归还。
这份未认证文件的引用是池的外部根：即使没有排队工作也不能 Claim/Prepare。
同时保留已受理任务在 Destroy 排空期间创建并关闭原生资源的能力，资源引用
而非拒绝合法工作负责阻止提前 join。

新增三组模块化及单头程序，共六个文件。每个拥有图程序覆盖 60 个休眠/join
外壳、60 个一至三 Job 精确图、20 个不透明/运行拒绝、20 个同步取消入口重入、
20 个不透明 finalizer 和 20 个真实延迟文件根；96 个 snapshot 预算实际失败
16 次，48 个创建预算实际失败 5 次/成功 43 次。另两组每通道分别覆盖 100 次
重入销毁及随后任务正常执行、40 个关闭准入后完成采用和异步关闭的真实文件。
全部检查实际引用、外部根、正常/失败/取消终态及准确分配数/字节平衡。

有两份同用例的因果对照：只使用旧公开 API 的 Submit 清理重入在修正前真实池
上 build=0/run=3，修正后 build=0/run=0；延迟资源初版错误拒绝 accepted task
中的原生资源采用，同一最终用例在保存的初版对象上 build=0/run=3，修正后为
0/0。均非 mock 或新 ABI 缺失造成的失败。

保留固定 v187 全部 91 个程序，加入三个原有异步文件程序及六个新程序，最终
36 组/100 个程序 GCC 110.058 秒、TCC 28.413 秒，2005 输入前后不变。原生完整
批次实际经历 95、98、100 三轮，不能宣称一轮完成。另有初次精简套件遗漏旧
atomic 依赖的失败、资源专项 TCC 对运行时结构体数组初始化的编译失败；后者
改为等价逐项赋值，保留全部断言和原失败证据。GCC O2 六程序 35.313 秒通过。

两个核心公开 ABI 测试通过；原通用 ABI 脚本第三项 xhttp 因固定源树原本不含
该扩展夹具而失败，原记录保留，不宣称该扩展通过。核心/runtime 单头一致性、
范围内 UTF-8/空白、脚本语法及 DEMO6 6558 文本文件编码检查通过。
五个既有 XRT 生产文件变化；DEMO6 生产源码、既有测试、Cgen、嵌入 XRT 与
发布 CLI/DLL/VFS 不变。Cgen 仍为 5086936 LF 字节。本批是原生契约检查点，
不是语言侧任务池拥有图或混合等待环已经接入的证明。

后续先给 XL6_TaskPool 建立实际 owning slot 和 frame/native 持有关系，再以
显式 lowering/verifier/module ABI 接入生成模块，随后验证混合 producer 依赖。
必要 native/network、组合卸载、同源码 CLI JIT/AOT、完整语言/平台/发布验收
及旧路径退役继续按原目标推进。
证据入口：`build/tmp/xlir_launch_v196_evidence/results.json`。

### v195：显式模块执行器拥有 ABI 与 V3 预检

ModuleAbi.executorOwnershipAbi 使用 RESIDENT_WAIT_SLOTS_JOIN_V1；lowering
从实际 lowered 操作计算，verifier 独立重算，不信任 execution/ownership/
retirement 三个缓存字段互相作证。Typed 阶段不能提前出现拥有契约，无执行器
的模块不能伪装成 owning executor；有执行器的模块不能沿用 V2 的排空含义。
新的专门合同含 23 个 C/header 前置拒绝，覆盖组合篡改、未知值、保留链接名和
text/JSON。原模块合同的 41 globals、236 other slots 人口不变，587 个原有
变异保留，再加 11 个 V3 变异，共 598 个。

V2 的 READY 仍表示资源排空；V3 的 READY 只表示可以开始检查拥有图。二者通过
独立 getter、精确 size/tag、同一个 V1 graph 身份和非空回调验证，并禁止同时
导出 V2/V3 来混淆语义。V3 不在看见图之前取消或排空图内执行器，也不消耗全局
变量或执行析构。真实 executor 全局槽恰好贡献一条拥有边，其 queue、worker
休眠槽、frame、producer 和 Watch 分别按原精确 adapter 准入及 Prepare。
普通 checked teardown 仍先 quiesce，再执行已经验证的全局 finalizer。

实际生成模块包含拥有代码租约的全局类对象；catch、finally 和该对象析构均
检查模块全局字符串仍然存活。测试只桥接外部调用和已验证 machine 的私有
入口，不拦截 ReserveOwned、不选择宿主 executor、不伪造全局拥有字段。该
私有入口不是新增公开 Future 参数/返回值的 C 导出支持。

GCC O0/O2 宿主分别运行真实 TCC 编译/relocate/delete 与真实 DLL 加载/卸载。
四个基本通道各 182 次：60 个一/二/三 producer 环、20 条 feeder、20 条无环
对照、112 次外部 root 保留、40 次 finally await 重新保留模块；30 个错误
契约拒绝、5 个有真实代码 guard 的旧协议显式 teardown、15 个 BUSY/无诊断
FAILED/有诊断 FAILED 重试。32 个 snapshot 预算各触发 12 次真实 OOM。全部
验证原 CFG、全局清理、线程退出之后的 MEM_FREE 及准确内存数量/字节平衡。

同时补查 V3 和旧 task pool 的交互。第一个纯 async leaf 被 eager 化，没有
任务池，不能当作混合服务证明；改为真正的 task.spawn。初期测试还混淆了
关闭后的新任务根准入与已有工作的继续执行，因此保留失败记录并拆分两个
边界，未擅自把 TaskRuntime 的 fresh code admission 改成 scoped inheritance。
最终同一修正前编译器的生成物、修正后的当前源码宿主证明：owner 仍开放时，
初版 preflight=READY，随后 task.spawn 因 module task resource is closing
失败，build=0/run=3。旧编译器的所有变化输入均由保存的版本及哈希证明。

修正根据已验证的两项服务 ABI，在仍有 accepted continuation 时先返回 BUSY，
不触碰旧任务池。新回归每通道 50 次：25 次开放 owner 的 await-to-task 正常
结果，以及 25 次关闭 owner 后对真正新任务的原有准入拒绝。四通道再增加
200 次真实卸载，总计 928 次；独立同输入回归修正后 build=0/run=0。这个 BUSY
边界保护已有语义，但不等于实现了 task/callback/native 混合等待环的回收。

发现批次 7/7、168.709 秒；首个完整批次 28/29、415.660 秒，唯一失败为原
模块合同把 async frame fixture 固定为 V2，两个该门真实运行通道仍通过。
旧静态架构门要求非空 executor 必须拒绝 trace 的断言也保留失败记录，随后
替换为明确验证 V3/拥有 ABI/真实边/混合池顺序，而不是删除保护。

最终完整 repaired 29/29、0 未运行、432.585 秒、峰值三；本批共两轮完整
验证，不能写成一轮全绿。4167 项输入前后同为
`758A6DC34AF326E157C654B3FBE7348E34A36D6C54F4E7BDC379F82529437ADA`。
文档更新后仍按原 InputSnapshot 算法核验，不重复运行未变化的源码。

八个既有生产文件变化，无新增生产文件；2781 原测试保留，2779 不改、两个
合同/宿主接线更新，新增六个测试文件和一个门。Cgen LF 字节 5085492 →
5086936（+1444）；三个跟踪生成 C 为 85599、66911、258230 字节，分别增加
253、354、463，主要是版本化边界声明和拥有边，不把线程/循环回收逻辑复制
到生成函数。最终源码驱动 68 TU/20.055 秒/49 hit；33 合同共享 23 TU/
16.277 秒/22 hit。八项工程检查及 6558 文本文件编码通过。

固定 XRT v187 的 91/91 只在重核 2003 输入后复用，不宣称新跑；嵌入 XRT、
发布 CLI/DLL/VFS 和默认 XLIR 后端没有改变。后续优先建模真正的 task pool/
Job 拥有关系及混合 producer 依赖，再推进必要 native/network、其他组合卸载、
同源码 CLI JIT/AOT、全语言/平台/发布验收与旧路径退役。
证据入口：`build/tmp/xlir_launch_v195_evidence/results.json`。

### v194：真实 worker 休眠拥有槽、join 与执行器终态保活

每次 WaitRunOwned 都向 worker 内的真实 wait.queue 字段取得一份队列引用，
退出该调用才归还。休眠与唤醒/执行的转换在 mutation/队列锁边界内完成；
休眠时引用不减少，而是由 executor Trace 逐槽报告，执行及原生入口仍不可
检查。普通 RunOne/WaitRunOne 的原生调用引用和拒绝规则不改。worker/control
分配是 executor 唯一拥有且 join 前不能释放的内部资源，不伪造独立 RC 节点；
线程只借用这些受 join 约束的内部存储，既不 detach，也不猜减休眠引用。

executor 本体有实际 owner/plan/native 引用。Create 提供 owner 槽，成功
Destroy 只消费这一个槽；计划或原生额外 Hold 可以保留同一个已退役的终态
分配和 gate，不复制影子执行器。Prepare 要求自己的 queue 已被同一计划认领
并关闭准入，且 worker 已执行完全部 accepted work 和正常清理。然后在所有
gate、mutation、freeze 之外做零超时 join 探测。已 join 才允许 Clear/Finish，
资源退役与本体最后引用释放分开；外部 queue 引用仍能阻止关停。

新增实际原生入口引用同时保护队列/worker 资源，而不只是保护 executor 外壳。
独立回归复现了初版缺口：宿主 acquire 回调先 Close 再重入 Destroy 时，销毁
竟然成功，外层 ReserveOwned 仍持有正在使用的 queue。修正为 Destroy 自己
是唯一活跃原生入口才可退役。acquire 及失败回滚 drop 两个回调都必须拒绝
重入销毁，外层准入按 CLOSED 正常回滚，退出原生调用后才可 join/Destroy。
原有 worker 自身、运行、release 中的销毁拒绝继续保留。

新门共 12 条运行通道、两次确定性 C/header/JSON 生成。GCC O0/O2/TCC 各完整
运行未改的原生 queue 和 executor 程序；包括 600 个队列事件顺序、50 对 FIFO、
100 并发交接，以及 100 独立派发、300 真实 Future Watch、150 阻塞/限额/失败、
50 多 worker 饱和与 50 补偿失败。每配置再加 120 个真实休眠拥有图、60 个
独立外部 queue 根、120 次准确 policy 拒绝、20 个 join 后额外保活的本体、
40 次 run/release 拒绝和 40 次宿主 acquire/drop 重入拒绝。4800 分配预算有
420 次真实 snapshot OOM，失败逐次回滚并核对完整分配平衡。

生成测试的宿主绑定选择真实常驻 executor，不再手动 RunOne；每配置 60 个
一/二/三 producer 环、20 条 feeder、320 次 executor/queue/frame/Watch 独立
拒绝和 40 次 finally await 外部清理后重建根。输出观察者或宿主 owner 引用
阻止取消；环内中断后由实际 worker 执行原 resume CFG，feeder 不被直接取消，
最后检查全部 outcome、catch/finally、join、代码拥有关系和内存精确平衡。
期间的瞬时准入拒绝只在 resolver 明确观测到 executor/queue 不可检查时重试，
不掩盖未知 policy、语义失败或永久错误。原 v193 无环/环/feeder 人口不变。

构建期补上 API 所需的 executor 声明头并修正新测试严格告警，原失败保留。
首次发现 1/6、26.962 秒，五门因合同构建记录中的 API 输入过期被拦下，并未
进入运行；重建该合同驱动后再发现 5/6、143.470 秒。新专项检出两处宿主问题：
重复进入 freeze，以及宏拦截后漏声明原 ReserveOwned；原原生 queue/executor
六通道已通过。另保留独立入口回归的 build=0/run=3 失败及修正后的 run=0。
合并三类修正并重建后，仅一次完整 verification：28/28、0 未运行、380.706
秒、峰值三；4160 项输入前后相同，指纹
`3B067F2D5F09D2C93B1DD7EFE4D9E76D195FBC49CC81081BA037426722016CC1`。

七个既有生产文件变化（含一处 frame 注释校正），无新增生产文件；原 2778
测试文件全部不改，新增三个测试文件及一个独立门。公开源码 68 TU/15.881 秒/
67 hit，32 合同共享 23 TU/14.030 秒/23 hit。Cgen LF 仍为 5085492，三个跟踪
生成 C 的字节和哈希均不变；没有把此协议再堆到生成 C。七项 DEMO6 工程门和
6551 文本文件编码通过，相邻 XRT 原有 `xllm_session_core.c:646` EOF 告警保留。
固定 XRT v187 的 91/91 仅在重核 2003 输入后复用，发布 CLI/DLL/VFS、嵌入
XRT 及默认 XLIR 后端均未改。

下一步必须给 generated module 的执行器拥有关系显式 IR ABI，并验证 trace、
准入、Prepare 和联合卸载；目前保留非空 executor 的 module trace 拒绝，不能
仅删掉该限制。真实 worker 的宿主绑定证明不是完整生成模块/CLI/DLL 验收。
混合 producer、必要 native/network、生成 Job/module 与 generator/async 联合
卸载、同源码 CLI JIT/AOT、全语言/平台/发布验收和旧路径退役仍开放。
证据入口：`build/tmp/xlir_launch_v194_evidence/results.json`。

### v193：真实队列拥有关系与实际等待环的关停授权

队列有真实引用计数和独立于 ready 链表的 owned ticket 链表。Reserve 取得的
物理帧基准引用在队列中逐项可见，Abort 或 dequeue 才移除；dequeue 后转为
常驻执行局部引用，直到正常退役，不从 RC 中猜减“可能属于执行器”的数量。
原生 RunOne/WaitRunOne 入口持有真实调用尾部引用，并令该阶段不可检查；
工作线程等待入口尚未建模，不能把等待中的线程忽略为无根。队列 owner、计划
及入口各自归还原有引用，销毁同步对象前必须已关闭且所有工作/入口均退出。

队列 adapter 在调用 storage Ownership 前要求准确静态表准入；queue/storage/
frame/producer/Watch 仍分别准入。Queue Prepare 的职责只是关闭新准入，不是
宣告任务已经结束。Producer 只交付一次协作取消，物理 frame 和 Future 自己的
Prepare 继续阻止提前 finalizer/clear。执行源 catch/finally 和清理 await 的
仍是原 resume CFG；不关闭共享源，不直接发布 cancelled outcome，不清空活帧。
CancelWait 在检查私有 producer 前先检查 committed 的发布状态。

不可达拥有图不等于死锁。新增无分配的冻结 wait-for 遍历：只有 pending 源的
准确 producer/output 身份、已提交且未 ready/running 的当前帧、准确的队列
拥有槽，以及同一计划的 claim/已准备队列才能提供一条依赖边。Floyd 成环及
入口判定只授权真正的环内成员；中断一员后该证明消失，其余成员及 feeder
通过正常结果传播继续执行。已就绪的无环尾部、私有/交接态及未知 producer
不能作为成环证明；混合 producer 环的支持仍未完成。

新独立门在 GCC O0/O2/TCC 每配置执行 120 个实际生成的一/二/三 producer 环、
40 条末端已有可运行工作的无环链、40 条汇入环但自身不在环内的 feeder 链。
每配置 800 次独立 queue/storage/frame/Watch 拒绝、160 次准确环内中断；
2400 个注入预算触发 270 次真实 snapshot OOM，每次失败检查完整回滚。
输出观察者或宿主拥有者任一外部引用都阻止取消。finally 新 await 外部清理时，
计划正确重判为 ROOTED，清理完成后再正常发布 outcome 和回收。200 个事务
逐项检查 queue/frame/owner 退役、引用和分配数量/字节精确平衡。

发现批次 5/6、78.030 秒：原 80 个环全部走完，最后真实 OOM 数为 20，未满足
新测试的 >20 聚合断言；扩大预算覆盖到全部轮次，保留原断言和失败日志。
第一轮完整 27/27、373.912 秒，指纹
`1649570EB35B6D403BEF3D4809F99A9C9BCAA3C90D66CA5E38A485BF0CC9F806`。
复核发现缺少无环对照，新增回归在尚未修正的生产代码上 GCC O0/O2/TCC 均失败，
证实初版把不可达误当作取消授权。集中补上真实 wait-cycle 证明，并追加自环、
无环、feeder 人口后，第二轮完整 27/27、0 未运行、372.470 秒、峰值三；
4156 项输入前后相同，指纹
`446F8EDFBF26CBA4694E35698A1E4423EEB4CB74D6B93A367F43046B6EDEA015`。
保留发现、首轮、失败回归和最终批次的各版输入与日志，不把首轮绿灯作为修正
之后的有效证据。原 26 项专项的记录人口逐项保留，没有 smoke 替代。

六个既有生产文件变化，无新增生产文件；原 2774 个测试文件完全不改，新增
四个测试文件和一个独立门。最终源码驱动 68 TU/15.301 秒/67 hit，32 合同共享
23 TU/13.374 秒/23 hit。Cgen LF 字节保持 5085492；async-storage 66557、
Future callback 257767、generator frame-loan 85346 字节及三份哈希均不变。
七项 DEMO6 工程门通过，6547 文本文件编码通过；相邻 XRT 原有
`extlibs/xllm-session/src/xllm_session_core.c:646` EOF 空行告警保留。
固定 XRT 的 v187 91/91 仅在重核 2003 输入后复用，不宣称本批新跑；发布
CLI/DLL/VFS、嵌入 XRT 和默认 XLIR 后端均未改变。

下一步是实际执行器/工作线程拥有图，而不是删除 generated module trace 对
非空 executor 的拒绝。宿主管理队列证明不覆盖生成执行器联合卸载；必要
native/network、生成 Job/module 与 generator/async 联合卸载、独立同源码 CLI
JIT/AOT、全语言/平台/发布验收和旧路径退役继续开放。
证据入口：`build/tmp/xlir_launch_v193_evidence/results.json`。

### v192：常驻取消观察与不关闭共享源的等待中断

Lowered IR 使用显式 `RESIDENT_AWAIT_ONCE_V2` cancellation ABI。构建、验证
和 Cgen 共同要求该 ABI；历史 AWAIT_ONCE、无效和未知值不能输出 C/声明头。
五个 machine 的 15 条取消负例加入原 139 条，frame 合同共 154 条。生成帧不再
携带或逐次复制 `cancelObserved`，通知包装仅调用 V4 Observe，然后进入既有
resume CFG。稳定 producer 持有一次性观察状态，因此后继帧里的 catch/finally
await 不会重新观察已经交付的取消。取消优先于源结果查询，主动中断 pending
源时不制造多余的 AGAIN 诊断或诊断分配。

V4 CancelWait 是需要真实帧/入口引用的显式 owning-host 操作，不是收集器的
不可达判定。它保留一份真实调用尾部引用，请求 output 的取消，只移除当前帧
自己的 Watch，再把 accepted activation 交还执行队列。原源 Future 及其他
Watch 完全独立。源通知、READY 注册和主动中断使用同一个常驻 Ready 仲裁，
最多发出一次 Ready；等待 Watch 返回时不持本操作的 mutation、帧锁或队列锁。
调用者也必须在其外层 ownership scope 之外调用，不能擅自暂停调用者作用域。
已 ready/running、同帧重入和已经观察过取消的后继均不重复中断。队列真实执行
引用始终存在；没有猜测 RC、提前发布取消结果、清空运行帧或跳过 finally。
StorageV2 增加私有 Ready 回调，仍要求同源码重编译，不承诺混用旧私有二进制。

新独立门在 GCC O0/O2/TCC 各运行原有 34560 个取消执行案例、20520 次取消请求
和 34560 个结果观察者，每配置 1289034 次分配精确平衡；保留所有输入结局、
catch/rethrow/finally、finally await、清理抛错及无诊断失败、producer ambient
隔离组合。旧宿主的“代码租约归零才允许 launch”不适用于仍活着的类对象：新
专项同步器在 launch 前检查真实执行队列空闲，在最终退役时同时检查全部真实
代码获取/归还及队列 credit，未改变原 runtime 的循环、期望结果或内存断言。

每配置再加 2106 个实际生成的主动中断，覆盖共享 pending 源、另一个真实 Watch、
清理 ready/pending 和四种结局、finally/析构的三种 outcome，以及 owning-host
ambient 隔离。2106 次传输中断在零分配预算下接受且无分配，2106 次同帧原生
取消监听重入安全拒绝。另有 300 次源完成/取消/执行器三线程实验，包括无强制
顺序和两种确定先后；真实额外帧引用跨越退役，最后按准确 adapter Hold/Drop
归还。逐案例和整个程序精确内存平衡。两个取消 IR fixture 各生成两次，全部
旧负例保留；历史 cell-cancel-race 的完整 runtime/CLI 人口不由此门执行，不
冒充已完成该历史门或全语言验收。

集中 discovery 0/5、58.032 秒，原失败日志完整保留：各 GCC 门共有新增 Observe
紧凑排版的严格告警，新宿主还有同类告警；TCC 暴露上述旧代码租约等待和新
私有队列遗漏 Close 的测试接线问题。集中修正后，仅一次完整 verification：
26/26、0 未运行、367.879 秒、峰值三；4151 输入前后相同，指纹
`2E78CB91FACA472F40D96A0C0B96F5D4AAA42AB42029F0DE2E4731A36BD2DCD0`。
没有第二轮完整复验或受影响补验，原 25 门的所有记录人口逐项保留。

九个既有生产文件变化，无新增生产文件；原 2771 个测试文件保留，2766 不改、
五个合同/接线更新，新增三个测试文件和一个独立门。源码驱动 68 TU/14.440 秒/
67 hit；31 合同共享 23 TU/12.679 秒/23 hit。Cgen LF 字节 5085922 → 5085492，
减少 430；async-storage 生成 C 67218 → 66557，Future callback 258103 →
257767，generator frame-loan 的 85346 字节及哈希不变。没有测量或改写发布
DLL；嵌入 XRT、发布 CLI/DLL/VFS 和默认 XLIR 后端未变。七项 DEMO6 工程门
通过，相邻 XRT 原有 `xllm_session_core.c:646` EOF 空行告警保留。固定 XRT 的
v187 91/91 在 2003 输入复核后复用，不宣称本批新跑。

后续必须在显式依赖证明及队列执行引用的真实授权基础上接入 SCC 关停，不能
因为有 CancelWait 就在 graph Prepare 中任意取消 rooted task。必要 native/
network、生成模块联合卸载、独立 CLI 同源码 JIT/AOT、全语言/平台/发布验收
及旧路径退役继续开放。证据：`build/tmp/xlir_launch_v192_evidence/results.json`。

### v191：稳定 async producer 与真实 Watch 的传递图

Lowered async machine 新增显式 `STABLE_PRODUCER_WATCH_V1` ownership ABI，
构建、验证、text/JSON dump 和 Cgen 使用同一事实；无效和未知 ABI 不能输出
C 或声明头。既有 43 个 frame slot 的 129 条负例保留，五个 machine 追加十条
producer ABI 负例，合计 139 条。Cgen 只选择已验证的 V3 Bind 入口，不恢复
AST 语义、不按输出字符串补出引用或关停动作。

复用 XRT 已有 bind-once producer 协议，不修改 XRT。每个逻辑 async activation
分配一个有真实引用的常驻 producer：output Future 持有它，已绑定帧也持有它，
它只持有当前物理帧。后继 Bind 保留同一 producer，Commit 在队列原子交接中
取得新帧引用并归还旧帧引用；旧帧正常退役后不会成为历史链。producer 保存的
outer Promise 地址只是由帧 owning slot 保活的身份判别，不能接受不同 output
的后继。Abort 仅撤销临时绑定；根的重试需要新的私有 Promise/Future pair，
不替换旧 pair 的一次性绑定。V2 私有帧也允许 Abort 后同一分配重新 Reserve/
Bind，并保留原物理 metadata 租约，不泄漏重复租约。

certified Watch 的 Data/ops 直接指向帧的同一物理节点；没有 Watch 影子节点。
通知和释放回调仅递交队列事件，实际引用由既有注册/释放路径取得和归还。
模块 resolver 分别允许稳定 producer、物理帧和准确的 XRT producer/Watch
policy；任一传递节点遗漏都拒绝，不能按不透明叶节点绕过。Prepare 在 current
仍存在时为 BUSY，正常执行和清理后才 READY。仅持有计划或取消观察者，不会
解除队列执行职责、移除 Watch 或跳过源 finally；依赖 SCC 关停尚未实现。

GCC O0/O2/TCC 各新增 100 个实际生成的 pending/ready、成功/失败/关闭/源取消/
输出取消图。Plan 在观察者释放后真实保活 output/producer/旧帧，前驱正常退役
后 Refresh 发现新帧，验证 producer 身份不变、不再追踪旧帧，完成后可以正常
提交和释放。每配置 1040 次独立 policy/身份拒绝、30 次根重试、640 次后继
回滚及再次交接、200 次跨线程 freeze；330 档分配预算触发 80 次实际 OOM。
十条 64 后继的 native 链逐步核对 live allocation count/bytes 恒定，所有事务
精确内存平衡。根/后继 setup 重试在 native 合同中证明，不冒充完整语言组验收。
原六条 async-storage 运行和两次确定性生成保留，追加到九条运行。

集中 discovery 5/5、72.887 秒。完整 verification 保留原 24 门，增加受链接
依赖变化影响的 capture 门，共 25 门；24 通过、1 失败、0 未运行，355.744 秒，
峰值三，4147 输入前后相同：
`1F417B49F9DEE5EFB12E483A558BD6C9976F3321BCBCE2B274C5182FE578E2AF`。
失败是 generator 的两个 async 图还写着旧大小；稳定 producer 实际多一个节点、
三条边，可达性/真实队列根断言通过。只更新该宿主的两组精确预期，完整补验
生成器专项 1/1、16.933 秒；保留每配置 2400 生命周期、1000 async 事务、2000
挂起、16000 检查探针及 6600 真实失败，未缩减人口。当前补验输入指纹为
`BBACB58BC3A0D40A7DE91EDD68FA65F7160AA1C987BC30EFBE39EE3E625FF32F`。
后续证据复核发现，公开源码和 generator-storage 门也把这个宿主列入输入；
追加完整补验这两项，2/2、118.152 秒，保持 generator-storage 的原 78 条运行/
50 次生成。两个补验批次的 4147 项指纹均为上述当前值。原失败日志和旧宿主
版本保留；按原 InputSnapshot 算法只替换这一行旧哈希重构首轮完整指纹，
证明其余 22 门输入未变。实际是一轮完整加三项受影响补验（1 + 2），不把两个
全局指纹称为相同，也不把首轮失败改写为通过。

本批十个既有生产文件变化；原 2770 测试文件保留，2765 不改、五个合同/接线/
图预期更新，新增一个 runtime 测试文件。公开源码构建 68 TU、15.697 秒、64 hit；
29 合同共享 23 TU、12.224 秒、23 hit。Cgen LF 字节 5,085,705 → 5,085,922，
仅增加 217；async-storage 生成 C 67,218、Future callback 258,103、generator
frame-loan 85,346 均未增加字节。前两个内容改为 V3 调用，后者逐字节不变。
不据此推断 DLL 体积。七项 DEMO6 工程检查通过；相邻 XRT 的既有不相关
`extlibs/xllm-session/src/xllm_session_core.c:646` EOF 空行告警保留。

嵌入 XRT、发布 CLI/DLL/VFS 和默认 XLIR 后端未变；v187 固定 XRT 的 91/91
在 2003 项输入重核后复用，不是新执行。后续按完整目标继续封闭 producer
依赖环正常关停、必要 native/network、生成 Job/模块与 generator/async 帧联合
真实卸载、独立 CLI 同源码 JIT/AOT、完整语言/平台/发布验收及旧路径退役。
证据入口：`build/tmp/xlir_launch_v191_evidence/results.json`。

### v190：通用 async 帧与内嵌 ticket 的真实生命周期

新增独立常驻模块 `xl6_continuation_frame.c`，不把语言语义加入队列。V2 封闭
描述符引用既有稀疏 activation 布局，并明确 ticket、Watch、source、outer 的
物理偏移；验证大小、对齐、范围和字段/捕获/初始化位之间的不重叠。私有分配
由调用者持一份实际引用，Commit 转移给队列，真实 Watch 另取/还一份；native
及 Plan 也增加相同分配的真实引用。引用操作另有物理锁，mutation admission
只负责与 freeze 协调，不被误当成多个并发写者之间的锁。

Bind 保留已接受代码租约的一份独立引用，因此即使队列把执行租约移交给后继，
或已完成最终执行退役，被额外持有的旧帧及其 DLL 描述符仍保持有效。运行、
未发布、交接和清理阶段拒绝 Count/Trace；准备阶段不会移除 Watch、取消任务
或跳过源 finally。常驻 Retire 在正常执行之后清理捕获、依次分离 source 与
outer，不持 mutation/队列锁调用析构；之后队列才释放自己的真实存储引用。
后继仍要等前驱清理及代码作用域退出才发布，但不必等待已清空旧帧的 native
引用归零。Prepare 只在这些真实职责完成后 READY，Clear 只标记，最终 Drop
才释放物理分配和代码租约。每个传递子节点依旧需要独立准入。

Lowered IR 改为 `RESIDENT_CLOSED_FRAME_V2`，历史 PHYSICAL_FRAME_V1 仍保留
枚举但不再是合法 Lowered ABI。Cgen 从已验证 IR 发出描述符，不再生成逐帧
Trace、Watch release 和完整帧销毁程序；保留的类型包装只调用常驻入口。
稀疏捕获的清理顺序、源 CFG 和 outcome barrier 没有变化。队列私有 ticket
结构新增 resident storage 字段，**要求同源码重编译**，不声称与旧尺寸的
预编译 native ticket 二进制混用兼容。旧 caller-owned/TraceBind 路径保留
原语义，但不会被自动认证为可回收布局。

GCC O0/O2/TCC 每配置追加 240 个生命周期事务，覆盖六种事件顺序、清理期间
Retain/Release、正常析构和 false/NULL 失败；80 个真实 Watch 事务覆盖 pending
与已完成 source 的成功/失败/关闭/取消。50 条实际生成的两次 await 链验证
前驱及后继分别保活、正常结果和四种终止路径。共 580 次跨线程 freeze、820
次 ABI/重复绑定/遗漏准入拒绝、120 个显式失败 outcome、10 次真实分配 OOM。
20 次真正 DLL 卸载把 layout、capture slots 和 run 本身放进 DLL，分别以
native 引用或 adapter Hold 为最后拥有者，均到最后 Drop 才观察 MEM_FREE。
所有事务精确核对内存计数/字节及非法释放；Plan 持有的干净帧可正常提交退役。
这些 DLL 实验与生成代码实验是独立证明，不充作完整生成模块联合卸载验收。

集中 discovery 2/3，46.330 秒：frame-ownership、continuation-scopes 通过，
async-storage 原三条运行通道全通过，新测试 TCC 也通过，GCC O0/O2 因测试
宏替换后遗漏原 ReserveSuccessor 函数声明而未构建，已补齐。工程审计的旧
Trace/cleanup 文本断言改为常驻 V2 布局/边界检查；独立运行时的源码列表也
接入 Windows 两种 DLL、Linux、公开源码宿主和独立验证宿主。所有修正均在
完整复验前完成，正式失败日志及各批次输入版本保留，没有删除合法语言用例。

唯一完整 verification 为 24/24、0 未运行、359.792 秒、峰值三；4146 项输入
前后一致，指纹
`68F123CE688B8183DE6E23BA7491D814D7869B30039956CA851986C9DFEFC565`。
原 2766 个测试文件保留：2762 不变，四个合同/接线更新，新增四个测试文件；
async-storage 原三条运行/两次生成保留，增至六条运行，各其他专项原人口逐项
对照未缩减。七项 DEMO6 工程检查通过；相邻活跃 XRT 原先不相关的
`extlibs/xllm-session/src/xllm_session_core.c:646` EOF 空行告警继续保留。

12 个既有生产文件变化，新增一个常驻生产模块；最终公开源码构建 68 TU、
15.266 秒（68 hit），29 合同驱动共享 23 TU、14.405 秒（23 hit）。这些命中
复用的是本批 discovery 的同源码对象，最终输入仍逐项重核。Cgen LF 字节
5,086,953 → 5,085,705，减少 1248；async-storage 生成 C 69,043 → 67,218，
减少 1825，Future callback 样例减少 1681，generator frame-loan 字节不变。
这不是 DLL 体积测量，也不声称生产源码总量下降。发布 CLI/DLL/VFS、嵌入 XRT
及默认 XLIR 后端未变；v187 XRT 91/91 只在重核 2003 项输入后复用。

下一批优先处理通用 async 的 pending output producer 和 Watch 的精确传递
拥有关系与准入。当前 real Watch 的保活已经实现，但 traced opaque policy
不会被 resolver 自动放行；不能把上述帧验收称为全部 pending 图已可回收。
封闭 producer 依赖 SCC 关停、必要 native/network、Job/生成器/async 与模块
联合真实卸载、独立 CLI 同源码 JIT/AOT、完整语言/平台/发布验收和旧路径退役
继续按原目标推进。证据入口：`build/tmp/xlir_launch_v190_evidence/results.json`。

### v189：生成器真实生命周期与可执行 CFG 的统一边界

新增 `XL6_GeneratorRuntimeV2`，控制对象和帧各自拥有真实引用；帧发布后还拥有
独立的共享代码租约引用，metadata 与回调代码保持到最后一次物理 Drop。每次
执行也持有真实控制引用，允许回调释放其他所有者，或在最后一次正常关闭中重新
Retain。语言仍按原有唯一拥有转移，额外 native/collector 引用不授权并发访问
payload。V1 公开控制布局、入口和原原生用例保留，但不认证其不透明回调。

Lowered IR 使用明确的 `RESIDENT_CLOSED_FRAME_V2`，保留历史 V1 枚举并拒绝它
作为新的可执行 Lowered ABI。Cgen 只序列化已验证的 activation 槽/初始化位/
清理顺序及 yield item 布局，不再为每个生成器生成 frame/item Trace/Drop
包装。模块 resolver 独立识别 control、frame，再要求每个传递子节点独立准入。
Plan Prepare 执行正常源 close/finally，frame Prepare 不代替控制对象履行职责；
Clear 只标记已准备对象，最终物理 Drop 才释放代码租约。

新增生成代码 fixture 形成 control → frame → object → finalizer context →
control 的真实环，同时 control 持有 yielded item；完整图六节点七条边。
GCC O0/O2/TCC 每配置执行 140 个事务、280 步源 finally（含 finally 内 yield）、
140 次对象析构及 context 释放、560 次跨线程 freeze、180 次真实外部根延期、
140 次漏接 frame 拒绝、3360 档计划创建 OOM（980 次实际失败）。40 次准备失败
包含有诊断与 false/NULL，重试不重复 finally；20 次析构失败保留已完成职责后
安全撤销，native Retain 复活阻止提交，独立保活的干净帧可晚于控制对象释放。
未通过重建已释放边来伪造重试图，每事务精确内存平衡。

另每配置 150 次最后拥有者关闭/释放重入、150 次稀疏 payload 析构，覆盖 Next
回调释放调用者所有权、关闭时控制复活、帧清理时独立 Retain/Release。100 次
真实 DLL 生命周期中，frame/item metadata 和 resume 本身位于 DLL；分别保留
control 或 frame 为最后引用，均到最后物理 Drop 才观察到代码页 MEM_FREE。
400 次跨线程 freeze、100 次真实分配失败和 250 次 ABI 拒绝均通过。

集中发现 2/3，131.915 秒：原生成器拥有图和 continuation scopes 通过；新增
合法动态值/finally fixture 在 IR 规划失败，且一个 native fixture 错把旧式
不透明析构当成已认证可分阶段回调。后者改为完整真实 lifetime/finalizer policy，
没有放宽旧式回调或修改 XRT。IR 根因是 finally 展开的四个 YIELD 中只有三个
从 CFG 入口可达：规划已跳过死路径，但机器覆盖仍要求全部四个。现由同一 CFG
可达性事实约束状态规划、块程序及验证器；保留原 IR 死分支，不赋予其可执行
挂起身份。未引用及已引用两个合法函数均验证 4/3，额外拒绝死路径的程序/挂起
身份和 V1 ABI，C/header 均不能输出部分 C。没有修改 Cgen 的语义恢复逻辑。

集中修正后只执行一次完整 24 门复验：24/24、0 未运行、345.231 秒、峰值 3；
4141 输入前后一致，指纹
`AA958FCC6F5C632BEBA172658F4D628432D1CE1A1FECAFFFC4B1768B44EF243B`。
原 generator-storage 的 72 条运行/48 次确定性生成保留，新增至 78/50；原五个
storage fixture 的 20 条畸形 IR 检查保留，追加十条 V2 边界检查。原 2759 个
测试文件中 2754 个不变，五个追加/更新合同接线，新增七个文件；原其他专项人口
逐项复核，没有 smoke 缩减。前序失败及每个修改过的输入版本保留。

十个生产文件改变；公开源码构建 67 TU、17.942 秒（66 hit），29 个合同驱动
共享 23 TU、15.582 秒（22 hit）。按 LF 字节，Cgen 5,089,128 → 5,086,953，
减少 2175；frame-loan 生成 C 88,777 → 85,346，减少 3431。async-storage 与
Future callback 样例逐字节不变，不据此推断 DLL 体积。嵌入 XRT、发布 CLI/DLL/
VFS、默认后端未变；v187 固定 XRT 的 91/91 是重核 2003 项输入后复用的既有
证据，不是本批新执行。

工程检查另发现一条旧断言要求生成器清理包装仍位于 Cgen，已改为验证实际常驻
清理及错误边界；七项 DEMO6 工程门通过。相邻活跃 XRT 仓库仍有不相关的
`extlibs/xllm-session/src/xllm_session_core.c:646` EOF 空行告警，未修改。
该审计脚本被公开 API 专项列为输入，因此另完整复验该一项，1/1、54.226 秒；
其他 23 项源码、用例、执行脚本和记录依赖均未变化。将唯一审计行恢复为其保存
哈希，可精确重现原 4141 行指纹；当前补验指纹单独记录为
`E3634C83C991A14CCC91D244AE5E93B12800E108D1EC00FB4B50DA0550C0FFD7`。
没有把两个输入指纹称为相同，也没有为独立审计修改重跑第二轮完整 24 门。

通用 async 堆帧/ticket 真实生命周期及传递子节点、封闭 producer 依赖环关停、
必要 native/network、生成 Job/模块及 generator 帧的联合真实模块卸载、独立
CLI 同源码 JIT/AOT 接线、完整语言/平台/发布验收和旧路径退役继续开放。
证据入口：`build/tmp/xlir_launch_v189_evidence/results.json`。

### v188：统一稀疏 activation payload 布局与清理

原 async/generator IR 已包含精确物理槽、destroy/trace ABI、初始化位和有序
destroy action，本批直接消费这些经验证的字段，不新增源名称猜测或 AST/HIR
回溯。Cgen 为两类帧输出同一种 `XL6_ActivationCaptureLayoutV1`：物理槽顺序
用于追踪，单独的 cleanup 索引序列逐项对应原 IR action，不能靠字段排列重猜
销毁次序。标量及受保护 loan 明确为不拥有的 TRIVIAL，初始化标记为 SIZE_MAX；
每个拥有槽有独立 unsigned-char 0/1 标记。生成器原 bool 标记也明确为此字节 ABI。

常驻 `xl6ActivationCaptureRuntimeV1Get` 验证槽宽度、范围、槽/标记冲突、完整且
不重复的清理序列，以及 metadata 与 payload 不重叠。仅访问已初始化的拥有槽；
标记和类型化空指针在同一 mutation 中脱离，实际释放在锁外。清理执行所有步骤，
以完整 error scope 保留首次失败（包括 false/NULL），保持独立的环境诊断。
task/callback 的原 V1 API 不变，和新 API 共用同一物理种类的 Trace/Drop 实现。

async 整帧 retire 与 generator 整帧 free 不再逐槽生成 release/trace；源 CFG 的
局部释放、转移、finally 与当前 yielded item 仍按原验证合同执行。本批不把新
payload metadata 冒充 frame/control 的生命周期证书：旧 generator 控制和帧
仍为唯一拥有，continuation ticket 的队列/Watch 引用也不是任意 collector pin。
后续须以真实可保活的帧存储及明确准备职责接入 Plan，不能只给旧 Count 加数字。

新增原生测试穷举 256 种八类拥有槽初始化组合，未初始化指针和借用槽使用毒值，
确认不误读、不追踪、不释放。300 组独立顺序/首次失败事务验证显式清理次序，
首次 false/NULL 与有诊断失败、环境诊断身份、重复清理；每程序共 1028 个真实
析构回调检查所属槽/标记已清空，并通过另一原生线程获取 freeze。成功清理在
FailAfter(0) 下无需分配，每个事务有精确分配/释放及非法访问计数平衡。GCC O0、
O2、TCC 均通过。原 task-storage 30 条通道保留，加三条至 33，原 14 次生成保留。
三份合同测试将实际 IR 槽及 action 顺序逐项对照生成 metadata，旧畸形 IR、
合法用例和运行人口不删除。

先集中执行 generator-storage、async-storage、task-storage 三门，3/3，
147.894 秒；后完整 24 门首次 22/24、0 未运行、326.364 秒，输入前后一致。
generator-ownership 在旧逐槽 C 文本断言失败，原生程序尚未执行；continuation
scope 的两个原生通道通过，两个 TCC 语言通道因手工宿主未绑定新入口而重定位
失败，不计为语言生命周期已执行。生产宿主符号表已接入。集中补齐实际 IR/layout
断言和测试宿主注册，原失败及其输入版本保留；生产代码未因这两个失败修改。

修正后完整复测 24/24、0 未运行、333.179 秒，并发峰值 3；4134 项输入前后
相同，指纹为 `3C31EAFF31D541756AAE2C0E9F1588FD66AA00ECC335E6228459B1589834780F`。
原 generator-ownership 的 GCC O0/O2/TCC 每配置仍执行 2400 个生成器生命周期、
1000 个 async 事务、2000 次真实挂起、16000 次图检查及 6600 次实际检查失败。
独立 TCC 宿主每优化级别通过 100 次真实关闭/恢复/闭包/调用/代码页释放生命周期。
本批实际执行了两次完整 24 门（首次有失败），不能表述为只跑一次或首次全绿。
原 2757 个测试文件中四个追加布局断言/接线，其余 2753 个字节未改；新增两个
测试文件。文档后只复核输入及工程检查，不重复第三次整批。

本批 IR/lowering 和固定 XRT 嵌入源码未改，不同步并发上游改动。原生 91/91 是
v187 固定来源的既有证据，已复核其 2003 项输入未变后复用，不虚报本批重跑。
公开源码构建 67 TU、16.979 秒（61 cache hit）；最终合同构建 28 驱动、23 TU、
11.711 秒（23 hit）。发布 CLI/DLL/VFS 和默认后端未改。

按 LF 字节计，Cgen 源 5,087,862 → 5,089,128（+1,266）；加入布局序列化，
不是二进制增长测量。生成 C 不保证逐文件缩短：frame-loan 样例 87,521 →
88,777，async-storage 69,392 → 69,043，Future callback 259,842 → 259,784。
本批成果是取消两套逐槽运行逻辑的生成展开，不能将其宣传为全面源码压缩。

帧实体及传递子节点准入、封闭 producer 依赖环关停、必要 native/network、
生成 Job/模块及 generator 帧的联合真实卸载、原单独动态迭代 CLI C-host 的
同源码 JIT/AOT 接线和完整语言/平台/发布验收继续开放。
证据入口：`build/tmp/xlir_launch_v188_evidence/results.json`。

### v187：托管原生快照游标与完整迭代拥有环

旧 `xvalueiter` 同时支持栈式和独占堆式使用，不能把其唯一拥有计数当作可保活的
collector 引用。本批新增不透明 `xvaluecursor`：Create/RCreate、Retain/Release、
Advance、Ownership 与独立 V1 adapter 共七个入口。堆节点内联拥有快照状态，
计数来自真实引用；只追踪实际 backing 和析构 identity 对象的实际 shell，
不把借用 item/key 或内联状态虚构成额外节点。推进及错误尾部拒绝图准入，
Clear 只移动拥有槽，Finish 在 freeze 外释放，可重复且不重复副作用。
原 `xvalueiter` 公共布局及 Begin/End/Create/Destroy 行为不变。

常驻 `XL6_ValueIterator` 改用这个托管子节点，模块 resolver 分别认证 wrapper、
cursor 和各个传递子节点。IR、lowering、Cgen 源本批逐字节未改，原 Future
样例生成 C 也逐字节不变；没有在 Cgen 中增加生命周期判断分支。

新增真实循环 fixture：语言 iterator → 原生 cursor → 对象 backing/实际 shell，
对象的拥有式 finalizer context 再保有该语言 iterator。加上真实 lifetime 和
字段值，共六个节点、七条边；所有权只来自这些真实槽。GCC O0/O2/TCC 各验证
75 个完整图、100 次真实外部根延期、75 次漏接原生子节点的拒绝、1800 档计划
创建 OOM（525 次实际失败）、75 次语义析构和 context 释放、150 次跨线程 freeze。
覆盖原始对象根、析构失败后撤销/重试、析构期间重新 Retain 原生游标阻止提交，
以及共享 Plan 的 Prepare/Finalize/Commit/重复 Finish；语义析构不重复，逐事务
分配/释放、存活字节和非法访问计数平衡。这是已明确授权 resident 对象策略的
运行时环证明，不是生成 async/generator 帧或生成模块的真实卸载证明。

原 task-storage 27 通道保留，新增三条至 30；原七组/14 次确定性生成、完整
动态迭代 5180 次人口、旧 Job 图、callback、Plan 及 pending 模块原人口保留。
既有 2756 个测试文件中仅捕获布局测试追加原生子节点独立准入断言，另增一个
完整循环测试；其余 2755 个文件字节不变。

先做原生与循环链的定向发现，修正两处诊断脚本的路径/数组组装问题；原生及
循环程序无需语义放宽即通过。随后只执行一次完整 24 门 verification：24/24、
0 未运行、352.647 秒，并发峰值 3。4132 项输入前后一致，最终指纹为
`56A20DE92E97B23B287EF9C31A4DA39CE5976B61730C450F6B127CA99C4EEC8F`。
没有第二次重复整批。定向探针之后仅修改的 scope 说明及失败脚本版本，按原始
SHA256 精确匹配保存，未把修正后的脚本冒称失败时输入。

XRT 以 v185 固定 vendor 为基线，只加入游标源码、声明、文档及两份新测试，
不导入并发 JSONL/XLONL/xllm-session 改动。原 34 组/89 个程序源文件全部保留，
新增 modular/single 两个程序后，GCC/TCC 各 91/91，耗时 112.705/26.629 秒，
2003 个原生验证输入前后一致。新增原生程序各执行 400 个快照图、1600 个实际
pin、400 次清理生命周期、3200 档 OOM 和 400 次实际失败，保留旧栈式/独占堆式
测试。四份生成头经原成熟度/生成检查正式同步；API 快照为 3016/3467（451 自有）。
这是固定来源的新鲜原生运行结果，不是整个并发上游 checkout 的通过声明。

公开源码 67 TU、38.729 秒（5 cache hit），28 个合同驱动共享 23 TU、19.873 秒
（1 hit）。本仓库七项工程检查及本批七个 XRT 路径检查通过；无关 xllm-session
EOF 空行告警保留。发布 CLI/DLL/VFS 和默认后端不变。完整 async/generator 帧、
producer 依赖环关停、必要 native/network、生成 Job/模块联合卸载证明、旧动态
迭代 CLI C-host 的同源码 JIT/AOT 接线及完整语言/平台/发布验收继续开放。
冻结入口：`build/tmp/xlir_launch_v187_evidence/results.json`。

### v186：精确 async 捕获销毁 ABI 与常驻迭代器存储

`asyncCaptureDestroyAbis` 与参数捕获/追踪向量并列，由 IR 类型和所有权合同
统一确定，验证器拒绝缺失、游离及错误析构种类，文本/JSON 完整保留。Lowered
任务存储规范改为封闭布局 V2；旧 opaque V1 原生 API 继续独立测试，但不能
冒充 Lowered 规范。Cgen 不再为普通 worker-pool async 生成逐字段 Drop/Trace，
只输出物理布局并调用共享 Cleanup，保留 TAKE 转移、失败回滚及首个错误。

共享 Capture 支持九种物理载体；新增常驻 `XL6_ValueIterator` 持有一个真实
XRT cursor，视图中的 item/key 为借用，不重复计边。推进期间拒绝图检查，
共享计划使用真实 Hold/Drop 引用与 Claim/Clear/Finish。这里只认证 wrapper：
原生 cursor 子节点仍缺生命周期适配器，因此不能宣称任意迭代器环已可回收。
`xl6ContinuationExecutorIsIdle` 观察真实队列 live 计数，包括物理释放和 code
owner 尾部；不停止生产者，也不能单独作为卸载许可。

原 task-storage 18 条原生通道全部保留，新增九条至 27 条；七组生成用例各做
两次确定性验证。原八个 task IR fixture 保留，636 次畸形 IR 在 C/header 输出
前拒绝。GCC O0/O2/TCC 各执行：九类布局的 100 次逆序清理与跨线程 freeze、
400 个真实迭代器快照、12 档 OOM；六类真实生成 async 捕获的 100 个成功/失败
事务和 24 档 OOM；完整动态迭代人口 5180 次执行、3620 次真实投影/cell 分配
故障（480 次在 pending 恢复后）、160 次 generator 关闭，逐例分配平衡。

首次完整 discovery 为 21/24、0 个专项未运行、323.765 秒。generator 专项的
旧生成文本断言先失败，未产出子报告或运行原生程序；不把专项已启动误计为所有
子程序已运行。其他失败包括旧 ABI 断言/漏接 generator 源、生成 C 格式警告，
以及测试对异步执行/释放时序的同步假设。修正宿主后等待 Future 终态和实际
executor idle，不轮询分配数来伪造释放证明。新增测试的生成可达性、generator
trace 参数及闭包 bool/输出参数调用 ABI 也经诊断修正，原失败均保留。

集中修正后只做一次完整 verification：24/24、0 未运行、332.293 秒；两批
并发峰值均为 3，4131 项输入分别前后一致。最终指纹为
`9B74259006324B66284862BD499079C25DE9CC9CB421DE839D8334ACF17ED4BF`。
相对批前 2752 个既有测试文件，七个按新合同/实际时序修改，其余 2745 个
字节不变，新增四个文件，没有删减原专项人口或合法功能断言。

公开源码最终构建 67 TU、16.232 秒（64 cache hit）；28 个合同驱动共享 23 TU，
最终 12.611 秒（23 hit）。按统一 LF 字节口径，Cgen 源净减少 1335 字节，
不是发布 DLL 体积测量。打包检查覆盖 68 个私有头映射和 64 处源文件清单。
本仓库七项工程检查通过；独立上游 xllm-session 的 EOF 空行告警单独保留。

本批没有修改或同步 XRT：复核 v185 固定 vendor 的 2001 项输入与四份嵌入头，
复用其 GCC/TCC 各 89 个程序的证明，不能写成新跑了 v186 XRT 测试。发布
CLI/DLL/VFS 和默认后端未变。原单独动态迭代 CLI C-host 门仍需同源码 JIT/AOT
接线；本批新增独立 TCC 原生通道不替代它。其余 async/generator 帧、原生
cursor 准入、producer 依赖环、必要 native/network、生成模块 Job 卸载联合
证明及完整语言/平台/发布验收继续开放。
冻结入口：`build/tmp/xlir_launch_v186_evidence/results.json`。

### v185：原生 Job 与 V2 语言任务帧参与真实拥有图

XRT 为所有原生 Job 输出建立实际 Future → Job 引用，执行器独立持有另一条
真实引用。新 `xrtTaskSubmitOwnedJobV1` 用不可变数据策略描述已转移的 Data
引用，同时保留原结果策略检查；Job 只追踪实际 Promise、取消令牌和任务数据，
不把借用 Future 或队列链接重复计边。旧的 opaque 数据不能被误当成无 producer
的 pending 源，也不因增加 producer 边而获得回收认证。

语言 V2 任务帧与其 forwarding Watch 使用封闭布局/不可变策略，实际源码宿主及
TCC 符号接线已接入 Frame、Job、Future V3 的独立准入。执行/释放期间拒绝图检查；
preparation 等待正常执行、捕获清理、结果转发及执行器归还职责，不取消、摘队列
或跳过已受理任务。帧的真实代码 lease 保留到最后一个物理引用释放。

新增完整图门纳入原 task-storage 门，原 15 条通道保留，新增三条至 18 条。
GCC O0/O2/TCC 各运行 200 个 raw/language 任务事务：raw 为 10 节点/12 边，
language 为 12 节点/18 边；两个类对象各包含 shell、backing 和 lifetime 三个
实际节点。外部结果释放后仍有真实执行器根。合计 600 个图、1200 次真实根延期、
1200 次排队 BUSY、600 个正常终态、1200 次类析构、450 次执行中前置拒绝及
1200 次析构跨线程 freeze，实际执行共享 Plan 的准备/析构/Clear/Finish，逐事务
分配、释放、存活量及非法访问计数平衡。此处不是生成模块的原生 Job 卸载证明。

完整 discovery 为 **23/24**、0 未运行、323.787 秒。task-storage 中旧图仍用
opaque 提交，新增 producer 边使完整追踪拒绝；新用例还漏算类 backing/lifetime。
保留六条失败通道后，旧图用例显式选择数据/结果合同，新图按真实节点计数，
原有样本和语义断言不减。架构门同步检查新的完整数据/结果入口与实际宿主接线。
仅一次完整 verification：**24/24**、0 未运行、293.824 秒，两批峰值均为 3，
4125 项输入分别前后同哈希；修正阶段没有新增生产代码或 Cgen 补丁。

独立 XRT 有并发 JSONL/XLONL/xllm-session 工作，本批不覆盖或顺带引入。第一次
尝试复现 v184 开始时的上游源码时发现已有未生成到嵌入头的 JSONL 改动，故保留
失败记录，改用 v183 最终实测输入；四份基线头重新生成后与原嵌入头逐字节一致。
只覆盖本批 Job 源码、测试和文档，再通过原成熟度/单头检查及正式同步入口导入。
快照为 3009/3460 API（451 自有）。固定 vendor 源码下原 26 组/68 个程序保留，
新增两个 Job 入口与八组/19 个 task/group/pool/coroutine 程序；GCC/TCC 各
**89/89**，112.602/32.867 秒，所有 2001 项测试输入随后仍一致。这不是并发上游
checkout 的全库验收。全上游 whitespace 检查一度报 xllm-session 的非本批 EOF
空行，保留原失败；本批八个 XRT 路径的独立检查通过，没有修改该无关文件。

公开源码 66 TU、40.969 秒，26 合同驱动共享 23 TU、20.235 秒；IR/lowering/Cgen
源及原 Future 样例生成 C 未改。发布 CLI/DLL/VFS 和默认后端未变。
其他 V1 task/async/generator 帧的完整布局认证、封闭 producer 依赖环关停、
必要 native/network 合同、原生 Job 与生成模块的实际卸载联合验证，以及完整
语言/平台/发布验收继续开放。冻结入口：`build/tmp/xlir_launch_v185_evidence/results.json`。

## v184 历史批次

### v184：真实宿主中的 pending 通知、对象析构与代码页释放

新增 `xlir_pending_module_preparation.xl`，由当前源码编译器生成两个 finally
闭包，捕获同一个具有语义析构的类对象。原生 Promise 槽实际存放在 TCC 模块
数据段，通过不可变 Trace/Finish/Prepare 描述符与生成模块合同明确组合。
语言目前不暴露 Promise 值，故不把该原生槽伪装成生成器自动支持的语言全局；
也不把 Promise 指针误当 Future，不增加假拥有边、宿主假根或额外 guard lease。

源码宿主实际执行 Create/Activate/BeginCall/Destroy 和拥有图收集，TCC 实际
编译、重定位及删除代码页。GCC O0/O2 宿主各运行八种模式，每种 25 个事务：

- 无外部引用、仅外部结果引用、回调中捕获对象重新变得可达、仅外部生产端引用。
- 外部生产端完成 resolved/failed/cancelled/closed 四种终态；最终结果及生产端
  保留到代码页释放之后，再读取状态、值/错误并释放。
- 全部 400 次生命周期中，800 次有序 finally 通知先于 400 次类析构；1200 次
  跨线程探针验证 API 锁与 ownership freeze 可获取、实际代码页仍为 MEM_COMMIT，
  并重入收集器确认不能提前卸载。350 个真实引用/复活延期，逐事务零 live delta。
- 直接断言源码宿主的运行阶段：400 次通知来自拥有图 Prepare，150 次对象析构
  来自图 Finalize 且早于 Clear。复活模式恢复为外部引用保有后的普通释放；外部
  完成模式走正常 Future 通知，不把这些事件虚报为图准备/图析构。
- 最终通过真实 `tcc_delete` 并检查原代码地址 MEM_FREE，不以回调计数或空指针
  代替页释放证明。两个生成回合的 C、声明头和 IR JSON 完全一致。

新独立门 `check_xlir_pending_module_preparation.ps1` 接入拥有图批次，原 23 门及
所有旧样本保留。首轮 discovery **24/24**、0 未运行，306.965 秒。复核后补强
上述宿主阶段断言，并将混合模块的 unchecked Unit 错误转交与生成入口保持一致；
这不是首轮运行失败，也不是生产实现修复。随后仅一次完整 verification：
**24/24**、0 未运行，308.984 秒，两批均峰值 3、4124 输入前后同哈希。

生产 src/lib、Cgen 和原 Future 样例生成 C 均未修改；复核全部 1314 个共享构建
输入后，复用 v183 最终 66 TU/26 合同驱动。未重建 release CLI/DLL/VFS，也未改
默认后端。独立 XRT 仓库同期有非本批的 JSONL/XLONL 工作，本批不覆盖或集成；
历史 GCC/TCC 各 68/68 仍是 v183 证据，不标成当前 XRT checkout 的新复测。
本批实际运行的是哈希固定的 DEMO6 嵌入运行时，仓库内快照检查仍为 3006/3457 API。

完整原生 Job/其他 task 与 async 帧认证、封闭 producer 依赖环关停、其他必要
native/network 合同，以及完整语言/平台/CLI/DLL/发布验收继续开放。此批关闭的是
上述混合 pending 模块的真实卸载证明，不是任意原生槽或全部异步图的完成证明。

冻结入口：`build/tmp/xlir_launch_v184_evidence/results.json`。

## v183 历史批次

### v183：正常通知先于捕获对象析构，每项语义职责后重建图

XRT 新增独立的 preparation 描述符与 certified Watch 策略，旧 lifecycle/Watch
存储 ABI 保持不变。Future V3 在子 Count/Trace 前匹配确切策略身份，返回同一
物理生命周期表及强制准备表；V1/V2 仍拒绝注册 Watch。只有没有实际 producer
的 pending 源可以通过正常 PromiseClose 通知；中间结果等待生产者，不通过
摘 Watch 跳过 catch/finally。回调帧保有真实代码 lease，只有正常通知和 cleanup
完成、传输槽退出后才允许 Clear。

上层 Plan V2 在每项准备及每次对象 Finalize 后都重新构建拥有图。回调复活对象
或创建新准备职责时，后续析构停止并重新检查；BUSY 不代替已受理工作。源码宿主
实际接入 V2，并明确区分快照/准入 ERROR 与语义 FAILED：临时分配失败可撤销后
重试，不能永久标记模块析构失败；false/NULL 的语义失败及已执行职责仍保留。

一次完整 discovery 为 **22/23**、0 未运行，284.246 秒；新两级 finally 图在
GCC O0/O2/TCC 都于准入处失败。定位到普通 Value 适配器把内联字符串误判为
外部借用：OWNED_DATA 只标识独立 Take 分配，内联数据实际随 Value 同分配拥有。
修复只认证这两种已知物理布局，仍拒绝未知外部字节。另用实际 TCC 模块证明
原 V2 快照 OOM 会错误触发 retirementFailed，随后从步骤结果协议统一修复。
这两项都没有在 Cgen 增加推导、AST 回查或专项分支。

集中修复后只运行一次完整 verification：**23/23**、0 未运行，297.590 秒，
峰值并发 3；4120 输入前后同哈希。原测试分母保留：

- Future callback 从 24 增至 27 条通道。每配置 100 个实际生成的两级 finally
  图，50 个外部只保有结果的延期，200 次有序通知先于 100 次类析构；完成整个
  Trace/Prepare/Finalize/Clear/Finish，逐事务零存活增量，无假根或 guard lease。
- Plan 门从 9 增至 12 条通道。新增 150 个循环，600 个描述符前置拒绝、150 个
  BUSY、50 个显式准备失败、25 个复活、50 项新准备职责、200 次通知与 300 次
  析构；200 次跨线程 freeze。另有 25 次通知完成后的实际快照分配失败，不重放通知。
- 原 221 个真实 TCC 事务保持，新加每配置 50 次 Prepare/Finalize 重建 OOM；
  均不记录语义失败，原图保持，重试后代码页 MEM_FREE 且内存平衡。

XRT 初测 GCC/TCC 各 68/68；内联数据修复后扩展测试先暴露 Clone 物理计数假设，
两编译器各执行 5/68；校正为六个节点/十二个实际拥有槽后，单头构建又暴露
比较 API 依赖，各执行 42/68。保留两轮失败及未执行身份，只修测试。最终原
26 组、68 个 modular/single 程序在 GCC/TCC 全部重跑通过，73.369/19.719 秒。
新 blob 测试每执行 200 个容器图、1200 个物理值、2400 个拥有槽；certified
Watch 每执行 100 个真实循环和跨线程 freeze，旧 producer/普通适配器分母不变。
DEMO6 的 4120 输入复核仍一致，未假称第三轮全量验证。

XRT/xruntime 快照 3006/3457 API（451 自有）；源码发布接线已同步。Cgen 源码与
原 Future 夹具生成 C 本批不变，发布 CLI/DLL/VFS 和默认后端不变。原生 Job、
任务/其他异步帧的完整认证、封闭 producer 依赖环的明确关停协议，以及新 pending
语言用例的真实宿主卸载联合证明仍开放；其他必要 native/network 合同和完整
语言/平台/发布验收也不能由本批专项绿色代替。

冻结入口：`build/tmp/xlir_launch_v183_evidence/results.json`。

## v182 历史批次

### v182：外部结果能够真实保有生产帧

新增 XRT ProducerBindTakeV1：只有新建、私有、pending、无 waiter 的
Future/Promise 对能接管一个实际生产者引用。绑定无分配，不调用生产者的
Count/Trace/Drop；唯一终态在锁内摘边，在自己的锁/mutation 外精确释放，
早于输出通知，保留原错误。新的 AdapterV2 以独立的不可变策略身份准入
producer 槽；V1 拒绝活跃 producer，V2 仍不认证孩子、仍拒绝已注册 waiter。

Future callback 的 V1/V2 和 language-output task frame 在输出发布前实际
增加并转移一个帧引用，失败回滚和提交拒绝只返回各自真实的 provisional
引用。帧的最终 lease release 在自身 mutation 外，保留原活动/注册/job
引用及真实代码驻留尾部。没有增加 Cgen 语义分支、假 Trace 边、计数扣减、
测试 guard lease 或 native 根登记。原生 task.spawn 的 Job 输出尚未接入
该生产者合同，不把 language-output 包装层等同于全部 native Job 完成。

新增 output-only-root 夹具通过真实 CodeOwner acquire/release 和 Promise
拥有槽构造循环。每配置 200 个外部只持有输出的图：全部七节点可达，十条
真实拥有边，唯一外部根为结果。100 次释放观察者后全图无根，但回调不被
取消或跳过；随后用新取得的真实 Promise 引用完成四种终态，各有 200 次
已受理回调和 cleanup、零 live delta。原生成语言 then/catch/finally 测试、
125 次 IR 变异拒绝、1000 次注册竞争与 DLL Close 后回调等分母保留。
Future callback 门由 21 增至 24 个运行通道，GCC O0/O2/TCC 全通过。

完整发现批次一次 **23/23**，0 未运行，274.043 秒、峰值并发 3；4116 个
输入前后同哈希。XRT 初测 TCC 68/68，GCC 因新测试未使用 testRequire 的
严格编译警告中止（2/68 执行），该失败/未执行证据保留。仅改测试调用已有
辅助函数后，重新执行原 26 组加两项 producer 测试：GCC/TCC 各 **68/68**，
71.938/18.795 秒。没有改动生产源码；重算完整批次输入仍为
95AC938CFEFFED80A07069C1B7723CEBBEEC5BC952938C96CF32D5090670C89A，
因此复用已通过的 23 门，不冒充又跑过一次全量 verification。

XRT/xruntime 快照为 3004/3455 API（451 自有）。源码发布接线、两个生产者
策略 getter 和真实 TCC 宿主绑定已同步，发布 CLI/DLL/VFS 及默认后端不变。
下一步必须解决 pending Watch 的关闭/通知与捕获对象语义析构顺序，再接入
完整帧和原生 Job 参与者；只摘 Watch 会跳过 finally，不能作为收集实现。
本批只有真实拥有关系及其生命周期证明，不是任意 pending 图的收集授权。
冻结入口：`build/tmp/xlir_launch_v182_evidence/results.json`。

## v181 历史批次

### v181：普通终态释放的隔离闭环

XRT Value/backing 在短 mutation 内完成实际引用归零和节点隔离。已认证类在
该 scope 外执行真实 receiver 的 Finalize，保留字段可访问/修改、弱引用不可
复活、finalizer 内游标延后释放，以及 context/lifetime/code 的真实拥有尾部。
普通四类容器的私有终态 backing 在 scope 外传递释放子对象，不暂停任何外部
调用方的 scope，也不引入假引用或合成 receiver。原生游标先脱离自己的拥有槽。

常驻 closure、cell、closure scope/payload/box 的终态路径遵守同一分相规则。
已知 callable/Future Value 桥通过新增 HandleOwnershipBindPhased 显式接入，
DeepClone 继承合同；trace-only Handle、未知 legacy finalizer/lifetime 保持保守。
这个新入口只描述普通 Drop 的隔离责任，不代替完整图准入或认证未知 native。
生成 C 和 IR 本批未增加新语义分支。XRT/xruntime 同步为 3002/3453 API（451 自有）。

一次完整 discovery 为 20/23、3 失败、0 未运行，270.247 秒；集中修正测试合同
和接线后，一次完整 verification 为 **23/23**、0 未运行、265.672 秒。两次
均峰值并发 3、4115 个输入前后同哈希；修正期间没有改动已构建的生产源码。

- v180 默认布局与独立 class/closure 的九条隔离失败通道全部通过，没有使用
  values-only。Future callback 原十八条通道保留，扩为二十一条且全部通过。
- 新增 21 路径 ×25 的传递释放，三种配置各 526 次真实 finalizer、528 次真实
  跨线程探针，涵盖四类容器、COW、普通及析构内游标、cell/closure scope、
  Future/closure 载荷、callable/Future Value 深拷贝、原 receiver/弱引用，以及
  实际语言 cleanup scope 的首错误保留；显式 native phase 和外部 scope 负例
  都保留。所有配置逐事务分配平衡，无新增 native 根登记或测试代码租约。
- 旧 resident-scope 测试仍覆盖全部 callback/factory 数量。已认证 class 的旧
  “析构期间必须冻结失败”断言改为冻结成功且 receiver Count/准入必须拒绝；
  原 legacy callback/factory 负例保持。空 stdout 的失败现在也完整记录所有配置。
- 退役测试不再假设第一次 Destroy 必然已经调用 Prepare。确定性反例证明并发
  mutation 可使 prepares=0、对象与字段均未释放；新增 150 次外线程准入延期，
  原 18 模式/900 生命周期/2700 内部 finalizer 和第一次实际 BUSY/FAILED 的
  全部断言保留。未通过增加 Join/Quiesce 隐藏资源准备问题。
- 修改 XRT 后实际重跑原 26 组、每编译器 66 个 modular/single 执行，GCC/TCC
  均全通过；不是复用旧报告。发布 CLI/DLL/VFS 和默认后端未改。

当前修复不等于任意活动图已经可收集。下一步仍是 pending Watch/producer 与
task/callback frame 的完整传递拥有关系、必要 native/network 资源合同，以及
更广的同源语言和发布验收。冻结入口：`build/tmp/xlir_launch_v181_evidence/results.json`。

## v180 历史批次

当时是未通过全部硬门的开发检查点，v179 是此前全通过的冻结证据。

### v180：封闭捕获布局接入与普通析构隔离反例

任务回调及 Future then/catch/finally 的捕获槽，现由 verified transfer/destroy/
trace ABI 发射为 `XL6_CaptureLayoutV1`。常驻 capture 运行时统一验证尺寸、种类、
范围和重叠，逐槽追踪真实子节点，按逆序以相应类型的 NULL 脱离拥有槽后释放，
保留首个错误。生成 C 不再为这些回调复制 frame Drop/Trace 和逐字段清理函数。
任务与 Future 分别通过独立 V2 Allocate/Cleanup 接入同一个布局执行器；原 V1
函数表、注册/提交及释放入口保持不变。其他 async frame 家族仍有其原有合同，
本批不声称它们已经全部迁为这一布局。

Lowered callback storage 必须选对应 V2；两种旧 V1、未知值及错误的 transport
组合都在 C/header 前拒绝。原 25 个边界的 75 次变异保留，增加旧 V1 拒绝后为
125 次；原 task storage 的 334 次变异和原三个 fixture/九条运行通道仍保留。
Windows/Linux/DLL/嵌入编译的源码和私有声明头接线已增加 capture 运行时，未重建
发布 DLL/CLI/VFS，也未改变默认后端。

完整 discovery 为 21/23、2 失败、0 未运行，273.600 秒、峰值并发 3，4113 个
输入前后完全相同。一项是白盒模块宿主同时链接 capture 源码和共享对象；仅修正
排除清单后，原 GCC O0/O2 实际 TCC 门补测通过。当前有效结果为 **22/23**，不是
一次完整全绿验证。工程检查为 8/8。

另一项是真实的语言生命周期缺口：普通 `xrtValueRelease` 的末次释放，在外层
ownership mutation 内调用类析构；常驻 closure 的普通释放又可能叠加一层
mutation。析构等待另一线程的 freeze 时不能前进。图计划的 Finalize 已有独立
阶段，普通释放尚未遵守同一隔离边界。新布局的默认硬门在 GCC O0/O2/TCC 均保留
失败；另加不调用 capture/task/Watch 的直接 class/closure 最小反例，三种配置
均复现。最小反例会汇合线程、完成一次析构和逐事务分配平衡后明确返回失败。

仅用于定位的 `--values-only` 在三个配置通过：每个配置 325 种/次畸形布局检查、
200 次激活、100 次分配失败、100 次无分配回滚和 600 次反序 finalizer、零 live
delta。它不执行跨线程隔离探针，**不计作默认硬门通过，也不替代失败的断言**。

下一步先统一普通 class、closure 及传递子对象末次释放的隔离和代码保有合同，
保留真实 receiver、错误结果、字段访问和返回尾部，再接 pending Watch/producer
与 task/callback frame 图。不能只摘 Watch 而跳过 finally，也不能让布局本身
授权未知 native 回调进入收集器。活动任务、完整 native/network 资源合同、同源
语言及 CLI/DLL/平台/发布验收仍开放，长期目标不结项。

证据入口：`build/tmp/xlir_launch_v180_evidence/results.json`。

## v179 历史批次

记录状态：2026-09-13，v179，开发中而非统一 IR 最终验收。

模块退役加入独立 V2 资源准备合同：Typed 阶段拒绝提前选择，Lowered 必须是
PREPARED_GRAPH_V2，旧 V1 值及未知值均在 C/header 前拒绝。V2 组合不变的 V1
graph/finish 描述符，分别验证尺寸、tag、同一 graph 身份和 prepare 入口；存在
未知或畸形 V2 时不得退回 V1 凑出准入。有效旧 V1 无 V2 的模块继续兼容。

宿主在停止新入口后先准备资源，持有一个真实 preparation 引用并排除竞争退役
驱动；调用期间不持 API 锁、mutation 或 freeze。资源准备不释放 global、不执行
它们的语义析构，也不把初始化阶段提前改为已清理。READY、BUSY、FAILED 独立于
诊断分配；后两者保留状态供重试。随后才创建拥有图，完成 Finalize、重验、Clear
和 Finish。全局对象与实际代码能力保留到各自的真实释放边界。

任务资源的自动准备必须不等待活跃生产者：新增常驻 xl6TaskPoolTryQuiesceV1，
保留原 V1 表和同步 PoolQuiesce。新入口先关闭受理，有提交、运行/通知尾部或
当前线程的自身任务时返回 BUSY；原生池严格 idle 后才回收空闲线程和资源。
不取消已受理任务，不通过统计中的单一 Running 数猜测全部回调已结束。
生成代码共享资源准备程序，由固定的退役/显式清理 ABI 选择非阻塞或同步模式；
两者的顺序和资源种类都来自已验证的 taskPoolAbi/continuationAbi。活动或尚未
销毁的 continuation 服务、未知 network/native 资源仍不能冒充空叶进入图。

本轮保留了一个重要负对照：首个完整批次 23/23 通过之后，新增真实 TCC 宿主
让任务等待调用方后续信号，证实旧准备实现会阻塞 Destroy。集中修复共享入口，
加入等待外部信号和任务自身退役两种永久场景后，再执行必要的第二次完整批次。
两次均为 23/23、0 未运行、峰值并发 3；分别 255.925/259.552 秒，各自 4110
输入前后相同。这不是“一次完整批次加测试配置补验”，两个源码版本分别归档。

- 新门为 18 模式 ×25 ×宿主 O0/O2，共 900 次真实 TCC 生命周期、2700 次内部
  finalizer；完成后检查真实 MEM_FREE 和逐事务分配平衡。最初 16 模式全部保留。
- 六种内部/外部 Future、COW、提取 class 与嵌套结果路径不调用测试专用 join；
  真实开放任务池由生产宿主准备并回收。250 次畸形 V2 拒绝、50 次有效 V1 兼容、
  50 次继承 mutation 延期及 100 次活跃生产者/自身退役包含在新门内。
- 每次 prepare 都由另一真实线程取得 API 锁及独占 freeze，证明没有带锁等待；
  重入 collect 不会删除被保有模块。失败无诊断和原错误优先各有 50 次检查。
- 原 22 门及 Future 300 次生命周期、对象/模块环、task/generator/continuation
  等原分母未缩减；2734 个旧测试逐字不变，三个有明确 ABI/接线差分，新增三个。
- XRT 源码及发布 CLI/DLL/VFS 未改。上一轮 XRT GCC/TCC 各 66/66 仅在完整输入
  和日志哈希不变后复用，不冒充本轮重跑。能力矩阵因现有 API 引用增加而更新，
  延期清单不变；默认后端本轮未改。

下一步仍需接入 pending Watch、task/callback frame 与 native 的完整传递拥有图，
以及必要 native/network 服务的资源合同，再做更广的同源语言、CLI/DLL、平台与
发布验收。资源准备并不等于任意活动对象图已可收集，长期目标不结项。
冻结入口：`build/tmp/xlir_launch_v179_evidence/results.json`。

## v178 历史批次

记录状态：2026-09-13，v178，开发中而非统一 IR 最终验收。

本批把已认证 class/closure/callable 图扩展到 Future 的物理控制块、转发 Owner、
嵌套结果、Error/Cause、取消 Parent 及 resident Future Value 桥。Future/Promise
仍共用一个真实计数节点；所有载荷、waiter 和子节点均须在 Count/Trace 前独立
准入，不将未知回调视为空叶。无观察者 pending 生产端在最终 Clear 才 CLOSED，
取消自身而不取消父令牌；真实拥有引用延迟到 freeze 外 Finish 释放。

六种结果载荷（含 closure）统一为常驻策略。闭包 Drop 原本就已常驻，本批统一
的是发布认证与选择方式，并非声称删除了原本不存在的生成析构函数。经 task
pool 完成的结果也在提交前选定策略：实际 Drop/context 不匹配时，在任务数据
释放和结果发布前转为 FAILED。Cgen 只以已验证 AsyncResultAbi 选择种类，移除
闭包结果的专用发布分支；原 trace-only API 兼容但不自动获得回收资格。

模块资源边界保持严格：已验证 guarded task pool 只有关闭并完全排空后才允许
追踪全局图；活动 pool、已注册 waiter、未知 native/network 服务仍拒绝。新增
真实 TCC 用例由宿主显式完成 pool quiesce，再检查别名与最终卸载；不能把这个
结果写成任意 async 模块已经可以自动准备、停止和回收。

验证结果：一次完整 22 门批次为 16 通过、6 失败、0 未运行，221.215 秒，门禁
峰值并发 3，4106 输入前后相同。六项分别为测试链接源遗漏和白盒提交参数迁移，
集中修正后定向补测全过，生产源码未再改；107 份已有 C/header/IR 产物字节不变。
有效主门禁为 22/22，不称第二次完整批次全绿。另补 capture、Future 与 aggregate
原有回归，最终均通过；额外旧脚本的源依赖遗漏也有失败记录和定向补验。

- 新增实际 TCC：六模式 ×25 ×宿主 O0/O2，共 300 次生命周期、900 次内部
  finalizer；外部 Future/COW 包装别名、取出的 class 及嵌套 Future 均保护代码。
- 物理计划 GCC O0/O2/TCC 各 225 图、289 次唯一 finalizer、189 次生产端关闭、
  50 次观察者前置拒绝；64 个 OOM 探针实际触发 22 次失败，恢复和重试保持状态。
- XRT 原 25 组 62 项保留，扩为 26 组 66 项/编译器，GCC/TCC 均全过，1997
  输入稳定。新增 Future 自环和 task 错配用例同时覆盖 modular/single。
- 原 Future 700 图、1500 载荷用例及 aggregate 3500 事务均在三个配置通过；
  旧测试文件 2728 个字节不变、五个有明确迁移差分，新增四个，未删原测试。

XRT/xruntime 快照为 3001/3452 API（451 自有）。发布 CLI/DLL/VFS 及默认后端
未动，独立 xllm 配置保留。Cgen UTF-8/LF 文本为 5,087,770 字节，较 v177 净减
37 字节；主要新增职责位于常驻运行时，不据此推断 DLL 大小。

下一步仍是核心语言工作：为模块加入明确的退休准备/资源排空合同，继续接入
pending Watch、task/callback frame 与 native 的完整传递拥有图，随后进行更广
语言验收。标准库仅保最低要求，不切换默认后端、不宣布长期目标完成。
冻结入口：`build/tmp/xlir_launch_v178_evidence/results.json`。

## v177 历史批次

记录状态：2026-09-13，v177，开发中而非最终验收。Class V2 的实际 receiver、
copyable lifetime、生成身份回调，以及 resident callable/Value Handle 桥已接入
拥有图；原对象十模式全部通过，并新增无测试保护租约的同一 10×25 真实 TCC
生命周期。模块/类/闭包内部环现在按完整已认证图区分外部根，不按 codeLeases
零/非零推断；真实代码获取次数与 snapshotRefs 均保留，不伪造引用。

一次 21 门 verification 为 20 通过、1 失败、0 未运行，216.356 秒、峰值 3，
4101 输入前后相同。唯一失败为新增 O0 白盒宿主链接配置；原受保护 250 次、
无保护 250 次以及 O2 析构失败 100 次均已通过。仅修正 O0 宿主的源码依赖开关
与 whole-program 配置，再补同一 C/header/IR 的四模式 100 次，全部通过。
证据合并为 21 门通过，不称第二次完整批次全绿；生产源码未在该补验中变化。
XRT 原 25 组各 62 个 modular/single 执行：GCC 62/62、TCC 62/62；工程八项通过。
快照已同步为 XRT 2997 / xruntime 3447 API（450 自有），独立 xllm 配置未回滚。
未重建发布 CLI/DLL/VFS，未切换默认后端。Cgen UTF-8/LF 文本为 5,087,807 字节，
较 v176 净增 165 字节；新增图协调/析构职责主要位于常驻运行时，不推断 DLL 大小。

### v160 提出的关联缺口（v177 更新）

不能把测试流程提速或底层能力专项记成对象自动保活完成。以下相互依赖项
仍须形成同一个生命周期闭环，不能通过缩小原十模式验收范围来结束任务：

| 边界 | 当前源码证据 | 必须完成的合同 |
| --- | --- | --- |
| 类发布、失败与复制 | v177 明确 Class V2 身份与 BOOL 析构结果，prepared/commit 和 copyable lifetime 保留；已验证 schema 默认反序列化出生路径也经同一工厂 | 类/复制与四个原 persistence 程序通过；完整旧语言门、其他原生类型出生路径仍须验收 |
| 闭包与动态包装 | 闭包 V2 捕获布局和 resident box 策略；已知 Value Handle 桥与 callable 节点分别认证，活动 Invoke 在 Trace 前拒绝 | 任意 V1 回调/第三方 native 策略不自动认证；捕获布局允许 Future 不代表 Future 任意状态已准入 |
| 模块内部环与宿主别名 | 原模块硬门、原对象 10×25 与新增无保护 10×25 全通过，保留内部-only/标量、COW 与 MEM_FREE/逐事务平衡 | 后续须扩到完整传递 Future/task/native 图；不能把有限认证策略的收敛当作全运行时验收 |
| 检查至最终卸载 | 实际 receiver 的物理职责先解除、跨撤销不重放；明确失败无诊断、原诊断优先、错误 Cause DAG 和重试；字段/global/code 保有至统一重验证之后 | 未建模并发容器、弱/raw/native 准入和所有语义析构路径仍需逐协议证明；Linux/发布/默认后端尚未验收 |

原十模式的次数、行为和内存验收保留，仅把旧聚合租约观测迁移为已认证图中
的外部可达性：恰好扣除一个实际调用方 handle，所有内部代码引用仍真实计数。
新增用例不替换旧分母；所有历史失败日志和当时的源码/测试版本继续保留。

### v177：类、动态 callable 与显式析构结果

IR `classStorageAbi` 选择 `RESIDENT_GRAPH_V2`：Typed 不允许提前选择，Lowered
拒绝旧 V1/未知值，原 22 个变异意图保留并增至 24。Cgen 仅消费该合同，调用
Class V2 Create/CommitTake/BindIdentity；反序列化默认构造同样保有代码。V1
两槽表及 void callback 不变，V2 是独立的 bool callback 表，禁止互转强制转换。
已验证的类 destroy 函数直接返回 outcome.ok，不把缺少 diagnostic 当作成功。

XRT 明确绑定静态 lifecycle 策略身份，并验证完整 callback tuple。计划借用
实际强保有的 receiver，不合成假外壳；回调前把物理 Finalizer 置空，Abort
不恢复职责。事务 backing Hold 独立记账，仅避免把真实保有误判为 COW 别名，
StrongCount 不变，第二个真实 shell/cursor 仍保持既有限制。Field/global/context/
code 保有至所有语义 Finalize 和新职责/外部根重验完成；机械释放在 freeze 外。
receiver 本身不可 retain/clone/release；合法 child 发布使重验恢复 ROOTED。

Callable 的活动引用覆盖参数/签名访问、用户入口、结果提交和失败回滚；活动
阶段不长时间占 mutation，而在任何 Trace 前明确拒绝。Clear 隔离实际环境，
Finish 在冻结外运行一次 resident Drop；已知桥不授权未知传递节点。错误对象
仅是不可变 native Cause DAG，实际引用与 Cause 边始终保留，外部错误别名不
反向拥有模块。宿主记录独立 retirementFailed；失败无诊断也经明确边界传播。

追加验收（均使用当前源码，不借发布编译器结果）：

- 原对象 guarded 10×25、新增 automatic 10×25：闭包、类、容器/提取子对象、
  shared global/COW、internal-only、trivial 均通过，真实 TCC 页释放及逐事务平衡。
- 真实生成析构的四种失败/原错误组合，O0/O2 各 100 模块、300 次唯一析构。
  失败后 Abort/重试结束余下职责，错误别名在真实卸载后仍有效；不重放失败职责。
- 新 resident 三通道各 450 图、546 唯一终结、50 新职责、96 准备/提交 probes
  （14 真故障）、50 原生线程活动准入拒绝。包含无诊断失败、错误 Cause、真实
  COW、弱隔离恢复、child 发布和跨撤销职责；synthetic owner 不冒充真实模块证据。
- 四个原 persistence 程序的 explicit XLIR C 输出，GCC O0/O2 各 25 次初始化/
  源级断言/清理，共 200 次且零 live delta；其中三程序只修正私有宿主 include
  被历史 build/tmp 同名文件遮蔽的问题，最终生成 C/header/IR 与最初产物一致。
- 原 generator 72、task 15、cell 15、Future callback 9、async 3 等分母不变。
  2728 原测试逐字不变，另外三个为 Class ABI 变异、getter 接线、外根观测迁移；
  新增两个宿主/机制测试，不删原用例。XRT25组/62项每编译器及工程八项均通过。

集中发现、候选快速复测、唯一完整 verification、O0 链接补验和 persistence
补验分别存档。公开构建 65 TU：5/60 hit/miss、37.962 秒；25 合同共享 23 TU：
1/22、19.142 秒。完整验证输入冻结期间无修改；末尾只改 O0 测试链接配置。
冻结入口：`build/tmp/xlir_launch_v177_evidence/results.json`。后续仍是全传递图和
语言验收，不结束统一 IR 总目标。

### v176：闭包布局与真实模块环闭环

IR 的 `closureStorageAbi` 迁为 `RESIDENT_LAYOUT_V2`，Typed 阶段不允许它，
Lowered 阶段拒绝旧 V1 和未知值。捕获复制/拥有 ABI 仍是唯一来源；Cgen 输出
`XL6_ClosureCaptureSlot`/`XL6_ClosureEnvironmentV1`，不再为每个环境生成 free/trace。
不可变字段包括 trivial、string、Value、Future、closure、cell；可写共享捕获
仍用 resident Cell。V2 校验尺寸、宽度、偏移、重叠和 kind，失败不接管输入。
Value 投影由 resident CreateValue 统一创建/回滚，typed 调用读取明确的 Value 槽。
该桥增加一份唯一环境分配，已覆盖故障；未据此承诺分配性能改善。

新增 IR `retirementAbi` 和独立 `xl_script_module_retirement_v1` 描述符，保留
CodeOwnerGraph 的原尺寸/固定前缀。描述符只声明 checked trace/finish 边界；
resolver 还须在 Count/Trace 前逐个认证子节点。未知服务/原生策略仍失败，不能
把 tracer 非空当成回收证书。Snapshot Hold、Claim、Clear、Finish 都使用真实
对象；模块清理至 Finish 保持全局退休槽与代码实际拥有，最后计划释放才可卸载。
宿主在非零 codeLeases 时也尝试已认证图，旧/无合同模块仍保留保守零租约路径。
正在执行的生成 CodeScope 在遍历模块之前拒绝；合法调用引用的解除才允许重试。
全局 driver 排除回调重入导致的宿主并发退休；忙态、abort 和 checked Finish
失败的计划不丢失，所有实际 snapshot 引用一直可追溯。

验收与证据：

- 原模块门源码未变：GCC O0/O2 各 50 真 TCC 生命周期、25 successor handoff、
  325 拒绝、600 inspect probes、200 真故障注入、逐事务内存平衡与真实页卸载。
- 新模块门：宿主 O0/O2 共用当前 resident 对象，各 221 真 TCC 生命周期，包含
  各 25 个原生线程活动作用域拒绝、旧合同拒绝、Finish 重试和忙态 abort；
  96 Create/Commit probes 中 21 个实际故障，原图不被破坏，最终 MEM_FREE/零 live。
- 新布局门：GCC O0/O2/TCC 各 400 闭包/cell/Value/owner 环、400 隔离弱提升、
  96 Prepare/Commit probes 中 14 真故障、496 实际宿主节点释放；另外 500
  Value bridge 调用中 200 真故障；重验发现新节点和真实强引用恢复。
- 原公开 raw callback 宿主只把两个 include 重定位到私有目录：O0/O2 各保留
  1000 次替换及 module unit/reinit、mixed/void 回调断言，均通过。
- 2725 个原测试逐字一致；另外四个只涉及新 getter 接线、明确 V1 原生负例工厂、
  闭包 ABI 断言/新增变异、模块退休 ABI/新增变异。原 closure 16 个变异意图全保留
  并增至 24，原 module 557 个变异全保留并增至 576。原对象十模式及其 25 次目标
  未修改，仍在首事务失败；未伪称 250 次执行完成。

二十门主验证耗时 214.71 秒、峰值 3，4098 输入前后哈希一致。
最后仅对 GCC 告警拆开 `if/goto` 和 `return` 两行，重新生成的 C 精确只差此拆行，
IR/header 字节一致。补验只替代三个 capture 通道，其余十二 cell 通道及另外
十九门沿用原证据。8 项工程检查通过。最终公开构建 65 TU、64/1 hit/miss、
14.690 秒；合同 25 drivers 共用 23 TU、22/1、12.097 秒。

同轮审计发现相邻 XRT 仓库独立新增 `xllm_examples` 配置并重生成三个头文件；
本任务未改动、未同步这些外部变化。验证始终使用未变化的仓内 bundled 快照。
snapshot 质量门证明仓内快照元数据一致，不证明与此刻相邻仓库逐字相同。
冻结入口：`build/tmp/xlir_launch_v176_evidence/results.json`。

下一批必须压实而非绕过的语言合同：

1. XRT class backing 的非终态析构：使用实际被保有的 receiver，物理析构职责
   在回调前解除且跨 abort 不重复，完整字段/global/code 在语义析构期间存活。
   计划对 backing 的额外真实引用不能错误触发 COW 或禁止合法析构字段写入；
   也不能无条件把共享 backing 当唯一。保留既有 receiver 不可 retain/clone/release
   的终结合同，弱隔离与合法外部 child 发布必须分别处理。
2. 类 lifetime 和 native callable/handle 节点以显式、已协调的物理协议准入，
   包括 context trace、release、代码边及 callback-return 后的尾部；不接受任意
   旧策略的函数指针猜测。随后才接入整个 transitive Future/task/native 图。
3. 原对象门 4/5 的 `automatic_leases == 0` 与真实内部租约计数已经冲突，后续
   应明确迁成实际图路径断言：仅扣除此刻真实的 caller handle 槽，验证外部引用
   是否可达模块。正反十种场景、各 25 事务、析构次数、标量存活、COW 与 MEM_FREE
   全保留，不能改 codeLeases 或降低验收来凑绿。本轮尚未改此旧断言。

### v175：真实图事务、普通 Value 接管与宿主退休尾部

`xl6_ownership_plan.c/.h` 使用唯一的 XRT V1 生命周期协议，不再维护另一套
Value 布局。准入在 Count/Trace 前进行；未知描述符拒绝。准备阶段保有实际
节点、以 plan token 隔离弱/raw 新准入；失败只回滚新持有/claim，不改旧计划。
计数为零的无拥有 anchor 不能以可能触发终态回调的 pin/drop 进入冻结回滚。
所有旧 plan 引用都作为实际 internal slots 重验，不减去猜测的宿主引用。
Finalize 在 freeze 外执行，物理适配器须跨撤销保存已执行职责；Commit 在
同一 freeze 内重新发现、判断外根/新析构职责并 Clear。Finish 和最终 Drop
在 freeze 外执行。调用者串行化同一 plan，保有 resolver、代码和宿主边界。

XRT Snapshot 复用原 Inspect 的物理身份、重复边和计数算法，保留发现顺序，
快照本身不 pin。普通 Array/IntMap/Object/Set 以 backing 为清理单位，独立
COW 外壳不会重复计算元素强边；外部可达 backing 不清理。Value Claim 暂时
使弱提升返回 null，撤销后恢复，强生命周期结束才 expired。自定义 handle、
identity、finalizer、lifetime、构造中对象和借用 blob 仍拒绝；NULL 上下文
也不能自动证明外部析构代码驻留。Clear 不分配、不执行用户语义析构；内部
容器记账释放仍须遵守既有 freeze 的分配器/错误 hook 不等待参与者合同。

CodeLease 最后释放先结束自身引用更新，再调用宿主 release；CodeScopeLeave
先退出作用域链变更，再释放 lease。不会擅自退出调用者已有的外层 mutation。
原 scope 专项仅把直接宿主 release 的冻结断言从拒绝迁为允许，其余闭包/
析构/工厂回调仍拒绝，全部循环、真实计数和释放平衡不变。新专项验证 100
直接和 100 scoped 尾部中实际 freeze、原生线程 join、原错误优先级与零泄漏。
冻结 v174 lease 源在完全相同的新测试中 GCC O0/O2 实际 exit 3，为独立负例。

新增九通道（GCC O0/O2/TCC × 事务/Value/尾部）全部通过。每事务程序 400
真实原生环覆盖提交、复活、新节点与析构失败，100 busy/100 准入拒绝、140
真准备故障、1420 分配归还；每 Value 程序 1600 四类容器/COW 事务、40000
并发隔离弱提升、84 准备/刷新/提交故障、1696 实际原生 owner 归还，零 live
delta。XRT modular/single 新专项同时验证 200 alias 快照、100 回调前拒绝、
200 快照真故障，以及 200 物理清理/200 完整撤销/400 自定义策略拒绝。

统一十九门耗时 217.969 秒、峰值 3，4095 输入哈希前后一致，17/2/0。
原 generator72/task15/Future9/async3/frame14/public10+recovery10/cell15/slot3/
copy20 均保留。原 module O0/O2 退休期限和 objects 十模式首事务仍失败，
250 个对象目标事务未完成。2725 旧测试文件逐字不变，另一文件只有上述
宿主尾部断言迁移；1281 个已有 XRT 测试输入版本未修改。

XRT 25 组原 58+新增4：GCC 首次 62/62；TCC 在第二个新程序的结构体数组
初始化处发生内部编译错误。改为两个等价赋值，断言/轮数不变；仅补 GCC 两个
受影响程序及 TCC 未完成组，分别 2/2、62/62。旧失败日志和新旧源版本均保留。
源码公开构建 65 TU（5 hit/60 miss，39.303 秒）；25 合同共享 23 TU（1/22，
20.565 秒）。声明变化按实际依赖使缓存失效。8 项质量通过，60 私有声明头
映射/56 源码放置。生产 src 四修改/两新增，IR/Cgen 不变；不推断 DLL 体积。

下一批必须将已有引擎接到真实模块与经过 IR 验证的闭包/类存储、代码 owner
适配器上：补非终态析构与可撤销字段保留、弱/raw 隔离、全部隐藏 payload 的
参与认证、模块真实 snapshot owner 与最终卸载。特别审计 snapshot pin 对
类 backing 唯一性/COW/析构字段访问的影响；不得在 Cgen 猜测环境布局或仅
为原 codeLeases 聚合观测改计数。两项原门清零前不计模块回收完成。

证据：`build/tmp/xlir_launch_v175_evidence/results.json`；统一批次、XRT 首验、
XRT 补验、旧 producer 对照均单独保留。长期目标继续开启。

### v174：Future 回调存储合同与常驻登记／退休

- `XL6_IrCallbackBoundary.storageAbi` 区分 resident task frame 与 resident
  Future frame；Typed 不得提前出现边界，Lowered 必须与 transport 精确匹配。
  三个原 XL fixture 的 15 个 Future 和 10 个 task 边界全部参与，75 个非法
  版本/交叉家族变异由 verifier、C 与 header 入口拒绝；text/JSON 同时记录。
- `xl6_future_callback.c/.h` 使用一个物理分配、一个图节点，保存真实 private /
  Watch 引用、源 Future、输出 Promise、取消令牌与生产端拥有型捕获。单独的
  源引用确保生产端清理源捕获后，借用输入载荷仍存活；结果析构/trace 的代码
  寿命仍须由结果自身合同保证。失败 Start 保留私有帧给 Discard，不伪装成功。
- 同步 READY、异步 PENDING、登记失败回滚共用常驻尾部；独立 frame 锁保护
  并发引用/活动转换，短 mutation 只用于检查协调。私有、发布、执行、清理
  阶段拒绝 Count/Trace；用户调用和清理不占 frame 锁或全局 mutation。
- Future continuation 可继承同线程、同一拥有者的已受理 CodeScope，因此
  Close 后仍可派生子 continuation；没有匹配作用域则必须取得新准入，关闭
  后拒绝。不能借其他线程的能力，也不改变 task.spawn 需要新根准入的规则。
- Cgen 不再生成 Future Watch notify/drop/Promise/Cancel/free 状态管理，
  只消费已验证的捕获 trace、调用、清理优先级和 then/catch/finally 转发计划。
  没有 AST 追溯、语法猜测或生成后的语义字符串修补；只增加私有声明头映射。

新增九通道为 resident／生成 pending 图／原语言 fixture × GCC O0/O2/TCC-UCRT。
每个 resident 程序含 100 pending 图、100 四结果 ready、200 执行/清理检查拒绝、
100 反向取消、100 真分配回滚、100 armed-OOM Discard、1000 登记/完成竞争、
100 真实 DLL 生命周期；80 次真实准备故障、11600 次真实分配、零 live delta。
DLL 父/子回调、析构及子闭包调用均实际经过 DLL 代码，Close 后仍可执行；
最后一个真实 lease 的常驻返回尾部释放后，FreeLibrary 与 VirtualQuery 证明
页已卸载。没有测试宿主 guard 或新增原生根登记。

常驻 pending 图精确 6 节点/8 边/2 外根，生成 Future-only 图精确 11/13/2；
后者 100 次事务、四种终态、100 次捕获析构和零 live delta。Promise/Future
句柄共享图节点，不能重复算成根。原 stdlib_future_continuation 的全部十个
continuation 调用点保持，覆盖重载、任务错误恢复、取消、int→string、finally
失败优先级和捕获 async；补上实际模块 fini 后检查平衡，不据此验收整个标准库。

发现阶段一次十八门为 15/3，新增门收集到计数漏计九个 task、图根假设和清理
入口接线错误；逐项修正，图断言从下界加强为精确值，原始失败源与日志保留。
一次完整 verification 208.356 秒、峰值 3，4089 输入哈希前后一致，16/2/0。
之后只修正新测试 GetTickCount64 的 TCC 隐式声明，改为已声明且溢出安全的
GetTickCount 经过时长，补跑三个 resident 通道；完整批次的旧输入版本也保留，
不宣称它与此补充修正后的全树哈希相同。最终三通道无该编译警告且均通过。
旧 v173 生成 Future-only C/header 在不改测试的 GCC O0/O2 对照中实际 exit 3，
失败于 pending 图检查；这是负例，不计绿色回归或原有失败门。

2719 个既有测试文件逐字一致，仅 one-shot TCC 宿主增添 include/getter 绑定。
原 generator72、task15、async3、frame14、public10+recovery10、cell15、slot3、
copy20 全部保留并通过；module O0/O2 退休期限、objects 十模式首事务八期限／
两 automatic_leases==0 仍失败，250 个目标事务未完成。不能用新增通过稀释分母。

公开源码构建 64 TU（57 hit/7 miss，20.412 秒）在哈希一致后复用；最终 25 合同
共享 23 TU（23 hit/0 miss，11.011 秒）。8 项质量检查通过，56 私有头映射／
52 源码放置；能力矩阵引用已机械更新，XRT 源码、单头、延期清单及发布二进制
不变。相对 v173，生产源码 7 修改/2 新增，Cgen UTF-8/LF 文本净少 1196 字节
（5090043 → 5088847）；Future-only 示例 C 为 48459 → 47970 字节。这不是
整个源码树或 DLL 的体积结论，未重建发布 VFS 或切换默认后端。

下一批主线是完整节点参与与安全候选接管，接入模块内部环退休和析构复活；
原两项失败、性能、完整语言/Linux/发布验收继续开放。
完整批次：`build/tmp/xlir_launch_v174_ownership_verification/results.json`；
计时辅助修正复验：`build/tmp/xlir_launch_v174_timer_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v174_evidence/results.json`。

### v173：常驻 task frame / pool 与显式协作通知

- `XL6_IrTaskStorageAbi` 在 Typed 为 NONE、Lowered 为 RESIDENT_FRAME_POOL_V1；
  async 参数和 callback capture 各自保存精确的 trace ABI。8 个 fixture、
  334 个非法变异由 verifier 与 C/header 入口拒绝，13 个 async 参数 trace、
  16 个 callback capture trace、4 个 pool 模块同时检查 text/JSON 确定性。
  不增加 AST 回溯、按语法猜析构或生成后的字符串语义修补。
- `xl6_task.c/.h` 的一个物理分配包含常驻头与对齐后的生产端数据，图中是
  一个节点。引用分别来自真实 job、提交调用者或已登记 Watch；没有额外
  模块根。Allocate 新任务必须取得新代码准入，Close 后不得继承旧准入；
  已受理 run/drop/notify/release 及常驻返回尾部保持真实租约，允许在现有
  执行作用域中创建子闭包。结果析构/trace 必须另有结果生命周期保证。
- pool 在短 mutation/gate 下关闭准入，等待提交额度和 native join 时退出
  协调区；同步取消/剪枝可在 Submit 中直接运行清理，因此 Submit 不持资源
  gate。真实活动链拒绝提交、run、drop、通知及代码释放尾部的自 join。
  frame 的引用/活动转换同时使用独立锁；mutation 只稳定 inspection，
  不能互斥 job、提交者、Watch 的并发写。私有、发布中和执行/清理中的
  frame 拒绝 Count/Trace，失败不改调用者输出。
- 新 `xrtFutureWatchInitPhased` 是显式参与合同，不从“有 trace”推断安全。
  Future 在短 mutation 中转移计数/槽及 Calling 状态，协作通知与 release
  在其外执行，保留真实通知引用至返回。原 Watch、内部 waiters 与 payload
  finalizer 仍保守保护，64 字节 Watch ABI 和 READY/PENDING/ERROR 职责不变。
- Cgen 删除生成的资源 gate/懒建/收尾和 outcome Watch 实现，仅留下模块
  壳与已验证捕获/清理代码。pool 需要 ownership 声明时选用统一声明头，
  不增加实现头副本或第二份手写 visitor ABI。Future-only 分支尚未常驻化。

新增门包含五类程序 × GCC O0/O2/TCC-UCRT，共 15 运行通道和三份真实 DLL，
全部通过。每个 resident 程序保留 200 排队图、200 执行拒绝、100 阻塞清理、
100 次四结果通知冻结/尾部 join、200 同步取消自 join 拒绝、100 分配回滚、
100 armed-OOM discard、100 真实 DLL 生命周期，以及 2000 次即时完成竞争。
280 次真实提交前故障、26646 次真实分配、零 live delta。排队 raw 图为
2 节点/1 边/1 外根，language 图为 6/7/4；不登记原生根。通知测试检查仍有
真实拥有者的 Future，不解引用可能已被迭代通知队列释放的 task frame。
真实 DLL run/drop 在 Close 后执行并保持页映射，子闭包最后释放后页卸载；
该测试的子闭包调用函数在宿主，不宣称它覆盖了 DLL 子闭包代码调用。

原 pool 210 worker/callback unload、60 自卸载拒绝、300 嵌套提交拒绝，
outcome 540 执行/60 反向取消/20 真故障，callback 1200 执行/6 取消/59 真故障
全部保留。原 25 个 ready/注册失败白盒案例迁入常驻实现测试，循环、断言和
载荷析构主体不变，仅适配真实 frame 存储及函数名。所有原 XL fixture 和
四类 runtime 主体未改；2708 个既有测试文件逐字一致，7 个宿主/合同适配。

发现阶段收集了新门六个失败通道及原十六门中的四个失败门：声明缺失使
pool-outcome、public 和 generator 的 pool 用例编译失败；原通知 mutation
过宽与新测试的失效 frame 假设共同暴露冻结问题。成组修复后仅跑一次完整
十七门 verification：215.924 秒、峰值 3，4080 输入哈希前后一致，15/2/0。
两组原硬门仍是 module O0/O2 退休期限，objects 首事务八期限/两自动租约
断言；十模式 250 个目标事务未完成，不能用新增通过数稀释。旧 XRT 对象
通过仅映射 API 名称的 baseline adapter 在当前原样测试中 O0/O2 实际 exit 3，
证明原保守通知无法满足协作冻结；这是负例，不计绿色回归。

最终公开构建 63 TU（5 hit/58 miss，37.981 秒），24 个合同共用 23 TU
（1 hit/22 miss，19.364 秒）；新增 XRT 声明使依赖缓存失效，未强行复用。
XRT 23 组专项 GCC 77.619 秒、TCC 19.107 秒，各精确执行 58 个程序且通过，
包含模块/单头与原 130 个保守通知窗口。1281 个既有 XRT 测试文件未变。
8 项质量检查通过：52 私有头映射、48 源码放置、6471 文本、401 PowerShell
脚本零语法错。相对 v172，生产 11 修改/2 新增，Cgen UTF-8/LF 文本净减少
6380 字节（5096423 → 5090043）；常驻实现新增，不声称总源码或 DLL 缩小。
XRT 新接口和单头、bundled 快照已同步，发布 CLI/DLL/VFS 仍未重建。

下一批需完成 Future-only 与其余节点参与证明，并把稳定快照、候选原子
接管、退出冻结后析构及复活处理接入真实模块退休。原两项失败、性能、
完整 24 语言门、Linux/发布/默认后端验收仍开放。
报告：`build/tmp/xlir_launch_v173_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v173_evidence/results.json`。

### v172：async 物理 frame 存储与 continuation 退休分相

- `XL6_IrAsyncStorageAbi` 在 Typed 阶段为 NONE，Lowered 阶段必须为
  RESIDENT_PHYSICAL_FRAME_V1；text/JSON 都记录。5 个 fixture 包含三种
  machine 程序、普通 pool frame 与无需 frame 的 ready future，20 个
  阶段/版本变异由 verifier 和 C/header 入口拒绝。Cgen 按既有已验证
  frame/capture/cleanup 计划生成代码，不增加 AST 追溯或语义猜测。
- `xl6_frame_storage.c/.h` 仅负责物理 Allocate/Free 的短 mutation，返回
  实际常驻函数地址；不增加 allocation header、引用计数、trace 回调或
  人造根。async machine 初始/后继 frame、失败回滚和普通 pool frame
  不再直接生成这些 raw malloc/free。初始化和共享槽转移合同仍各自保留。
- queue 在短 mutation 中撤销 retiring ticket，然后在协调区和 queue 锁
  之外调用 release。独立取得的旧 ticket 与尚有 predecessor 的 successor
  均拒绝 Count/Trace/Inspect；真实 resident CodeScope、原 owning lease
  和 queue credit 一直保留。最终物理 Free 再单独参与 mutation；清理与
  scope 尾部完成后才发布 successor。未对任意 native release 作参与认证。
- 新三通道每程序完成 200 次分配/释放冻结、100 次真实生成 launch、200
  次生成 Watch 退休、100 次阻塞清理/后继接续、100 次真实 OOM 与 100 次
  armed-OOM free；精确图、引用/额度、失败输出不变性与逐事务 allocator
  都验证，3600 次真实分配、零 live delta。没有登记原生根或插入宿主 guard。
- 冻结 v171 producer 在同一生成 launch 冻结测试实际 exit 3；冻结 v171
  queue 在同一阻塞清理测试也实际 exit 3。两例链接同一当前源码 XRT，
  分别选择旧生成入口或旧 queue，保留确切错误日志，不计为绿色通道。
- 发现轮先暴露新 fixture 的 export-future ABI 不合法，改为合法 void
  export 后直接测试私有 launcher；随后修正消费者出队短 mutation 与
  FreezeTry 的竞争假设，等待短窗口结束再断言。两次失败报告完整保留，
  没有把调度窗口当作语言故障，也没有减少最终循环。原 resident 门的
  100 次清理观察改为检查可冻结、frame 拒绝和真实租约/额度，其他主体不变。

公开构建 62 TU（52 hit/10 miss，21.198 秒），20 合同共享 23 TU（20 hit/
3 miss，16.461 秒）。发现修正未改变生产实现，逐项确认共享构建输入仍是
当前字节后直接复用。一次完整十六门 verification 207.611 秒、峰值 3，
4072 输入前后一致：14 通过、2 失败、0 未运行；原十五门状态完全保持。
frame14、generator3、public10+recovery10、cell15、slot3、copy20、generator
storage72 等原分母全部保留；module 两通道仍退休期限失败，objects 十模式
仍首事务八期限/两自动租约断言，250 目标事务尚未完成。

与冻结 v171 相比，2709 个已有测试文件逐字一致；仅 one-shot 宿主增加
新 getter 接线，resident 测试改变上述清理期安全断言。生产源码 10 文件
修改、2 文件新增；Cgen UTF-8/LF 文本增加 643 字节，不声称二进制缩小。
8 项质量检查通过（48 头映射/44 源码放置、6463 文本、400 PowerShell
脚本零语法错）；XRT/bundled/发布文件未改，未重跑历史 XRT 全套专项。
下一步处理普通 pool 回调代码保活、其余拥有节点参与，再共同验收模块环
退休、候选接管/提交、析构复活与性能；完整 24 语言门和发布验收仍开放。
报告：`build/tmp/xlir_launch_v172_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v172_evidence/results.json`。

### v171：generator 控制、frame 与产出值生命周期常驻化

- `XL6_IrGeneratorStorageAbi` 在 Typed 阶段为 NONE，Lowered 阶段必须为
  RESIDENT_SCOPED_FRAME_V1；text/JSON 显式记录，5 个 fixture 的 20 个
  阶段/版本变异由 verifier 和 C/header 入口拒绝。原 generator 状态、close
  分支、yield/cleanup 转移仍由统一 IR 决定，不在常驻运行时分析源程序。
- `xl6_generator.c/.h` 接管控制块、当前项、Next/Close/Destroy、状态更新和
  frame 分配/撤销。frame 的常驻私有头记录真实唯一拥有者；控制块及独立
  发现的物理 frame 在运行或退休时都拒绝计数/trace，不能仅依赖外层根拒绝。
  先在 mutation 中撤销边，再在作用域外执行 frame/item 的生成回调；最后
  内存释放重新进入 mutation。共享槽内容继续使用 v170 的已验证计划。
- 工厂成功消费 frame，失败仍由生成端按已验证初始化前缀回滚。真实
  AcquireScoped/CodeScope 保护 descriptor、resume、item/frame 回调与返回
  尾部；宿主 Close 后的活动回调可把同一已接纳租约传给新子拥有者。回调
  或等待过程不持长期 mutation，也没有要求原生线程注册根或新增宿主 guard。
- 冻结 v170 producer 生成同一新源程序，在实际冻结入口测试中进入了
  generator body 并触发 exit 3 断言；新 producer 三通道通过。两者使用同一
  当前源码 XRT，负例调用的是旧生成 core，不把旁边链接的新实现当成旧实现。
- 新 resident 程序每通道完成 200 次生成入口/创建冻结、200 次活动控制块与
  frame 拒绝、200 次真实分配失败回滚、100 次 armed-OOM 无分配转移，以及
  100 次真实 DLL 生命周期（50 resume、50 直接 destroy），均包含 Close 后
  子租约及实际 MEM_FREE 检查。最终在首例前明确开启内存调试，记录 3400
  次真实分配和零 live delta；修正前专项只保留为发现记录，不冒充该内存证据。
- 同一新增门还执行原 15 个 payload fixture、5 个 close fixture、拥有项
  cleanup 及 loan 的优化/基线两变体：共 24 个场景 × GCC O0/O2/源码 TCC，
  72 通道全部通过；重复生成 48 次，C/header/IR/binding 确定性通过。保留
  900 个关闭源执行/2700 个重复关闭序列、2700 个拥有项关闭、2400 个载荷
  生命周期、每 loan 变体 3600 执行及 42101 分配等原主体。创建/yield/async
  分配失败分别为 80/160/120，pool 20 个提交前失败，未使用 smoke 缩减。
- 22 个原语言文件与 11 个 runtime 测试主体逐字匹配冻结 v170；8 个 host
  只加私有产物 include 参数，原断言、循环与错误顺序保持。旧 generator
  拥有图每通道 2400 生命周期/1000 async 事务/2000 真暂停/1800 busy 拒绝/
  16000 inspection 探测/6600 真失败也继续通过，未被新门替代。

公开构建 61 TU（54 hit/7 miss，20.576 秒），19 合同共享 23 TU（20 hit/
3 miss，15.677 秒）。修复稳定后逐项确认这些共享构建仍是当前字节，直接
复用，不为更换目录标签再次构建。一次十五门 verification 207.690 秒，
峰值 3、4065 输入前后一致：13 通过、2 失败、0 未运行；原十四门仍 12/2。
module O0/O2 仍在退休期限失败；objects 十模式仍首事务八期限/两自动租约
断言，250 目标事务没有完成。另 8 项质量检查通过（44 头映射、40 源码放置、
6456 文本文件、399 个 PowerShell 脚本），未重建 release/VFS 或执行 Linux。

相对 v170，生产源码 7 文件修改、2 文件新增；`src/cgen` UTF-8/LF 文本
净减少 9880 字节。这是生命周期实现迁出与渲染收口，不是 DLL 体积结论。
XRT sibling/bundled 实现、延期清单和发布二进制未变；历史 XRT 全套专项
不计为本轮重跑。下一步处理 async frame 与其余节点参与能力，再共同验收
候选快照/claim/commit、模块退休和析构复活。完整 24 语言门与性能仍开放。
报告：`build/tmp/xlir_launch_v171_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v171_evidence/results.json`。

### v170：共享槽与显式 frame 转移使用同一常驻写入计划

- `XL6_IrSlotTransferAbi` 在 Typed 阶段必须为 NONE，Lowered 阶段必须为
  RESIDENT_ORDERED_WRITES_V1；text/JSON 显式记录。5 个语言 fixture 的
  20 个阶段/版本变异均被 verifier 和 C/header 入口拒绝。
- `xl6_slot.c/.h` 提供无分配、无析构、无回调的有序物理写入。先验证整份
  描述符，再在一个 mutation scope 中依序写入；后项源读取观察前项结果，
  并非并行 copy。类型、所有权和初始化位来自已验证 IR，清空使用真正的
  typed compound literal。它不是普通并发变量锁，也不接纳任意原生写入。
- 同步、async、generator 的 STORE/PRODUCT_STORE_MOVE 共享一个渲染函数；
  共享/间接/frame 存储走同一常驻提交，普通私有标量局部不额外加入作用域。
  显式 frame 捕获、spill、retire、cell 发布和拥有槽 detach 共用计划；async
  handoff/rollback 将 outer promise、载荷/初始化位和源清空置于同一计划。
  需要析构时先完成 detach，再离开计划执行析构，不持计划调用用户代码。
- 真实负例使用冻结 v169 producer 生成同一新共享全局程序，并链接同一新
  常驻实现：直接调用该生成函数仍穿过 freeze，实际 exit 3 触发断言。新
  producer 则通过。此测试刻意隔离 API 入口租约，证明生成写入本身的边界；
  不是模块卸载或完整 frame 生命周期的证明。
- 新门 GCC O0/O2、源码 TCC/UCRT 三通道全通过；每程序 100 次生成全局
  冻结写入、100 次冻结 handoff、100 次真实 OOM 开关下的无分配提交、
  20000 次竞争 handoff。实际冻结图探测分别为 11540/3671/4018 次，检查
  值与初始化位一致，以及精确 cell/value 图、真实外根和 allocator 平衡。
- 公开构建 60 TU，59 hit/1 miss，13.878 秒；12 合同共享 23 TU 全 hit，
  7.601 秒。首次十四门验证 100.285 秒，8 绿/6 红；六门被测试宿主缺少
  slot 接口绑定/链接挡住。只修改这组接线，再统一复验六门及声明依赖受影响
  的 public 门，80.645 秒，5 绿/2 红。另七门依赖逐项验证仍是原字节，
  保留原结果；不能把这两次执行写成一次全绿验证。两批均峰值 3、4056 输入
  前后一致，最终合并 14 门 12 绿/2 红/0 未运行。
- 原 frame14、generator3、public10+recovery10、cell15、copy20 等分母保留。
  module O0/O2 已越过宿主接线并重现原退休期限失败；objects 十模式仍是
  首事务八个退休期限、两个 `automatic_leases == 0`，250 目标事务未完成。
  新绿色专项不能稀释这两个原失败门。另 8 项质量检查通过；40 私有头映射、
  36 源码放置检查通过，但没有生成 release/VFS 或执行 Linux。

相对冻结 v169，生产源码 14 文件修改、3 文件新增；`src/cgen` 的 UTF-8/LF
文本净增 849 字节（包括新增共用文件），不声称二进制缩小。XRT sibling 与
bundled 实现、原七个强验证语言 fixture 和延期清单逐项哈希确认未改。
当前合并报告校验 3883 个输入版本；两个修复前宿主/脚本版本保留其准确旧字节。

下一批仍需完整处理 generator/core/frame 生灭及其余拥有图节点参与合同，
随后把安全快照、候选 claim/commit、模块退休与析构复活共同接通验收。
本轮只关闭显式槽转移的物理一致性缺口，不颁发整图证书，不启用不安全回收。
完整 24 门语言、性能与发布/默认后端切换仍未验收。
合并入口：`build/tmp/xlir_launch_v170_ownership_composite/results.json`；
冻结入口：`build/tmp/xlir_launch_v170_evidence/results.json`。

### v169：cell 物理存储与拥有权交换收口

- `XL6_IrCellStorageAbi` 在 Typed 阶段必须为 NONE，Lowered 阶段必须为
  RESIDENT_TRANSFER_V1。text/JSON 显式输出；5 个原语言 fixture 的 20 个
  非法变异均被 verifier 和 C/header 入口拒绝。同期保持 22 分配、22 初始化、
  14 拥有型交换以及原分配失败控制流，不在 Cgen 猜测所有权。
- 常驻 `xl6_cell.c/.h` 覆盖 8 类载荷；tagged nullable 仅允许已验证的无指针
  carrier，不作为任意原生拥有数据的后门。Create 可失败；Initialize 消费
  输入；Load 借用；Store 仅写平凡值；Exchange 消费新 owner 并返回旧 owner，
  不分配、不释放。最后释放包括 payload 析构尾部参与现有 mutation 域。
  这不是并发变量锁，也尚未保护所有外层槽的发布/撤销。
- 新门有 resident、原 allocation、suspension、thread、capture 五组，每组
  GCC O0/O2 与源码 TCC/UCRT，共 15 通道全部通过。逐 resident 程序完成
  400 生命周期/精确物理图、450 真实分配失败、250 冻结转换和 50 析构尾部。
  图检查分别确认原生引用是外根，以及扣除真实内部槽后的无外根结果。
- 原分配门每通道 13 场景、260 基线、1800 真实失败（1200 创建、540 调用、
  60 捕获），9700 分配平衡；并发门每通道 8 workers、800000 原始引用操作、
  48240 源级 reader、30 最后拥有者竞争；capture 每通道 700 原生成图。
  原 IR 分母仍是 allocation 224、suspension 3、thread 9、capture 22 反例。
- 重启旧 capture 门发现陈旧测试合同：mode 4 的类拥有常驻 finalizer 节点，
  实际为 7 节点/6 边，冻结 v168 producer 同样检出原 6/5 预期失败。现检查
  精确真实节点，未改根计数；缺少生命周期合同的闭包在创建时拒绝，另用
  真实接纳而 trace 拒绝的 adapter 保留原 inspection 回滚断言。7 模式及
  100 轮分母完整保留，不使用 XFAIL，也不要求原生根登记。
- 常驻源码接入 DLL/Linux/embedded 构建清单、宿主符号注册与四个 VFS 清单；
  36 私有头映射/32 源码放置检查通过，但没有重建 release 或执行 Linux。
  能力矩阵改为从公共声明区识别静态内联函数，独立记录、不虚增 TCC 符号；
  未声明 API 仍拒绝。3 个单测通过，扩展延期清单仍为空且与 v168 一致。

公开源码共享构建 59 TU，58 hit/1 miss，13.873 秒；11 合同共享 23 TU，
22 hit/1 miss，8.429 秒。一次十三门 verification 126.964 秒，峰值 3，
4048 输入前后一致：11 绿/2 红/0 未运行。原 frame14、generator3、public10
与 recovery10、class/copy/closure/continuation 原分母继续保留，模块两通道
与对象十模式仍红，250 对象目标事务仍未完成。另 8 项质量检查通过。
`src/cgen` 的 UTF-8/LF 文本相对冻结 v168 减少 6074 字节；这是实现迁出与
重复映射收口后的源码变化，不是编译器 DLL 或生成二进制的体积结论。

下一批优先成组处理外层 frame/global 槽、节点参与能力与其余 native/task
拥有边；安全快照/候选 claim/commit 必须与模块退休、析构复活共同验收。
24 门完整语言总验收、性能与发布/默认后端切换仍未完成。
当前报告：`build/tmp/xlir_launch_v169_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v169_evidence/results.json`。

### v168：常驻代码拥有边与 continuation 队列共同参与

- 统一私有 `xl6_ownership_scope.h` 为常驻转换提供无分配的栈 scope；覆盖
  CodeLease 的实际 atomic64、获取/回滚/最后宿主释放、活动栈的进入/退出和
  同线程继承、闭包创建/释放/装箱、类生命周期绑定与提交。准入在宿主及
  活动栈锁之前，特别不能持栈锁在 AcquireScoped 的 Retain 里等待冻结。
  不改变成功/失败/消费语义，也不把普通调用时长作为 mutation。
- 同一批保护 continuation Reserve/Successor、TraceBind、事件发布、Abort、
  队列 Close/Destroy 与 Future watch 的 owning-ref/登记/回滚。取票先进入
  mutation 再取队列锁；若需等待，先退出 mutation，等待后重新准入并检查
  队列。另一消费者先取走通知是合法竞争，不使用持队列锁升级的方式。
- `run(data)` 不在 mutation 内：运行票据仍拒绝图检查，活动 CodeScope
  保留真实引用。仅回调已返回后的物理退休进入独立 scope，覆盖 data release、
  代码尾部、successor 发布和 credit 归还。不能凭这点宣称任意 frame/data
  已具备冻结能力，也不能把 callback 的任意 mutable 数据视作可检查叶。
- 词法平衡审计确认五个实现文件的 31 个原签名/操作体保留（租约 9、闭包 8、
  类/析构绑定 4、队列 9、watch 登记 1）；除显式 dispatcher 重写和新包装
  外原文件未变。整个 src 相对冻结 v167 仅改变这些常驻文件/相关头，IR/Cgen
  源码未改，没有追加 AST 回查或语言语义特判。四个 VFS 清单与本地 header
  staging 同步声明新私有头：32 header mappings、28 source placements。
- 新测试每个程序完成 100 个冻结租约/栈/闭包转换、50 个在途回调尾部、
  50 个真实 allocator-held 工厂窗口、10 个真实 Pending 调用，以及 100 个
  冻结队列/传输转换、10 个实际 Pending dispatch 和 10 个物理退休回调中
  Close 后的子闭包逃逸。检查实际计数变化、图、credit、一次性释放和逐事务
  allocator；原生线程不登记根，不添加宿主代码 guard。全局协调数据是借用
  的测试控制记录，不伪造其 owning edge。
- 对同一新检查编译冻结 v167 的五个常驻实现，分别检出租约转换、半完成
  工厂和队列转换的真实缺口。它们是三个负向发现，不能记作三个通过程序。
  最终 GCC O0/O2、源码 TCC/UCRT 三通道都完成两条精确完成标记；TCC 自行
  编译当前 XRT 实现，不借 GCC COFF 对象或旧 release 的语义。
- 共享公开构建 58 TU，51 命中、7 重编，13.280 秒；七合同 23 TU 全命中，
  6.570 秒，新鲜链接。冻结生产修改后只作一次十二门 verification：92.396
  秒、峰值三进程、4,043 输入前后一致，10 通过、2 失败、0 未运行。原有
  frame14/generator3/public10+recovery10、copy20 与构造/发布/闭包/
  continuation 等真实释放分母全部保留；新三通道是新增门，不是原红门修复。
- 原 module O0/O2 仍在退休期限失败，objects 十模式仍在首事务失败（八
  退休期限、两 `automatic_leases == 0`），250 目标事务未完成。XRT sibling
  与 bundled implementation 本轮未编辑，不重复运行上一轮 58/58 专项；
  当前新增测试仍直接运行真实源码 XRT。v166 性能风险未验收，本轮未测新倍率。
- 质量检查发现能力矩阵尚未反映新增常驻 API 调用，已用官方生成器更新
  文档矩阵；延期清单逐字未变。该派生文档不在运行测试输入中，生产源码与
  二进制没有因此变化，只重做质量检查，不重跑完整语言批次。

下一步须落实生成 cell/frame/global 和其余 native/task/transport 的完整
参与能力证明，再接稳定快照、候选接管、退出冻结后的清理/复活与最终卸载。
模块 API 锁的释放/重入、直接宿主 owner 操作仍须共同设计，不能把这里的
常驻转换准入当作全图能力声明。原模块/对象两门、完整 24 语言门、其他类型
出生路径、发布/VFS/Linux 与默认后端切换仍未验收。
当前报告：`build/tmp/xlir_launch_v168_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v168_evidence/results.json`。

### v167：Future 异步拥有转换统一参与，保留等待进展

- 原先只保护 RC 的实际 CAS，PromiseRef 的两个计数、终态字段和通知尾部
  仍可分开观察。当前在 28 个公开入口、3 个内部 waiter 边界、2 个共同工厂
  入口建立完整外层 scope；后两者覆盖原有 6 个组合器和 10 个 continuation
  前端，没有复制每个 API 的装配实现。准入先于相应内部锁，直到通知、回滚、
  延迟注销和最后释放返回才退出。原 Value 包装复用同一个私有作用域宏。
- 显式规定准入失败返回值，特别是 WatchAdd 返回 `XFUTURE_WATCH_ERROR`，
  不能用表示 READY 的零值。词法平衡审计确认 33 个原签名/操作体保留，四个
  异步实现文件除这些包装外逐字一致；原分片、RC、四个 Value 源文件和协程
  Await 文件未变。没有新增 IR/Cgen 语义分支或追溯 AST。
- 不把整个 FutureWait/Await 放进 mutation。真实等待引用已经由旧实现保有，
  仍按外部根计数；内部 Add/Detach/Remove 只保护自己的边转换。反向地，
  尚未结束的通知及注销仍属在途转换，不能在其半途中冻结。此举不把旧
  opaque waiter 当作可检查叶，也不替用户回调数据补造拥有边。
- 新 `future_scope_tests` 每个程序检查 100 个冻结发布/引用/Watch 转换、
  130 个在途通知/析构/立即与延后工厂窗口、20 个并发注销、10 个真实 Pending
  原生等待；冻结期间对比实际终态与图，注销必须等回调返回，每个事务精确
  校验 allocator。回调内继续普通 Value/Promise 保有，验证拒绝 freeze 不会
  排队阻塞已有 reader。没有额外代码 guard 或强制原生 root 登记。
- 同一个新测试编译到已冻结 v166 单头实现，首个终态发布事务就触发预期
  失败，证明新检查能检出实际缺口；它是负向发现，不是 XFAIL 或完成的程序。
  当前 GCC 源码/单头窄验证随后通过，再冻结本组生产修改做集中验证。
- XRT 最终选择 23 套，保留上一轮 35 个程序，并加入新专项及原 Cancel、
  Future、continue/combine 的线程/OOM、协程 Await、bridge 回归。GCC/TCC
  各精确执行 58 个程序，分别 65.731 / 15.611 秒，均通过；1,987 输入前后
  一致，源码与单头路径均包含在内。未将这些 Windows 结果冒充 Linux 验证。
- 公开驱动共享 58 TU（5 命中、53 重编，38.378 秒）；七合同驱动共享 23
  TU（1 命中、22 重编，16.041 秒）。按实际预处理/工具链键构建和新鲜链接。
  一次十一门 verification 为 89.324 秒、峰值三进程、4,040 输入一致：9 绿
  2 红、0 未运行。原 frame14/generator3/public10+recovery10 与构造、发布、
  闭包、continuation、class-copy 原分母保留，五个既有复制程序仍为 20 阶段绿。
- 原 module O0/O2 仍在实际退休期限处失败，objects 十模式仍首事务失败
  （八退休期限、两聚合租约断言），250 目标事务未完成。新底层绿色没有减少
  这两个原红门。v166 的裸 RC 性能风险仍待验收，本轮未作新的性能倍率测量。

接下来需要让 CodeLease 的直接 atomic64、活动代码作用域以及生成存储的
变更与同一稳定图协议协调。不能机械地在已持有 API/作用域链锁之后再进入
mutation；模块退休辅助函数还有临时释放调用方 API 锁的路径，必须一起核对。
任务/传输与 native/callable 的外层状态、回调自身拥有数据的参与能力也未完成。
最终 claim/commit 之后须退出冻结再执行用户析构，并处理析构中新建/发布子
对象的代码能力，不能用一次 codeLeases 复查替代全局清理/卸载合同。
完整 24 语言门、其他类型出生路径、发布/VFS/Linux、legacy 退役和默认后端
切换仍未验收。发布二进制和 IR/Cgen 生产源码本轮未变。
当前报告：`build/tmp/xlir_launch_v167_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v167_evidence/results.json`。

### v166：协作冻结的生产边界已建立，完整收集协议尚未接通

- 新 `xrtOwnershipMutationBegin/FreezeTryBegin/ScopeEnd` 使用调用栈上的零
  初始化 scope，不申请 heap/TLS，不要求宿主登记普通强引用。scope 不能复制、
  跨原生线程或带未结束子 scope 退出；它不是强根或代码租约。mutation 共享
  准入，freeze 非排队尝试，繁忙保持零输出与既有诊断，不做 reader 升级。
- 最终实现按原生线程 ID 分为 64 个带间隔的准入分片；热路径只触碰本分片。
  freeze 的冷协调锁负责统一取得全部分片，任一繁忙就撤销已取得的锁；冻结
  指针在任一分片锁下读、全部分片锁下写。持有 freeze 的同一线程允许嵌套
  平衡的 Trace 游标，其他线程的参与转换等到退出 freeze 后继续。
- `xrtRefRetain/Release` 的原 CAS body 未改。85 个 Value 入口用同一外层
  scope 包装，覆盖拥有式接管、复制/最后释放、weak lock、字段/COW/merge/
  take/reserve、绑定、游标生命周期、DeepClone/Equal/Hash/ScalarEqual，所有
  提前返回归还准入。词法平衡审计比较 v165 的 129 个公开签名/函数 body：
  85 个 Value 与 2 个 RC 原 body 保留，42 个只读或私有分配入口未变，两个
  commit 仍使用原共享实现。未增加 IR/Cgen 分支或 AST 回查。
- 每个新 scope 测试程序完成 100 次无注册原生 weak 提升、80 次容器转换、
  25 次未返回字段析构、25 次未返回直接 identity 哈希；四线程 80,000 对
  retain/release 与至少 100 个稳定冻结窗口。65 个同时存活的原生 reader
  强制覆盖分片碰撞，逐个退出后才允许冻结；四个竞争收集线程各至少成功
  30 次，验证独占、回滚和嵌套进展。另有非法 scope、真实 inspection OOM、
  无分配准入与逐事务 allocator 平衡；尝试次数不计作成功次数。
- 集中 XRT 验证保留原 core/ref/ref-threads、graph、iterator、copy/finalizer、
  Future/waiter/map/task 等 12 套，各编译器精确执行 35 个程序；GCC 45.542
  秒、TCC 11.323 秒，源码/单头均通过，1,983 项输入前后一致。第一次报告
  因父进程 stdout 缓冲把四条执行标记与子输出粘连而误判，原 exit 0 日志经
  只读精确审计证明无遗漏；最终使用无缓冲输出和首个 exe 路径匹配。
- 性能发现必须单列：最初单一全局锁使四线程独立 RC 微基准约慢 97.75 倍，
  因此修改生产准入为分片，再重建并复验。五次交替 GCC O2 测量同一公开
  函数指针边界、每轮 100 万对实际 RC：v165 / 最终串行中位数 8.105 / 29.453
  毫秒，独立四线程 2.394 / 8.267 毫秒，即仍为 3.63 / 3.45 倍。消除了严重
  全局争用，未消除新增保护的成本；不能把这个微基准当应用性能或 Linux 验收。
- 最终公开驱动共享 58 TU（57 命中、1 重编，22.710 秒），七合同驱动共享
  23 TU 全命中（6.910 秒），新鲜链接。最终十一门 88.148 秒、峰值三进程、
  4,040 输入一致，9 通过、2 失败、0 未运行。frame14/generator3/public10
  与 recovery10、publication/closure/continuation/class-storage/class-copy
  的既有分母保留；五个既有复制程序仍是 20 阶段通过。
- 原 module O0/O2 仍在退休期限处失败；objects 十模式仍在首事务失败
  （八退休期限、两聚合租约断言），目标 250 事务未完成。修改分片前也有
  一次十一门 9/2 验证；两次属于不同生产输入，分别保存，不把历史通过数
  相加，不声称本轮只有一次完整验证，更不把原红项改成 XFAIL。

下一步的必要闭环仍是所有外层参与方、稳定快照、候选 claim/commit 和退出
冻结后的析构。scope 必须在每个参与对象自己的锁之前进入，不能拿着 Future
锁再等待 freeze。生成代码的直接计数、cell/frame/global 写入和异模块入口
需接通；Trace 的分配器/错误通知不能等待被阻塞参与者，任意用户回调、join、
fiber 挂起不能放进冻结区。析构中新建/发布子对象可能复活代码能力，模块全局
清理与最终卸载必须有明确合同，不能用一次额外 codeLeases 复查代替。
完整旧语言 24 门、其他类出生路径、发布/VFS/Linux、legacy 退役及默认后端
切换尚未验收。本轮不把部分生产准入宣称为可安全启用的模块收集器。
最终报告：`build/tmp/xlir_launch_v166_ownership_final/results.json`；
XRT 报告：`build/tmp/xlir_launch_v166_xrt_final/results.json`；
冻结入口：`build/tmp/xlir_launch_v166_evidence/results.json`。

### v165：复制能力与析构身份解耦，复制回归已修复

- XRT 的 ObjectLifetimeBindOwned 把不可变 context 放入独立原子 RC 节点，
  每个实际 backing 拥有一条边；shallow clone 共享 backing，COW/deep clone
  的新 backing retain 同一节点。Trace 只报告 context 的真实拥有关系，不按
  复制品重复捏造模块引用。最后 backing 的字段与 finalizer-context 回调全部
  返回后才执行 lifetime Release；可能卸载代码的 Release 必须常驻。
- ConstructionPrepare/Commit 支持没有用户析构的普通构造。实际析构仍用
  prepared finalizer、仍禁止 COW 分裂，失败构造不调用用户析构。原生 deep
  clone 不复制唯一析构职责，语言层既有 copy/clone 策略未被替换。常驻类
  finalizer 借用 lifetime 保有的租约；删除 class_no_finalizer，没有新 Cgen
  语义分支、AST 回查或按程序修补生成代码。
- 相邻审计修复 ObjectMerge 空目标快路：对象不能直接换成源 backing 而
  丢弃目标 lifetime/构造状态。对象现在走克隆目标再分阶段合并的原正常路径，
  IntMap 快路不变；外来模块 B 的字段合入 A，不把 A 的类型代码能力换成 B。
- 新 XRT lifetime-copy 套件每条路径检查 200 shallow/COW/deep/cursor 家族、
  50 唯一析构职责、50 目标能力保留的空合并、5 类原子绑定拒绝、40 次真实
  deep-clone 分配拒绝，以及精确图/内存平衡。与原 construction/publication/
  finalizer-lifetime 三套一起，在 GCC/TCC × 模块化/单头四路径通过；1,918
  项输入前后一致。本能力不提供全图安全点，也没有启用环收集。
- 新 class-copy 门的 GCC O0/O2 每条原生通道各 175 个真实 DLL 生命周期：
  100 复制/深复制/构造前 COW/游标家族、25 次双模块空合并、25 次四线程
  复制 backing 最后释放；实际调用 DLL 哈希代码并检查最后卸载。每条实际
  TCC 通道 150 生命周期（75 浅复制/COW、75 深复制），Retain/Clone/cursor
  保有复制品，源对象先销毁；其中 100 次在 Close 后再次调用生成哈希。
  没有显式 code guard 或强制 root 注册，逐事务检查真实代码边和 allocator。
- 五个既有 copy/clone 程序完全未改，二十阶段通过，重复 C/header/IR 一致。
  原 class-storage 仍是四分配/四提交、两个字段绑定、22 拒绝变异；native
  每条 225 DLL 生命周期不变。新增独立节点使 Create 从四个变为五个分配点，
  故真实 OOM 从每条 100 增至 125，不缩减原失败点；TCC 每条 175 不变。
- 一次十一门集中验证 89.339 秒、峰值三进程、4,040 项输入前后一致，九绿
  两红。frame 原 14 通道、generator 三通道、public 十程序和十轮恢复、
  finalizer/publication/closure/continuation/class-storage 均通过。module
  O0/O2 仍在实际退休等待处失败；objects 十模式仍首事务失败（八退休超时、
  两聚合租约断言），目标 250 事务未完成。新绿没有减少这两个原红门。

下一批回到模块内部环的稳定快照/原子接管/提交，必须协调 native Retain、
COW、weak promotion 和正在执行的生成回调，不把模块锁当全图安全点。
完整外层方法/线程适配器、其他类型出生路径、原 24 语言门、发布/VFS/Linux
和 legacy 退役仍未完成；本轮不评估切换默认后端。
当前集中报告：`build/tmp/xlir_launch_v165_ownership_verification/results.json`；
冻结入口：`build/tmp/xlir_launch_v165_evidence/results.json`。早期 copy discovery
在追加关闭后哈希断言之前执行，只作历史发现记录，当前结论来自最终集中批次。

### v164：构造期代码保活接通，复制语义仍需解耦

- XRT 增加 FinalizerPrepareOwned / FinalizerCommit：前者只接管唯一对象的
  context，后者可在外壳/backing/游标共享时无分配提交。未提交对象仅释放字段
  和 context，不调用用户析构；原唯一析构身份的 COW 限制没有被偷偷放开。
- 常驻 ClassRuntimeV1Get 的 Create/CommitTake 持有实际 CodeLease，生成
  C 只选择经验证的布局 finalizer。无用户析构类也被保活，但当前借用空 finalizer
  的做法导致普通派生 copy/COW 回归，尚不能作为最终代码能力承载设计。
- Typed/Lowered 明示 classStorageAbi；验证器独立核对准备分配、唯一 pending
  owner、消费和成功提交的配对。构造/析构体中的隐式字段访问原本漏过归一化，
  本轮按确切 receiver TypeId/field declaration 补齐，不让 Cgen 回查 AST。
  专项四个分配/提交点（含两处 DynamicFields）和两个字段绑定，22 项变异
  均在 C/header 输出前拒绝；原发布门扩展为五提交、30 项变异。
- GCC O0/O2 每条原生通道 225 次真实 DLL 生命周期，覆盖无析构类、构造
  放弃、Retain/Clone/游标、最后回调内创建游标/子对象、四原生线程别名释放；
  另有每条 100 次真实分配拒绝。每条实际 TCC 通道 175 生命周期：25 无析构
  owner、75 成功提交、75 提前发布后失败；检查真实模块回指、实际卸载和
  allocator 平衡，没有额外显式代码守护或强制 root 注册。
- 最终十门集中验证 84.962 秒、峰值三进程、4,034 项输入前后一致，八绿两红。
  frame 原 14 通道、generator 三通道、public 十程序与十次恢复、finalizer、
  publication、closure、continuation-scopes 和 class-storage 通过。模块 O0/O2
  与原对象十模式仍首事务失败，250 目标事务未完成。第一次集中验证的三个红门
  包含测试链接漏项、旧人口断言；修正后重新共享构建合同驱动再集中复验，
  不把第一次结果覆盖，也不把测试装配修正算成原退休问题减少。
- 追加复制核验保持五个既有源程序不变：15 个 emit/run/module 阶段，九通过、
  六失败。cgen_class_copy、cgen_copy_clone、cgen_generic_class_copy_clone
  的两种运行方式均报 invalid state；两种 custom copy/clone 均通过。
  源码还显示 runtime-deep → xrtValueDeepClone 复制生成 identity 回调却不接管
  finalizer/context，属于独立代码边缺口，不能通过拒绝合法 clone 来掩盖。
- XRT GCC/TCC 的模块化和单头四路径均执行新 construction 与原 publication/
  lifetime 三套测试，源码输入前后一致。外层脚本误按短名 tcc 查成功标记，
  官方日志实际输出完整可执行路径；保留原报告并精确核对退出码及原始标记，
  另存只读审计，不为文本标记重跑重型验证。

下一批先分离普通类的可复制代码能力和不可分裂析构职责，统一覆盖 shallow
copy、COW split、deep clone 的新 backing/字段图；随后与模块内部环的稳定
快照、原子接管、提交闭环。完整外层方法/线程适配器、发布与 Linux、原 24
语言门和 legacy 退役仍未完成。本轮不评估切换默认后端。
证据入口：`build/tmp/xlir_launch_v164_evidence/results.json`；十门复验：
`build/tmp/xlir_launch_v164_ownership_final/results.json`；复制发现：
`build/tmp/xlir_launch_v164_copy_probe/results.json`。

### v163：已接受异步执行与闭包代码拥有关系接通

- ticket 不再复制 context/release/graph ops 原始 pin，改为拥有实际 CodeLease
  节点。根执行使用普通 Acquire，关闭后即使处于已接受回调内也不能创建新根；
  successor 无分配地继承同一租约，预留失败/Abort 不多释放或多获取一次。
- resident run_one 在 run 与 release 外侧持有 CodeScope。前驱的生成回调和
  作用域全部结束后才发布 successor；末次 host drop 返回前继续占用 queue
  执行额度，允许回调重入 Close/Destroy 而不提前释放正在使用的队列或代码。
- GCC O0/O2 每条原生通道各 100 次实际 DLL 生命周期、200 对 run/release、
  350 个关闭后创建的子闭包、100 次无分配 successor abort/recommit、25 次
  四原生线程排空，另含 100 次根租约真实 OOM 和 100 次容量回滚。检查精确
  拥有图、主错误、最后生成回调返回后的实际卸载及 allocator 平衡。
- 新语言程序由同源公开编译器生成，经真实 TCC 模块执行。GCC O0/O2 每条
  各 100 次 Close → async 恢复 → 创建闭包 → 调用 → 实际卸载；50 次由 Future
  保有，50 次抽离后由原生宿主 retain 保有。没有额外显式 owner 守护，也没有
  替换生产 reservation/executor；C/header/IR 重复生成一致。
- 一次九门集中验证耗时 88.584 秒，峰值三进程，4,026 项输入前后一致。
  frame 原 14 条（含 queue/handoff/executor/progress）、generator 原三条、
  finalizer、public、publication、closure 与新增 continuation-scopes 门通过。
  原 worker-record/thread-record 分配失败仍逐点执行，不被新增 lease 分配
  抢占失败注入；原分母不缩小。
- module O0/O2 的实际图计数已包含 ticket → CodeLease → module，仍在退休
  等待处失败；objects 十模式仍首事务断言退出（八个退休超时、两个聚合租约
  断言），250 次目标未完成。不会用专项绿色替代这两个原门。

本批没有修改 IR/Cgen 的语义选择或 XRT。旧专项的源码链接补齐 resident
lease/closure 依赖；集中批次未选中的旧专项只检查脚本语法，不算全部重跑。
批次完成后修正架构检查的旧位置要求：原子闭包 RC/环境字段检查迁至实际常驻
定义，同时禁止 Cgen 重复实现；加强普通根接纳、真实 lease 边及作用域/发布
顺序检查。保存批次使用的旧脚本精确哈希副本，不把改后的脚本冒充批次输入。

下一批主线仍为类对象自动代码边与模块内部环的安全退休，联合处理原生
Retain/COW/weak promotion、稳定快照/原子接管/提交。作用域接通是运行中根
可见性的前置能力，不是全图安全点或环收集器。
证据入口：`build/tmp/xlir_launch_v163_evidence/results.json`；集中报告：
`build/tmp/xlir_launch_v163_ownership_verification/results.json`。

### v162 的已验证增量与新增整合失败

- `xl6_closure.h/.c` 提供版本化 resident API 表。返回实际常驻回调地址，避免
  将生成 DLL 内的 PE import thunk 当作常驻析构地址。Cgen 不再复制闭包结构、
  refs/ownership ops 和最后释放算法，仅消费已验证的固定物理 ABI。
- 普通调用的 Enter/Leave 同时持有闭包及代码作用域。动态装箱记录独立保有
  producer closure 和 adapter lease；Trace 报告两条实际拥有边，最后释放和
  invoke 返回尾部在常驻代码中结束。环境为空也不据此推断代码为常驻。
- 独立 IR 门：4 个源程序、16 个 Typed/Lowered 错配变异均在 C/header 输出前
  拒绝；text/JSON 明示 ABI。GCC O0/O2 每条各 50 次真实 A/B 双 DLL 生命周期、
  25 次双 owner 环根变化、100000 次原生线程引用对；逐分配点测试每条各 50 次
  Create 拒绝、100 次 Box 拒绝和 50 次成功，原输入及主错误保持，分配平衡。
- 5 个闭包语言程序的 emit/repeat/run/module 共 20 阶段通过，C/header/IR 重复
  相同。原 raw C export fixture 的全部断言及 1000 次保存/替换，在同源 XRT 的
  O0/O2 通过；该 AOT 通道中的原生代码是外部常驻，不证明任意 native DLL 保活。
- 八门集中回归 4,018 项输入前后一致；frame 14 条、generator 3 条、finalizer、
  public 十程序与十次恢复、publication 及新增 closure 门通过。module 门先在
  真实图从 7 节点/7 边变为 8 节点/9 边时失败。仅更新真实图及租约数预期后的
  O0/O2 定点重测继续在等待实际卸载处失败，未移除原 50 生命周期验收要求。
- 原对象门仍保留十模式、每模式 25 次目标及内存/实际卸载断言；当前十模式
  均在首事务断言退出，没有完成 250 次事务。八个模式退休等待超时，internal
  与 scalar 两模式检测到全局闭包租约。不得记成 XFAIL 或声称比 v161 更少红项。

v162 当时的下一批计划为处理真实图的安全退休与类对象自动代码拥有边。必须联合处理原生
Retain、COW、weak promotion、运行中回调和检查至提交边界，不能用一次 Count
重读/API 锁冒充全图静止。continuation 继承和其余外层适配器仍须随后闭环。
证据入口：`build/tmp/xlir_launch_v162_evidence/results.json`；集中报告：
`build/tmp/xlir_launch_v162_ownership_verification/results.json`；模块形状修正后：
`build/tmp/xlir_launch_v162_module_shape_retest/results.json`。

### v161 的已验证增量与边界

- `CLASS_DESTROY_BIND` 在两个结果上都消费输入。类型/析构计划在分裂 CFG
  之前绑定，避免将 payload 元数据误写入末尾的清理指令。生成器仅调用 XRT
  的固定 `xrtValueObjectFinalizerBindTake` ABI，不自行决定回滚和错误传播。
- `XL6_CodeScope` 使用调用方栈上记录和常驻互斥链表，不依赖首次进入原生线程
  时分配 TLS 表。它真实持有租约引用，执行期间是图外部根；Leave 在生成回调
  返回之后释放。只有同线程栈中完全匹配 context/acquire/release/graph ops 的
  能力可由 `AcquireScoped` 继承，普通 Acquire 在 Close 后仍拒绝。
- 独立发布门通过 3 个绑定、18 个 C/header 前变异拒绝；GCC O0/O2 各 100 次
  成功和 100 次实际原生游标绑定拒绝，检查 catch、前置局部、字段和分配平衡。
  CodeScope O0/O2 各 50 次真实 DLL 回调返回后卸载、25 次图根变化、10 万次
  无分配原生线程继承，以及跨线程和错误 owner 身份拒绝。
- 一轮七门集中回归为六通过、一失败；原六门中的五个绿门保持，新增发布门
  通过，对象门原十模式每种 25 次仍八红两绿。4,011 项输入指纹前后一致。
  这不是完整 24 语言门的重跑，也不证明自动模块环回收或生产安全点已完成。

证据入口：`build/tmp/xlir_launch_v161_evidence/results.json`；具体集中报告：
`build/tmp/xlir_launch_v161_ownership_verification/results.json`。

## 已存在的运行时能力

当前 `lib/xruntime/xruntime.h` 已包含以下公共合同，不能再沿用“运行时没有
对象图/弱引用”的判断：

- `xrtobject`：拥有型强引用、弱引用、实例负载和实例 Trace/Drop。
- `xrtObjectGraphCollect`：调用方保证静止安全点；依据实际强引用数减去图内
  入边识别根，再收集不可达强引用环。并不是并发 GC。
- `xrtObjectGraphCollectValueRoots`：允许调用方明确提供外部 Value 根。
- `xrtValueTraceRuntimeObjects`：追踪 Value 图实际拥有的 runtime-object
  引用；单次调用内去重共享外壳和 backing。
- `xrtDynamicFieldsGetRef/SetRef`：保留 Value 身份；默认 Set/Copy 的深复制
  合同与它不同。动态字段并未给 XLIR 提供未知宿主持有者的根清单。

这些能力适合完整、符合真实拥有关系的图，不能把任意 Value 外壳直接省略。

## 批量验证的关键反例

`test/cases/xlir_object_graph_boundary.c` 用两个常驻宿主对象模拟模块和对象：
模块经 Value 拥有对象，对象反向拥有模块。描述符、Trace、Drop 全部常驻，
不涉及可卸载生成代码。它刻意验证一个“不完整的扁平化适配器”，不是新的
生产实现，也不是对 XRT 既有合同的语言级担保。

通过 `tools/check_xlir_object_graph_boundary.ps1` 分别运行：

- 从当前 xruntime 快照头独立编译的运行时；
- 链接现有 `release/xl.dll` 的驻留运行时。

每套 1,200 个事务：800 个根/别名场景、100 个共享边重复计数拒绝场景、
300 个真实分配失败。两套结果一致，均检查精确 XRT 内存平衡。

直接 `xrtObjectRef` 或新建独立 runtime-object Value 外壳会增加对象的实际
强引用，收集器能识别外部根；释放外部拥有者后，内部环可以收集。

但是 `xrtValueRetain` 只增加同一 Value 外壳的引用，容器 `xrtValueClone`
也可能只新建共享 backing 的 COW 外壳。省略这些中间拥有者后，对象强引用
没有增加，外部别名便不可见。探针验证此不完整图会在宿主仍持有 Value 时
终结两个对象；只检查弱引用过期，不调用已终结负载，随后平衡释放外壳。
若明确把宿主 Value 列为根，则不会误终结，宿主释放之后可以收集。

另一个反例是两个实例共享同一 Value 外壳：每个实例单独调用 Value trace，
跨调用不共享去重状态，求和后会把同一实际强引用算两次。收集器对此拒绝，
保持输出统计、对象负载和图成员不变。三个快照分配点失败也保持同样的失败
原子性。不能通过压低计数或忽略追踪错误解决这个建模问题。

## 语言验收门保留的要求

`xlir_object_escape.xl` 和实际 TCC 宿主已扩展至十种模式，各 25 次：
原闭包、类、容器、抽离子项、纯内部对象、普通标量六种不删除；新增共享
全局 callable、共享全局类、共享全局 COW 容器、共享全局容器子项四种。

原四种自动保活失败仍失败，原两个无须保活模式仍通过，新增四种失败。
失败统一源于缺少自动拥有关系，不能记成四个新的编译器回归。测试仍在发现
缺少自动 lease 后显式加测试专用守护，保证析构/调用不发生在真实代码页卸载
之后；守护不能当作生产修复。每轮最后验证实际 TCC 页释放和精确内存平衡。

## 接入决策与边界

不能直接采取“给所有对象加模块 pin，再调用现有对象图收集器”。pin 会产生
模块自持环；当前 Value 桥接信息不足以把这个环与未登记宿主根安全地区分。
也不能把所有经过 Value 的边都算作外部根：那会使纯内部环永久保活。

保留当前宿主 `xrtValueRetain` 语义的推荐方向，是在运行时层建立包含 Value
外壳、共享 backing、代码拥有对象和模块状态的完整拥有关系合同，或提供
等价的稳定追踪/根可见性机制。单独暴露瞬时 RefCount 不够；还必须定义
安全点、共享边身份、callable 环境及 native 载荷追踪、失败原子性，以及
最后一次生成 Trace/Drop 返回之前描述符和代码页的保活顺序。

另一条路线是要求所有宿主逃逸都经过显式拥有句柄/根登记 API。它能利用
已有显式根收集，但会改变当前任意 native 回调可自行 `xrtValueRetain` 的
嵌入合同；未经用户确认不能悄悄替换。

推荐方向需要扩展 xruntime 公共合同，不应由 XLIR 读取 XRT 私有结构或手改
amalgamated 头来实现。在未获得修改 sibling XRT 的授权前，不实施这一跨仓库
变更。随后 XLIR 再以显式类型/实例/capture/模块拥有关系描述该 ABI，Cgen
只序列化经验证的 IR；不能回到 AST 查找或按函数名补 pin。

当前固定验收账本仍为 24 门中 23 门绿、对象门一门红；本次 runtime 探针
独立列为 capability probe，不扩大准入通过数。完整异步/网络、公开编译链、
Linux 和同源发布准入等仍另有未完成工作，不能由本批次推导总进度。

### v148 授权与边界更新

用户现已允许修改 sibling XRT。本轮先实现并验证了公共内部线程存储退役接口，
关闭 standalone DLL 卸载后 FLS 回调到无效代码页的问题；这不解决本文的
对象拥有图问题。原 24 门再次批量验证仍为 23 通过、对象逃逸 1 失败。
对象图的后续实现已不再受“禁止修改 XRT”限制，但仍必须保持原有
`xrtValueRetain` 嵌入合同，并提供前述共享 backing、native root、Trace/Drop
代码驻留及模块内部环的完整语义，不能借线程存储退役来宣告自动对象保活完成。

### v149：完整物理图与 native 环收集已经落地，语言模块接入未完成

Sibling XRT 新增按物理控制块身份工作的 `xrtOwnershipInspect` 与
`xrtOwnershipInspectReachable`。Value 外壳和 COW backing 分别入图；每个实际
拥有槽保留一条边，共享 backing 的元素边只展开一次。Count 统一在 Trace 的
临时迭代引用归还之后读取。既不改宿主 Retain 语义，也不把宿主别名当作内部引用。

Callables、native object payload、handle 及 finalizer context 通过明确适配器
接入。弱引用没有强拥有边；未知 handle/context 和未建模的 IdentityUserData
拒绝检查，不能默认当作叶子。旧的扁平 object-only Trace 不充当完整适配器。

`xrtObjectGraphCollectOwned` 已把完整图接到实际的 native 环收集流程：
先建立临时强引用快照，完整检查，再验证、统一 claim、Drop 和释放快照。
只扣除收集器自己确实持有的快照引用。外部 Value/COW 别名保持整个可达环，
宿主释放之后才真正终结；不再只是一个统计探针。域外 native 对象作为保守根，
避免域外弱引用提升后看见本域已被部分终结的环。

验证结果（每个编译器/布局独立执行相同断言）：

- GCC/TCC × 模块化/单头 × 两套新测试，共 8 个进程全部通过。
- core/value 套件：800 个别名环、200 个共享图、400 个分配失败、200 个
  无效图，另含深度 4096、finalizer context、IdentityUserData 拒绝和可达位原子性。
- runtime 套件：600 个实际收集的别名/环场景、200 个共享图、700 个真实分配
  失败、100 个 Trace 失败、100 个跨域弱引用/部分收集场景；检查精确内存平衡。
- 原 XRT 相关回归维持单独分母，24/24 个测试进程通过。

可复现入口：`D:/GIT/xrt/tools/check_ownership_graph.ps1`。
冻结报告：`D:/GIT/xrt/out/ownership_graph_v149_final/results.json`，SHA256：
`2C9CB827284392A1C1A44EFC955F94193AEE36A7AF01DA179C951B5364C70376`。
已按生成流程同步 DEMO6 快照：XRT 2964、xruntime 3411 个公共 API。
未重建发布二进制或 VFS。

**未完成的语言侧要求仍保留**：XLIR 类型/实例、闭包 capture 和共享 cell 的
完整描述；Future/native 复杂载荷的完整适配；生成对象与模块退役域的连接；
安全点从检查一直保持到提交，以及最后一次生成 Trace/Drop 返回后的代码释放。
新的 native 收集器要求调用方提供整个传递图的静止和代码驻留保证，不自动建立
这些保证；也不替语言模块决定全局变量的销毁顺序。

因此不能把运行时新测试加入原语言 24 门的分母，更不能把对象逃逸门的测试专用
guard 当作生产保活。v148 的原语言门结果为 23/24，v149 的运行时收集成功并未
使那一门自动转绿。长期任务没有因此完成。

同步后已从当前源码重新构建 56 个翻译单元的公开编译驱动，10 组
emit/确定性/run/module 和恢复测试通过。实际 TCC 对象逃逸子门也已重新跑完：
十种模式各 25 个事务，八种需要对象自动保活的模式仍返回 2，internal-only
及 trivial-value 返回 0。该门仍使用其历史 released-runtime lane，测试专用
guard 保证调用/析构及真实代码页释放安全，不能作为新 XRT 收集器已经接入
生成语言对象的证据。本轮未复跑原语言 24 门整批，不更新整批通过率。

### v150：生成闭包、cell 和 callable 的物理拥有边接入

Cgen 消费已验证的 `XL6_IrClosureCaptureAbi` 来发射 Trace：每个
RETAIN_XVALUE、RETAIN_CLOSURE、RETAIN_CELL 槽各有一条真实拥有边。
唯一拥有的环境不是额外的引用计数节点；复制字符串是独占叶子分配。
共享 cell 自身入图，其 Value 载荷只展开一次。闭包/cell 保存创建方 ops
身份，不随观察方改变；不回查 AST 或按源名称猜边。

闭包装箱的 native callable 绑定环境适配器；反向 checked cast/容器投影
追踪其实际持有的 Value；绑定方法追踪 receiver。未知非空环境拒绝检查。
非空 Future capture 的完整载荷/continuation 适配尚缺，明确拒绝图检查，
但没有禁止原来的语言执行。生成 Trace/ops 仍需调用方保持整个图静止、
所有生成模块驻留；本轮不建立安全点，也不提供自动代码 pin。

新增真实生成 C 宿主验证七种图各 100 轮：共享可写 cell、嵌套闭包、两个槽
指向同一闭包、绑定 receiver、checked cast 和容器函数投影。检查精确
节点/边数、原生 Retain/COW 别名、函数结果和内存收支。GCC O0/O2、独立
TinyCC 三进程全部通过，每进程 700 个图事务。22 个 capture/cell 畸形 IR
变异在 C/声明头输出前被拒绝；生成 C/H/JSON 确定性通过。TinyCC 发现的
C11 标签后直接声明问题统一修为 `label:;`，不针对某条指令打补丁。

对象门已使用当前源码 XRT，复用对象须通过源码构建清单和哈希校验。旧 DLL
仅作为 TCC 提供者，TCC 自己的分配器不纳入新 XRT 账本。当前 XRT 符号必须
先于旧环境默认符号注册，并用实际运行的 canonical ops 身份探针校验。
十模式各 25 次仍是八种自动保活 exit 2，内部及标量 exit 0；guard、析构
断言和真实 MEM_FREE 检查均保留。

另发现待解决正例：前端接受 `func() as int restored = boxed`（var callable），
XLIR emission 却拒绝 initializer/storage ABI。原样保存在
`test/pending/xlir_callable_implicit_projection.xl`，不改成“预期拒绝通过”。
显式 `as func() as int` 和类型化容器取出路径已验证。

### v151：消除上下文 callable 转换的语言缺口

上一节 pending 初始化正例已移到 `test/cases/xlir_callable_implicit_projection.xl`
并通过公开入口运行。初始化、赋值、参数、普通/延迟返回共用经过验证的非空
callable checked-cast 合同；转换失败保留原目标，转换产生的 owned closure
使用其结果表示处理所有权。async/generator 的实际生成模块分别在 GCC O0/O2
和独立 TinyCC 下完成 100 轮内存平衡验证，详见批次收敛流程 v151。

这项修复复用 v150 的 callable/closure 拥有图适配，不新增宿主 Retain 语义，
不提供模块自动 pin 或全图静止机制。对象逃逸十模式仍有八个自动保活红项，
不可用这轮语言回归成功替代它们的验收。后续核心工作仍是复杂 Future/frame
载荷、模块全局内部环、并发检查/提交及生成析构返回后的代码退役。

### v152：Future 物理节点与原子载荷 Trace 合同

Sibling XRT 新增 xrtFutureOwnership/xrtPromiseOwnership：二者规范化为同一个
控制块，FutureRef 和 PromiseRef 每次都对应一个实际强引用。结果转发只追踪
真实保留的源 Future，不对借用的结果再造一条拥有边。取消父链及错误 cause
分别通过新增 xrtCancelOwnership/xrtErrorOwnership 描述，静态永生错误为空视图。

xrtPromiseResolveOwnedTraced 将值、析构、上下文和 Trace 一次发布，失败不消费
资源、不调用用户回调；旧 ResolveOwned 语义不变。Trace 必须准确枚举值和
析构上下文所释放的拥有槽，包括重复边。未知 owned payload 或挂接的 waiter
拒绝检查。无 waiter 的 pending 可检查，但仍有 Promise 的节点保留外部根。

新门 GCC/TinyCC 各模块化/单头共四个进程通过；每进程 100 轮，覆盖 producer
别名、转发、共享载荷/上下文、错误/取消父链、自引用环和 1,200 个分配失败
注入点探针，逐轮内存平衡。原 core/cancel/future/continue/combine 19 个回归
进程全部通过，单独记账。冻结报告在 sibling `out/future_ownership_v152/results.json`。

重要边界：本轮提供可被语言适配器消费的基础合同，尚未把生成 Future 的
所有生产路径改成 traced publication，也未接入 runtime ValueFuture 包装的
拥有边、挂起 frame/Watch 的拥有图或模块自动退役。现有 Future capture 仍
保持 fail-closed。调用方必须继续保证全图静止、无进行中的通知/释放且全部
Trace/Drop 代码驻留。本接口不能单独作为卸载许可。

### v153：已接通语言结果生产端和 ValueFuture / Future capture

v152 的基础接口现在已被 ready async、状态机完成、任务池 async、task.spawn
及 Future continuation 消费。结果析构/Trace 从同一已验证 AsyncResultAbi 选择；
resident payload 不保存生产模块的描述地址，closure payload 追踪实际生成闭包。
ValueFuture/Take 和 closure Future capture 均接入实际 Future 物理节点。

任务池新增 additive xrtTaskSubmitTraced，在队列可见之前固定结果描述。公开
xtaskargs/xtaskvalue 保持三字段布局；旧 Submit/ResolveOwned/native Retain 语义
不变，未描述 owned 结果仍拒绝检查。任务数据清理仍先于终态发布，新接口没有
描述运行中的任务/frame，也没有增加模块保活或安全点机制。

新语言门 GCC O0/O2/TinyCC 每条 700 个图事务与 1500 个 resident 载荷场景均
通过，另有 25 个精确结果/capture ABI 变异拒绝。XRT 14 个编译器/布局进程通过；
原 26 个相关回归和两个 Atomic 测试依赖检查分别通过。详细分母见批次流程 v153。
所有图检查都在 worker join 后进行且代码始终驻留，这不是生产卸载测试。

后续必须继续覆盖：聚合/网络原生载荷的真实拥有槽、挂起 frame/Watch、模块全局
变量与对象形成的内部环、从 Inspect 到 claim/commit 的全图静止，以及生成析构
完成之后的代码释放。对象逃逸十模式仍为八个自动保活红项，不能以终态 Future
适配成功替代这项语言保证。

### v154：显式 Watch 与原生聚合节点

新增 XRT traced Watch 初始化和 CancelWatch 物理视图，保持公开 Watch 的 64 字节
布局。Any/All/Race 的共享组、每个源槽和取消监听按真实引用入图；相同源出现
多次就是多条拥有边，不能对借用的 Pick/All 结果再造拥有边。未知语言 waiter
仍拒绝检查。GCC/TinyCC 两种布局共 12 个拥有图测试进程、原 26 个相关回归及
两个 Atomic 依赖检查通过；本批不构成语言 frame 已接入的声明。

### v155：语言聚合结果和一次受理的映射

原生聚合器新增 All/Any/RaceMapOwnedTraced。在激活源监听前准备全部传输资源，
启动返回 NULL 不取消输入、不遗留组/监听，也不消费映射上下文。受理后映射
失败体现为结果 Future 的失败终态。Mapper/Drop/Trace 共同定义独占上下文的
真实拥有槽；并发完成后迟到的监听插入会补偿摘除。

XLIR 的 task.all、task.race 和 task.whenAny 使用该路径。all 不再创建 legacy
元数据和多层 continuation/cancel-link；whenAny 的映射/析构代码驻留在运行时，
Cgen 仅发射已验证操作的调用。原始失败错误保持身份；取消、关闭和成功状态
分别传递。无返回值 all 同样检查源终态，不能用原生 All 的“协调成功”冒充
语言成功。同步 await 与 async 恢复共用 resident outcome observer；取消诊断
保留语言消息，在诊断分配失败时仍保留取消种类，不改变终态优先级。

实际生成图在 GCC O0/O2/TinyCC 每条运行 3,500 次，覆盖字面量/数组、重复源、
闭包载荷、原始错误、源/输出取消和关闭，验证精确节点/边/根和逐轮内存平衡。
每条另验证 192 个桥接启动注入点（34 次准备失败、8 次受理后映射失败）。
9 个 IR 操作的 36 个畸形变异在 C/声明头输出前拒绝。详见批次流程 v155。

尚未闭合的是完整语言生命周期：聚合器的生成 boxer 仍需要调用方保持代码
驻留；挂起的通用语言 frame/continuation、其它原生载荷、模块全局内部环、
Inspect 至 claim/commit 的静止与最终生成析构返回后的代码退役仍需接通。
对象逃逸十模式已在当前源码复测，仍有八个自动保活红项；不能拿本批完整
聚合物理图或测试专用 guard 代替生产模块自动保活。

### v156：通用 async 帧与 Watch 的物理租约接入

XLIR 的公共 continuation frame slot 现在携带 `traceAbi`。绑定 resident loan
之后按精确 destroy ABI 固定追踪合同，async/generator verifier 均独立重算；
借用视图不增加拥有边，托管字段只在 init bit 有效时追踪。Cgen 不按 C 类型、
函数名或源码语法补找引用。Value、Future、closure、cell、error 已接入；字符串
和纯值没有子拥有槽。尚未描述的 generator/iterator 状态显式标为 OPAQUE，
存在该已初始化载荷时拒绝检查，不能假装叶子。

运行时 ticket 记录两种真实物理租约：注册者的帧所有权在 Commit 后转给队列；
Watch 另持一个存储访问租约，并在 TransportReleased 中归还。源码 Future 的
traced Watch 只报告后者，ticket 再追踪整个帧拥有的槽。队列执行租约始终保留
为根，不从图中扣除。Abort、READY、真实终态、前后帧交接和最后一次物理释放
均更新或核对同一计数。未 Commit、运行中、交接中、未绑定 Trace 或未知宿主
code owner 都拒绝检查；不会遗漏 code-owner 边后仍声称图完整。

当前源码 GCC O0/O2/TinyCC 每条验证 1,500 个实际生成事务：两个真正 pending
悬停点共 3,000 次，另有 100 次 ready-source 恢复，共 3,100 次物理帧退役。
共享参数、closure/cell、错误、原始失败身份、源/输出取消和关闭均检查精确
节点/边/根及内存平衡。图门使用宿主驱动队列提供全图静止；生产执行策略另由
既有终态门和队列/执行器回归覆盖，不把人工静止当作生产安全点实现。

本批未修改 sibling XRT。绑定模块的 code-owner 图、generator/iterator 与
其它 native 载荷、模块全局环、Inspect 到 claim/commit 的生产静止及最终生成
Trace/Drop 返回后的自动代码退役仍未闭合。对象逃逸十模式在当前源码下复测
仍为八红、两绿，发布二进制没有重建。完整证据见批次流程 v156。

### v157：生成器、迭代器及跨执行模式的消费合同

XRT 新增 `xrtValueIterOwnership`。栈/堆迭代器的唯一拥有权对应一次 End/Destroy，
唯一子边是 retained backing 快照；源 Value 壳、当前元素和键均不是额外拥有边。
四种容器、正逆序、重复元素、原生 Retain、COW 壳、源修改/释放后的快照和
检查失败原子性均有固定图断言。NULL 是空视图，已 End 的有效游标是独占空节点。

生成器控制块保留创建模块的规范 Ops 地址，分别报告唯一帧和当前 yield 载荷。
init bit 决定帧槽是否已拥有；GET 不伪造引用，TAKE 把当前载荷拥有权转给调用者。
`GeneratorMachine.itemTraceAbi` 与共享帧 `traceAbi` 由 IR 固定并独立验证，Cgen
只使用统一的帧槽发射器。执行或清理中的生成器拒绝检查。没有把这个局部 busy
检查当成整个引用图的并发安全点，也没有把描述符地址当成模块保活租约。

真实语言组合暴露并修复两个独立问题：catch 的合成 error 缺少前端 ABI 标识，
导致与声明 error 形成两个 TypeId；async/generator 未接通已有的 generator
消费指令。前者在类型池输入处统一 `xlang.error`，后者让普通、async、generator
共用 NEXT/GET/TAKE 与独立成功/结束结果的发射逻辑，没有回退到 AST 或旧后端。

同源 GCC O0/O2/TinyCC 每条通过 2,400 个生成器生命周期、1,000 个跨 await 的
迭代事务、2,000 次真实挂起、1,800 次运行中拒绝检查和 6,600 次实际检查分配
失败；图节点/边数、错误身份、提前关闭、终态及逐例内存收支固定。八个 machine、
23 个槽和 16 个消费操作共 141 个 IR 变异拒绝，包含 generator 内继续消费
另一个 generator。原公开 generator 断言及新的组合程序在当前源码的 run 和
relocated module 入口执行通过；不冒充最终发布 DLL/CLI 的 JIT/AOT 验收。

尚未闭合：绑定模块的 code-owner 图与自动 pin、模块全局内部环、其它未知
native/网络载荷、Inspect 到 claim/commit 的生产静止，以及最终生成 Trace/Drop
返回后才解除代码映射。原十模式在同一源码下复测仍八红、两绿；native Retain
语义不变，未用测试 guard 或强制宿主根登记替代这些语言生命周期保证。

### v158：模块节点、全局槽与可追踪代码租约

`XL6_CodeOwner` 的四字段前缀保持不变，新增带标记的 `XL6_CodeOwnerGraph`
扩展。Ops 与 acquire/release 操作的是同一个物理模块节点，不以函数地址
伪造拥有者。生成模块在绑定时复制完整的已识别能力；没有扩展、未知标记或
缺少完整模块 Trace 的旧宿主仍允许原租约行为，但拒绝图检查。

宿主按真实模块句柄和 codeLeases 计数，退休队列只充当发现锚点。入口正在
执行、清理进行中或清理尚未确认完成时拒绝检查。模块 Trace 使用新的保留
入口 `xl_script_module_ownership_trace`，保留全局 Value 外壳、共享 backing
与捕获的实际拥有边。Lowered storage.moduleTraceAbi 由同一物理 ABI 词汇
派生，验证器独立重算，Cgen 共用已有视图映射；不追溯源名称或 AST。
未描述的全局物理表示及模块任务池/网络服务状态仍拒绝，不能被当作叶子。

共享 CodeLease 节点报告自己的真实原子引用数，但对模块只有一次 acquire
所对应的一条边。continuation 的同一代码租约和规范 Ops 身份随 successor
转移，既不重复 acquire，也不把继承关系计成外部根。最后释放依旧由常驻
代码在生成回调和帧清理返回之后执行。这一轮没有新增收集提交或自动 pin。

兼容性反例验证：只复制旧前缀的模块不能收到扩展长度。`legacy-prefix`
在修复前实际失败；现在仅当模块提供新 Trace 入口时传递扩展，否则传递
准确的旧前缀大小。不是通过放宽旧模块断言完成兼容。

实际 TCC 门在 GCC O0/O2 宿主下各执行 50 次模块生命周期、25 次 successor
交接、325 次状态/不透明图的原子拒绝、600 次检查分配探针（200 次实际失败）。
模块未初始化时为一个节点；初始化后固定七节点/七边，共享租约为八节点/八边，
带拥有值的 ticket 为八节点/九边。闭包独占环境的槽归于闭包节点，不重复
制造引用计数节点。原生 Retain 别名增加真实根；逐事务内存平衡和 TCC 页面
实际释放均通过。这里的宿主明确持有代码租约或已受理 ticket，并提供全图
静止，不能冒充外部对象自动保活、生产并发安全点或循环收集。

原对象门仍八红、两绿。下一步须把逃逸对象自身的代码拥有关系接入常驻释放
路径，再完成模块内部环、检查到提交的安全机制和最后一次生成析构返回后
的自动卸载。详细同源回归与证据见批次流程 v158。

### v159：原子终结职责、字段之后的常驻释放与真实类/游标回归

发现并复现了原 XRT 的实际缺陷：终结器只在 Value 外壳释放前检查 backing
计数为一；若迭代器成为最后 owner，就会漏掉终结器。并发释放多个共享
backing 外壳也不能用这个非原子预检查决定唯一终结职责。旧源码最小反例
在 sibling `out/value_finalizer_v159_discovery.log` 真实失败，未改为预期通过。

现在由 backing 的原子最后释放取得职责。带终结器的公开迭代器同时拥有
真实来源外壳和 backing；End 先清空游标再依序释放两槽。普通无终结器 COW
快照仍只保留 backing。图按实际槽计边，检查器的临时栈视图不取得外壳引用。
终结器仍可读写/枚举字段，不能 retain/clone/release 自身；运行中图检查拒绝。

新增 `xrtValueObjectFinalizerBindOwned`：唯一外壳和 backing 上无分配原子
绑定，失败不接管上下文。Finalize 返回及所有 backing 字段释放之后，才运行
上下文 Release；Finalize 新建并留下的游标可以继续保留已终结 backing，字段
与上下文随其 End 延后释放。旧 Bind 的上下文管理合同保持不变。

常驻 `xl6CodeFinalizerBind` 复制回调、保留已受理的共享 CodeLease，并追踪
上下文实际拥有槽。它把生成 Finalize、字段 Drop、生成上下文 Release 和
最后常驻 CodeLeaseRelease 分开；关闭后继承不重新申请模块入口。新的分配
和接管失败均可回滚，未把现有 infallible 类绑定偷偷改成分配失败后 abort。
该适配器已接入源码/API 符号/构建源清单，**尚未由生产 XLIR 自动选择**。

同时修正公开 C sidecar 包装器：先包含与 XLIR 相同的完整声明头，而不是
先包含已裁剪的实现头、锁住特性 guard 后再让 sidecar 请求更多声明。真实
语言程序通过原生 C 保留类对象的正/反向、原壳/COW 游标，每个入口执行
200 次并精确断言析构次数、receiver 字段和模块全局；emit/重复 C/H/IR、run、
module 均通过。它的模块入口仍活动，不是自动模块保活测试的替代品。

新 XRT 四个编译器/布局进程各 501 次生命周期、100 次四线程事务、2400 次
绑定拒绝、4800 次图探针/1600 次实际失败。常驻适配器 O0/O2 各 250 次真实
DLL 生命周期，500 次终结/上下文释放、250 次字段 Drop；25 次分配失败、
25 次发布拒绝、4900 次图探针/1400 次实际失败，最终 MEM_FREE 和精确内存
平衡通过。这里代码租约仍由测试宿主明确提供，未解决模块自持环。

剩余仍是自动对象代码拥有关系及其可失败 IR 合同、全局内部环收集、未知
native/模块载荷、生产 Inspect 到 commit 的安全点与自动卸载闭环。原十模式
仍八红、两绿；完整 24 语言门未整批复跑，23/24 仍只为 v148 基线。
