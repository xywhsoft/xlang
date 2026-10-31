# 当前 xlang IDE 开发验收候选

2026-10-31：当前根 `release/` 版本默认不启用 C LSP，不再随包携带 clangd；源码由 `XIDE_ENABLE_C_LSP=0` 保留关闭，C 语法编辑及 xlang 驱动的编译运行继续可用，xlang-ls 保留。详见 [C LSP 构建选项](docs/c-lsp-build-option.md)。本轮默认便携载荷重新验证，不更新下方历史 ZIP 摘要。

2026-10-29 目录调整：当前运行入口改为 [根目录 release/xlang_ide.exe](../release/xlang_ide.exe)，使用同目录的 xlang 引擎。以下 `ide/release` 的候选说明与 ZIP 摘要属于历史记录；本次迁移见[统一发布目录](docs/unified-release.md)。

2026-10-30 补齐根目录最新引擎的 schema 2 构建收据及源码报告兼容，保留 schema 1 支持，并重新构建传统 IDE 与 CLI 语言服务。当前运行文件及安装摘要见根目录 `dev/release-materials/provenance/ide-receipt-compatibility.json`；以下旧 ZIP 未重新生成。

2026-10-22 已支持四种构建形态，无需先创建工作空间：

| 打开形态 | F7 构建 | F5 运行 |
| --- | --- | --- |
| `.c` 文件 | 当前 C 文件 | 构建成功后运行 |
| `.xl` 文件 | 当前 xlang 文件 | 构建成功后运行 |
| 独立 `.xlp` | 当前项目配置 | 构建并运行该项目 |
| `.xlw` 工作空间 | 所选项目配置 | 构建并运行启动项目 |

已有项目时也可通过“项目 → 构建当前文件／构建并运行当前文件”显式选择文件。未命名源文件先通过 XUI FileDialog 另存为。直接以 `.c`／`.xl` 启动优先于上次工作空间恢复；单文件不生成项目或工作空间清单。产物、依赖和目标规则见 [四种构建形态](docs/standalone-builds.md)。

`release` 已同步为本候选的完整运行副本，`dist` 仅保留本候选；旧发布与本轮中间包移入工作区外可恢复归档，见 [工作区与发布目录](docs/workspace-hygiene.md)。

- [启动 IDE](../release/xlang_ide.exe)
- [Windows x64 便携 ZIP](dist/portable-20261022-standalone-v2/xlang-ide-windows-x64.zip)
- [使用说明](../release/QUICKSTART.md)
- [本轮核验](build/standalone-targets-20261021/result.json)
- [源码构建](docs/current-source-build.md)

当前严格 Release 构建为 `cb-bd75b462`：88 个编译对象（87 个 IDE 模块与 1 个 C 输入观察辅助对象）、109 项 EXE/DLL 产物，构建期间源码变化为空。本轮同一构建通过 20 个相关原生 fixture；新增 40 项断言覆盖实际 C／xlang 构建运行、源目录工作路径、头文件／导入、自动保存、错误诊断、旧程序保护、未保存依赖、未命名取消、独立项目、工作空间所选／启动目标与显式文件覆盖。7 次真实主程序入口检查包含历史工作空间后的源文件启动。

便携包通过 15 项载荷／解压拒绝检查、2 次包内默认工具链运行与搬迁、4 次包内主程序启动及 12 张新图。发布后使用随包编译器再次通过 7 次入口构建运行；`release` 的 5,241 份载荷文件与候选逐文件核对，无测试程序或测试会话，全部打包输入可核验。

SDK 保持 XGE 3.0.0，上游提交 `f248fd17192b3eb81c2a29dc34549b696e6e4c43`，XUI 代理 16，沿用两处已记录的 Unicode 查找兼容补丁；默认编译器与 C 服务器保持原版本。SDK 使用上游正式参数，IDE 应用使用严格警告检查。上次 SDK 的标准分段回归与来源核验见 [XGE 3 更新](docs/xge3-upgrade.md)；本轮未重复全量标准回归、全部标量／聚合 ABI 专项或性能基准。

首版继续为传统 IDE：左侧命令列表，中央编辑器及其下方单栏帮助／输出／终端，右侧上方工作空间、下方项目属性；底栏只占中央宽度。保留 C／xlang 双语法、三层管理及 XUI Window／MsgBox／FileDialog。干净 Windows、实体 IME／混合 DPI 与正式发行验收仍待完成；整体 IDE 长期目标继续。

ZIP SHA-256：`dca6307bdf2bfd82b96e0cde21c623878e5db5b9cee8ff67394eaf1f410750ba`。
