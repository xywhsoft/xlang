# XLIR 原生嵌入命令拥有链

2026-09-19，v215。嵌入命令已补齐成功转移、出队释放和拥有图投影；完整发现后
修复固定对象体积回归，修复后聚焦三配置各 37/37。原样 XLIR 规划器的 Engine
场景由 96 扩至 192，并在最终布局上三个配置全部通过。这是
transport 终结调度的底层依赖，不代表 port/completion、完整 transport 图或
生成模块已经完成集成；语言生产集成仍为 v209。

## 合同与实现

`xrtNetPostOwnedV1` 使用既有无分配 Post 和 Pending 门。私有
`__xrtNetEnginePostInternalOwnedV1` 将同一合同提供给宿主自己串行管理的
嵌入 continuation。成功消费一个真实 context 引用，失败不消费。该引用必须
保护节点存储和借用 Worker，Task 只借用该引用，执行器在 Task 返回后 Drop。

执行器在回调前摘除节点全部元数据，单独保存这次提交的 policy/context；
Task 或 Drop 重投同一 Post 不会覆盖上一代的释放职责。回调后不再访问旧嵌入
存储，用户 Task/Drop 都在 mutation 和生命周期锁之外执行。

Engine 在整个图冻结、Worker parked/finished 且无 submitter 时，遍历真实
producer stack 和 owner ready tail。逐项验证精确 policy 身份、公共 wrapper
形状和实际 InternalPending 总数，每个提交投影一次真实 context 引用。
节点不是伪 RC 对象，wrapper 的借用 Post 指针不重复算拥有边，同一 context
被多条命令拥有时也不去重引用。

旧借用命令仍拒绝图准入，活动回调/Drop 尾部仍让 Engine 拒绝 Count；裸端口
暴露边界未放宽。旧 transport continuation 仍按原方式手动持有/释放引用；
今后迁移必须同时补齐实际宿主 Ops，并移除相应手动 self-drop，不能只追加
策略字段就声明其可回收。生产 XLIR planner、Cgen 和正式嵌入头没有因此修改。

## 验证及失败记录

快速语法预检发现新测试将 `for` 和后续 return 写在同一行，触发
`-Werror=misleading-indentation`。原测试版本及诊断保存在
`build/tmp/xlir_launch_v215_preflight/`，修正格式后再启动完整聚焦批次。

初次聚焦 GCC O0/O2、TCC 各 26/26；修复布局后加入原 TCP 组，各 37/37。
新增普通/单头测试各验证：

- 24 个实际提交、重复 Pending 拒绝、Task/Drop 重入与私有直接命令场景；
- 24 次零分配预算下成功提交，未触发分配失败，不通过临时堆节点绕过约束；
- 42 次旧借用/错误身份等拒绝，错误策略在子节点 Count/Trace 前拒绝；
- 6 组 8 个发送线程竞争同一 Post，恰好一个成功，7 个失败各保留其引用；
- 6 组 300 个嵌入命令，真实 Worker 预算执行 256 个后检查剩余 44 个 ready
  节点。测试只在实际 owner 上安装等待门，不伪造 parked 状态、引用或队列。

逐场景内存精确平衡；Task 和 Drop 内由另一线程取得 freeze 并确认活动
Engine 拒绝检查。公共 Post 存储仍为 64 字节，静态布局检查保持有效。

已有 Engine 规划器测试保留原全部 96 个场景，再加入 96 个 embedded Post
变体。三个配置各 192 轮，192 次 Task/Timer/finalizer/实际释放，96 次错误
policy 拒绝、96 次复活撤销重建、96 次待定/执行后图变体；精确节点/边计数
和内存断言保留。缓冲规划器各 96 轮、网络任务规划器各 80 轮也在同一候选
复验通过。各门仍链接未修改的生产 `xl6_ownership_plan.c`。

统一原生批次扩至 294 个程序，保留此前全部 198 个，并加入受内部节点布局
影响的 TCP/UDP/TLS、dial、future 和 Windows IOCP 测试。初次完整编译都成功，
但超长链接命令的响应文件未被执行器识别，各 84 项通过、210 项未运行。第一
次补跑又遇到 Python 并发日志前缀/命令交错，完成 GCC 后不能识别 4 个已有
TCC 成功程序。原记录和中断进度均保留。

修正产物映射后，复用全部已有执行（包括失败），只补跑 210 个 TCC 程序，
没有重新编译。完整发现结果为 GCC/TCC 各 293/294，唯一实际失败都是 TCP
固定对象尺寸断言，不能把它算作执行器问题。原发现证据分别位于
`build/tmp/xlir_launch_v215_native_verified/`、`xlir_launch_v215_native_completed/`
及 `xlir_launch_v215_native_all_executed/`。

### 固定布局修正及保护

原尺寸断言和上限保持逐字节不变。缓冲重入深度改为有溢出拒绝的 `uint32`，
与三个标志共用原有尾部，64 位 `xnetbuf` 从 64 恢复至 56 字节；不是将嵌套
保护改成布尔，也没有新增旁挂分配。TCP Future 的状态和计数重新排列以利用
现有对齐空隙，保留每个独立状态、拥有引用和 Pending 门。

GCC O0/O2 四种布局的实际前后值如下，全部仍服从原上限：

| TCP 配置 | 修复前 | 修复后 / 原上限 |
| --- | ---: | ---: |
| core | 592 | 576 |
| dial | 632 | 616 |
| future | 664 | 640 |
| dial + future | 704 | 680 |

缓冲拥有测试增加每程序 18 次计数饱和拒绝，验证 Append、Move 任一端溢出
都不部分修改链或计数；最大允许深度仍能平衡进入/退出。原有嵌套回调、跨线程
freeze、回调复活、terminal Unit 和精确内存检查全部保留。

修复后的统一验证只运行一轮，GCC/TCC 各 294/294、0 失败、0 未运行，输入
指纹前后相同。结果记录于 `build/tmp/xlir_launch_v215_native_final/`；此前发现
批次没有被覆盖。最终头与三项 XLIR 规划器使用的候选逐字节核对，Engine 每
配置 192 轮、buffer 每配置 96 轮、task 每配置 80 轮通过；没有修改生产规划器。
总检查点入口为 `build/tmp/xlir_launch_v215_checkpoint/`，其总任务 Complete
继续为 false，不把本批原生通过视为完整语言/模块验收。

## 保留的验证缺口

本轮还真实复测了 v211 留下的 ResolverFuture planner：GCC O0/O2 通过，TCC
编译成功但产物在执行前被 Defender 隔离。2026-09-19 21:27 的只读检测记录
与缺失文件相符，保存于 `build/tmp/xlir_launch_v215_resolver_future_plan_verified/`。
没有改变安全设置，没有改名重试绕过隔离，该 TCC 专项继续记为未运行，而非
被本轮其他 TCC 通过抵消。

独立布局小探针的 `before_tcc_core.exe` 也在 21:53:50 被隔离，导致其原执行器
在读取二进制指纹时中断。已有八份 GCC baseline 输出保留，未伪造当时未写入
的退出码；只补做最终 GCC O0/O2 八个布局组合，没有改名重试 TCC。布局矩阵
因此仍标记完整验收为 false。普通 TCP 的 GCC/TCC 尺寸断言是独立运行证据，
不替代该四配置 TCC 小探针。见 `xlir_launch_v215_layout_verified/interrupted_attempt.json`
与 `xlir_launch_v215_layout_gcc_verified/results.json`。安全设置均未修改。

原生 port/completion、readiness watcher、OS 请求缓冲和完整 transport 拥有图，
生成模块接入、原 18 个服务图门、混合对象/DLL 卸载、全语言/FFI、同源发布、
部署隔离、平台及旧路径退役仍然开放。标准库不扩大范围，默认 Cgen 不变。
