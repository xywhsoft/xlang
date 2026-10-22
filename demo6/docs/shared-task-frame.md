# 计算池与网络任务的共同激活记录

状态：2026-10-23，P0／P3 前置完成；不是完整语言 TaskNet 发布验收。

## 设计边界

`xl6_task_frame.c` 统一实现任务激活记录、代码租约、捕获布局、checked 执行、
结果发布、捕获清理、原生 Future 观察及拥有权适配。旧 `xl6_task.c` 只保留
计算池受理／活动计数／提交适配；TaskRuntime V1、V2、V3 的公开 ABI 不变。
`xl6_task_net.c` 将同一记录提交给真实拥有型 XRT TaskNet，并不复制一套结果、
异常、取消或清理实现。

共同记录只允许可选的、永久驻留的 typed 计算池能力表。它不是生成代码提供的
通用执行器插件：所有回调来自驻留 provider，临时提交 Context 仅在同步调用内
借用，不进入激活记录。网络提交的 pool／binding 均为空，不建立默认计算池，
也不为每次任务增加线程或协程栈。实际产物闭包验证网络路径没有旧 `xl6_task.c`、
`xl6_task_pool.c`、原生 TaskPool 或 Executor 依赖。

所有可失败的语言输出／Watch／取消令牌准备发生在目标受理之前。受理失败不
消费调用者的私有记录及捕获；成功 Begin 必须配对一次 End。代码租约、原生
Job、输出 producer、Watch 和可选 pool 都是实际引用，不以伪造计数替代。原生
body 失败／取消／关闭优先于后续捕获清理失败；成功 body 之后的 checked 清理
失败不能被当作成功。可分配、提交或报告错误的操作并非 pure／no-raise 合同。

`xl6_ownership_domain_task_net.c` 注册精确身份的 TaskNet Data、原生 Post／Timer、
Job Data、CancelWatch 和原生 Frame quiescence。共同 Frame 的 Ops 身份唯一，
没有制造第二个同址拥有节点；未登记的同形 policy 在用户 Count／Trace 前拒绝。
共同记录不能通过提前拆 Watch、清理捕获来让尚在执行的工作看起来已静止。

本批没有修改 Cgen、独立 XRT、固定快照或时间 ABI。独立 XRT 继续毫秒／For，
xlang 快照继续微秒／Until；本批底层延迟入口使用后者，公开语言单位仍由后续
接口层明确转换。没有更新发布 DLL／SO／VFS。

## 已完成验证

证据根：`F:/xlang-validation/five-groups-20261023/p3-task-frame`。

| 验收 | Windows | Linux |
| --- | ---: | ---: |
| 网络共同记录与实际目录图 | 39 | 43 |
| 原有计算池控制、拥有图与 whitebox | 61 | 63 |
| 原有计算池 checked outcome | 30 | 32 |
| 现有 TaskPool 语言与组合回归 | 124 | 124 |
| 合计 | 254 | 262 |

共 516 个阶段；构建、拒绝反证及优化重复不等于独立语言程序数量。当前源码
分别重建 884／883 个编译单元，TU 不混计验收阶段。Windows 正式目录依次为
`net-win-r6`、`pool-win-r5`、`checked-win-r1`、`language-win-r1`；Linux 为
`net-linux-r3`、`pool-linux-r4`、`checked-linux-r1`、`language-linux-r1`。
源码构建两端均为 `source-*-r1`。输入、日志和产物 SHA 全部复核；原始 Linux
目录与 F 盘镜像保留。失败／准备轮不计成功；提交身份见最终 `verified-commit.json`。

选中闭包用严格 GCC O0／O2、TCC 和 Linux ASan／UBSan 验证。每种网络配置
检查 386 个受理场景，覆盖立即任务和零期限 Timer 的 checked body／清理组合。
另有 80 轮真实 XLIR pending 图：立即任务 11 节点／20 边，Timer 12 节点／23 边，
均只有一个真实外部根；检查精确内存账本、32 次错误 policy 拒绝和 40 次实际
语义终结器复活。捕获槽 Release 在输出终态前发生，collector pin 可将 Closure
环境的物理释放延至 Finish；物理释放不能冒充可复活的语义终结。旧计算池每种
配置检查 193 个 outcome 场景，5 类共同实现突变和计算池 drain 次序反证均在
成功编译后被运行测试拒绝。

124 阶段语言回归覆盖 JIT／module／独立 AOT、严格 C O0／O2、11 个原有正例、
5 个静态负例、6 个 Promise／TaskGroup／Executor 回归及 3 个预期错误退出；
包括 47 项输出前 IR 坏合同拒绝。这证明现有计算池接口未因拆分失效，不代表
新增 NetEngine 泛型公开接口已经实现。

48 份 PowerShell 和 2 份 shell 的既有直接源清单只机械加入共同 provider；
另调整两项源码定位 guard。50 份 PowerShell 均完成语法解析，无错误；未据此
声称所有脚本、完整 RC0 或发布产品已通过。本批没有应用 Socket、网络数据、
DNS、畸形协议或网络安全性质专项测试。

## 尚未完成

下一批将既有 typed result／checked closure 能力接入 NetEngine 泛型提交方法，
共享 Closure 准备而不拖入计算池，并完成 TaskGroup 拥有型组合与语言 JIT／AOT
验收。NetFile、完整服务退役图、P4 密钥容器格式、X.509、TLS 及 shipping
联合验收继续开放；长期目标保持 active。未决策 CRT 0／XRT 11，对象池最后。

2026-10-23 后续：上述 NetEngine 泛型方法、共享结果 Closure 准备及 TaskGroup
原始拥有型 Future 组合已经接入，另完成 250 阶段双平台专项，见
[网络结果任务](net-engine-tasks.md)。这更新本节的“下一批”状态，不追溯扩大
此前 516 阶段的覆盖面。NetFile、完整服务图及 shipping 等仍未完成。
