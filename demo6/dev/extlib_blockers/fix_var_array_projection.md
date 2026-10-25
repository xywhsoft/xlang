# FIX 语言数组进入原生 `xvalue*` 后不再是数组

2026-10-25 使用当前 Windows `release/xl.exe`，从 `demo6/release` 运行：

```powershell
.\xl.exe check ..\dev\extlib_blockers\fix_var_array_projection.xl
.\xl.exe run ..\dev\extlib_blockers\fix_var_array_projection.xl
.\xl.exe build ..\dev\extlib_blockers\fix_var_array_projection.xl -o ..\build\test\fix_var_array_projection.exe
..\build\test\fix_var_array_projection.exe
```

`check` 报 `ok`，`build` 成功；JIT 与独立 EXE 都退出 1：
`ValueError: FIX build needs a BeginString and an array of field objects`。
最小文件只构造一个 `fix.field(35, "D")`，将其放入语言数组后传给
`fix.build`。完整 `extlibs/fix/test/boundaries.xl` 在第一条同形调用失败。

C 的 `xfix_build` 在处理任何字段前检查 `xrtValueType(pFields) == XVALUE_ARRAY`；
当前错误正来自这条检查。`tools/run_extlib_c.ps1 -Package fix -Source
release/extlibs/fix/test/test_fix.c` 退出 0，说明同一原生编解码器的 C
路径可用。此现象与 `foreign_var_container_projection.md` 中的其他包同形；
尚未在编译器内部定位具体丢失类型身份的步骤。

应在 xlang 的 extern `var` 到 C `xvalue*` 边界统一核对语言数组的
类型、元素所有权和调用期寿命。修复后先重跑本最小文件的 JIT/EXE，
再运行 FIX 的语言边界、双语范例及默认包门禁，并回归其他接收
`var` 容器的扩展包。普通扩展库任务不修改主线，也不在 FIX 内识别
私有句柄或重建数组作为兼容绕行。
