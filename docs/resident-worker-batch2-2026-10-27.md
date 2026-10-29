# 驻留运行时验证迁移第二批（2026-10-27）

本批迁移六个既有验证脚本。产品编译器、XRT 算法和拥有权协议不变；使用冻结 r5 源码/对象及已核验的独立 TCC SDK，修正旧验证的依赖清单和宿主驱动时序。

## 依赖及入口

各脚本从原有显式运行时根和真实夹具的已验证 IR 计划计算驻留组件闭包，保留完整的源 XRT 提供方。补齐发行声明头路径，使用统一的编译器响应文件及退出码日志助手，避免 PowerShell 将编译诊断当作脚本终止条件。闭包、提供方、实际编译命令和全部原始断言均进入散列证据。

固定工作线程模型将异步初始入口交给驻留 launcher。两个旧宿主过去只在生成 C 中截获调度调用，已无法控制实际初始入口。本批在真实 `xl6_continuation_frame.c` 编译单元截获同一调度边界，仍执行其余完整实现。

- 依赖环宿主正常初始化/终结真实模块。先驱动初始入口进入 boot 等待，再执行原有 boot → peer 转移及等待环检查。
- 取消等待宿主先驱动初始入口到真实待决 await。只有生成的 await 后继帧进入原有待决帧检查，初始就绪帧不冒充已 armed 的 await。
- 原始 cancel-watch transport 已包含真实 Future transport。闭包中的对应编译单元由该观测编译单元提供一次，避免重复定义。

没有使用虚构 executor、放宽取消规则、移除断言或降低验证规模。

## 本批验收

Windows GCC O0、GCC O2 和独立 TCC 的 24 个编译/运行配置全部通过：

| 验证脚本 | 配置 | 保留的核心覆盖 |
| --- | --- | --- |
| `check_xlir_resident_ownership_scopes.ps1` | 3 | 冻结租约/闭包/队列、回调尾部、工厂转移及退役后的子对象逃逸 |
| `check_xlir_task_pool_graph.ps1` | 3 | 280 个生命周期、240 个精确队列图、520 次真实准备 |
| `check_xlir_slot_transfers.ps1` | 3 | 全局冻结、物理交接、OOM 下无分配提交、20000 次竞态交接 |
| `check_xlir_callable_contextual.ps1` | 6 | 3 个源码场景的 JIT/模块与确定性生成；2 个严格宿主场景各 100 次模块周期 |
| `check_xlir_async_dependency_cycles.ps1` | 3 | 120 个等待环、40 个可执行链、40 个馈入者、800 次拒绝、160 次获准中断及每配置 290 次实际 OOM |
| `check_xlir_async_cancel_wait.ps1` | 6 | 原有 34560 次 cancel-watch 执行；2106 次拥有宿主中断及 300 次三线程竞态 |

独立证据审计：`F:/xlang-validation/five-groups-20261027/p7-shipping/resident-workers-batch2.proof.json`。初次完整批次的原始失败报告及中间失败重现均保留。本批定向绿灯不等于完整 74 脚本复验、Linux 参数化验收或最终发行验收完成。
