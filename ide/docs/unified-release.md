# 统一发布目录

2026-10-29，编译器由 `demo6/` 提升到仓库根目录后，IDE 的运行目录合并到根目录 `release/`。

```text
release/
  xlang_ide.exe、xlang_ide_console.exe、xlang-ls.exe、xge.dll
  xl.exe、xl.dll、xlc.dll
  include/、extlibs/、examples/、projects/、res/
  data/                  命令帮助、离线字体
  option/                IDE 设置及会话
  toolchains/clangd/     C 语言分析工具
  notices/、provenance/、sources/
```

IDE 与命令行共用根目录现有引擎和 SDK。迁移不使用旧 IDE 包的引擎、头文件和扩展库覆盖根目录资产。旧 `toolchains/xlang/` 随迁移前目录保存在 `ide/build/` 的可恢复备份中。默认编译器为相对 `option/settings.json` 的 `../xl.exe`，clangd 为 `../toolchains/clangd/bin/clangd.exe`；用户明确选择的外部工具仍使用原配置。

`ide/build_current.bat -Configuration Release -Tests` 继续生成独立源码快照与收据，输出位于 `ide/build/cb-*/release/`。`ide/build.bat` 普通构建输出根 `release/`；带 `-Tests` 时输出 `ide/build/<配置>/release/`。`ide/test.bat` 使用同一测试目录。两个 PowerShell 入口均支持 `-ReleaseDirectory`，供隔离构建与验收使用。

LSP 原生前端保留独立源码摘要守卫。默认优先使用开发配置中固定的前端源码；固定目录不存在时，使用随包的 `release/sources/xlang/`，最后才尝试仓库根源码。源码不匹配仍会拒绝构建；目录合并不放宽检查。可通过 `-FrontendRoot` 显式指定前端。

便携打包同样采用同层布局：IDE、语言服务、引擎位于载荷根目录，头文件与扩展库直接位于 `include/`、`extlibs/`。打包继续验证显式固定的编译器、SDK、clangd 材料；它与本地目录迁移是独立操作。

迁移工具为 `ide/tools/merge_release.py`。它检查冲突、保留根目录已有引擎，只更新 IDE 自有文件，保留设置中的未知字段，并记录安装摘要与引擎绑定。迁移前清单保存在 `release/provenance/ide-original-package-manifest.json`，仅用于追溯历史包；当前引擎以根目录二进制及迁移收据摘要为准。

旧目录在新入口验证后移入 `ide/build/release-unify-*/previous-ide-release/`，不会直接删除。源码、测试、构建中间文件和运行日志仍留在 `ide/` 与 `ide/build/`。

本次目录迁移沿用原发布的 GUI、匹配的 XGE DLL 与 `xlang-ls 0.4`，四个程序文件逐字节保留。开发构建的 0.98 原生分析尚不支持完整的 C / xlang 混编工程配置，继续留在隔离构建中；迁移工具可用 `--language-server` 明确选择保留的服务。当前源码已调整同目录编译器发现及相对路径保存，供后续源码构建使用；本次运行副本通过 `option/settings.json` 的相对路径使用统一引擎。

本轮验证包括实际发布 IDE 的 C / xlang 单文件编译运行、中文空格路径搬迁、原发布语言服务调用，以及隔离开发构建中的设置、命令目录、帮助、来源查看和构建目标检查。迁移结果与各项原始记录位于 `ide/build/release-unify-dd532c61/`；未重新生成发行 ZIP，也未将开发服务替换进运行目录。

提升编译器目录后，命令目录的来源索引同步移除旧 `demo6/` 前缀，绑定现有命令目录的新摘要，335 份原文载荷不变。最终复核核对所有迁移输入、四个发布程序及默认工具路径；外部工具和字体的相对配置转换为原位置的绝对路径，已有统一目录设置以自身所在位置解析。索引的 LF 规范化单独记录，未放宽源码或资产摘要校验。
