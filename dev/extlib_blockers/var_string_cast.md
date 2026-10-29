# 共享主线缺陷：var 字符串 `as string` 结果损坏

- 2026-10-12，当前 `release/xl.exe`。
- 在 `demo6` 运行 `release/xl.exe run dev/extlib_blockers/var_string_cast.xl`。
- 期望：断言通过；实际：`AssertionError: var to string cast`。带外部 C 返回的
  `xrtValueString` 再转换时，还观察到控制台报告无效 UTF-8。
- 影响：扩展包不能安全地把 C 返回的 `xvalue*` 字符串经 `var as string` 转成
  语言字符串。`terraform_state` 使用借用的 `const char*` 桥接，当前未被此缺陷阻断。
- 建议：核对 `as string` 的 XLIR lowering、`xlir_xvalue_cast_checked` 返回值
  与拥有权，以及动态值字符串到语言字符串的复制路径；补 JIT/AOT 和释放后边界。
- 本任务不修改主线，修复后应重跑受影响的扩展包。
- 2026-10-16 `plist` 的构树限额复核再现同类问题：`value is string` 成立，
  但 `string text = value as string` 后，`string.byteLen(text)` 对六字节的
  `"你好"` 返回错误长度；`string(value)` 在相同路径返回 6。扩展包暂用后者，
  此对照不代表主线缺陷已修复。
