# 由夹具 IR 选择驻留对象（2026-10-27）

`safe index` 和 `nullable getter` 的严格 C 宿主此前把完整源码编译器中的所有 `src/runtime/*.o` 都直接链接进程序。新增的内嵌 xlang 上下文策略依赖编译器 API，因此与这些夹具无关的 `xlDestroyContext` 也变成了强制链接依赖。

修复采用真实依赖选择，而非排除某个文件名。`xlir_resident_objects.ps1` 复用既有已验证 IR 计划和驻留组件注册表闭包：

1. 核验完整源码对象集及其输入散列；拒绝 runtime-only 角色。
2. 对明确列出的实际夹具分别 lowering/验证并检查计划确定性。
3. 将合并后的驻留源闭包逐个映射为该对象集内唯一的物理编译单元。
4. 核验每个被选对象的散列，拒绝缺失、歧义或重复提供方。
5. 源 XRT 组合对象只提供一次，其已内嵌的外部错误组件也只提供一次。

两个工作脚本的原有案例、闭合 descriptor/branch 变异、严格 C、源码及发行 JIT/AOT、隐私拒绝和完整逻辑 OOM 前缀均保持执行。没有修改产品或 Cgen。

Windows 完整复测：`check_xlir_safe_index.ps1` 382 项、`check_xlir_nullable_getter.ps1` 249 项，合计 631 项通过。两组唯一预期拒绝均明确验证退出码 1；其余检查均退出码 0。实际计划不包含无关的内嵌编译器上下文策略。

独立审计重算 7527 个输入、输出、日志及编译响应文件散列，证据为 `F:/xlang-validation/five-groups-20261027/p7-shipping/fixture-objects.proof.json`。使用冻结 r5 源码/产物验证本批工具修复，不据此宣称完整参数化联合验收或 Linux 验收完成。
