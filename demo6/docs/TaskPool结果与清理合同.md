# TaskPool 结果与清理合同

2026-10-20，P1 公开 TaskPool 接入的共同前置。不代表公开泛型 TaskPool、真实
worker-exit 或完整新 DLL／SO／VFS 已完成；整项五组长期任务仍进行中。

## 已确认并修复的问题

原生 XRT 在 Promise 发布前释放任务数据；它的任务过程返回显式 Outcome，数据
Drop 则是 void。xlang 的语言输出 Future 通过已有 Watch 转发原生结果。旧路径
未单独保存语言捕获清理的 checked 结果：body 成功、捕获析构失败时，语言 Future
仍会成为 RESOLVED。真实原生执行反例已复现，不只是根据代码推测。

修复仍在驻留 `xl6_task.c` 中，复用已有同一个 native Job、语言输出 Promise、
Watch、捕获布局和代码租约，不创建新队列、映射 Future 或 generated 包装算法。
不修改 XRT、Cgen、公开函数表或 V1／V2／V3 接口表布局；私有 TaskFrame 仅新增
内部 checked 清理状态。

## 明确的终态优先级

| 已选原生结果 | 语言捕获清理结果 | 语言结果 |
| --- | --- | --- |
| 成功 | 成功 | 转发同一个拥有值 |
| 成功 | checked 失败 | 依既有语言错误映射失败／取消／关闭；不交付原成功值 |
| body／原生前置／结果 policy 验证失败 | 任意 | 保留较早的原始失败 |
| 取消／关闭 | 任意 | 保留已选取消／关闭 |

body 的 checked 作用域覆盖实际运行及返回尾部；失败 bit 独立于诊断指针，即使
过程错误返回 SUCCESS，也不能掩盖已经登记的 checked 失败。捕获清理在另一事务
内按已有逆序 detach／释放合同运行；保存首个失败，不能用 NULL 指针代表成功。

这里必须说明物理边界：固定 XRT 的 PromiseReject 要求非空诊断，原生任务也会
把无诊断 FAILED 物化为无分配 INTERNAL。语言适配在选择正确的失败之后遵守该
严格边界，绝不把这种失败映射为 CLOSED，也不会让后来的析构错误取代原 body。
没有放宽旧 Reject 合同或新增另一套不完整的 nullable-error Future 体系。

准备失败／原生拒绝不把私有帧视为已接受任务。回滚恢复原有 caller cleanup 语义；
raw task.spawn 的显式原生 outcome 模式保持独立。取消仍不执行尚未运行的 body；
所有物理捕获均释放一次。源成功值在输出失败后仍由原生 Source 的实际拥有者
释放，不伪造结果引用或在接收线程泄漏未交付值。

## 验收与证据

证据根目录：`F:/xlang-validation/five-groups-20261019/p1-task-checked-outcome/`。
维护入口：`tools/check_stdlib_task_checked_outcome.py`，重复 `--compiler`，Linux
可加 `--sanitize`。输出必须是新目录，失败和中间报告不覆写。

`task_checked_outcome_native.c` 每个完整正例 profile 运行 193 个实际受理任务，
涵盖 body／清理的已分配和无诊断失败、成功枚举但 checked 失败、预取消、过程
取消、CancelledError、错误成功值 Drop policy、终态和精确内存账本。最小 profile
使用同一实际部署计划及原生 selectors，不额外开启调试能力；debug profile 显式
增加 memory_debug，只用于账本证明，不能冒充零测试增量的最小产物。

五种坏实现必须先成功编译，再由运行断言拒绝：丢失清理结果、把 NULL 清理失败
当成功、覆盖较早原生失败、覆盖取消，以及忽略已登记的 body 失败 bit。
Linux sanitizer 覆盖选定 native／resident／夹具闭包，不冒称全部语言运行时。

旧 `check_stdlib_task_pool_control.py` 同时回归两种头的原生控制、最小部署、
已有 280 次生命周期／240 个 queued 图／520 次准备，以及 400 次 ready／注册
通知运输。它们与新反例各有覆盖目标，不能用旧绿色替代新语义回归。

正式使用 `win-final`／`linux-final` 和 `control-win-r1`／`control-linux-r1`。
初轮 r1 的功能 lane 已执行；补充头文件合同说明后，其当前输入哈希不再等同于
冻结源，Linux r1 的最终 guard 也明确拒绝。旧报告保留，正式新矩阵整轮重跑。
正式报告须 Complete／InputsUnchanged／LogsUnchanged 为 true、Failures 为空，
提交前再核对输入、日志和实际产物哈希。没有网络通信或安全专项。

最终四份报告均满足上述条件：新矩阵 Windows 30／Linux 32 阶段，旧控制与图
回归 Windows 59／Linux 61 阶段，共 182 阶段（含 10 个预期运行断言拒绝）。
每端 8 个新正例 profile 各执行 193 个任务，Linux 额外 instrumented profile
执行同一矩阵。没有将 JIT／模块／AOT 或完整新产品验收计入本批通过项。

## 下一步

公开 TaskPool 仍需精确的 `func() as T` → `future<T>` 物理合同，返回值独立拥有，
不能按相同指针／尺寸强转 callback 签名。需组合已有 Execution Source／资源
家族、真正原生 worker-exit、配置／控制／统计和普通语言验收；不借默认池替代。
这次修复让这些后续入口可以复用正确的 body／清理终态，不关闭 P1 或后续阶段。
