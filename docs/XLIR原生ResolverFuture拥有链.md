# XLIR 原生 Resolver Future 拥有链

2026-09-16，v211。本轮迁移实际 `xrtNetResolveAsync`，没有增加替代真实调用链的
测试工厂。DEMO6 语言生产集成仍为 v209；本轮原生候选尚未同步四份嵌入/声明头，
不覆盖发布 DLL，不移除原 18 个服务模块图场景的保守拒绝。

## 实现

- Future producer、实际 Promise、ResolveOp、认证 CancelWatch 和成功 AddrList
  进入精确物理拥有图；借用 Promise 不重复报边。默认 pending 请求有 7 个节点、
  11 条对象边，加两个真实入口拥有槽后为 13 条边、0 个外部根。
- 创建者、producer、请求、监听分别持有实际引用。所有可能失败的 bridge 装配在
  接纳解析请求之前完成；失败无隐藏已接纳请求、无消费未转移引用，并保留原诊断。
- Done 先等待 Ready 再读取发布字段；Cancel 在锁内取得独立操作引用，锁外取消。
  监听注销、Promise 通知和 callback/drop 尾部不占着 bridge/context 自己的 mutation。
- bridge 保持 32 字节。Ready 先清除修改标志再发布 Setup，完成线程可以立即注销。
  未发布、注册/注销活动及旧借用监听拒绝拥有视图查询，输出与诊断不变。
- 新 `xrtThreadStateTry` 在冻结中只做非阻塞读取，既不等待线程状态锁，也不改
  所有权。Resolver 的退出上下文完成后允许重新采集，Prepare 仍须确认 Joined。
  不把线程过程返回、上下文清理完成和 OS 原生线程/DLL 卸载证明混为一谈。

## 真实规划器接入发现的缺陷

首次把新链路接到**未修改的** `xl6_ownership_plan.c` 后，GCC O0/O2 均超时：
Done 和 Lookup 已完成，但最后拒绝节点持续为 Resolver，Prepare/Refresh 为
ERROR。原因是 Count 要求 Joined，规划器又必须先通过 Count 才能再次调用
Prepare/Join，形成循环等待。原始失败和诊断保留在：

- `build/tmp/xlir_launch_v211_plan_discovery/results.json`
- `build/tmp/xlir_launch_v211_plan_diagnosed/results.json`

修复不跳过重验证，也不降低清理活动期间的拒绝。持久原生测试实际阻塞线程键
清理回调，证明期间 Count 拒绝且诊断不变；释放回调后，**不先再调 Prepare**，
Count 即能只读地观察完成，然后 Prepare 确认 Joined。两种状态分别断言。

## 验证记录

初次发现批次 GCC O0/O2 各 15/15；TCC 被新夹具的结构体数组函数返回值初始化
触发内部编译错误，15 个程序未运行。改用等价的显式赋值后，原断言全部保留。
最终聚焦 `xlir_launch_v211_native_integrated/results.json` 三通道各 **15/15**，
所有输入前后不变；包含模块化、单头及原 TCP/TLS dial Future、task network、
bridge 正常/OOM 程序。

修复归并后只执行一次完整原生批次：
`build/tmp/xlir_launch_v211_native_verified/results.json`，
GCC **155/155**、TCC **155/155**，无失败或未运行，上一轮 139 个程序全部保留。
本批同时包含线程/线程键旧测试，生成核心和 xruntime 私有候选头；没有覆盖 DEMO6
生产头。GCC 编译约 148.8 秒、运行约 31.6 秒，TCC 编译约 13.9 秒、运行约 24.2 秒。
所有输入前后哈希相同。源码编码 6629 文本文件和源码接线检查均通过。

每个新增 Resolver Future 程序执行 24 个精确 pending 图/清理顺序场景、6 个
通知中跨线程冻结、6 个阻塞查询 Prepare、6 个真实退出清理窗口、96 个取消竞争，
48 个分配预算实际触发 6 次失败。每案分别核对活动分配数量、字节和非法释放。
每个 bridge 程序还执行 8 次同步取消/重入、8 次额外 Watch pin、8 次 legacy
拒绝，以及 256 次发布后立即注销竞争。

规划器专项 `xlir_launch_v211_plan_integrated/results.json` 的 GCC O0/O2 各完成
96 个真实拥有循环，包含 48 次 pending 准入、48 次错误策略拒绝、48 次复活撤销，
各 96 次通知、finalizer 和实际释放，内存精确平衡。循环由 owner 实际持有 Resolver/
Future、Resolver 的 owned lookup 与 Future waiter 实际持有 owner 构成，没有伪造
根或推测减 RC。独立结果引用允许在原图清理后存活。

TCC 此专项已编译，但测试可执行文件被 Defender 实时保护隔离，未运行。事件记录
路径明确匹配对应新产物；没有关闭安全设置、改变断言或把 GCC 通过当作 TCC 通过。
该专项总状态保持未完成。两条失败/隔离记录均保留，不能删除以改变验收数字。

持久专项入口随后对完整原生候选执行 GCC O0/O2 验证：
`build/tmp/xlir_launch_v211_plan_verified/results.json`。两通道各 96 个循环通过，
输入哈希相同；TCC 明确登记缺失，`SelectedLanesPassed=true` 而 `Complete=false`。
没有因换成持久入口而再次尝试绕过安全隔离。

持久专项入口：
`tools/check_xlir_native_resolver_future_plan.cjs --native-directory <原生已验证目录> --directory <新的证据目录>`。
它检查候选完整性和输入哈希，私有 staging 使用候选头，直接链接现有规划器。
`--gcc-only` 只允许诊断/推进，报告明确保留缺失 TCC 通道，不将总状态标为完成。

## 剩余范围

Engine 的 worker/command、port、buffer、timer 等真实拥有图、完整服务图准入、
同源 XLIR 运行时/产物依赖、原 18 个模块场景、混合对象和真实 DLL 卸载仍需推进。
全语言/FFI、同源发布/隔离部署、平台与旧路径退役保持原目标。原生组件和规划器
夹具通过都不等价于生成模块验收；本次也没有重跑或声称重跑 v209 的语言总批次。
