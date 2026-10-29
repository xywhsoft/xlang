# 类存储与构造发布门禁对齐

本批只更新验证合同及其构建依赖，不修改语言语义、XRT 或 Cgen。
DynamicFields 目前有 `clone()`、`__clone__()` 和默认构造三条分配路径；
门禁原先只计算其中两条，因而在验证实际当前 IR 前就失败。

存储门禁现在检查全部 5 次分配、5 次 prepared commit 和 27 个拒绝变异；
构造发布门禁检查全部 6 个发布点和 36 个拒绝变异。每个变异仍须同时被
IR 验证、C 输出和头文件输出拒绝，不通过删除断言或减少 fixture 绕过失败。
两个运行脚本改用规范驻留依赖闭包和实际 C 响应文件，避免新增运行时依赖
被手写源清单漏掉。类的原生构造、发布失败、原生回调驻留及析构人口保持。

## 验证证据

- Windows `class-contract-workers-r1`：两个门禁全部 8 个完整配置通过。
  包括 storage 的 native/language-TCC × O0/O2，以及 publication 的
  scope/publication × O0/O2；语言发射、重复发射、执行和模块入口均通过。
- Linux `ir-contracts-r8`：4 个当前 IR 合同的 12 个实际构建／运行阶段通过，
  包括本批两个类合同；重复 C／头文件产物一致。
- 独立重新计算输入、工具、产物及日志哈希：Windows 7213 个，Linux 6651 个。
  报告分别为 `F:/xlang-validation/five-groups-20261027/p7-shipping/class-contracts-win-r1.proof.json`
  和同目录的 `class-contracts-linux-r1.proof.json`。

这些是固定完整源码 cohort 的门禁证据，不能替代普通发布产品验收或全部
74 项参数化门禁复跑，也不宣称解决任意模块循环回收。
