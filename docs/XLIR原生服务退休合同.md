# XLIR 原生服务退休合同

v218 已建立 readiness watch 代次拥有合同，真实 Select／epoll 场景通过。
kqueue 原生运行、完整 Engine／transport 与语言服务图仍待验证和接入，
不解除完整服务拒绝。见 [原生 Watch 拥有链](XLIR原生Watch拥有链.md)。

v217 已补齐 IOCP／io_uring 请求的真实拥有槽与终态转移；原样 XLIR 规划器在
Windows／Linux 上分别三配置各 128／96 轮通过。watch、transport 与完整
服务图尚未认证，语言生产集成仍为 v209。扩大回归及边界见
[原生 OS 请求拥有链](XLIR原生OS请求拥有链.md)。

v216 已接通 Port Post 到交付事件及 Engine 回调尾部的真实引用转移，并修复
Prepare 冻结边界和并发不透明标志。聚焦三配置各 40/40，原样 XLIR 规划器各
64 个端口循环，扩大原生回归 GCC／TCC 各 307/307（保留原 294 程序）。
OS 请求／watch／transport 仍未完成，不解除完整服务图拒绝。见
[原生端口拥有链](XLIR原生端口拥有链.md)。

v215 已提供嵌入命令的实际拥有转移和双队列投影，保留旧借用命令拒绝，
不将内部队列一律当作叶子。完整发现的 TCP 尺寸回归已通过紧凑布局修复，
没有调高原预算；修复后聚焦三配置各 37/37，真实 Engine 规划器各
192 轮通过；完整 port/completion/transport 的生命周期仍需继续接入。见
[原生嵌入命令拥有链](XLIR原生嵌入命令拥有链.md)。

v214 已加入实际 buffer/pool/byte 拥有合同，区分语义 Release 与物理 Unit，
保留旧接口回调追加，Engine 图退休可让独立缓冲继续保护关闭的池。聚焦三配置
各 68/68，真实规划器各 96 轮通过；完整 port/transport 和生成模块图仍未认证。
见 [原生缓冲拥有链](XLIR原生缓冲拥有链.md)。

v213 已为原生网络任务补齐共享 Frame 的真实拥有关系和受理前装配，三配置
聚焦各 32/32、实际 XLIR 规划器各 80 轮通过。任务的 Engine 引用只保护计数
外壳，不以 LiveObjects 阻止服务排空。完整 port/buffer/transport 与模块图仍
未认证。见 [网络任务拥有链](XLIR原生网络任务拥有链.md)。

v212 已增加 Engine 的真实 RC、拥有型命令／定时器及分阶段退休。完整原生
批次 GCC/TCC 各 157/157，原 155 程序保留；真实 XLIR 规划器三通道各 96
个循环通过。完整 port／buffer／transport 和生成模块图仍开放，不能以基础
Engine 图替代服务图验收。见 [Engine 拥有链](XLIR原生Engine拥有链.md)。

v211 已推进实际 Resolver Future 拥有链，并修复非阻塞 Join 与图重验证之间的
循环等待。只读线程完成观察不会放行仍活动的线程清理。独立规划器专项 GCC
两通道通过，TCC 该专项因安全隔离未运行；Engine 和 18 个真实模块图场景仍未
闭合。当前证据和边界见 [Resolver Future 拥有链](XLIR原生ResolverFuture拥有链.md)。

2026-09-16 补充：v210 已实现并验证原生 Resolver/ResolveOp/地址列表的物理拥有
合同，GCC 与 TCC 的统一原生批次各 139/139，保留原 129 个程序。候选尚未同步到
DEMO6；当前语言集成与下文 v209 一致，完整服务图仍开放。见
[XLIR 原生 Resolver 拥有图合同](XLIR原生Resolver拥有图合同.md)。

当前状态：2026-09-16，v209。原生可重试退休已接入 XLIR、常驻服务槽和宿主，并通过
聚焦验证与统一回归覆盖（首轮 43/44，检查器修正后剩余 1/1）。完整原生服务拥有图
仍未完成。标准库维持最低
必要基线，本项属于语言运行时的通用生命周期正确性，不是扩展协议开发。

## v209：显式 IR、常驻服务槽和可重试宿主

`networkServiceAbi` 从可达、精确的 runtimeBridge 身份派生，验证器重新推导，Cgen
核对实现依赖，产物计划登记独立 NETWORK_SERVICE 常驻组件。没有根据源名称或
生成 C 文本追溯资源语义。六个真实模块的 54 个错误 IR/C/header 合同均拒绝；
单独 TLS stream 不虚构模块级 Engine/Resolver 所有权。

`XL6_NetworkServices` 在创建成功时立即安装实际拥有槽，部分初始化失败仍保留
可退休状态；resolver 先排空，以免其回调仍需使用 engine。仅 READY 清空对应槽。
语言 finalizer 与物理退休分开，语言状态为零仍不允许忽略剩余服务或重新初始化。
旧 checked-unit 保留同步策略，新 V4 `step` 返回 READY/BUSY/FAILED；宿主无须
通过诊断是否分配成功猜测语义，只有 READY 才能提交代码释放。

通用对象图计划增加独立验证的 Finish 描述符和 V3 resolver。完成步骤在冻结之外
执行，每轮每个未完成节点至多调用一次；BUSY 仍保留所有真实 plan pins，但不阻止
其他参与者前进。已 READY 的节点不重复完成；FAILED 即使没有诊断仍是失败事件。
这不是仅对模块增加一个特殊 busy 标志，旧 bool Finish 明确转换为 READY/FAILED。

当前同源编译器构建 70 TU 成功。聚焦结果：

- 对象图 15/15 编译运行通道，保留全部旧程序，新增每通道 501 个实际循环、900 次
  BUSY、200 次无诊断失败、100 个非法返回、400 个前置描述符拒绝和精确内存平衡。
- 服务运行时 GCC O0/O2/TCC 全过，每通道 12 个 Pin、12 个 resolver 自身 worker
  退休、12 个真实 TLS 析构尾部、12 个真实停机失败重试及至少 4 个部分创建拥有槽。
- 宿主 ABI 验证实际 TCC 编译、relocate 和 MEM_FREE：70 次模块卸载、70 个非法
  合同拒绝、20 个真实 module/code-lease 循环计划，含一次性运行、初始化失败和
  无诊断失败；不调用旧 bool 后备，不重复每次激活的语言 finalizer。
- 原 6 个网络程序 JIT/AOT/EXE、30 次公开生命周期和 32 次停止 worker 拒绝仍过；
  新 typed-step 18/18，通过真实 Pin/worker 的 BUSY、阶段零重试、重初始化阻止及
  READY 后重新初始化，未修改生成文本。
- 原 v208 的 18 个发现案例原样复测：全部 OwnerRetained/CleanupComplete 为 true，
  HostRescuedOrphan 为 0；TraceComplete 仍全为 false，旧门继续红色。

聚焦证据位于 `build/tmp/xlir_launch_v209_{ownership_discovery,service_runtime_discovery,
host_discovery,network_discovery,steps_discovery,service_graph_discovery}/results.json`。
统一批次入口为 `build/tmp/xlir_launch_v209_integration_batch/results.json`。仅执行一次
完整批次：44 项、43 通过、1 失败、0 未运行，818.297 秒，3 路并行，4233 输入前后
指纹一致。唯一失败是产物检查器仍把组件数截断为 23；实际 24 个组件均已编译、
所有原 17 程序的 68 阶段及原生闭包检查均已成功，但新增导出被旧计数误报归属。

统一修正检查器的枚举读取及动态依赖矩阵计数后，独立产物门专项复验通过：24 个
组件、576 对依赖、177 个实际导出；17 程序、68 阶段全部保留。静态架构门也已
扩展为 V2/V3/V4 互斥及阶段零的真实服务拒绝检查并通过，未删除旧安全边界。
失败现场保留；修正仅涉及两个检查脚本与一个测试程序的统计输出，未修改编译器、
运行时或发布 DLL，也未因此重跑其余 43 个门。最终工程检查 13/13 已通过。
复验入口为 `build/tmp/xlir_launch_v209_artifact_runtime_repaired/results.json`；完整
最终封存入口为 `build/tmp/xlir_launch_v209_checkpoint/results.json`。

DEMO6 四份嵌入/声明头来自 v208 精确测试候选，3486 个公开原生链接身份已重新
生成。完整成熟度资产从 v200 恢复到新的私有包装目录，不修改原 v208 封存副本。
2020 个原生测试输入只重核沿用，不虚报重跑；三个发布二进制保持 v209 基线哈希。

后续必须继续建模真实 resolver/resolveop、producer/bridge/cancel、engine/worker
队列与句柄。现有服务图拒绝是安全边界，不能以服务当前空闲、语言状态归零或
TryDestroy 成功，替代活动服务的完整拥有边证明。全语言/FFI、混合生命周期、
同源发布/隔离部署、平台及旧路径退役也继续开放。

## v208：已保留的真实反例

`build/tmp/xlir_launch_v208_service_discovery/results.json` 使用 v207 同源网络门保存的
六个真实 AOT 程序，经公开 module init/checked unit 执行 idle、pinned-engine、
resolver-worker 三种场景，共 18 个，不替换生成文本、不伪造服务或引用。

- 18 个场景全部构建成功，但模块均无法提供完整服务拥有图，整个发现批次仍为红色。
- 12 个真实 Pin/Resolver 自身回调退出场景，清理失败后生成代码丢失服务拥有指针。
- 宿主只在记录失败后救回孤儿对象，恢复内存平衡不计为验收成功。

共同根因是同步 bool 销毁无法表示可重试的 BUSY，生成代码却在成功前清空拥有槽；
同时模块语言 finalizer 已将初始化状态复位，后续 checked unit 会跳过剩余服务清理。
`xl_script_module_ownership_trace` 的服务拒绝不能删掉：原生队列、回调、producer、
取消观察器与句柄的拥有关系尚未完整建模。

## v208：已实现和验证的原生基础

在独立 XRT 源仓中实现 `xrtNetEngineTryDestroy`、`xrtNetResolverTryDestroy` 和共享的
`xnetretireresult`，以 READY/BUSY/ERROR 明确区分创建者拥有权的消费、等待和失败。
只有 READY 消费拥有权；BUSY/ERROR 均保留原指针。BUSY/READY 保留调用前诊断身份。
旧同步 Destroy 共用停机/join/释放提交点，并允许完成已开始的非等待退休。

Engine 的 Pin 使其继续运行，提交者、活动回调、线程清理尾部和外借缓冲块分别形成
真实等待条件。Resolver 关闭接纳后仍排空已受理查询和回调；所有线程 join 前不
销毁线程句柄或共享表。自身 Worker 请求退休返回 BUSY，不自等待。真实任务链
不收敛仍为 ERROR，后续物理重试成功不能抹掉此前语义失败。

原生源码、模块清单及单头文件测试由已验证 v200 输入加明确覆盖文件构成私有候选，
不是任意当前发布 DLL。最终统一原生批次：GCC 129/129、124.888 秒；TCC 129/129、
26.530 秒；原有 110 个程序逐项保留，2020 个记录输入前后相同，仅执行一次该全批次。

入口：`build/tmp/xlir_launch_v208_native_native_verified/results.json`，SHA-256：
`CE24CC18DC1261A9C721FC832804A75ECF1A54F2001568EAD1262F0E9DADFDEE`。

两个新入口在 GCC、GCC O2、TCC 的修复后聚焦批次均通过。每个进程实际覆盖 240 个
服务创建者、96 组回调排空、72 个线程键析构尾部、24 个 Pin、24 个外借池块，以及
24 次真实停机失败/保留拥有权重试；末尾检查分配/释放、活动字节和非法访问精确平衡。
早期私有副本缺 LICENSE，以及探针提前终结线程存储/跨创建操作检查错误身份的失败
均保留；修正的是测试接线，未移除生产拥有权、join、无分配或内存平衡断言。

DEMO6 的四份嵌入/声明头、xl.dll、xlc.dll、xl.exe 与 v208 基线一致，尚未同步新 API；
本批没有修改 Cgen 生产源码、重跑整个语言批次或宣称完成同源发布验收。

## 下一接入必须满足的语义边界

1. 从精确已验证 IR 需求导出服务 ABI 和产物依赖，不让 Cgen 根据源名称、生成 C
   文本或运行时的某次偶然失败决定资源语义。
2. 将一次性语言 finalizer 与可重试物理退休分开。语言状态归零不是服务拥有槽已空的
   证明；BUSY 重试不能重复用户析构、跳过剩余服务、提前重新初始化或丢失首个错误。
3. 宿主必须接收显式退休结果。现有 bool checked-unit 的 false 被宿主记为语义失败，
   不能直接将 BUSY 塞进旧接口，也不能将它当成功来提前删除 TCC 模块。
4. 真实 native 服务/操作/producer/cancel 的拥有槽、引用数、变更冻结和代码寿命仍需
   独立建模与准入。仅新增 TryDestroy 不提供完整 trace，也不授权取消外部有根工作。
5. 保留原 18 个反例并补充重复调用、callback/TLS 尾部、错误无诊断、外部根、初始化
   失败、实际 DLL 卸载及混合对象测试；源码稳定后与此前语言人口统一复测。

本项未关闭原生服务图、混合生命周期、全语言/FFI、同源发布/隔离部署、平台和旧路径
退役要求。最终源码与失败证据封存于 `build/tmp/xlir_launch_v208_native_checkpoint/results.json`。
