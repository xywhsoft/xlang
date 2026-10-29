# Future 取消验证迁移（2026-10-27）

`check_xlir_future_cancel_core.ps1` 的发射见证仍写旧的 15 个变异，真实合同驱动已执行 18 个 C/header 拒绝变异，因此脚本在运行前就误判失败。

本批同步精确见证为 18，驻留链接采用实际夹具 IR 和注册表闭包。使用前一批已验收的真实 frame 编译单元调度边界，使初始入队到达待决 await 后再执行原有取消人口。没有增加产品包装、替代算法或放宽取消合同。

完整原有工作脚本通过 17 个实际阶段，包括确定性发射、GCC O0/O2 的 producer 终结竞态及 continuation 提交交握、GCC O0/O2/TCC 的生成代码中断与显式取消。每个生成代码配置保留 2106 次拥有宿主中断、300 次三线程竞态、150 次公开取消、50 次后继发布交握和 200 次原生重入断言。

独立审计重算 6813 个输入、输出、SDK 文件和日志散列：`F:/xlang-validation/five-groups-20261027/p7-shipping/future-cancel-core.proof.json`。这是 Windows 定向验证，完整联合验收继续单独进行。
