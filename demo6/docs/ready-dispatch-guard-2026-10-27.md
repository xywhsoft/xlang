# 就绪入口调度门禁

旧门禁仍要求 eager-prefix 的 execution ABI 2，并认为容量已满的第三次
调用会启动函数体、执行 finally，再返回一个失败 Future。当前 ABI 3 会在
执行任何函数体指令之前预留根：满容量由启动结果同步报告范围错误，返回
空 Future，也不运行尚未进入的函数体或 finally。

本批对齐该公开合同，仍保留全部 12 个配置、每配置 25 轮，共 300 次事务。
单独检查 25 次真实容量拒绝及准确 finally 次数；已接受 Future 在 Watch
注册失败时仍必须交付错误并执行 finally。无诊断失败、有诊断失败、第二次
Watch 失败、Close 后继、两根并存和阻塞对照人口均保留。

运行脚本增加独立证据目录，检查当前 execution ABI 3，按规范驻留闭包链接
实际源码 XRT，使用响应文件并逐配置收集退出码。没有修改产品实现。

Windows `dispatch-worker-r4` 全部 12 配置通过；Linux `dispatch-linux-r1`
从原生编译器源码对象构建生成器，重新发射确定性 C／头文件／IR，再执行
相同 12 配置，20 个实际构建／发射／运行阶段全部通过。
两边均验证返回值、错误身份、finally 次数与完整内存平衡。

独立哈希复核分别为 6747／6656 项，证据在：

- `F:/xlang-validation/five-groups-20261027/p7-shipping/dispatch-win-r1.proof.json`
- 同目录 `dispatch-linux-r1.proof.json`

这是固定完整源码 cohort 的门禁验证，不替代普通发布产品验收，也不代表
全体 74 项参数化门禁已通过。
