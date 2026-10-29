# Class/callable 原生交握验证修正（2026-10-27）

初次参数化验证在 `active_entry` 的线程交握期限断言失败。固定的 xlang XRT 快照明确以微秒定义 `xrtDeadlineAfter` 和 `xrtThreadWaitFor`；旧夹具的裸常量 `5000` 仅为 5 毫秒，会把调度延迟误判为交握失败。

夹具改为 `5 * XRT_TIME_SECOND`，明确给予有界的五秒交握预算。保留进入、离开、冻结及活跃 callable 拒绝断言，未修改等待算法、XRT 时间 ABI 或产品代码。没有无限等待或移除失败路径。

脚本同时改为既有注册表的驻留依赖闭包、当前发行声明头路径和编译器响应文件，继续分别编译 GCC O0/O2 及源 TCC 提供方。

三个配置全部通过，每个配置仍执行 450 个精确物理图、546 次接收者终结、50 次延迟职责、96 次准备/提交探测及 50 次真实活跃线程的 pre-trace 拒绝，要求零存活内存差值。

独立审计重算 5339 个输入、输出及日志散列：`F:/xlang-validation/five-groups-20261027/p7-shipping/class-callable-clock.proof.json`。这是 Windows 定向验证；完整联合复验仍独立进行。
