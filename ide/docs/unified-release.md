# 统一发布目录

2026-10-29，编译器由 `demo6/` 提升到仓库根目录后，IDE 的运行目录合并到根目录 `release/`。

```text
release/
  xlang_ide.exe、xlang_ide_console.exe、xlang-ls.exe、xge.dll
  xl.exe、xl.dll、xlc.dll
  include/、extlibs/、examples/、projects/、res/
  data/                  命令帮助、离线字体
  option/                IDE 设置及会话
  notices/、provenance/、sources/
```

IDE 与命令行共用根目录现有引擎和 SDK。迁移不使用旧 IDE 包的引擎、头文件和扩展库覆盖根目录资产。旧 `toolchains/xlang/` 随迁移前目录保存在 `ide/build/` 的可恢复备份中。默认编译器为相对 `option/settings.json` 的 `../xl.exe`。2026-10-31 起 C LSP 默认关闭，clangd 及专用材料已移除，xlang-ls 和 C 编译运行保留，见 [C LSP 构建选项](c-lsp-build-option.md)。

`ide/build_current.bat -Configuration Release -Tests` 继续生成独立源码快照与收据，输出位于 `ide/build/cb-*/release/`。`ide/build.bat` 普通构建输出根 `release/`；带 `-Tests` 时输出 `ide/build/<配置>/release/`。`ide/test.bat` 使用同一测试目录。两个 PowerShell 入口均支持 `-ReleaseDirectory`，供隔离构建与验收使用。

LSP 原生前端保留独立源码摘要守卫。默认优先使用开发配置中固定的前端源码；固定目录不存在时，使用随包的 `release/sources/xlang/`，最后才尝试仓库根源码。源码不匹配仍会拒绝构建；目录合并不放宽检查。可通过 `-FrontendRoot` 显式指定前端。

便携打包同样采用同层布局：IDE、语言服务、引擎位于载荷根目录，头文件与扩展库直接位于 `include/`、`extlibs/`。打包继续验证显式固定的编译器和 SDK；默认不读取 C 服务器或其清单。它与本地目录迁移是独立操作。

迁移工具为 `ide/tools/merge_release.py`。它检查冲突、保留根目录已有引擎，只更新 IDE 自有文件，保留设置中的未知字段，并记录安装摘要与引擎绑定。迁移前清单保存在 `release/provenance/ide-original-package-manifest.json`，仅用于追溯历史包；当前引擎以根目录二进制及迁移收据摘要为准。

旧目录在新入口验证后移入 `ide/build/release-unify-*/previous-ide-release/`，不会直接删除。源码、测试、构建中间文件和运行日志仍留在 `ide/` 与 `ide/build/`。

本次目录迁移沿用原发布的 GUI、匹配的 XGE DLL 与 `xlang-ls 0.4`，四个程序文件逐字节保留。开发构建的 0.98 原生分析尚不支持完整的 C / xlang 混编工程配置，继续留在隔离构建中；迁移工具可用 `--language-server` 明确选择保留的服务。当前源码已调整同目录编译器发现及相对路径保存，供后续源码构建使用；本次运行副本通过 `option/settings.json` 的相对路径使用统一引擎。

本轮验证包括实际发布 IDE 的 C / xlang 单文件编译运行、中文空格路径搬迁、原发布语言服务调用，以及隔离开发构建中的设置、命令目录、帮助、来源查看和构建目标检查。迁移结果与各项原始记录位于 `ide/build/release-unify-dd532c61/`；未重新生成发行 ZIP，也未将开发服务替换进运行目录。

提升编译器目录后，命令目录的来源索引同步移除旧 `demo6/` 前缀，绑定现有命令目录的新摘要，335 份原文载荷不变。最终复核核对所有迁移输入、四个发布程序及默认工具路径；外部工具和字体的相对配置转换为原位置的绝对路径，已有统一目录设置以自身所在位置解析。索引的 LF 规范化单独记录，未放宽源码或资产摘要校验。

## 2026-10-30 引擎更新后的兼容复核

根目录最新引擎的构建收据及源码报告已采用 schema 2，时间字段改为 XRT 公元 UTC 毫秒 `modifiedMs` / `changedMs`。IDE 同时支持 schema 1 的 Unix 微秒字段与 schema 2，按报告版本绑定字段，拒绝混合单位、未知版本和异常字段类型。磁盘读取前后仍使用完整微秒精度比较文件身份、大小和时间，并验证内容摘要；仅在与报告比较时转换时间。

本轮以已发布的传统 IDE / CLI 语言服务功能基线重新构建 88 个模块，补入同目录引擎发现和上述兼容修复。XGE DLL 保持原文件；开发中的原生分析服务仍在隔离目录，本次语言服务继续为 CLI 后端的 0.4 功能线。重建输入、派生导入库及原发布 SDK 的头文件、配置和 DLL 绑定均记录在验收目录。

旧、新引擎分别通过 111 项真实构建输入检查，包含损坏收据、时间单位不匹配、身份替换和构建期间变更；语言服务通过 57 项源码快照及异常响应边界检查。7 个真实 IDE 入口场景覆盖 C / xlang 单文件、独立项目、工作空间、显式文件覆盖和会话恢复；另验收无编译器参数的同目录发现及中文空格路径搬迁。验收资料位于 `ide/build/release-compat-20261030-a6c17d92/`，正式安装记录放在根 `release/provenance/ide-receipt-compatibility.json`。
