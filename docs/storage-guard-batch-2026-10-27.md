# Cell / Executor / 拥有计划验证迁移（2026-10-27）

本批完成三个既有 Windows 工作脚本的全部人口验证，共 45 个 GCC O0/O2/TCC 配置通过。

`check_xlir_cell_storage.ps1` 和 `check_xlir_executor_ownership.ps1` 的手工源清单缺少当前数组、标量转换、捕获及错误组件。两者改用实际夹具 IR 和驻留注册表的传递闭包，保留原有显式宿主依赖，并记录全部编译输入与响应文件。基础 cell 用自己的小闭包，不因生成场景而链接整组组件。

Executor 生成代码宿主过去截获 Cgen 内的 Reserve 调用。现在初始入口已交给驻留 launcher，宿主改为在 launcher 边界明确选择其真实 executor 和 code owner，随后调用完整的真实 launcher；promise、producer、frame、入队及发布流程均未替换。

| 原有工作脚本 | 通过配置 | 保留范围 |
| --- | --- | --- |
| Cell storage | 15 | 驻留 cell、分配失败、异步悬挂、线程生命周期、700 个捕获图；完整 IR 变异及确定性发射 |
| Executor ownership | 12 | 原生 queue/executor/拥有权；每配置 420 次真实 OOM；生成的 60 个等待环、20 个馈入者、320 次拒绝和 40 次清理恢复 |
| Ownership plans | 18 | 6 组原有 native completion / completion / preparation / plan / Value-COW / code-lease-tail；只修复 SDK 选择即可通过，无需改其断言 |

Cell 的捕获宿主采用先前已验收的当前 ABI 版本，其断言保持。原始失败批次和中间链接失败均保留，没有只统计成功子场景。

独立审计重算 7176 项输入、输出、SDK 文件和日志散列：`F:/xlang-validation/five-groups-20261027/p7-shipping/storage-workers.proof.json`。这些是定向工作脚本验收，不代表完整参数化联合复验、Linux 或普通发行联合清理已经通过。
