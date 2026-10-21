# Windows x64 便携发布

2026-10-22 已支持 C／xlang 单文件、独立 `.xlp` 和 `.xlw` 工作空间构建运行。无项目时 F7／F5 使用当前源文件；项目上下文保持所选／启动目标规则，新增“项目 → 构建当前文件／构建并运行当前文件”。显式源文件启动优先于历史工作空间恢复。严格构建 `cb-bd75b462` 为 88 个编译对象／109 项产物，构建期间源码变化为空。20 个相关原生 fixture、40 项单文件断言、7 次真实主程序入口与便携验收通过；发布目录已更新。SDK、默认编译器和 C 服务器沿用上一候选。本轮为相关范围回归，未重复全量标准／ABI 专项。见 [四种构建形态](standalone-builds.md) 和 [当前候选](../CURRENT-CANDIDATE.md)。

2026-10-21 已更新为 XGE 3.0.0（`f248fd17`，XUI 代理 16），保留有来源记录的 Unicode 查找兼容补丁。严格构建 `cb-4a6a6215` 为 88 个编译对象（87 个 IDE 模块与 1 个 C 输入观察辅助对象）／108 项产物，构建期间源码变化为空；当前提供者对应的标准检查分段完成（含 29 张基础渲染，原脚本资源夹具分支失败保留），14 项实际编译器／语言服务检查及便携搬迁验收通过。`release` 为完整干净载荷，`dist` 仅保留本候选。适配、证据与范围见 [XGE 3 更新](xge3-upgrade.md) 和 [当前候选](../CURRENT-CANDIDATE.md)。下文保留历史阶段记录。

2026-10-21 工作区整理后，`ide/release` 已改为[当前候选](../CURRENT-CANDIDATE.md)的完整干净副本，`ide/dist` 仅保留该候选；历史阶段的包已归档。此前直接开发构建会向 `release` 写入测试程序，后续请使用 `build_current.bat` 的隔离构建目录，再用 `package_current.bat` 生成候选。目录职责与归档清单见[工作区与发布目录](workspace-hygiene.md)。下文旧阶段的链接与验收数量保留作历史记录。

2026-10-11 当前扩展库交付：SDK67，84 模块／106 产物重新严格构建，含渲染的完整回归和十项实际便携门通过。默认编译器修正 stdio 外部／内联声明冲突，并为真实扩展函数 C 原型提供可验证的 xlang 来源映射。CRC32、MD5 的实际混编、stdio 格式化、诊断、悬停、补全、F12 和间接头变化撤回已通过开发与包内检查；最终构建的 17 项原生界面断言和三张截图、九项编译器兼容、41 项源码材料及原有 74 项聚合／37 项标量对照通过。 详见[扩展库分析与导航](c-native-extensions.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文保留此前阶段的数量和验收记录。

SDK 来源、具名与内嵌许可文本、编译器材料及待完成的发行核对见[发行材料](release-materials.md)。当前打包配置绑定与 XGE 3.0.0 相符的隔离源码及 752 份材料；`-SdkSourceRoot` 必须匹配所选成套 SDK，不从正在变化的依赖工作树补取其他版本文本。

当前[SDK67 开发候选](../CURRENT-CANDIDATE.md)提供[主程序](../dist/portable-20261011-c-record-layout-v1/xlang-ide/xlang_ide.exe)和[ZIP](../dist/portable-20261011-c-record-layout-v1/xlang-ide-windows-x64.zip)。SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 包内源码绑定实际重建产物；完整回归和便携包使用同一精确源码构建。见[C 聚合与位域布局](c-record-layout.md)。此前候选保留，正式发行继续。

首版需要独立运行目录和可复核发布清单。直接开发构建可能向 release 写入测试程序与探针，不能把未经清单核对的目录整体复制为用户发行包。本流程只装配必要程序、离线命令目录、显式选择的完整工具链运行资产、来源材料与相对默认设置。发布候选仍需通过原生和构建运行验收；正式首版的完整输入图、许可证材料、性能及安装环境核对继续按实施计划完成。

## 布局与用户数据

```text
xlang-ide/
  xlang_ide.exe
  xlang_ide_console.exe
  xlang-ls.exe
  xge.dll
  data/commands.json
  data/fonts/{NotoSansMonoCJKsc-Regular.otf,LICENSE,COPYRIGHT.txt,source.json}
  option/settings.json
  toolchains/xlang/{xl.exe,xl.dll,xlc.dll,include/,extlibs/}
  toolchains/clangd/{bin/,lib/,LICENSE.TXT}
  provenance/
  notices/
  sources/{xge/,tcc/,xlang/}
  sources/COMPILER-REBUILD.md
  package-manifest.json
  QUICKSTART.md
```

默认设置使用相对于 option/settings.json 的编译器和 C 服务器路径。程序自己的默认设置重新选择 toolchains 下的工具时也保存相对路径；外部工具或 --settings 指定的外部配置保持绝对绑定。命令文档原文已嵌入 commands.json，不依赖仓库 Markdown 路径。默认优先使用 Windows 字体，缺失时使用随包的开放许可中文等宽字体；随包字体偏好可相对保存。用户选择的外部字体和工作空间不会被打包。

解压到可写目录。option 是用户可变状态区；打包器不读取开发目录的 option，也不覆盖已经存在的候选目录或用户数据。Windows 字体和系统 DLL 属于运行环境；不复制 Windows 字体或系统 DLL。随包 Noto 字体、上游许可证与来源记录属于发行载荷。独立 xlang 服务和编译器各自使用匹配运行时，GUI 的唯一 XRT 仍由固定 xge.dll 提供。

## 创建候选

```powershell
.\tools\package.ps1 `
  -CompilerRoot <已验证工具链的 release 绝对目录> `
  -CServerRoot <已验证 clangd 的 bin/lib/许可证所在绝对目录> `
  -CompilerManifest <编译器冻结输入 JSON> `
  -CServerManifest <clangd 冻结输入 JSON> `
  -SdkSourceRoot <与固定 SDK 摘要一致的源码快照绝对目录> `
  -CompilerSourceManifest <编译器源码及构建输入清单绝对路径> `
  -CompilerSourceBuildReport <对应实际独立构建记录绝对路径>
```

构建开发机需要 Python、PowerShell 与现有 GCC 工具；候选运行时不需要 Python 或 GCC。工具使用新建阶段目录，在输入、复制内容、PE 依赖和 ZIP 全部核对后一次发布候选目录与归档。输入变化、缺少资源、路径冲突和不支持的程序类型均失败；失败阶段保留便于检查，没有递归删除。

候选 manifest 记录每个文件的长度和摘要、相对配置、程序导入依赖、工具链来源和材料范围。普通 SDK、编译器和 clangd 输入保持只读，包中复制件独立。SHA-256 用于内容完整性核对，不作为发行方签名。当前固定 SDK 来自带未提交修改的工作树，必须保留此事实。

## 验收

候选创建后运行 `tools/test.ps1 -SkipBuild -PortablePackagePath <候选目录>`；完整功能回归还需按 [README](../README.md) 同时指定全部已验证编译器与 C 服务器。也可单独执行 `python tools/check_portable.py --package <候选目录> --fixture release/portable_view_test.exe`。原生夹具只注入私有解压副本，不进入发行 ZIP。

在脱离仓库布局、中文空格路径、仅 Windows 系统 PATH 的副本中验证：相对默认设置、4,465 条命令及 45 份长文档、真实 C/xlang 高亮、未保存 xlang 的 C 分析和 F12、混编构建运行、主题与用户偏好保持。归档解压前拒绝越界、重复、连接点/符号链接和超限内容；复核载荷摘要后运行。既有目标和已修改用户配置不能被重打包覆盖。

独立目录运行验收不等于已完成所有干净 Windows 设备、复杂工程和正式首版发布。固定依赖的完整许可证/来源材料及未验证范围必须在候选说明中列出，不因打包成功宣告完成。当前状态见 [实施进度](progress.md)。

上一通过验收的候选位于 [portable-20261008-navigation-final](../dist/portable-20261008-navigation-final/)，主程序为 [xlang_ide.exe](../dist/portable-20261008-navigation-final/xlang-ide/xlang_ide.exe)，可分发归档为 [Windows x64 ZIP](../dist/portable-20261008-navigation-final/xlang-ide-windows-x64.zip)。它包含启动前完整草稿 VFS 与 Ctrl+G 行列定位，避免首次头文件解析读取旧依赖，并提供 Unicode 字素簇、折叠目标展开和整行视口余量；见 [C 语义服务](c-language-service.md) 和 [行列定位](goto-line.md)。上一完整候选 [portable-20261008-drafts](../dist/portable-20261008-drafts/) 及历史 [portable-20261007](../dist/portable-20261007/) 保留原字节和验收。首次导航候选 [portable-20261008-navigation](../dist/portable-20261008-navigation/) 的功能门通过后，在视觉检查中发现底部目标行需补余量；其 [被替代状态](../dist/portable-20261008-navigation/acceptance-status.json) 和 [原记录](../build/goto-pre-margin/verification.json) 保留，不作为当前候选。独立编译器仍采用已验证的 Unicode 模块目录转 UTF-8 修复，已有 512 字节路径预算保持；其他长路径与设备矩阵继续验收。最初的 `portable-20261007-first` 已标记验收失败并保留诊断，不作为当前候选。


2026-10-08 后续客户端候选已装配到 [portable-20261008-input-trace-client](../dist/portable-20261008-input-trace-client/)，已通过最终完整回归与独立迁移验收。它绑定本轮实际 Release 主程序，增加可协商的编译输入观察客户端。默认依旧是固定官方 clangd 的有限显式守卫；隔离的实验性观察服务器保留在开发目录，未装入该包。新增服务器的选择、复现与覆盖边界见 [C 语义服务](c-language-service.md)。历史候选原始产物与回执保留；重建 release 后不能拿旧候选回执中的原始路径作为当前构建输入证明。


2026-10-08 全局修复增量通过完整回归的候选为 [portable-20261008-state-repair-final](../dist/portable-20261008-state-repair-final/)，[主程序](../dist/portable-20261008-state-repair-final/xlang-ide/xlang_ide.exe) 与 [Windows x64 ZIP](../dist/portable-20261008-state-repair-final/xlang-ide-windows-x64.zip) 已包含全局索引、主窗口位置和程序偏好的独立备份/修复入口。它使用本轮冻结 Release 产物，并通过 15 项实际拒绝门、2 次默认原生迁移、4 次包内运行和 12 张便携图。默认 C 服务器仍是官方 clangd，实验性输入观察服务器未装入。修复操作使用真实偏好路径，在该文件父目录新建 `state-backups`，不随包携带开发者原始用户数据。协议见 [恢复维护](recovery-maintenance.md)，完整构建、回归、摘要与剩余发布条件见 [实施进度](progress.md) 和 [最终核验](../build/global-state-final-verification.json)。上一客户端候选及其原始回执保留；本候选仍为开发验收候选，正式首版与干净设备发布矩阵尚未完成。


2026-10-08 上一资源创建候选为 [portable-20261008-resource-create-final](../dist/portable-20261008-resource-create-final/)，[主程序](../dist/portable-20261008-resource-create-final/xlang-ide/xlang_ide.exe) 与 [Windows x64 ZIP](../dist/portable-20261008-resource-create-final/xlang-ide-windows-x64.zip) 已包含零资源项目的新建 Windows 资源配置入口、单栏精确预览、JSON/兼容 XLON 发布及引用更新。当前程序经 72 模块/64 类完整 Release 回归、181 项 xlang 协议检查及独立便携验收；包内默认仍是官方 clangd。2472 个文件通过 15 项拒绝门、2 次默认原生迁移、4 次包内执行和 12 张图。使用见 [资源创建](resource-creation.md)，冻结/历史失败与剩余验收见 [实施进度](progress.md) 和 [最终核验](../build/resource-create-final-verification.json)。初步资源候选的 [被替代状态](../dist/portable-20261008-resource-create/acceptance-status.json) 保留；当前包仍为开发验收候选，正式首版未完成。


2026-10-08 上一候选为 [portable-20261008-project-tests-final](../dist/portable-20261008-project-tests-final/)，[主程序](../dist/portable-20261008-project-tests-final/xlang-ide/xlang_ide.exe) 和 [Windows x64 ZIP](../dist/portable-20261008-project-tests-final/xlang-ide-windows-x64.zip) 包含显式项目测试配置、Ctrl+F7、独立原始日志及 stdin/退出/停止处理。72 模块、66 类测试、181 项 xlang 协议与 146 张原生图通过完整回归；便携验收仍有 2472 个文件、15 项拒绝门、2 次默认原生迁移、4 次包内执行和 12 张图，并在两次默认原生运行中实际执行已配置的 C/xlang 测试程序。测试是所选项目的显式程序，不要求编译器且不隐含构建。见 [项目测试](project-tests.md)、[实施进度](progress.md) 及 [最终核验](../build/project-test-final-verification.json)。ZIP SHA-256：`079a0dafcb715fe5169a911e9cb05609647dcfe2f7fcd45d3811c270d98a655a`。旧资源创建候选与原回执保留，新包仍为开发验收候选，正式首版未完成。


2026-10-08 上一开发验收候选为 [portable-20261008-resource-inputs-final](../dist/portable-20261008-resource-inputs-final/)，提供 [主程序](../dist/portable-20261008-resource-inputs-final/xlang-ide/xlang_ide.exe) 和 [Windows x64 ZIP](../dist/portable-20261008-resource-inputs-final/xlang-ide-windows-x64.zip)。包含资源素材字节保护、目录外脏素材守护、完整项目快照与守护的重复请求核对。73 模块、67 类测试、97 项资源输入检查、181 项 xlang 协议和 146 张原生图通过完整回归；包内保持 2472 文件、15 拒绝门、2 次原生迁移、4 次包内执行与 12 张便携图。迁移前后均执行显式项目测试，采用官方 clangd。ZIP SHA-256：`7ee7a27398df925b9720ec85fe1ce8e9ac0d523239f9341cbfd492d543a75a33`。见 [资源构建输入](resource-build-inputs.md)、[最终核验](../build/resource-input-final-verification.json) 和 [实施进度](progress.md)。首轮 [resource-input 候选](../dist/portable-20261008-resource-input-final/acceptance-status.json) 因后续合并守护修正被替代，成功回归及原记录保留；上一项目测试候选和载荷不改写。整个项目依赖图、现代 XLON 表单、性能、许可和干净 Windows 发布验收仍未完成，formalRelease 保持 false。


2026-10-08 上一开发验收候选为 [portable-20261008-resource-text-final](../dist/portable-20261008-resource-text-final/)，提供 [主程序](../dist/portable-20261008-resource-text-final/xlang-ide/xlang_ide.exe) 与 [Windows x64 ZIP](../dist/portable-20261008-resource-text-final/xlang-ide-windows-x64.zip)。包含现代 XLON 资源属性字段编辑与扩展正文保留；实际 C/xlang 混编、PE 资源和原生单次撤销重做通过。严格 Release 与完整回归通过 74 模块、68 类测试、72 项资源文本检查、181 项协议和 147 张原生图；便携包保持 2472 文件、15 拒绝门、2 次迁移、4 次包内执行及 12 张图，采用官方 clangd。ZIP SHA-256：`0c8a815b0f93cc021db3379eaeb6ae9c6ab6948ba53a766a0fe409c77af63295`。见 [资源属性](windows-resources.md)、[最终核验](../build/resource-text-final-verification.json) 与 [实施进度](progress.md)。上一候选与回执/状态/载荷不改写。完整输入图、资源改名、性能、物理设备、许可及干净 Windows 发布验收继续，formalRelease 为 false。


2026-10-08 当前开发验收候选为 [portable-20261008-resource-rename-v2-final](../dist/portable-20261008-resource-rename-v2-final/)，提供 [主程序](../dist/portable-20261008-resource-rename-v2-final/xlang-ide/xlang_ide.exe) 和 [Windows x64 ZIP](../dist/portable-20261008-resource-rename-v2-final/xlang-ide-windows-x64.zip)。包含资源素材改名路径联动，保持现代 XLON 扩展、脏正文和单步撤销；部分提交失败保留剩余补丁。严格 Release、69 类测试、51 项改名检查、181 项协议、149 张原生图以及真实混编 PE 图标/清单通过；便携保持 2472 文件、15 拒绝门、2 次迁移、4 次包内执行和 12 张图。ZIP SHA-256：`5f8b884a53299e5062ea5e1ccf2f8ba8335957e1488a7ec38de7851655805bb1`。见 [最终核验](../build/resource-rename-v2-final-verification.json) 与 [实施进度](progress.md)。上一已接受候选和初版待修订候选保留原字节。完整输入图、跨文件崩溃重放、性能、物理设备、许可及干净 Windows 发布仍待验收，formalRelease 为 false。

## 迁移正文维护开发预览

[portable-20261009-migration-storage-preview-v1](../dist/portable-20261009-migration-storage-preview-v1/) 通过独立便携验收，提供新增正文所有权和退休副本回收功能。其 [专项报告](../build/migration-storage-preview-v1-verification.json) 明确记录完整回归尚未通过，DPI GPU 资源和 4K 最大化内存失败继续定位。该预览不替换页首完整验收候选。

当前源码材料候选新增 1,532 份实际构建输入与对应来源记录；源码构建在新的可写副本运行，步骤见包内 `sources/COMPILER-REBUILD.md`。省略源码参数时保留明确未提供状态，已有候选不被覆盖；许可和正式发布标志继续保持未完成。


2026-10-11 默认 C 输入观察交付：默认便携 C 后端已加入实际输入观察，未打开的间接头文件变化会撤回旧语义结果并重新分析；目录守卫支持超过 MAX_PATH 的 Unicode 深目录。SDK67 沿用已严格构建的匹配材料，应用重新严格构建为 84 模块／104 产物，含渲染的完整回归及六项实际便携门通过。真实输入观察 113 项接口、21 项界面及 52 项导航上下文检查，69 项短／长目录观察检查和 13 项材料拒绝检查通过；普通 clangd 的五项兼容门和修正后草稿启动门也通过。 提供者为此前成功源码构建的精确复用，本轮没有重新编译 LLVM。材料范围为适配差异和既有构建记录，完整上游源码按固定提交获取；正式发行核对仍继续。详见[默认 C 输入观察](c-input-trace-delivery.md)和[当前候选](../CURRENT-CANDIDATE.md)。
