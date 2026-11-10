# 可空的原生拥有式工厂

原生函数可以成功返回空结果，例如空场景栈的 `currentScene()`。该合同直接
使用现有的指针输出工厂；不增加方法包装、运行时操作码或另一套拥有模型。

```xlang
extern "Native*" class Native @header("native.h") @lifetime("owned") @policy("nativePolicy")
 native point handle
end
extern "nativeRelease" @header("native.h") @no_raise func releaseNative(Native value)
extern "nativeCurrent" @header("native.h") @no_raise @c_result("owned_view") @factory(0,0,releaseNative) func current() as Native?
```

对应的原生声明是 `int nativeCurrent(Native** output)`：零状态且输出为空映射为
`null`，零状态且非空交付一个真正拥有的视图。非零状态始终失败，即使输出为空；
失败时已经交付的非空原生义务仍由现有结果探针负责清理。原生设置诊断也会失败。

非空语言结果继续拒绝原生空指针。回滚函数仍只接受精确的非空 `Native`，避免
用可空输入模糊实际清理义务。语义检查与 XLIR 都拒绝 caller、依附 owner、回调
视图或原地存储工厂借此获得可空拥有结果；原地记录不会用空指针表示不存在。

Cgen 和驻留结果探针原本已实现这套空值分支，本次补齐前端准入和 XLIR 的
精确类型检查。结果根继续在原生生产之前预留，因此空结果也可能产生一次临时
根分配；成功空值会释放它。这样保留了失败回滚的保证，不宣称该冷路径没有成本。

`tools/check_foreign_nullable_factory.py` 覆盖直接、绑定、协议和函数值调用，
空/非空结果、非空结果拒绝、状态与诊断失败、原生输出后失败及 OOM 回滚。
`test/cases/foreign_nullable_factory_ir.c` 另拒绝 20 个伪造的 XLIR 合同，确保非法
模块在 C 与公共头输出前被拒绝。

Windows、Linux 完整 O2/TCC/XRT/VFS 候选各通过六项公开 CLI/JIT/独立 AOT 检查；
20 项 IR 变异检查使用明确标注的诊断编译器，两个被修改的编译单元与完整
候选相同。Linux 此项使用完整 r2 候选；后续 Scene 代码拥有能力由独立批次验收。
本批没有替换正式 release/IDE，证据见
`dev/release-materials/provenance/foreign-nullable-factories-20261110.json`。
