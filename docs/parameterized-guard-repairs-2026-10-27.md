# 参数化整批发现与生命周期测试校正

原 R13 统一候选完成 1,295 单元、未复用源对象的 Windows source-driver
构建后，首阶段 73 个 worker 实际全部结束：52 通过、21 失败、0 未运行。
第二阶段依赖 aggregate 门，未执行。保留原始状态，不把后续独立通过项
回填 R13，也不把本批测试校正称为整个 74-worker 验收完成。

## 五组修正

| worker | 根因及修正 | 独立复测 |
|---|---|---|
| C owned exports | PATH 中同一 gcc 的两种路径写法使 Get-Command 返回多个 Application；选取实际解析顺序的第一个 | 13 项全部通过，保留重复 PATH |
| mixed dependencies | 断言仍要求旧同步 Watch／frame ABI；GCC 调用也未迁入已有的响应文件助手 | TCC／DLL × O0／O2 四个配置，分别 132 次真实模块卸载及精确内存平衡 |
| aggregate dependencies | 同样的回调 ABI 与参数传递问题；独立队列探测目录还遗漏 queued callback storage | 四个完整配置，分别 170 次真实卸载、80 份独立计划、160 个成员、108 个诊断预算与 18 个 Close 后完成 |
| async dependency cycles | 把非阻塞 TryCommit 一次 BUSY 当永久失败 | GCC O0／O2、TCC 三个完整配置，各 290 次实际 OOM；全部循环／链／feeders 与清理断言保留 |
| native Future map | exclusive callback 测试把运行时终结当成线程清理，在两个竞争线程中调用 | 完整 29 阶段通过，包含每个优化配置的 1,000 次取消／完成竞态 |

本批只修改测试控制和测试宿主，未修改语言、Cgen、驻留运行时、XRT 或 ABI。

`xrtRuntimeRetireThreadStorage()` 的声明明确要求宿主停止准入、join 工作并
释放资源，之后不得再次进入该运行时实例。工作线程只清理自己的错误，
由原有物理线程 join 保证它们退出；终结调用移到 main 的最后。额外在竞态
结束后创建并清理一个真实错误，确认运行时仍可用，再检查内存平衡。

TryCommit 只允许在无诊断的 BUSY 状态下有界重试，仍须取得真实 READY
才能 Finish。aggregate 的生产者 Prepare 也可能在全局 freeze 准入前返回
BUSY，故只重试尚未触发目标分配失败的调用。每次仍要求原诊断指针不变、
没有取消／catch／finally／全局清理；最终必须真的触发 scratch OOM。
原来的 12 次资源失败、12 次真实生产者 BUSY、172 次边界拒绝均保留。

测试按闭合目录同时识别 produced continuation 和 queued callback 的实际
storage，未开放任意类型。三个 GCC 入口统一使用已有响应文件／argv 助手，
不缩减对象集、源码、优化配置或运行断言。三个新 C 字符串 define 使用
助手要求的原始参数形式。

## 尚未解释的一次失败

r17 曾在 aggregate DLL/O2 的精确内存快照断言失败。随后增加了模式、
事务、计划、预算、存活数量／字节及分配／释放次数诊断，保持原谓词。
r18 又暴露了上述 Prepare 的物理准入假设；修正后 r20 四个配置通过，
同一个 DLL/O2 可执行文件又分别复跑 3 次、30 次，全部通过。

这些复跑没有解释 r17 的快照差异，不能将它认定为已修复，也不能通过
重试整次内存断言、调整基线或折扣计数来消除它。本批将该项保持待定位，
以后若再发生可获得精确差值。整体发布验收仍未通过。

首次测试前置失败也保留：r14 已通过 C owner 场景，但两个 dependency
控制仍受直接 GCC 调用影响；r15 的 mixed 四配置通过，aggregate 才执行到
缺少 storage 的真实探测；r20 的 cycle TCC 通过，GCC 检出新 yield 调用
遗漏 THREAD 声明。补充显式模块声明后 r21 三配置完整重跑通过。

## 证据

Windows 根：`E:/xlang-validation/five-groups-20261027/p7-shipping/`。

| 路径 | 内容 |
|---|---|
| parameterized-r13/phase1.results.json | 52／21／0 原始完整首阶段终态 |
| parameterized-r13/results.json | 原第二阶段依赖失败 |
| guard-corrections-r14/check_xlir_c_owned_exports | 13 项完整 C owner 验收 |
| guard-corrections-r15/check_xlir_mixed_dependencies | mixed 四配置 |
| guard-corrections-r17、r18 | 原失败及新增诊断后的发现 |
| lifecycle-corrections-r20/check_xlir_aggregate_dependencies | aggregate 四配置 |
| lifecycle-corrections-r20/native-future-map | 完整 29 阶段 |
| cycle-correction-r21 | cycle GCC O0／O2／TCC |
| aggregate-repeat-r22、r23 | 同一 DLL/O2 的 3＋30 次复跑 |
| guard-repair-proof-r24/results.json | 独立 reader、五个源快照、日志／对象／原生 argv 收据核对 |

本批针对 Windows 原参数化 worker；没有借用这些结果宣称 Linux 最终产品
验收。R13 统一候选不包含后续两批 sem 修复，整批合并验证须使用新的
统一快照。继续处理其他失败 worker 的共同根因、source admission 下限、
真正的语言缺口和上述快照问题；长期任务 active，仅普通离线／生命周期测试。
