# 发布构建被 Linux 运行时源清单漏项阻断（2026-10-25）

## 复现

在 `demo6` 目录执行 `cmd /c build.bat`。标准库索引生成后，
`build_dll.bat` 的 `check_stdlib_shipping_sources.py` 前置守门返回 1：

```text
windows-runtime: 853/853
windows-compiler: 853/853
linux-runtime: 852/853, missing src/runtime/xl6_native_class_callback.c
linux-compiler: 852/853, missing src/runtime/xl6_native_class_callback.c
```

可不编译二进制直接复现：

```text
python tools/check_stdlib_shipping_sources.py --root .
```

## 边界与修复建议

`src/compile/xl6_compile_unit.c` 的运行时注册表需要该源文件，
`build_dll.bat` 的 Windows 列表已包含它，但 `build_linux.sh`
的 `RUNTIME_SOURCES` 在 `xl6_native_class_result.c` 附近缺少
`xl6_native_class_callback.c`。应在 Linux 清单中加入同一源文件，
重跑上述守门，再分别构建 Windows 和 Linux 发布物，并验证回调
相关用例。不要删除注册表条目或跳过跨平台清单校验来让 Windows
构建表面通过。

这是 xlang 主线构建配置问题，不是任何扩展库的源代码问题。
本普通扩展库任务只记录复现，不修改主线构建文件。
