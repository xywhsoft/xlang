# BLAKE2 AOT：运行时 closure 源缺错误结果声明

2026-10-19，在 `demo6` 的 Windows 构建上执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_blake2.ps1
```

严格 C 边界、分配故障、原生层、C 范例和 XLang JIT 均通过；首次 AOT 构建 `extlib_blake2-build` 失败：

```text
tcc: D:/GIT/x-lang/src/runtime/xl6_closure.c:218: error: 'xl6_error_result' undeclared
```

`xl6_error_result` 定义在 `src/api/xl_foreign_error.h`；当前 `src/runtime/xl6_closure.c` 的直接包含列表没有该声明，也使用了 `xl6ErrorScopePropagateFailure`。建议主线维护者检查该源文件的直接头依赖与独立编译，修复后重跑本包完整门禁及其他引用相同运行时源的 AOT 目标。本任务不修改编译器或运行时主线；此失败不代表 BLAKE2 算法错误，不能签收本包完整 AOT。
