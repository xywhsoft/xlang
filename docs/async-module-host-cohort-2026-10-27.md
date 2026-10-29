# 异步模块宿主的真实源码依赖

DLL 与 TCC 异步模块门禁原先缺少正式头文件路径，并用过时的少量驻留源
清单链接直接包含 API 实现的宿主。它们在新完整批次中均于编译前置失败。

两个工具现在从已验证的完整 contract／public source cohort 提取实际对象，
只排除宿主确实包含的 API、API path、console 三个 TU 和原来的 driver main。
不复刻运行时，不把某个未知拥有域当作 leaf。共用助手核对源码角色、原始
runtime 哈希及完整输入；执行前捕获、执行后复核所有输入。

GCC 使用响应文件与实际退出码。每个门禁有独立证据目录，实际 DLL 路径与
TCC runtime 头路径显式传递；保留 O2 严格编译、重复 C／头文件／IR 对比。
导出到 artifact 的宿主实现使用完整实际对象与导出符号，不通过旧的
whole-program 私有化裁剪掩盖缺失依赖。

`async-module-workers-r10` 两个门禁全部 12 模式通过：各保留 6 模式 × 25 次
生命周期，合计 150 次真实 DLL 卸载和 150 次真实 TCC／MEM_FREE 循环。
继续验证 pending entry、退休期间 finally、从真实调用中退役，以及在生产
模块已释放后读取／销毁 resident payload；没有增加公开语言 Future 形参
导出的声明，这仍是原有测试专用 canonical machine 入口。

独立复核 6684 项输入、工具、产物与日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/async-module-r10.proof.json`。
底座为 R8 实际重建的 1295 个 TU，证明见 `source-win-r8.proof.json`。
原完整 R8 批次中的失败不回填，下一次完整门禁仍须验证提交后的工具接线。
