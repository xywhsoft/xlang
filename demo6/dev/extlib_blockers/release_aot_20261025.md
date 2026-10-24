# 2026-10-25 当前工作树的发布版 AOT 阻断

这是扩展库验收时发现的主线构建问题，不是包内错误。测试时其他线程仍在修改主线；以下结果只对应当时的工作树，待主线改动收口后需要重新构建并复验。

在 `demo6` 执行 `build.bat`，退出码为 0，生成 `release/xl.exe`、`release/xl.dll`、`release/xlc.dll`。构建前需运行 `python tools/gen_xrt_xlang_matrix.py --root . --sync`，否则快照检查报告能力矩阵过期。

同一套新产物下：

| 命令 | 结果 |
| --- | --- |
| PowerShell: `$env:XLANG_PACKAGE_PATH='test/fixtures'; .\release\xl.exe run test/cases/xlir_c_owned_import_exports.xl` | 退出 0，输出 `imported C owner results ok` |
| `release/xl.exe build test/cases/xlir_c_owned_import_exports.xl -o build/test/c_owned_import_current.exe` | 进程退出 `0xC0000005`，没有生成 EXE |
| `release/xl.exe build test/cases/extlib_slugify.xl -o build/test/slugify_probe.exe` | 两次均退出 `0xC0000005`，没有生成 EXE |
| `release/xl.exe build test/cases/cgen_basic.xl -o build/test/cgen_basic_probe.exe` | TCC 报 `unresolved reference to 'xl6ErrorScopeEnd'` |
| `release/xl.exe build test/cases/xlir_c_owned_basic.xl -o build/test/c_owned_basic_probe.exe` | TCC 报 `unresolved reference to 'xrtAtomic64Init'` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/slugify/test/check.ps1 -SkipAot` | 严格 C、线程、XLang 边界和 C/XLang 同输出均通过 |

`xl6ErrorScopeEnd` 在 `src/api/xl_foreign_error.c` 定义，`xrtAtomic64Init` 属于 XRT；不能把链接错误归为扩展库缺少实现。建议主线先在稳定工作树复现，再分别检查：生成 C 的运行时依赖集合、裁剪链接符号表，以及 `xl build` 的访问冲突。扩展库任务不修改这些主线文件，也不因 JIT 通过而签收 AOT。
