# plist 导入后 XLIR C emitter 的运行时操作 ABI 未解析

**状态：plist 包内已解除（2026-10-13）。** 原因是包把明确的数组/字典结果声明为 `var`，导致动态 `append` 的 XLIR 运行时 ABI 无法生成；改为 `array<var>` / `dict<var>` 后，最小导入、包级边界和教学范例 JIT 均可运行。未修改主线。`var` 数组追加是否仍需主线单独修复，应由主线任务以独立最小用例判定；本文件保留原始复现记录，不再作为 plist 的发布阻断。

## 最小复现

`demo6/dev/extlib_blockers/plist_runtime_abi.xl` 只有 `import plist` 和返回 0 的 `main`。

在 `demo6/release` 执行：

```text
xl check ../dev/extlib_blockers/plist_runtime_abi.xl
# ok
xl run ../dev/extlib_blockers/plist_runtime_abi.xl
# ..\dev\extlib_blockers\plist_runtime_abi.xl:119:26: [checked] XLIR C emitter runtime operation ABI is unresolved
```

运行诊断中的 119 行对应 `release/extlibs/plist/src/plist.xl:119` 的 `result.append(__plist_to_value(...))`，不是最小复现的第 119 行。原有 `test/cases/extlib_plist.xl` 在相同位置失败。

## 预期与修复建议

导入成功后，即使没有调用 `plist` 函数，也应能为导入源码中的 `var` 数组追加操作生成有类型的 XLIR runtime ABI。请主线检查此 `append` lowering 的 `runtimeOp`、操作数类型及 `xl6_ir_cgen_verify_instructions.c` 的允许集合是否一致；修复应覆盖独立 `var` 数组追加和导入模块编译两条路径，而不是在 plist 中复制运行时代码或弱化校验。完成后重跑最小复现、`test/cases/extlib_plist.xl` 及 plist C/XLang 包级测试。

本任务按约定不改 lexer/sem/XLIR/cgen 主线，暂停 plist 的发布验收，继续下一个包。
