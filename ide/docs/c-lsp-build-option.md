# C LSP 构建选项

2026-10-31 起，传统 IDE 默认不启用 C 语言 LSP。此前随包的服务是 **clangd**；`xlang-ls.exe` 是 xlang 自身的语言服务，继续保留。

## 默认行为

- `src/app/features.h` 默认定义 `XIDE_ENABLE_C_LSP=0`。C 服务、后台输入观察、语义补全及 C 视图实现保留在源码中，通过宏排除；构建脚本也不编译对应的 9 个翻译单元。
- 不创建、轮询或启动 C 服务，不显示 C 服务器选择入口、`--c-server` 参数和 C 分析专用配置项。C 的 LSP 诊断、定义跳转、悬浮和语义补全停用。
- C／xlang 双语法编辑、中文与 IME、撤销、本地命令目录补全继续可用。C 单文件、混合项目的编译运行仍由 xlang 工具链负责，不需要 clangd。
- xlang 的自动诊断、语义补全、定义跳转、悬浮、大纲和参数提示保持原有服务路径；本轮没有更换 `xlang-ls.exe`、XGE 或编译引擎。
- 旧偏好中的 `toolchain.cServer` 在加载时忽略，下一次合法保存时移除；原有编译器、字体和未知字段保留，外部修改冲突仍受保护。项目清单中的 `cAnalysis` 兼容数据保留，常规构建头文件路径等配置不受影响。

根 `release/` 已清理 `toolchains/clangd/` 以及对应的 adaptation 源码、通知和来源材料。默认便携包只打包 xlang 工具链，不再要求 C 服务器输入目录或其清单；载荷和 ZIP 校验拒绝在关闭 C LSP 时带入这些资产。`portable-build-inputs.json` 的 `c_lsp_enabled` 默认为 `false`。

## 构建与验证

在仓库根执行：

```powershell
ide/build.bat -Configuration Release -GuiOnly -LanguageServerPath D:\GIT\x-lang\release\xlang-ls.exe -ReleaseDirectory D:\GIT\x-lang\release
python -X utf8 -B ide/tools/check_c_lsp_disabled.py --output ide/build/Release/c-lsp-disabled-check
```

验证工具使用 `ide/build/Release/gui-build-inputs.json` 中的同一组头文件、编译参数和对象，检查实际发布程序不含 C 后端符号及参数，启动真实 XUI 应用验证输入／撤销和通过 xlang 编译运行 C 单文件。夹具程序运行于根 `release/`，仅使用测试桌面保护用户窗口，结束后移出发布根目录。重复运行需选择新的证据目录。

本轮同源 Release 构建通过 `-Wall -Wextra -Werror`，100 个 GUI 翻译单元；共享代码宏 0／1 的 20 项语法检查及保留的 8 个 C 专用模块宏 1 语法检查通过。已在根 release 运行输入／IME、设置、项目配置、项目测试、工具栏、恢复、xlang 自动诊断、补全帧、定义和符号视图回归。原有夹具中的菜单位置、动作总数和执行目标假设同步为当前工具栏语义。

这次回归不代表完整性能验收或实体 IME／混合 DPI 验收。既有 LSP 整体性能未通过项继续有效。详细来源和验证范围见 `release/provenance/ide-c-lsp-disabled-20261031.json`；原始记录保留于 `ide/build/Release/no-c-lsp-20261031/`。

额外运行的通用文件选择回归停在“导入窗口选择目录后按 Escape 关闭”断言；目录选择与原文件保护已通过，尚未确定该失败原因，不记为该整套用例通过。C 服务入口不存在、旧配置移除及 C 编译运行由独立的 45 项实际 GUI 检查覆盖。

## 保留的开发开关

需要研究旧实现时，给 `build.ps1`／`test.ps1` 显式传入 `-EnableCLsp`，它会一致地为全部消费者定义 `XIDE_ENABLE_C_LSP=1`。便携组装另有显式 `--with-c-lsp` 开关，须自行提供匹配的 C 服务器及来源清单；默认发行不准备这些依赖。更改宏后必须整体重编 GUI 和测试，不能复用另一个宏配置的对象。

本轮验证了保留代码可编译，没有重新签收可选 C LSP 的完整运行和打包链路。
