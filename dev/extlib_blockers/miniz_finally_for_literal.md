# finally 中数组字面量迭代触发 XLIR cleanup 检查

## 最小复现

`dev/extlib_blockers/miniz_finally_for_literal.xl` 只使用语言和标准库；
没有导入 `miniz`。从 `demo6` 目录运行：

```text
release/xl.exe check dev/extlib_blockers/miniz_finally_for_literal.xl
release/xl.exe run dev/extlib_blockers/miniz_finally_for_literal.xl
```

`check` 返回 `ok`。`run` 在 Windows 与 Linux 当前发布运行器上均失败：

```text
dev/extlib_blockers/miniz_finally_for_literal.xl:11:36: [checked] XLIR evaluation cleanup must protect its exact pending result, commit only on success and follow its typed failure route
```

期望 `finally` 中的遍历与清理正常执行，然后程序返回 0。复现文件使用
`dir.temp` 创建独立临时目录；编译失败时不会执行文件操作。

## 影响与建议

发现于修正 `extlib_miniz_progress.xl` 的临时目录清理代码时。该用例改为
逐项清理后可运行；这只是测试夹具写法，不代表上述语言错误已解决。

初步定位为 `try/finally` 内数组字面量 `for-in` 与 `path.join` 调用的
临时结果/异常清理顺序；`check` 与 `run` 走的降低阶段不同。主线维护者应
检查 typed XLIR 对该临时数组和循环体调用的 pending result 保护与提交，
修复后将复现加入编译器回归，并分别运行 JIT、AOT 与异常路径。扩展库任务
不修改 lexer/parser/IR/cgen/runtime。
