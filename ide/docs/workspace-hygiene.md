# 工作区与发布目录

2026-10-31：当前运行目录统一为仓库根目录 `release/`。发布清理已迁出验收夹具、移除临时编译缓存与开发示例生成的 EXE，保留源码、用户设置、固定工具链和许可来源材料。当前规则以 [IDE 构建约定](../AGENTS.md) 和 [发布目录说明](../../release/README.md) 为准；清理结果见 [本次记录](../../docs/reports/release-cleanup-20261031.json)。

源码快照和交付／验收报告已从 `release/sources`、`release/provenance` 迁到 [dev/release-materials](../../dev/release-materials/README.md)，完整保留，不属于可删除的构建缓存。日常程序和项目编译不需要这两个目录；重建与打包工具使用新路径。

清理使用仓库 `tools/clean_release.ps1`，只处理明确的生成文件，检查路径边界和重解析点，并校验保留内容及迁出证据的哈希。运行验收使用根 `release/` 的真实程序，日志及夹具默认放在 `ide/build/Release/<本轮目录>/`，不为验证材料创建 `release/tmp`。专项临时文件测试结束后清理自有内容，以及本轮新建的空父目录。此前清理迁出的证据按报告中的映射在 `build/release-cleanup-*` 查找。

以下为 2026-10-22 及之前的历史记录，候选名称与绝对路径保留原文。

2026-10-22 四种构建形态交付后，`release` 为 `portable-20261022-standalone-v2` 的完整干净运行副本，`dist` 仅保留该候选。当前构建 `cb-bd75b462`、相关回归与便携验收绑定同一精确源码。本轮归档位于 `D:\GIT\x-lang-ide-archive\20261022-standalone-targets`，`publication.json` 记录原位置和归档位置，保留上一发布、上一构建及本轮中间候选／构建；旧文件通过同卷移动保留，未删除。

上次 XGE 3 更新材料仍保留在 `D:\GIT\x-lang-ide-archive\20261021-xge3-upgrade`。固定 SDK 源码与编译器、C 服务器材料继续在工作区供打包核验使用。

此前工作区清理归档了 148 项、268,813 个普通文件，约 38.26 GiB，仍保存在 `20261021-cleanup-v1`；这属于此前阶段数量，不包含本次升级归档，也不表示释放了相应磁盘空间。

## 当前目录

| 路径 | 职责 |
| --- | --- |
| `src`、`res`、`tests`、`tools`、`docs` | 源码、资源、测试与开发资料 |
| `data`、`third_party` | 固定数据、匹配 SDK 与来源材料 |
| `release` | 当前验证包的完整运行副本，可直接运行 `xlang_ide.exe` |
| `dist/portable-20261022-standalone-v2` | 原验证 ZIP 与原始打包收据；重复解包副本已清理 |
| `build/current-build.json`、`build/cb-bd75b462` | 当前成功构建的指针、源码快照、编译日志及应用／测试产物 |
| `build/compiler-c-native-extensions-v1` | 固定编译器、对应源码及构建证据 |
| `build/c-trace-provider-v1` | 固定 C 服务器、运行资产及来源材料 |
| `build/xge3-upgrade-20261021/source` | 当前 XGE 3 提交的隔离源码及明确记录的查找兼容补丁，打包固定输入 |
| `build/editor-sdk-find-boundaries-051be50ae701` | SDK67 历史冻结材料 |
| `build/*manifest*.json` 与固定输入／构建报告 | `data/portable-build-inputs.json` 依赖的摘要与证据 |
| `build/tests/portable/acceptance-ac792df5adc24d7e80a15f96391e9854` | 当前候选的便携验收证据 |
| `build/standalone-targets-20261021` | 本轮单文件、项目与工作空间构建运行证据、发布后入口检查 |
| `build/xui-file-dialogs-20261012`、`build/window-msgbox-20261012` | 此前 XUI 窗口与文件选择增量的核验记录 |

不要整体删除 `build`：当前打包配置和收据仍引用其中的编译器、C 服务器、SDK 源码、实际构建及其证据。这些材料与普通缓存不同。

`release` 顶层应用程序仅为 `xlang_ide.exe`、`xlang_ide_console.exe`、`xlang-ls.exe` 与 `xge.dll`；另含运行数据、相对默认设置、工具链、来源、许可和清单。旧 `*_test.exe`、探针与验证辅助目录已归档。`option` 初始只含包内默认 `settings.json`，没有带入开发会话或恢复文件。使用 IDE 后生成的偏好与恢复文件属于用户状态，后续整理时应先保留。

## 日常开发

2026-10-29 清理了早期 LSP 验证批次的重复源码快照、二进制与大型原始转录，保留小型阶段回执、日志和当前 M70 开发目录。历史回执记录当时的验收结果，不表示已清理的产物仍可用于复测；需要复测时应重新构建。便携候选的 5,241 份解包文件逐项核对 ZIP 路径、长度和 CRC 后删除重复副本，ZIP 与打包收据保持原字节，日常运行和使用说明继续使用 `release`。

```powershell
.\build_current.bat -Configuration Release -Tests
$build = Get-Content .\build\current-build.json -Raw | ConvertFrom-Json
& (Join-Path $build.root 'release\xlang_ide.exe')
```

构建与测试程序保留在新建的 `build/cb-xxxxxxxx` 内。`-Tests` 只编译测试；回归调用该快照中的 `tools/test.ps1 -SkipBuild`，并按验收范围指定真实编译器与 C 服务器。通过相关检查后，用 `package_current.bat` 创建新的便携候选；打包器检查当前源码、构建与固定输入，不从工作区 `release` 补取程序。

直接入口 `build.bat`、`test.bat` 仍兼容旧开发流程，会向工作区 `release` 写入产物。需要保持此目录干净时，使用上面的隔离构建流程。新的发布目录应从已核验候选的载荷完整复制，并用 `package-manifest.json` 验证全部文件、摘要和额外文件，不能只替换 EXE 或 DLL。

## 历史归档

归档根目录：`D:\GIT\x-lang-ide-archive\20261021-cleanup-v1`。

保留了 77 个旧便携候选、61 份受保护状态目录、4 份旧源码构建、2 份旧便携验收目录、2 份旧全量回归目录、Python 缓存，以及整理前的完整 `release`。原有相对目录结构保留，例如历史 `ide/dist/portable-20261010-editor-status-v1` 对应归档中的 `dist/portable-20261010-editor-status-v1`。

归档根目录的 `cleanup-report.json` 记录每个原位置、归档位置、文件数、字节数与移动状态；`plan.txt`、`resume-plan.txt`、`cleanup.ps1` 和 `verify.py` 保留实际过程。回归夹具中的目录联接单独记录，不遍历或修改目标。旧文档、日志和收据的历史绝对路径没有改写，查找已移动产物时按这份映射定位。

恢复旧产物时，先核对清单，使用对应归档路径；当前位置已有文件时应恢复到独立位置并比较，避免覆盖当前候选或新用户状态。

## 本次检查

当前候选的 5,241 份载荷文件、ZIP、打包收据与 5,235 项输入逐项核对。`release` 与候选使用同一清单和摘要，没有额外文件。20 个相关原生 fixture、7 次主程序入口及便携验收通过；发布后再次使用随包编译器检查七种入口场景（含历史会话），测试设置和证据只写入 `build/standalone-targets-20261021/release-smoke`，之后再次严格核对载荷。

本轮证据见 [构建形态核验](../build/standalone-targets-20261021/result.json)，SDK 更新见 [上轮核验](../build/xge3-upgrade-20261021/result.json)，此前清理见 [原清理核验](../build/workspace-cleanup-20261021.json)。历史绝对路径保留原文；查找已移动产物时查看对应归档的 `cleanup-report.json` 或 `publication.json`，按映射定位。

ZIP SHA-256：`dca6307bdf2bfd82b96e0cde21c623878e5db5b9cee8ff67394eaf1f410750ba`。

正式发行和跨设备验收状态以 [当前候选](../CURRENT-CANDIDATE.md) 为准。
